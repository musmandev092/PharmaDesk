// DB-backed tests for the manual stock-adjustment path (service/StockAdjuster).
// Each test creates its OWN medicine + batch with identifiers prefixed "TADJ_"
// so it never collides with — or depends on — any other module's data. The DB
// is NOT assumed clean: assertions are made relative to values captured before
// the action under test.
//
// Coverage: positive adjustment (qty up, cost impact, ADJUSTMENT movement),
// negative adjustment (qty down), would-go-negative guard, zero-delta guard,
// notes-required-for-OTHER guard, and the quarantine guard (only
// EXPIRY_WRITEOFF may touch a quarantined batch).

#include "data/BatchRepository.h"
#include "data/MedicineRepository.h"
#include "framework/TestStats.h"
#include "service/StockAdjuster.h"

#include <QDate>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

namespace pharmadesk_tests {

namespace {

// Process-wide counter so repeated runs / many fixtures get unique SKUs /
// batch numbers without assuming a clean DB.
int g_seq = 0;

QString uniq(const QString &tag)
{
    return QStringLiteral("TADJ_%1_%2").arg(tag).arg(++g_seq);
}

// Create one medicine (base unit TABLET) and return its id.
qint64 makeMedicine(QSqlDatabase db, qint64 userId)
{
    MedicineRepository meds(db);
    MedicineDraft m;
    m.sku = uniq(QStringLiteral("SKU"));
    m.brandName = uniq(QStringLiteral("Brand"));
    m.genericName = QStringLiteral("Generic");
    m.strength = QStringLiteral("100mg");
    m.baseUnit = QStringLiteral("TABLET");
    m.purchaseUnit = QStringLiteral("BOX");
    m.unitsPerPurchase = 100;
    return meds.create(m, userId);
}

// Stock one batch of `qty` base units at the given cost. Returns batch id.
qint64 makeBatch(QSqlDatabase db, qint64 userId, qint64 medId, int qty, const QString &cost,
                 const QDate &expiry)
{
    BatchRepository batches(db);
    StockInDraft s;
    s.medicineId = medId;
    s.batchNumber = uniq(QStringLiteral("BN"));
    s.expiry = expiry;
    s.quantity = qty;
    s.costPerUnit = cost;
    s.mrpPerUnit = QStringLiteral("10.00");
    return batches.addStock(s, userId);
}

int batchQty(QSqlDatabase db, qint64 batchId)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT current_qty FROM batches WHERE id = ?"));
    q.addBindValue(batchId);
    if (q.exec() && q.next()) return q.value(0).toInt();
    return -1;
}

// Count inventory_movements for a batch with the given type.
int movementCount(QSqlDatabase db, qint64 batchId, const QString &type)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM inventory_movements WHERE batch_id = ? AND movement_type = ?"));
    q.addBindValue(batchId);
    q.addBindValue(type);
    if (q.exec() && q.next()) return q.value(0).toInt();
    return -1;
}

} // namespace

