#include "domain/DesktopIntegration.h"

#include "Branding.h"
#include "data/SettingsRepository.h"
#include "domain/SettingsKeys.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

namespace DesktopIntegration {
namespace {

// Relative locations of the bundled assets inside the mounted AppDir. We probe
// both the flat top-level layout (linuxdeploy default) and the conventional
// usr/share tree.
QStringList desktopSourceCandidates(const QString &appDir)
{
    const QString base = Branding::desktopId() + QStringLiteral(".desktop");
    return {
        appDir + QStringLiteral("/") + base,
        appDir + QStringLiteral("/usr/share/applications/") + base,
    };
}

QStringList iconSourceCandidates(const QString &appDir)
{
    const QString base = Branding::iconName() + QStringLiteral(".png");
    return {
        appDir + QStringLiteral("/") + base,
        appDir + QStringLiteral("/usr/share/icons/hicolor/256x256/apps/") + base,
    };
}

// Returns the first existing path from the candidate list, or an empty string.
QString firstExisting(const QStringList &candidates)
{
    for (const QString &path : candidates) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    return QString();
}

// Resolve the mounted AppDir: prefer $APPDIR, fall back to the directory that
// contains the $APPIMAGE file.
QString resolveAppDir()
{
    if (qEnvironmentVariableIsSet("APPDIR")) {
        const QString appDir = qEnvironmentVariable("APPDIR");
        if (!appDir.isEmpty()) {
            return appDir;
        }
    }
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    if (appImage.isEmpty()) {
        return QString();
    }
    return QFileInfo(appImage).absolutePath();
}

// ~/.local/share (or the platform equivalent).
QString userDataDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
}

// Copy src -> dest, creating the destination directory. Overwrites only when
// the existing destination is older (or absent). Returns true if dest exists
// afterwards.
bool installFile(const QString &src, const QString &dest)
{
    if (src.isEmpty() || !QFile::exists(src)) {
        return false;
    }

    const QFileInfo destInfo(dest);
    QDir().mkpath(destInfo.absolutePath());

    if (destInfo.exists()) {
        const QFileInfo srcInfo(src);
        if (destInfo.lastModified() >= srcInfo.lastModified()) {
            return true; // already up to date
        }
        QFile::remove(dest);
    }

    return QFile::copy(src, dest);
}

// Install the bundled .desktop into the user's applications dir, rewriting the
// Exec line to launch the actual AppImage file. The bundled copy ships
// "Exec=pharmadesk" — correct *inside* the AppImage, where AppRun puts the
// binary on PATH — but a menu/dock launcher runs Exec directly, so the
// installed entry must point at the absolute AppImage path or clicking it does
// nothing. Also pins StartupWMClass so the running window maps to this entry
// (dock icon on GNOME/Wayland). Returns true if the entry exists afterwards.
bool installDesktopFile(const QString &src, const QString &dest, const QString &appImage)
{
    if (src.isEmpty() || appImage.isEmpty()) {
        return false;
    }
    QFile in(src);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QStringList lines = QString::fromUtf8(in.readAll()).split(QLatin1Char('\n'));
    in.close();

    bool haveWmClass = false;
    for (QString &line : lines) {
        if (line.startsWith(QStringLiteral("Exec="))) {
            line = QStringLiteral("Exec=\"%1\" %U").arg(appImage);
        } else if (line.startsWith(QStringLiteral("StartupWMClass="))) {
            haveWmClass = true;
        }
    }
    if (!haveWmClass) {
        lines << QStringLiteral("StartupWMClass=") + Branding::desktopId();
    }
    const QByteArray out = lines.join(QLatin1Char('\n')).toUtf8();

    QDir().mkpath(QFileInfo(dest).absolutePath());
    // Only rewrite when the content actually changed, so we don't reset the
    // file's mtime (and trip cache refreshes) on every launch.
    QFile existing(dest);
    if (existing.exists() && existing.open(QIODevice::ReadOnly)) {
        const bool same = existing.readAll() == out;
        existing.close();
        if (same) {
            return true;
        }
        QFile::remove(dest);
    }
    QFile f(dest);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    f.write(out);
    return true;
}

// Fire-and-forget cache refresh. Failures (tool missing, non-zero exit) are
// ignored by design.
void refreshCaches(const QString &applicationsDir, const QString &hicolorDir)
{
    QProcess::startDetached(QStringLiteral("update-desktop-database"), {applicationsDir});
    QProcess::startDetached(QStringLiteral("gtk-update-icon-cache"), {hicolorDir});
}

} // namespace

QString installIfAppImage(SettingsRepository &settings)
{
    if (!qEnvironmentVariableIsSet("APPIMAGE")) {
        return QString(); // not running from an AppImage
    }

    const QString appDir = resolveAppDir();
    if (appDir.isEmpty()) {
        return QString();
    }

    const QString dataDir = userDataDir();
    if (dataDir.isEmpty()) {
        return QString();
    }

    const QString applicationsDir = dataDir + QStringLiteral("/applications");
    const QString hicolorDir = dataDir + QStringLiteral("/icons/hicolor");
    const QString iconAppsDir = hicolorDir + QStringLiteral("/256x256/apps");

    const QString desktopDest = applicationsDir + QStringLiteral("/") + Branding::desktopId()
                                + QStringLiteral(".desktop");
    const QString iconDest
        = iconAppsDir + QStringLiteral("/") + Branding::iconName() + QStringLiteral(".png");

    const QString desktopSrc = firstExisting(desktopSourceCandidates(appDir));
    const QString iconSrc = firstExisting(iconSourceCandidates(appDir));

    // Skip silently if neither bundled asset is present.
    if (desktopSrc.isEmpty() && iconSrc.isEmpty()) {
        return QString();
    }

    installDesktopFile(desktopSrc, desktopDest, qEnvironmentVariable("APPIMAGE"));
    installFile(iconSrc, iconDest);

    refreshCaches(applicationsDir, hicolorDir);

    // Record the installed version so we can distinguish a first install from
    // an upgrade, and announce it once. updatedBy = -1: no user is logged in
    // this early in startup.
    const QString prev = settings.get(SettingsKeys::InstalledVersion);
    const QString cur = Branding::appVersion();
    if (prev != cur) {
        settings.set(SettingsKeys::InstalledVersion, cur, -1);
    }
    if (prev.isEmpty()) {
        return QStringLiteral("%1 has been added to your applications menu.\n"
                              "Next time, launch it from the menu (or pin it to your dock).")
            .arg(Branding::productName());
    }
    if (prev != cur) {
        return QStringLiteral("Updated to v%1 successfully.").arg(cur);
    }
    return QString();
}

} // namespace DesktopIntegration
