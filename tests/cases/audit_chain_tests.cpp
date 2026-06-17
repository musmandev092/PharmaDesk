// Tests for the audit-log HMAC tamper chain (data/Audit). Uses an ISOLATED
// in-memory SQLite connection so the positive + negative (tamper) cases don't
// pollute the shared test DB's real audit chain (which INV-11 verifies).

#include "framework/TestStats.h"

#include "data/Audit.h"

#include <QSqlDatabase>
#include <QSqlQuery>

namespace pharmadesk_tests {

TestStats run_audit_chain_tests(QSqlDatabase, qint64)
{
    TestStats s;
    s.module = QStringLiteral("audit_chain");

    const QString conn = QStringLiteral("audit_chain_conn");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(QStringLiteral(":memory:"));
        const bool opened = db.open();
        s.check(opened, QStringLiteral("isolated in-memory DB opens"));
        if (opened) {
            QSqlQuery ddl(db);
            ddl.exec(QStringLiteral(
                "CREATE TABLE audit_log (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp TEXT, "
                "user_id INTEGER, action_type TEXT NOT NULL, entity_type TEXT NOT NULL, "
                "entity_id INTEGER, before_value TEXT, after_value TEXT, reason TEXT, "
                "ip_address TEXT, user_agent TEXT, prev_hmac TEXT NOT NULL DEFAULT '', "
                "row_hmac TEXT NOT NULL DEFAULT '')"));

            // Three chained writes through the real Audit::write.
            s.check(Audit::write(db, 1, QStringLiteral("A1"), QStringLiteral("t"), 1),
                    QStringLiteral("write row 1"));
            s.check(Audit::write(db, 1, QStringLiteral("A2"), QStringLiteral("t"), 2,
                                 QStringLiteral("{\"k\":1}")),
                    QStringLiteral("write row 2 (with before)"));
            s.check(Audit::write(db, 2, QStringLiteral("A3"), QStringLiteral("t"), 3),
                    QStringLiteral("write row 3"));

            const Audit::ChainResult ok = Audit::verifyChain(db);
            s.check(ok.ok, QStringLiteral("chain verifies clean"));
            s.check(ok.checked == 3, QStringLiteral("all 3 rows are chained"));

            // Every chained row must carry a non-empty row_hmac, and row 1's
            // prev_hmac is empty (chain genesis).
            QSqlQuery chk(db);
            chk.exec(QStringLiteral("SELECT COUNT(*) FROM audit_log WHERE row_hmac = ''"));
            s.check(chk.next() && chk.value(0).toInt() == 0,
                    QStringLiteral("no row left with empty row_hmac"));

            // Tamper: raw-insert a forged row with a bogus (non-empty) row_hmac so
            // it's IN the chain but doesn't reconcile. The verifier must catch it.
            QSqlQuery forge(db);
            forge.exec(QStringLiteral(
                "INSERT INTO audit_log(timestamp, action_type, entity_type, prev_hmac, row_hmac) "
                "VALUES('2026-01-01 00:00:00','FORGED','t','deadbeef','deadbeef')"));
            const qint64 forgedId = forge.lastInsertId().toLongLong();
            const Audit::ChainResult bad = Audit::verifyChain(db);
            s.check(!bad.ok, QStringLiteral("forged row breaks the chain"));
            s.check(bad.brokenAtId == forgedId,
                    QStringLiteral("verifier reports the forged row id"));
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(conn);
    return s;
}

} // namespace pharmadesk_tests
