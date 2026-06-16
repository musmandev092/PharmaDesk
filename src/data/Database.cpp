#include "data/Database.h"

#include "domain/AppPaths.h"

#include <QFile>
#include <QSqlError>
#include <QSqlQuery>

bool Database::open()
{
    if (!AppPaths::ensureDirs()) {
        m_error = QStringLiteral("Could not create the application data directory.");
        return false;
    }
    AppPaths::migrateLegacyData(); // carry over data from the pre-rename location

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    m_db.setDatabaseName(AppPaths::dbFilePath());
    if (!m_db.open()) {
        m_error = m_db.lastError().text();
        return false;
    }

    // foreign_keys: off by default in SQLite. journal_mode=WAL: fast + safe.
    // synchronous=FULL: this is a cash till on a single PC with no guaranteed
    // UPS — the last committed sale must survive a power cut, not just an app
    // crash (WAL's default NORMAL can lose the final commit on power loss).
    // busy_timeout: wait out a transient lock (WAL checkpoint, auto-backup
    // reader) instead of failing a sale the instant another connection holds it.
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("PRAGMA foreign_keys = ON;"))
        || !q.exec(QStringLiteral("PRAGMA journal_mode = WAL;"))
        || !q.exec(QStringLiteral("PRAGMA synchronous = FULL;"))
        || !q.exec(QStringLiteral("PRAGMA busy_timeout = 5000;"))) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

bool Database::isFreshDb()
{
    QSqlQuery q(m_db);
    q.exec(
        QStringLiteral("SELECT 1 FROM sqlite_master WHERE type='table' AND name='users' LIMIT 1"));
    return !q.next();
}

// Splits a SQL script into statements. Aware of single-quoted strings and of
// CREATE TRIGGER ... BEGIN ... END blocks, so a ';' inside a trigger body (or a
// string) is not mistaken for a statement terminator.
static QStringList splitSqlStatements(const QString &sql)
{
    QStringList out;
    QString cur;
    QString word;
    bool inString = false;
    int blockDepth = 0;

    auto flushWord = [&]() {
        if (word.isEmpty()) {
            return;
        }
        const QString u = word.toUpper();
        if (u == QLatin1String("BEGIN") || u == QLatin1String("CASE")) {
            ++blockDepth;
        } else if (u == QLatin1String("END")) {
            blockDepth = qMax(0, blockDepth - 1);
        }
        word.clear();
    };

    for (int i = 0; i < sql.size(); ++i) {
        const QChar c = sql.at(i);
        if (inString) {
            cur += c;
            if (c == QLatin1Char('\'')) {
                if (i + 1 < sql.size() && sql.at(i + 1) == QLatin1Char('\'')) {
                    cur += sql.at(++i); // escaped '' inside a string
                } else {
                    inString = false;
                }
            }
            continue;
        }
        if (c == QLatin1Char('\'')) {
            flushWord();
            inString = true;
            cur += c;
            continue;
        }
        if (c.isLetter() || c == QLatin1Char('_')) {
            word += c;
            cur += c;
            continue;
        }
        flushWord();
        if (c == QLatin1Char(';') && blockDepth == 0) {
            const QString s = cur.trimmed();
            if (!s.isEmpty()) {
                out << s;
            }
            cur.clear();
        } else {
            cur += c;
        }
    }
    flushWord();
    const QString tail = cur.trimmed();
    if (!tail.isEmpty()) {
        out << tail;
    }
    return out;
}

