// Whole-database integrity invariants (Phase 5). This module runs LAST, so by the
// time it executes the shared test DB has been populated by every other module's
// sales, returns, voids, adjustments, and GRNs. Each invariant below is a pure-SQL
// scan that MUST return zero offending rows on a healthy database — the same set a
// nightly self-check could run in production. A failure here is a real data-
// integrity defect, not a flaky test.

#include "framework/TestStats.h"

#include "data/Audit.h"

#include <QSqlQuery>
#include <QString>

namespace pharmadesk_tests {
namespace {

// Run a COUNT(*) scan; return the number of offending rows (-1 on query error).
int offending(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery q(db);
    if (!q.exec(sql) || !q.next()) {
        return -1;
    }
    return q.value(0).toInt();
}

} // namespace

TestStats run_invariant_tests(QSqlDatabase db, qint64)
{
    TestStats s;
    s.module = QStringLiteral("invariant");

    // INV-1: ledger arithmetic — qty_after must equal qty_before + qty_delta.
    s.check(offending(db, QStringLiteral("SELECT COUNT(*) FROM inventory_movements "
                                         "WHERE qty_after <> qty_before + qty_delta"))
                == 0,
            QStringLiteral("INV-1: every movement is arithmetically consistent"));

    // INV-2: each batch's current_qty equals the net of its ledger rows. Every
    // stock change (incl. the opening GRN_RECEIPT) writes a movement, so the live
    // quantity must equal the sum of deltas.
    // Scoped to batches with >=1 movement so raw schema-test fixtures (direct
    // INSERT, no ledger) aren't false positives; every production-path batch has
    // its opening GRN_RECEIPT movement, so this still catches real drift.
    s.check(
        offending(db,
                  QStringLiteral(
                      "SELECT COUNT(*) FROM batches b "
                      "WHERE EXISTS (SELECT 1 FROM inventory_movements m WHERE m.batch_id = b.id) "
                      "  AND b.current_qty <> "
                      "      COALESCE((SELECT SUM(m.qty_delta) FROM inventory_movements m "
                      "                WHERE m.batch_id = b.id), 0)"))
            == 0,
        QStringLiteral("INV-2: ledgered batch.current_qty == SUM(ledger deltas)"));

    // INV-3: no negative stock (a DB CHECK exists; verify nothing bypassed it).
    s.check(offending(db, QStringLiteral("SELECT COUNT(*) FROM batches WHERE current_qty < 0"))
                == 0,
            QStringLiteral("INV-3: no negative stock"));

    // INV-4: movement-type ↔ delta-sign consistency. Stock-changing types must have
    // a non-zero delta; the zero-delta bookkeeping types must have delta 0.
    s.check(offending(
                db, QStringLiteral(
                        "SELECT COUNT(*) FROM inventory_movements WHERE "
                        "(movement_type IN ('SALE','GRN_RECEIPT','ADJUSTMENT','RETURN_RESTOCK') "
                        "   AND qty_delta = 0) OR "
                        "(movement_type IN ('RETURN_QUARANTINE','WRITE_OFF') AND qty_delta <> 0)"))
                == 0,
            QStringLiteral("INV-4: movement type matches delta sign"));

    // INV-5: returns never exceed what was sold on a line (over-refund guard).
    s.check(
        offending(db, QStringLiteral(
                          "SELECT COUNT(*) FROM (SELECT si.id FROM sale_items si "
                          "JOIN returns r ON r.sale_item_id = si.id "
                          "WHERE r.status IN ('APPROVED_RESTOCK','RETURN_TO_SUPPLIER','WRITE_OFF') "
                          "GROUP BY si.id "
                          "HAVING SUM(r.qty_returned_units) > si.qty_in_base_units)"))
            == 0,
        QStringLiteral("INV-5: returns never exceed sold quantity"));

    // INV-6: every stock-adjustment row has a matching ledger movement (no silent
    // mutation of stock without a ledger entry).
    s.check(offending(
                db, QStringLiteral("SELECT COUNT(*) FROM stock_adjustments a WHERE NOT EXISTS "
                                   "(SELECT 1 FROM inventory_movements m "
                                   " WHERE m.ref_table = 'stock_adjustments' AND m.ref_id = a.id)"))
                == 0,
            QStringLiteral("INV-6: every stock adjustment has a ledger row"));

    // INV-7: money TEXT columns never carry float-drift artifacts (>4 frac digits
    // or scientific notation) — catches any SQL-side float arithmetic (H5).
    s.check(offending(db, QStringLiteral("SELECT COUNT(*) FROM cashier_sessions "
                                         "WHERE total_refunds_paid LIKE '%.%____%' "
                                         "   OR total_refunds_paid LIKE '%e%' "
                                         "   OR total_cash_sales LIKE '%.%____%'"))
                == 0,
            QStringLiteral("INV-7: no float-drift in money TEXT columns"));

    // INV-8: the audit log's append-only triggers are still installed.
    s.check(
        offending(db, QStringLiteral("SELECT 2 - COUNT(*) FROM sqlite_master WHERE type='trigger' "
                                     "AND name IN ('audit_log_no_update','audit_log_no_delete')"))
            == 0,
        QStringLiteral("INV-8: audit_log append-only triggers present"));

    // INV-9: the ledger-consistency trigger (migration v6) is installed.
    s.check(
        offending(db, QStringLiteral("SELECT 1 - COUNT(*) FROM sqlite_master WHERE type='trigger' "
                                     "AND name = 'inv_mv_consistent'"))
            == 0,
        QStringLiteral("INV-9: inventory_movements consistency trigger present"));

    // INV-10: foreign-key graph is intact at runtime.
    s.check(offending(db, QStringLiteral("SELECT COUNT(*) FROM (SELECT 1 FROM "
                                         "pragma_foreign_key_check())"))
                == 0,
            QStringLiteral("INV-10: PRAGMA foreign_key_check is clean"));

    // INV-11: the audit-log HMAC chain over EVERY chained row written this run (by
    // every module's sales/returns/adjustments/etc.) reconciles end-to-end.
    const Audit::ChainResult chain = Audit::verifyChain(db);
    s.check(chain.ok, QStringLiteral("INV-11: audit HMAC chain verifies clean"));
    s.check(chain.checked > 0, QStringLiteral("INV-11: audit chain is non-empty"));

    return s;
}

} // namespace pharmadesk_tests
