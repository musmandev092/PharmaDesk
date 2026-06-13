#pragma once

#include <QSqlDatabase>
#include <QString>

class SettingsRepository;

// Automated SQLite backup. Pharmacy records aren't recoverable if lost, so the
// DB file is copied (after a WAL checkpoint to fold the -wal into the main file)
// to an operator-chosen folder — ideally off-machine (USB / network share).
class BackupService
{
public:
    explicit BackupService(QSqlDatabase db) : m_db(std::move(db)) {}

    struct Result
    {
        bool ok = false;
        QString path;
        QString error;
    };

    // Copies the live DB to destDir/pharmadesk_YYYYMMDD_HHMMSS.sqlite.
    Result backupNow(const QString &destDir, const QString &nowStamp);

    // Runs a backup when auto-backup is on, a folder is set, and the last
    // backup is missing or older than 24h. Records last_backup_at on success.
    // Returns true if a backup was performed.
    bool autoBackupIfDue(SettingsRepository &settings, const QString &nowIso,
                         const QString &nowStamp);

private:
    QSqlDatabase m_db;
};
