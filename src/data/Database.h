#pragma once

#include <QSqlDatabase>
#include <QString>

// Owns the single SQLite connection. Opens the DB at AppPaths::dbFilePath(),
// enables foreign keys + WAL, runs the bundled schema on a fresh file, and
// answers the first-run probe. Data access only — no business rules.
class Database
{
public:
    // Opens (or creates) the DB and runs PRAGMAs. Returns false on failure;
    // errorString() then holds the reason.
    bool open();

    // True when the DB has no schema yet (no `users` table). Call before
    // bootstrap().
    bool isFreshDb();

    // Executes sql/schema_sqlite.sql (from the qrc) inside a transaction.
    // Returns false + errorString() on failure.
    bool bootstrap();

    // First-run probe: true when no active, non-deleted ADMIN user exists.
    // Robust to a DB that was bootstrapped but whose wizard was abandoned.
    bool isFirstRun();

    // Applies every pending schema migration, gated by PRAGMA user_version, each
    // in its own transaction. schema_sqlite.sql is the frozen v0 baseline; all
    // later schema changes ship as numbered migrations (never edit the baseline).
    // Idempotent and safe on both fresh and existing DBs. Call after bootstrap().
    bool migrate();

    // Current on-disk schema version (PRAGMA user_version; 0 on a baseline DB).
    int schemaVersion();

    // True if `column` exists on `table` (PRAGMA table_info). For guard-clauses
    // in migrations that must stay idempotent across partially-upgraded DBs.
    bool columnExists(const QString &table, const QString &column);

    QSqlDatabase &handle() { return m_db; }
    QString errorString() const { return m_error; }

private:
    bool exec(const QString &sql);
    bool setSchemaVersion(int version);

    QSqlDatabase m_db;
    QString m_error;
};