bool Database::bootstrap()
{
    QFile f(QStringLiteral(":/sql/schema_sqlite.sql"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_error = QStringLiteral("Bundled schema resource is missing.");
        return false;
    }
    const QString raw = QString::fromUtf8(f.readAll());

    // Strip "--" line comments first: they may contain ';' (which would
    // otherwise create bogus statement fragments when we split). No "--" or
    // ";" appears inside a string literal in this schema, so cutting each line
    // at its first "--" is safe.
    QString schema;
    const QStringList lines = raw.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const int c = line.indexOf(QStringLiteral("--"));
        schema += (c >= 0 ? line.left(c) : line);
        schema += QLatin1Char('\n');
    }

    if (!m_db.transaction()) {
        m_error = m_db.lastError().text();
        return false;
    }

    // Split into statements (comments already removed); trigger/string aware.
    const QStringList statements = splitSqlStatements(schema);
    QSqlQuery q(m_db);
    for (const QString &raw : statements) {
        const QString stmt = raw.trimmed();
        if (stmt.isEmpty()) {
            continue;
        }
        if (!q.exec(stmt)) {
            m_error = q.lastError().text() + QStringLiteral("\n--- in statement ---\n") + stmt;
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit()) {
        m_error = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}

bool Database::isFirstRun()
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT 1 FROM users WHERE role='ADMIN' AND is_active=1 AND deleted_at IS NULL LIMIT 1"));
    return !q.next();
}

bool Database::exec(const QString &sql)
{
    QSqlQuery q(m_db);
    if (!q.exec(sql)) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

// ── Schema migrations ───────────────────────────────────────────────────────
// Append-only, ordered registry. schema_sqlite.sql is the version-0 baseline;
// every schema change after it is added here as the next numbered migration and
// NEVER folded back into the baseline file. A fresh DB bootstraps the baseline
// (user_version stays 0) and then runs 1..N; an existing DB runs (current+1)..N.
// Both paths converge on the same schema.
namespace {

struct Migration
{
    int version;
    QStringList statements;
};

const QVector<Migration> &migrationRegistry()
{
    static const QVector<Migration> kMigrations = {
        // v1 (Phase 2): supplier-return settlement tracking on `returns`. The
        // baseline schema already carries the PENDING_REVIEW/RETURN_TO_SUPPLIER
        // statuses; these columns close the supplier-credit loop.
        {1,
         {
             QStringLiteral("ALTER TABLE returns ADD COLUMN supplier_settled_at TEXT"),
             QStringLiteral("ALTER TABLE returns ADD COLUMN supplier_settled_by INTEGER"),
             QStringLiteral("ALTER TABLE returns ADD COLUMN supplier_reference TEXT"),
         }},
        // v2 (Phase 4): append-only archive of HMAC-signed end-of-shift Z-reports
        // (one per session) for tamper-evident day-close records.
        {2,
         {
             QStringLiteral("CREATE TABLE IF NOT EXISTS z_report_archive ("
                            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                            " session_id INTEGER NOT NULL UNIQUE,"
                            " generated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
                            " generated_by INTEGER,"
                            " payload TEXT NOT NULL,"
                            " signature_hex TEXT NOT NULL)"),
         }},
        // v3 (Redesign): collapse the old multi-theme keys to the two shipped
        // themes — Light ("") and Dark ("dark"). Old "6" (Night) → dark; every
        // other legacy key (1..5) → light.
        {3,
         {
             QStringLiteral("UPDATE settings SET value='dark' WHERE key='theme' AND value='6'"),
             QStringLiteral(
                 "UPDATE settings SET value='' WHERE key='theme' AND value NOT IN ('','dark')"),
         }},
        // v4 (Security): brute-force PIN-login lockout. Track consecutive failed
        // attempts and an exponential-backoff lock window on the users row.
        {4,
         {
             QStringLiteral(
                 "ALTER TABLE users ADD COLUMN failed_attempts INTEGER NOT NULL DEFAULT 0"),
             QStringLiteral("ALTER TABLE users ADD COLUMN locked_until TEXT"),
         }},
        // v5 (Security/audit): make the signed Z-report archive append-only at
        // the DB level, mirroring audit_log_no_update/_no_delete. A regulator-
        // facing day-close record must not be silently altered or removed by
        // anyone with raw SQLite access.
        {5,
         {
             QStringLiteral("CREATE TRIGGER IF NOT EXISTS z_report_archive_no_update "
                            "BEFORE UPDATE ON z_report_archive BEGIN "
                            "SELECT RAISE(ABORT, 'z_report_archive is append-only: UPDATE is not "
                            "allowed'); END"),
             QStringLiteral("CREATE TRIGGER IF NOT EXISTS z_report_archive_no_delete "
                            "BEFORE DELETE ON z_report_archive BEGIN "
                            "SELECT RAISE(ABORT, 'z_report_archive is append-only: DELETE is not "
                            "allowed'); END"),
         }},
        // v6 (Data integrity, Phase 5): index foreign-key / hot-join columns that
        // SQLite does not auto-index (esp. returns.sale_item_id, scanned on every
        // return), and enforce the inventory-ledger arithmetic invariant
        // (qty_after = qty_before + qty_delta) with a trigger — SQLite cannot ADD a
        // CHECK to an existing table, and a trigger needs no risky table rebuild.
        // All additive and reversible (DROP INDEX / DROP TRIGGER).
        {6,
         {
             QStringLiteral("CREATE INDEX IF NOT EXISTS idx_returns_sale_item "
                            "ON returns(sale_item_id)"),
             QStringLiteral("CREATE INDEX IF NOT EXISTS idx_returns_medicine "
                            "ON returns(medicine_id)"),
             QStringLiteral("CREATE INDEX IF NOT EXISTS idx_returns_batch "
                            "ON returns(batch_id)"),
             QStringLiteral("CREATE INDEX IF NOT EXISTS idx_batches_supplier "
                            "ON batches(supplier_id)"),
             QStringLiteral("CREATE INDEX IF NOT EXISTS idx_batches_grn "
                            "ON batches(grn_id)"),
             QStringLiteral("CREATE INDEX IF NOT EXISTS idx_grn_lines_medicine "
                            "ON grn_lines(medicine_id)"),
             QStringLiteral("CREATE INDEX IF NOT EXISTS idx_grn_lines_batch "
                            "ON grn_lines(batch_id)"),
             QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sales_session "
                            "ON sales(cashier_session_id)"),
             QStringLiteral("CREATE INDEX IF NOT EXISTS idx_stock_adj_medicine "
                            "ON stock_adjustments(medicine_id)"),
             QStringLiteral("CREATE INDEX IF NOT EXISTS idx_grn_docs_received_by "
                            "ON grn_documents(received_by)"),
             QStringLiteral("CREATE TRIGGER IF NOT EXISTS inv_mv_consistent "
                            "BEFORE INSERT ON inventory_movements "
                            "WHEN NEW.qty_after <> NEW.qty_before + NEW.qty_delta BEGIN "
                            "SELECT RAISE(ABORT, 'inventory_movements: qty_after must equal "
                            "qty_before + qty_delta'); END"),
         }},
    };
    return kMigrations;
}

} // namespace

int Database::schemaVersion()
{
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("PRAGMA user_version")) || !q.next()) {
        return 0;
    }
    return q.value(0).toInt();
}

bool Database::setSchemaVersion(int version)
{
    // PRAGMA does not accept bound parameters; version is an internal int.
    return exec(QStringLiteral("PRAGMA user_version = %1").arg(version));
}

bool Database::columnExists(const QString &table, const QString &column)
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table));
    while (q.next()) {
        if (q.value(1).toString() == column) {
            return true;
        }
    }
    return false;
}

bool Database::migrate()
{
    int current = schemaVersion();
    for (const Migration &m : migrationRegistry()) {
        if (m.version <= current) {
            continue; // already applied
        }
        if (!m_db.transaction()) {
            m_error = m_db.lastError().text();
            return false;
        }
        QSqlQuery q(m_db);
        for (const QString &stmt : m.statements) {
            if (!q.exec(stmt)) {
                m_error = q.lastError().text()
                          + QStringLiteral("\n--- in migration %1 ---\n").arg(m.version) + stmt;
                m_db.rollback();
                return false;
            }
        }
        if (!setSchemaVersion(m.version) || !m_db.commit()) {
            if (m_error.isEmpty()) {
                m_error = m_db.lastError().text();
            }
            m_db.rollback();
            return false;
        }
        current = m.version;
    }
    return true;
}
