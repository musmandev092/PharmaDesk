#include "service/BackupService.h"

#include "Branding.h"
#include "data/SettingsRepository.h"
#include "domain/AppPaths.h"
#include "domain/SettingsKeys.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSqlQuery>

BackupService::Result BackupService::backupNow(const QString &destDir, const QString &nowStamp)
{
    Result r;
    if (destDir.trimmed().isEmpty()) {
        r.error = QStringLiteral("No backup folder is set.");
        return r;
    }
    QDir dir(destDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        r.error = QStringLiteral("Backup folder does not exist and could not be created.");
        return r;
    }

    // Fold the WAL into the main DB file so a plain file copy is consistent.
    QSqlQuery(m_db).exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"));

    const QString src = AppPaths::dbFilePath();
    const QString dest
        = dir.filePath(QStringLiteral("%1_%2.sqlite").arg(Branding::backupPrefix(), nowStamp));
    QFile::remove(dest);
    if (!QFile::copy(src, dest)) {
        r.error = QStringLiteral("Could not copy the database to %1.").arg(dest);
        return r;
    }
    // The backup is a full copy of the DB — bcrypt PIN hashes, patient names,
    // controlled-drug records. Lock it to the owner (best-effort: a FAT/exFAT
    // USB stick won't honor POSIX perms, but a real filesystem will).
    QFile::setPermissions(dest, QFile::ReadOwner | QFile::WriteOwner);
    r.ok = true;
    r.path = dest;
    return r;
}

bool BackupService::autoBackupIfDue(SettingsRepository &settings, const QString &nowIso,
                                    const QString &nowStamp)
{
    if (settings.get(SettingsKeys::AutoBackup) != QLatin1String("1")) {
        return false;
    }
    const QString dir = settings.get(SettingsKeys::BackupDir);
    if (dir.isEmpty()) {
        return false;
    }
    const QString last = settings.get(SettingsKeys::LastBackupAt);
    if (!last.isEmpty()) {
        const QDateTime lastDt = QDateTime::fromString(last, Qt::ISODate);
        const QDateTime nowDt = QDateTime::fromString(nowIso, Qt::ISODate);
        if (lastDt.isValid() && nowDt.isValid() && lastDt.secsTo(nowDt) < 24 * 3600) {
            return false; // backed up within the last 24h
        }
    }
    const Result r = backupNow(dir, nowStamp);
    if (r.ok) {
        settings.set(SettingsKeys::LastBackupAt, nowIso, -1);
        return true;
    }
    return false;
}
