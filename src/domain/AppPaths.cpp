#include "domain/AppPaths.h"

#include "Branding.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace AppPaths {

static QString baseDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

bool ensureDirs()
{
    QDir dir(baseDir());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }
    dir.mkpath(QStringLiteral("logs"));
    return dir.mkpath(QStringLiteral("logos"));
}

QString dbFilePath()
{
    return baseDir() + QStringLiteral("/") + Branding::dbFileName();
}

QString logosDir()
{
    return baseDir() + QStringLiteral("/logos");
}

QString logsDir()
{
    return baseDir() + QStringLiteral("/logs");
}

void hardenPermissions()
{
    const QFile::Permissions ownerOnlyDir = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
    const QFile::Permissions ownerOnlyFile = QFile::ReadOwner | QFile::WriteOwner;

    QFile::setPermissions(baseDir(), ownerOnlyDir);
    for (const QString &suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm")}) {
        const QString f = dbFilePath() + suffix;
        if (QFile::exists(f)) {
            QFile::setPermissions(f, ownerOnlyFile);
        }
    }

    // Secrets + logs are owner-only too: zreport.key signs the tamper-evident
    // Z-report archive, and the DB / logs hold bcrypt PIN hashes and patient
    // identifiers. Don't leave them world-readable.
    const QString keyFile = baseDir() + QStringLiteral("/zreport.key");
    if (QFile::exists(keyFile)) {
        QFile::setPermissions(keyFile, ownerOnlyFile);
    }
    if (QFile::exists(logsDir())) {
        QFile::setPermissions(logsDir(), ownerOnlyDir);
        QDir ld(logsDir());
        for (const QString &f : ld.entryList(QDir::Files)) {
            QFile::setPermissions(ld.filePath(f), ownerOnlyFile);
        }
    }
}

// Copies a legacy DB (plus -wal/-shm sidecars and logos/) into the current
// location. Returns true if a legacy DB existed and was migrated.
static bool migrateFrom(const QString &legacyBaseDir, const QString &legacyDbFileName)
{
    const QString legacyDb = legacyBaseDir + QStringLiteral("/") + legacyDbFileName;
    if (!QFile::exists(legacyDb)) {
        return false;
    }
    ensureDirs();
    // Copy the DB (and WAL sidecars + logos) so the user keeps their setup.
    for (const QString &suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm")}) {
        const QString src = legacyDb + suffix;
        if (QFile::exists(src)) {
            QFile::copy(src, dbFilePath() + suffix);
        }
    }
    const QDir legacyLogos(legacyBaseDir + QStringLiteral("/logos"));
    if (legacyLogos.exists()) {
        for (const QString &f : legacyLogos.entryList(QDir::Files)) {
            QFile::copy(legacyLogos.filePath(f), logosDir() + QLatin1Char('/') + f);
        }
    }
    return true;
}

void migrateLegacyData()
{
    if (QStandardPaths::isTestModeEnabled()) {
        return; // tests use an isolated location; never import real data
    }
    if (QFile::exists(dbFilePath())) {
        return; // current location already has data
    }
    const QString home = QDir::homePath();
    // Older interim build that shipped under the new product name.
    if (migrateFrom(
            home
                + QStringLiteral("/.local/share/PharmacyManagementSystem/PharmacyManagementSystem"),
            QStringLiteral("cphc_pharmacy.sqlite"))) {
        return;
    }
    // Original CPHC Pharmacy location.
    if (migrateFrom(home + QStringLiteral("/.local/share/CPHC/CPHC Pharmacy"),
                    QStringLiteral("cphc_pharmacy.sqlite"))) {
        return;
    }
}

} // namespace AppPaths