TestStats run_adjustment_tests(QSqlDatabase db, qint64 userId)
{
    TestStats s;
    s.module = QStringLiteral("adjustment");

    const QDate future = QDate::currentDate().addYears(2);

    // ====================================================================
    // 1) Positive adjustment: qty up by delta, ADJUSTMENT movement with the
    //    signed delta, cost impact == delta * cost_per_unit at scale 2.
    // ====================================================================
    {
        const qint64 medId = makeMedicine(db, userId);
        s.check(medId > 0, QStringLiteral("pos: medicine created"));
        const qint64 batchId = makeBatch(db, userId, medId, 100, QStringLiteral("5.0000"), future);
        s.check(batchId > 0, QStringLiteral("pos: batch created"));
        s.check(batchQty(db, batchId) == 100, QStringLiteral("pos: initial qty 100"));

        const int adjBefore = movementCount(db, batchId, QStringLiteral("ADJUSTMENT"));

        StockAdjuster adj(db, userId);
        const AdjustResult r = adj.adjust(batchId, 10, QStringLiteral("DONATION"));
        s.check(r.ok, QStringLiteral("pos: adjust +10 ok"));
        s.check(r.adjustmentId > 0, QStringLiteral("pos: adjustmentId assigned"));
        s.check(!r.adjustmentNumber.isEmpty(), QStringLiteral("pos: adjustment number set"));
        s.check(batchQty(db, batchId) == 110, QStringLiteral("pos: qty now 110"));
        s.check(movementCount(db, batchId, QStringLiteral("ADJUSTMENT")) == adjBefore + 1,
                QStringLiteral("pos: one ADJUSTMENT movement added"));
        // cost impact = 10 * 5.0000 = 50.00
        s.check(r.costImpact == QStringLiteral("50.00"),
                QStringLiteral("pos: cost impact 10 x 5.0000 = 50.00"));

        // The ADJUSTMENT movement carries the signed delta.
        QSqlQuery mv(db);
        mv.prepare(QStringLiteral(
            "SELECT qty_delta, qty_before, qty_after FROM inventory_movements "
            " WHERE batch_id = ? AND movement_type = 'ADJUSTMENT' ORDER BY id DESC LIMIT 1"));
        mv.addBindValue(batchId);
        s.check(mv.exec() && mv.next(), QStringLiteral("pos: movement row readable"));
        s.check(mv.value(0).toInt() == 10, QStringLiteral("pos: movement qty_delta 10"));
        s.check(mv.value(2).toInt() == mv.value(1).toInt() + 10,
                QStringLiteral("pos: qty_after == qty_before + 10"));

        // ----------------------------------------------------------------
        // 2) Negative adjustment on the same batch: qty down by delta.
        // ----------------------------------------------------------------
        const AdjustResult neg = adj.adjust(batchId, -20, QStringLiteral("SHRINKAGE"));
        s.check(neg.ok, QStringLiteral("neg: adjust -20 ok"));
        s.check(batchQty(db, batchId) == 90, QStringLiteral("neg: qty now 90"));

        // ----------------------------------------------------------------
        // 3) Would-go-negative is refused; qty unchanged.
        // ----------------------------------------------------------------
        const int qtyBeforeBig = batchQty(db, batchId);
        const AdjustResult big = adj.adjust(batchId, -100000, QStringLiteral("SHRINKAGE"));
        s.check(!big.ok, QStringLiteral("negguard: oversize removal refused"));
        s.check(!big.error.isEmpty(), QStringLiteral("negguard: error message set"));
        s.check(batchQty(db, batchId) == qtyBeforeBig,
                QStringLiteral("negguard: qty unchanged after refusal"));

        // ----------------------------------------------------------------
        // 4) Zero delta is refused.
        // ----------------------------------------------------------------
        const AdjustResult zero = adj.adjust(batchId, 0, QStringLiteral("DAMAGE"));
        s.check(!zero.ok, QStringLiteral("zero: zero-delta refused"));
        s.check(batchQty(db, batchId) == qtyBeforeBig,
                QStringLiteral("zero: qty unchanged after refusal"));

        // ----------------------------------------------------------------
        // 5) reason OTHER requires notes.
        // ----------------------------------------------------------------
        const AdjustResult noNotes = adj.adjust(batchId, -1, QStringLiteral("OTHER"));
        s.check(!noNotes.ok, QStringLiteral("notes: OTHER without notes refused"));
        s.check(batchQty(db, batchId) == qtyBeforeBig,
                QStringLiteral("notes: qty unchanged on refused OTHER"));
        const AdjustResult withNotes
            = adj.adjust(batchId, -1, QStringLiteral("OTHER"), QStringLiteral("reason"));
        s.check(withNotes.ok, QStringLiteral("notes: OTHER with notes ok"));
        s.check(batchQty(db, batchId) == qtyBeforeBig - 1,
                QStringLiteral("notes: qty -1 after noted OTHER"));
    }

    // ====================================================================
    // 6) Quarantine guard: a quarantined batch refuses normal reasons but
    //    accepts EXPIRY_WRITEOFF.
    // ====================================================================
    {
        const qint64 medId = makeMedicine(db, userId);
        const qint64 batchId = makeBatch(db, userId, medId, 100, QStringLiteral("5.0000"), future);
        s.check(batchId > 0, QStringLiteral("quar: batch created"));

        QSqlQuery up(db);
        up.prepare(QStringLiteral("UPDATE batches SET is_quarantined = 1 WHERE id = ?"));
        up.addBindValue(batchId);
        s.check(up.exec(), QStringLiteral("quar: batch marked quarantined"));

        StockAdjuster adj(db, userId);
        const int qtyBefore = batchQty(db, batchId);
        const AdjustResult blocked = adj.adjust(batchId, -1, QStringLiteral("SHRINKAGE"));
        s.check(!blocked.ok, QStringLiteral("quar: SHRINKAGE on quarantined batch refused"));
        s.check(batchQty(db, batchId) == qtyBefore,
                QStringLiteral("quar: qty unchanged after refusal"));

        const AdjustResult writeoff = adj.adjust(batchId, -1, QStringLiteral("EXPIRY_WRITEOFF"));
        s.check(writeoff.ok, QStringLiteral("quar: EXPIRY_WRITEOFF on quarantined batch ok"));
        s.check(batchQty(db, batchId) == qtyBefore - 1,
                QStringLiteral("quar: qty -1 after EXPIRY_WRITEOFF"));
    }

    // ====================================================================
    // 7) Audit fail-CLOSED: if the audit insert fails mid-adjustment, the
    //    whole adjustment must roll back — no qty change, no ADJUSTMENT
    //    movement. Force the failure with a temporary BEFORE INSERT trigger
    //    on audit_log (dropped immediately after, even though adjust() catches
    //    its own exception and returns instead of throwing out).
    // ====================================================================
    {
        const qint64 medId = makeMedicine(db, userId);
        const qint64 batchId = makeBatch(db, userId, medId, 100, QStringLiteral("5.0000"), future);
        const int qtyBefore = batchQty(db, batchId);

        QSqlQuery(db).exec(
            QStringLiteral("CREATE TRIGGER tadj_block_audit BEFORE INSERT ON audit_log "
                           "BEGIN SELECT RAISE(ABORT, 'audit blocked for test'); END"));

        StockAdjuster adj(db, userId);
        const AdjustResult r = adj.adjust(batchId, -5, QStringLiteral("SHRINKAGE"));

        QSqlQuery(db).exec(QStringLiteral("DROP TRIGGER tadj_block_audit"));

        s.check(!r.ok, QStringLiteral("auditfail: adjustment refused when audit write fails"));
        s.check(batchQty(db, batchId) == qtyBefore,
                QStringLiteral("auditfail: qty rolled back unchanged"));
        s.check(movementCount(db, batchId, QStringLiteral("ADJUSTMENT")) == 0,
                QStringLiteral("auditfail: no ADJUSTMENT movement persisted"));
    }

    return s;
}

} // namespace pharmadesk_tests
