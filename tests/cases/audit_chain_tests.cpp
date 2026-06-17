// Tests for the audit-log HMAC tamper chain (data/Audit). Uses an ISOLATED
// in-memory SQLite connection so the positive + negative (tamper) cases don't
// pollute the shared test DB's real audit chain (which INV-11 verifies).

#include "framework/TestStats.h"

#include "data/Audit.h"
#include "domain/AppPaths.h"

#include <QDir>
#include <QFile>
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

    // ── Key rotation: rows signed before a rotation still verify ─────────────
    // rotateKey() touches the shared audit.key / audit.key.archive files, so
    // snapshot + restore them to keep this test free of side effects.
    {
        const QString keyP = QDir(AppPaths::logosDir()).filePath(QStringLiteral("../audit.key"));
        const QString arcP
            = QDir(AppPaths::logosDir()).filePath(QStringLiteral("../audit.key.archive"));
        auto readAll = [](const QString &p, bool *had) {
            QFile f(p);
            *had = f.exists();
            return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
        };
        auto writeOrRemove = [](const QString &p, bool had, const QByteArray &data) {
            if (!had) {
                QFile::remove(p);
                return;
            }
            QFile f(p);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                f.write(data);
            }
        };
        bool hadKey = false, hadArc = false;
        const QByteArray savedKey = readAll(keyP, &hadKey);
        const QByteArray savedArc = readAll(arcP, &hadArc);

        const QString rconn = QStringLiteral("audit_rotate_conn");
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), rconn);
            db.setDatabaseName(QStringLiteral(":memory:"));
            if (db.open()) {
                QSqlQuery ddl(db);
                ddl.exec(QStringLiteral(
                    "CREATE TABLE audit_log (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp TEXT, "
                    "user_id INTEGER, action_type TEXT NOT NULL, entity_type TEXT NOT NULL, "
                    "entity_id INTEGER, before_value TEXT, after_value TEXT, reason TEXT, "
                    "ip_address TEXT, user_agent TEXT, prev_hmac TEXT NOT NULL DEFAULT '', "
                    "row_hmac TEXT NOT NULL DEFAULT '')"));
                // Two rows under the current key.
                Audit::write(db, 1, QStringLiteral("R1"), QStringLiteral("t"), 1);
                Audit::write(db, 1, QStringLiteral("R2"), QStringLiteral("t"), 2);
                s.check(Audit::rotateKey(), QStringLiteral("rotate: rotateKey succeeds"));
                // Two more rows under the NEW key, continuing the same chain.
                Audit::write(db, 2, QStringLiteral("R3"), QStringLiteral("t"), 3);
                Audit::write(db, 2, QStringLiteral("R4"), QStringLiteral("t"), 4);
                const Audit::ChainResult r = Audit::verifyChain(db);
                s.check(r.ok && r.checked == 4,
                        QStringLiteral("rotate: chain spanning old+new keys verifies"));
                // A forged row still breaks it.
                QSqlQuery forge(db);
                forge.exec(QStringLiteral(
                    "INSERT INTO audit_log(timestamp,action_type,entity_type,prev_hmac,row_hmac) "
                    "VALUES('2026-01-01 00:00:00','FORGED','t','beef','beef')"));
                s.check(!Audit::verifyChain(db).ok,
                        QStringLiteral("rotate: a forged row is still rejected after rotation"));
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(rconn);

        writeOrRemove(keyP, hadKey, savedKey);
        writeOrRemove(arcP, hadArc, savedArc);
    }

    return s;
}

} // namespace pharmadesk_tests
