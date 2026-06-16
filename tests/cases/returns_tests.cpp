// DB-backed tests for the Returns / Void module (service/ReturnService +
// data/ReturnRepository). Each test creates its OWN medicines, stock, and sales
// with identifiers prefixed "TRET_" so it never collides with — or depends on —
// any other module's data. The DB is NOT assumed clean: every assertion is made
// relative to values captured immediately before the action under test.
//
// Coverage: partial return (refund math, restock → on-hand up + RETURN_RESTOCK
// movement, returns row, sale status), full return, over-return blocked,
// write-off (no restock, WRITE_OFF movement), void whole sale (all lines
// restocked, status VOIDED, excluded from session/sales totals), reason
// validation, qty<1 guard, already-voided / refunded-full guards, and the
// ReturnRepository read path (lookupByReceipt + listRecent).

#include "data/BatchRepository.h"
#include "data/MedicineRepository.h"
#include "data/ReturnRepository.h"
#include "data/UserRepository.h"
#include "domain/Money.h"
#include "framework/TestStats.h"
#include "service/ReturnService.h"
#include "service/SaleService.h"

#include <QDate>
#include <QSqlQuery>
#include <QString>
#include <QVariant>
#include <QVector>

namespace pharmadesk_tests {

namespace {

// Process-wide counter so repeated test runs (and many fixtures in one run) get
// unique SKUs / batch numbers without assuming a clean DB.
int g_seq = 0;

QString uniq(const QString &tag)
{
    return QStringLiteral("TRET_%1_%2").arg(tag).arg(++g_seq);
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

// Create one controlled (NARCOTIC) medicine and return its id.
qint64 makeNarcoticMedicine(QSqlDatabase db, qint64 userId)
{
    MedicineRepository meds(db);
    MedicineDraft m;
    m.sku = uniq(QStringLiteral("NSKU"));
    m.brandName = uniq(QStringLiteral("NBrand"));
    m.genericName = QStringLiteral("Generic");
    m.strength = QStringLiteral("100mg");
    m.baseUnit = QStringLiteral("TABLET");
    m.purchaseUnit = QStringLiteral("BOX");
    m.unitsPerPurchase = 100;
    m.controlledSchedule = QStringLiteral("NARCOTIC");
    return meds.create(m, userId);
}

// Create an active staff user with a unique username; returns its id (or -1).
qint64 makeStaff(QSqlDatabase db, qint64 actingUserId, const QString &role)
{
    UserRepository repo(db);
    const int n = ++g_seq;
    const QString username = QStringLiteral("tret_user_%1").arg(n, 5, 10, QLatin1Char('0'));
    const QString fullName = QStringLiteral("TRet %1 %2").arg(role).arg(n);
    return repo.createUser(fullName, username, role, QStringLiteral("x-pin-hash"),
                           /*isActive=*/true, actingUserId);
}

// Stock one batch of `qty` base units at the given MRP. Returns batch id.
qint64 makeBatch(QSqlDatabase db, qint64 userId, qint64 medId, int qty, const QString &mrp,
                 const QDate &expiry)
{
    BatchRepository batches(db);
    StockInDraft s;
    s.medicineId = medId;
    s.batchNumber = uniq(QStringLiteral("BN"));
    s.expiry = expiry;
    s.quantity = qty;
    s.costPerUnit = QStringLiteral("1.0000");
    s.mrpPerUnit = mrp;
    return batches.addStock(s, userId);
}

// Commit a single-line CASH sale of `qty` base units at `mrp`. Returns the
// SaleResult so the caller can read receiptNumber / saleId.
SaleResult sellOne(QSqlDatabase db, qint64 userId, qint64 medId, int qty, const QString &mrp)
{
    SaleService sale(db, userId);
    SaleInput in;
    SaleLineInput li;
    li.medicineId = medId;
    li.qtySoldDisplay = qty;
    li.soldUnitLabel = QStringLiteral("TABLET");
    li.soldUnitFactor = 1;
    li.unitMrp = mrp;
    in.items << li;
    in.paymentMode = QStringLiteral("CASH");
    in.amountTendered = QStringLiteral("100000");
    return sale.commit(in);
}

// First sale_item id for a sale (single-line sales here).
qint64 firstSaleItemId(QSqlDatabase db, qint64 saleId)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT id FROM sale_items WHERE sale_id = ? ORDER BY id LIMIT 1"));
    q.addBindValue(saleId);
    if (q.exec() && q.next()) return q.value(0).toLongLong();
    return -1;
}

int batchQty(QSqlDatabase db, qint64 batchId)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT current_qty FROM batches WHERE id = ?"));
    q.addBindValue(batchId);
    if (q.exec() && q.next()) return q.value(0).toInt();
    return -1;
}

QString saleStatus(QSqlDatabase db, qint64 saleId)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT status FROM sales WHERE id = ?"));
    q.addBindValue(saleId);
    if (q.exec() && q.next()) return q.value(0).toString();
    return QString();
}

// Count inventory_movements for a batch with the given type, optionally scoped
// to a ref_table/ref_id pair.
int movementCount(QSqlDatabase db, qint64 batchId, const QString &type,
                  const QString &refTable = QString(), qint64 refId = -1)
{
    QString sql = QStringLiteral(
        "SELECT COUNT(*) FROM inventory_movements WHERE batch_id = ? AND movement_type = ?");
    if (!refTable.isEmpty()) sql += QStringLiteral(" AND ref_table = ? AND ref_id = ?");
    QSqlQuery q(db);
    q.prepare(sql);
    q.addBindValue(batchId);
    q.addBindValue(type);
    if (!refTable.isEmpty()) {
        q.addBindValue(refTable);
        q.addBindValue(refId);
    }
    if (q.exec() && q.next()) return q.value(0).toInt();
    return -1;
}

int returnsRowCount(QSqlDatabase db, qint64 saleId)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM returns WHERE original_sale_id = ?"));
    q.addBindValue(saleId);
    if (q.exec() && q.next()) return q.value(0).toInt();
    return -1;
}

// Batch id behind a sale_item (single-batch sale lines here).
qint64 saleItemBatchId(QSqlDatabase db, qint64 saleItemId)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT batch_id FROM sale_items WHERE id = ?"));
    q.addBindValue(saleItemId);
    if (q.exec() && q.next()) return q.value(0).toLongLong();
    return -1;
}

QString returnStatus(QSqlDatabase db, qint64 returnId)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT status FROM returns WHERE id = ?"));
    q.addBindValue(returnId);
    if (q.exec() && q.next()) return q.value(0).toString();
    return QString();
}

} // namespace

TestStats run_returns_tests(QSqlDatabase db, qint64 userId)
{
    TestStats s;
    s.module = QStringLiteral("returns");

    const QDate future = QDate::currentDate().addYears(2);

    // ====================================================================
    // 1) Partial return WITH restock: refund math, on-hand up, RETURN_RESTOCK
    //    movement written, returns row created, sale → REFUNDED_PARTIAL.
    // ====================================================================
    {
        const qint64 medId = makeMedicine(db, userId);
        s.check(medId > 0, QStringLiteral("partial: medicine created"));
        [[maybe_unused]] const qint64 batchId
            = makeBatch(db, userId, medId, 50, QStringLiteral("2.50"), future);
        s.check(batchId > 0, QStringLiteral("partial: batch created"));

        const SaleResult sale = sellOne(db, userId, medId, 10, QStringLiteral("2.50"));
        s.check(sale.ok, QStringLiteral("partial: sale committed"));
        const qint64 saleId = sale.saleId;
        const qint64 itemId = firstSaleItemId(db, saleId);
        s.check(itemId > 0, QStringLiteral("partial: sale_item resolved"));

        const int qtyBeforeReturn = batchQty(db, batchId); // 50 - 10 = 40
        s.check(qtyBeforeReturn == 40, QStringLiteral("partial: on-hand 40 after sale of 10"));
        const int restockMvBefore = movementCount(db, batchId, QStringLiteral("RETURN_RESTOCK"));

        ReturnService svc(db, userId);
        const ReturnResult r = svc.commitReturn(itemId, 3, QStringLiteral("CUSTOMER_CHANGED_MIND"),
                                                /*restock*/ true);
        s.check(r.ok, QStringLiteral("partial: commitReturn ok"));
        s.check(r.returnId > 0, QStringLiteral("partial: returnId assigned"));
        s.check(!r.returnNumber.isEmpty() && r.returnNumber.startsWith(QStringLiteral("RET-")),
                QStringLiteral("partial: return number formatted RET-"));
        // refund = 3 * 2.50 = 7.50
        s.check(r.refundAmount == QStringLiteral("7.50"),
                QStringLiteral("partial: refund 3 x 2.50 = 7.50"));
        s.check(r.saleStatus == QStringLiteral("REFUNDED_PARTIAL"),
                QStringLiteral("partial: sale status REFUNDED_PARTIAL"));
        s.check(saleStatus(db, saleId) == QStringLiteral("REFUNDED_PARTIAL"),
                QStringLiteral("partial: DB sale status REFUNDED_PARTIAL"));

        // Restock pushed 3 units back: 40 -> 43.
        s.check(batchQty(db, batchId) == qtyBeforeReturn + 3,
                QStringLiteral("partial: on-hand restocked +3 (43)"));
        s.check(movementCount(db, batchId, QStringLiteral("RETURN_RESTOCK")) == restockMvBefore + 1,
                QStringLiteral("partial: one RETURN_RESTOCK movement added"));
        s.check(movementCount(db, batchId, QStringLiteral("RETURN_RESTOCK"),
                              QStringLiteral("returns"), r.returnId)
                    == 1,
                QStringLiteral("partial: movement linked to returns/returnId"));
        s.check(returnsRowCount(db, saleId) == 1, QStringLiteral("partial: one returns row"));

        // Refund amount persisted on the returns row matches.
        QSqlQuery rq(db);
        rq.prepare(QStringLiteral(
            "SELECT refund_amount, qty_returned_units, status, reason FROM returns WHERE id = ?"));
        rq.addBindValue(r.returnId);
        s.check(rq.exec() && rq.next(), QStringLiteral("partial: returns row readable"));
        s.check(rq.value(0).toString() == QStringLiteral("7.50"),
                QStringLiteral("partial: stored refund 7.50"));
        s.check(rq.value(1).toInt() == 3, QStringLiteral("partial: stored qty 3"));
        s.check(rq.value(2).toString() == QStringLiteral("APPROVED_RESTOCK"),
                QStringLiteral("partial: returns status APPROVED_RESTOCK"));
        s.check(rq.value(3).toString() == QStringLiteral("CUSTOMER_CHANGED_MIND"),
                QStringLiteral("partial: reason stored"));
    }

    // ====================================================================
    // 2) Second partial return on the same line accumulates; over-return of
    //    the remaining balance is blocked. Then a return that exhausts the
    //    remainder flips the sale to REFUNDED_FULL.
    // ====================================================================
    {
        const qint64 medId = makeMedicine(db, userId);
        [[maybe_unused]] const qint64 batchId
            = makeBatch(db, userId, medId, 20, QStringLiteral("4.00"), future);
        const SaleResult sale = sellOne(db, userId, medId, 8, QStringLiteral("4.00"));
        s.check(sale.ok, QStringLiteral("accum: sale committed"));
        const qint64 itemId = firstSaleItemId(db, sale.saleId);

        ReturnService svc(db, userId);
        // First return 3 of 8.
        const ReturnResult r1 = svc.commitReturn(itemId, 3, QStringLiteral("DAMAGED"), true);
        s.check(r1.ok && r1.saleStatus == QStringLiteral("REFUNDED_PARTIAL"),
                QStringLiteral("accum: first return partial"));

        // Over-return: only 5 remain, asking for 6 must be blocked, no row added.
        const int rowsBefore = returnsRowCount(db, sale.saleId);
        const ReturnResult over = svc.commitReturn(itemId, 6, QStringLiteral("DAMAGED"), true);
        s.check(!over.ok, QStringLiteral("accum: over-return (6 of remaining 5) blocked"));
        s.check(!over.error.isEmpty(), QStringLiteral("accum: over-return error message set"));
        s.check(returnsRowCount(db, sale.saleId) == rowsBefore,
                QStringLiteral("accum: no returns row written on over-return"));

        // Return exactly the remaining 5 → full refund.
        const ReturnResult r2 = svc.commitReturn(itemId, 5, QStringLiteral("DAMAGED"), true);
        s.check(r2.ok, QStringLiteral("accum: remaining 5 returned"));
        s.check(r2.saleStatus == QStringLiteral("REFUNDED_FULL"),
                QStringLiteral("accum: sale now REFUNDED_FULL"));
        s.check(saleStatus(db, sale.saleId) == QStringLiteral("REFUNDED_FULL"),
                QStringLiteral("accum: DB status REFUNDED_FULL"));

        // After REFUNDED_FULL, any further return is refused.
        const ReturnResult r3 = svc.commitReturn(itemId, 1, QStringLiteral("OTHER"), true);
        s.check(!r3.ok, QStringLiteral("accum: return on REFUNDED_FULL refused"));
    }

    // ====================================================================
    // 3) Full return in one shot (qty == qtySold) → REFUNDED_FULL, refund
    //    equals the full line, all units restocked.
    // ====================================================================
    {
        const qint64 medId = makeMedicine(db, userId);
        [[maybe_unused]] const qint64 batchId
            = makeBatch(db, userId, medId, 12, QStringLiteral("3.00"), future);
        const SaleResult sale = sellOne(db, userId, medId, 5, QStringLiteral("3.00"));
        s.check(sale.ok, QStringLiteral("full: sale committed"));
        const qint64 itemId = firstSaleItemId(db, sale.saleId);

        const int qtyAfterSale = batchQty(db, batchId); // 12 - 5 = 7
        ReturnService svc(db, userId);
        const ReturnResult r = svc.commitReturn(itemId, 5, QStringLiteral("WRONG_ITEM"), true);
        s.check(r.ok, QStringLiteral("full: commitReturn ok"));
        s.check(r.refundAmount == QStringLiteral("15.00"),
                QStringLiteral("full: refund 5 x 3.00 = 15.00"));
        s.check(r.saleStatus == QStringLiteral("REFUNDED_FULL"),
                QStringLiteral("full: status REFUNDED_FULL"));
        s.check(batchQty(db, batchId) == qtyAfterSale + 5,
                QStringLiteral("full: all 5 units restocked"));
    }

    // ====================================================================
    // 4) Write-off return does NOT restock: on-hand unchanged, a WRITE_OFF
    //    (zero-delta) movement is written, returns status WRITE_OFF, and the
    //    sale status still recomputes (write-offs count toward returned qty).
    // ====================================================================
    {
        const qint64 medId = makeMedicine(db, userId);
        [[maybe_unused]] const qint64 batchId
            = makeBatch(db, userId, medId, 30, QStringLiteral("5.00"), future);
        const SaleResult sale = sellOne(db, userId, medId, 6, QStringLiteral("5.00"));
        s.check(sale.ok, QStringLiteral("writeoff: sale committed"));
        const qint64 itemId = firstSaleItemId(db, sale.saleId);

        const int qtyAfterSale = batchQty(db, batchId); // 24
        const int writeoffBefore = movementCount(db, batchId, QStringLiteral("WRITE_OFF"));
        const int restockBefore = movementCount(db, batchId, QStringLiteral("RETURN_RESTOCK"));

        ReturnService svc(db, userId);
        const ReturnResult r
            = svc.commitReturn(itemId, 2, QStringLiteral("DAMAGED"), /*restock*/ false);
        s.check(r.ok, QStringLiteral("writeoff: commitReturn ok"));
        s.check(r.refundAmount == QStringLiteral("10.00"),
                QStringLiteral("writeoff: refund 2 x 5.00 = 10.00"));
        s.check(batchQty(db, batchId) == qtyAfterSale,
                QStringLiteral("writeoff: on-hand UNCHANGED (no restock)"));
        s.check(movementCount(db, batchId, QStringLiteral("WRITE_OFF")) == writeoffBefore + 1,
                QStringLiteral("writeoff: one WRITE_OFF movement written"));
        s.check(movementCount(db, batchId, QStringLiteral("RETURN_RESTOCK")) == restockBefore,
                QStringLiteral("writeoff: no RETURN_RESTOCK movement"));

        // WRITE_OFF movement is zero-delta with equal before/after.
        QSqlQuery mv(db);
        mv.prepare(QStringLiteral(
            "SELECT qty_delta, qty_before, qty_after FROM inventory_movements "
            " WHERE batch_id = ? AND movement_type = 'WRITE_OFF' ORDER BY id DESC LIMIT 1"));
        mv.addBindValue(batchId);
        s.check(mv.exec() && mv.next(), QStringLiteral("writeoff: movement row readable"));
        s.check(mv.value(0).toInt() == 0, QStringLiteral("writeoff: qty_delta 0"));
        s.check(mv.value(1).toInt() == mv.value(2).toInt(),
                QStringLiteral("writeoff: qty_before == qty_after"));

        // Returns row recorded the WRITE_OFF disposition; sale recomputed partial.
        QSqlQuery rq(db);
        rq.prepare(QStringLiteral("SELECT status FROM returns WHERE id = ?"));
        rq.addBindValue(r.returnId);
        s.check(rq.exec() && rq.next() && rq.value(0).toString() == QStringLiteral("WRITE_OFF"),
                QStringLiteral("writeoff: returns status WRITE_OFF"));
        s.check(r.saleStatus == QStringLiteral("REFUNDED_PARTIAL"),
                QStringLiteral("writeoff: sale REFUNDED_PARTIAL (write-off counts)"));
    }

    // ====================================================================
    // 5) Input validation: qty < 1 and invalid reason are rejected, no row.
    // ====================================================================
    {
        const qint64 medId = makeMedicine(db, userId);
        [[maybe_unused]] const qint64 batchId
            = makeBatch(db, userId, medId, 10, QStringLiteral("1.00"), future);
        const SaleResult sale = sellOne(db, userId, medId, 4, QStringLiteral("1.00"));
        const qint64 itemId = firstSaleItemId(db, sale.saleId);
        ReturnService svc(db, userId);

        for (int badQty : {0, -1, -5}) {
            const ReturnResult r = svc.commitReturn(itemId, badQty, QStringLiteral("OTHER"), true);
            s.check(!r.ok, QStringLiteral("validate: qty %1 rejected").arg(badQty));
        }
        const ReturnResult badReason
            = svc.commitReturn(itemId, 1, QStringLiteral("NOT_A_REASON"), true);
        s.check(!badReason.ok, QStringLiteral("validate: invalid reason rejected"));
        s.check(returnsRowCount(db, sale.saleId) == 0,
                QStringLiteral("validate: no returns row from bad inputs"));

        // A nonexistent sale_item id is rejected too.
        const ReturnResult missing
            = svc.commitReturn(qint64(99999999), 1, QStringLiteral("OTHER"), true);
        s.check(!missing.ok, QStringLiteral("validate: unknown sale_item rejected"));

        // Each valid reason value is accepted (returns 1 unit each, capacity 4).
        const QStringList reasons
            = {QStringLiteral("CUSTOMER_CHANGED_MIND"), QStringLiteral("WRONG_ITEM"),
               QStringLiteral("ADVERSE_REACTION"), QStringLiteral("EXPIRED")};
        bool allReasonsOk = true;
        for (const QString &reason : reasons) {
            const ReturnResult r = svc.commitReturn(itemId, 1, reason, true);
            if (!r.ok) allReasonsOk = false;
        }
        s.check(allReasonsOk, QStringLiteral("validate: all valid reasons accepted"));
    }

    // ====================================================================
    // 6) Void a whole sale: all lines restocked, status VOIDED, and the sale
    //    is excluded from completed-sales totals afterward.
    // ====================================================================
    {
        const qint64 medA = makeMedicine(db, userId);
        const qint64 medB = makeMedicine(db, userId);
        const qint64 batchA = makeBatch(db, userId, medA, 40, QStringLiteral("2.00"), future);
        const qint64 batchB = makeBatch(db, userId, medB, 40, QStringLiteral("6.00"), future);

        // Two-line sale.
        SaleService sale(db, userId);
        SaleInput in;
        SaleLineInput la;
        la.medicineId = medA;
        la.qtySoldDisplay = 7;
        la.soldUnitLabel = QStringLiteral("TABLET");
        la.soldUnitFactor = 1;
        la.unitMrp = QStringLiteral("2.00");
        SaleLineInput lb;
        lb.medicineId = medB;
        lb.qtySoldDisplay = 4;
        lb.soldUnitLabel = QStringLiteral("TABLET");
        lb.soldUnitFactor = 1;
        lb.unitMrp = QStringLiteral("6.00");
        in.items << la << lb;
        in.paymentMode = QStringLiteral("CASH");
        in.amountTendered = QStringLiteral("100000");
        const SaleResult sr = sale.commit(in);
        s.check(sr.ok, QStringLiteral("void: two-line sale committed"));
        const qint64 saleId = sr.saleId;

        const int aAfterSale = batchQty(db, batchA); // 33
        const int bAfterSale = batchQty(db, batchB); // 36
        const int aRestockBefore = movementCount(db, batchA, QStringLiteral("RETURN_RESTOCK"));
        const int bRestockBefore = movementCount(db, batchB, QStringLiteral("RETURN_RESTOCK"));

        // Completed-sales grand-total sum before the void (only this sale matters
        // for the delta).
        // Sum COMPLETED grand_totals with exact fixed-point Money — NOT float.
        // CLAUDE.md forbids float for stored money, and an epsilon compare could
        // mask a sub-cent rounding bug in the very path we claim is PHP-exact.
        auto completedTotal = [&]() -> Money {
            QSqlQuery q(db);
            q.exec(QStringLiteral("SELECT grand_total FROM sales WHERE status = 'COMPLETED'"));
            Money sum;
            while (q.next()) {
                sum = sum + Money::fromString(q.value(0).toString());
            }
            return sum;
        };
        const Money totalBefore = completedTotal();

        ReturnService svc(db, userId);
        const VoidResult v = svc.voidSale(saleId);
        s.check(v.ok, QStringLiteral("void: voidSale ok"));
        s.check(v.saleId == saleId, QStringLiteral("void: returns same saleId"));
        s.check(saleStatus(db, saleId) == QStringLiteral("VOIDED"),
                QStringLiteral("void: status VOIDED"));

        // Both lines restocked to their pre-void on-hand.
        s.check(batchQty(db, batchA) == aAfterSale + 7,
                QStringLiteral("void: line A restocked +7"));
        s.check(batchQty(db, batchB) == bAfterSale + 4,
                QStringLiteral("void: line B restocked +4"));
        s.check(movementCount(db, batchA, QStringLiteral("RETURN_RESTOCK")) == aRestockBefore + 1,
                QStringLiteral("void: RETURN_RESTOCK movement for A"));
        s.check(movementCount(db, batchB, QStringLiteral("RETURN_RESTOCK")) == bRestockBefore + 1,
                QStringLiteral("void: RETURN_RESTOCK movement for B"));
        // Void movements are linked to the sale, not a return.
        s.check(movementCount(db, batchA, QStringLiteral("RETURN_RESTOCK"), QStringLiteral("sales"),
                              saleId)
                    == 1,
                QStringLiteral("void: A movement ref_table=sales"));

        // Excluded from completed totals afterward: this sale's grand_total drops
        // out of the COMPLETED sum. grand = 7*2 + 4*6 = 38.00.
        const Money totalAfter = completedTotal();
        s.check((totalBefore - totalAfter).toString() == QStringLiteral("38.00"),
                QStringLiteral("void: sale excluded from COMPLETED totals (-38.00)"));

        // Re-voiding a VOIDED sale is refused.
        const VoidResult v2 = svc.voidSale(saleId);
        s.check(!v2.ok, QStringLiteral("void: re-void refused"));

        // A return against a VOIDED sale's line is refused.
        const qint64 itemA = firstSaleItemId(db, saleId);
        const ReturnResult ra = svc.commitReturn(itemA, 1, QStringLiteral("OTHER"), true);
        s.check(!ra.ok, QStringLiteral("void: return on VOIDED sale refused"));
    }

    // ====================================================================
    // 6b) KEYSTONE — voidSale authorization at the SERVICE boundary.
    //     A cashier must NOT be able to void a sale: the service rejects it
    //     and writes NOTHING (sale stays COMPLETED, stock unchanged, no audit
    //     row). A manager/admin still can. This proves the authorized-write
    //     boundary: a denied role writes nothing; an authorized role succeeds.
    // ====================================================================
    {
        const qint64 med = makeMedicine(db, userId);
        const qint64 batch = makeBatch(db, userId, med, 100, QStringLiteral("5.00"),
                                       QDate::currentDate().addYears(1));
        const SaleResult sale = sellOne(db, userId, med, 3, QStringLiteral("5.00"));
        s.check(sale.ok && sale.saleId > 0, QStringLiteral("keystone: setup sale committed"));
        const int qtyAfterSale = batchQty(db, batch);

        auto auditCount = [&]() {
            QSqlQuery q(db);
            q.exec(
                QStringLiteral("SELECT count(*) FROM audit_log WHERE action_type = 'SALE_VOIDED'"));
            return (q.next()) ? q.value(0).toInt() : -1;
        };
        const int voidAuditsBefore = auditCount();

        // A CASHIER is denied — and nothing is written.
        const qint64 cashierId = makeStaff(db, userId, QStringLiteral("CASHIER"));
        ReturnService cashierSvc(db, cashierId);
        const VoidResult denied = cashierSvc.voidSale(sale.saleId);
        s.check(!denied.ok, QStringLiteral("keystone: cashier void DENIED"));
        s.check(denied.error.contains(QStringLiteral("manager"), Qt::CaseInsensitive),
                QStringLiteral("keystone: denial message names manager/admin"));
        s.check(saleStatus(db, sale.saleId) == QStringLiteral("COMPLETED"),
                QStringLiteral("keystone: sale still COMPLETED after denied void"));
        s.check(batchQty(db, batch) == qtyAfterSale,
                QStringLiteral("keystone: stock NOT restocked by denied void"));
        s.check(auditCount() == voidAuditsBefore,
                QStringLiteral("keystone: no SALE_VOIDED audit row written"));

        // A MANAGER is authorized — the void succeeds and is recorded.
        const qint64 managerId = makeStaff(db, userId, QStringLiteral("MANAGER"));
        ReturnService managerSvc(db, managerId);
        const VoidResult allowed = managerSvc.voidSale(sale.saleId);
        s.check(allowed.ok, QStringLiteral("keystone: manager void ALLOWED"));
        s.check(saleStatus(db, sale.saleId) == QStringLiteral("VOIDED"),
                QStringLiteral("keystone: manager void sets VOIDED"));
        s.check(batchQty(db, batch) == qtyAfterSale + 3,
                QStringLiteral("keystone: manager void restocks the 3 units"));
        s.check(auditCount() == voidAuditsBefore + 1,
                QStringLiteral("keystone: SALE_VOIDED audit row written by manager"));
    }

    // ====================================================================
    // 7) voidSale sameDayOnly: a sale made today CAN be voided same-day-only.
    //    (Sales committed in-test are stamped CURRENT_TIMESTAMP = today.)
    // ====================================================================
    {
        const qint64 medId = makeMedicine(db, userId);
        [[maybe_unused]] const qint64 batchId
            = makeBatch(db, userId, medId, 10, QStringLiteral("1.50"), future);
        const SaleResult sale = sellOne(db, userId, medId, 3, QStringLiteral("1.50"));
        s.check(sale.ok, QStringLiteral("sameday: sale committed"));
        const int afterSale = batchQty(db, batchId);

        ReturnService svc(db, userId);
        const VoidResult v = svc.voidSale(sale.saleId, /*sameDayOnly*/ true);
        s.check(v.ok, QStringLiteral("sameday: same-day void allowed for today's sale"));
        s.check(saleStatus(db, sale.saleId) == QStringLiteral("VOIDED"),
                QStringLiteral("sameday: status VOIDED"));
        s.check(batchQty(db, batchId) == afterSale + 3, QStringLiteral("sameday: stock restored"));
    }

    // voidSale on a non-existent sale id is refused.
    {
        ReturnService svc(db, userId);
        const VoidResult v = svc.voidSale(qint64(98765432));
        s.check(!v.ok, QStringLiteral("void: unknown sale id refused"));
    }

    // ====================================================================
    // 8) ReturnRepository read path: lookupByReceipt returns the header + the
    //    returnable line with qtyAlreadyReturned reflecting prior returns;
    //    listRecent surfaces the new return.
    // ====================================================================
    {
        const qint64 medId = makeMedicine(db, userId);
        [[maybe_unused]] const qint64 batchId
            = makeBatch(db, userId, medId, 25, QStringLiteral("8.00"), future);
        const SaleResult sale = sellOne(db, userId, medId, 9, QStringLiteral("8.00"));
        s.check(sale.ok, QStringLiteral("repo: sale committed"));

        ReturnRepository repo(db);
        const ReturnSaleHeader hdr = repo.lookupByReceipt(sale.receiptNumber);
        s.check(hdr.found, QStringLiteral("repo: lookupByReceipt found sale"));
        s.check(hdr.saleId == sale.saleId, QStringLiteral("repo: saleId matches"));
        s.check(hdr.receiptNumber == sale.receiptNumber,
                QStringLiteral("repo: receipt number matches"));
        s.check(hdr.status == QStringLiteral("COMPLETED"),
                QStringLiteral("repo: status COMPLETED before any return"));
        s.check(hdr.lines.size() == 1, QStringLiteral("repo: one returnable line"));

        qint64 itemId = -1;
        if (!hdr.lines.isEmpty()) {
            const ReturnableLine &l = hdr.lines.first();
            itemId = l.saleItemId;
            s.check(l.qtySold == 9, QStringLiteral("repo: qtySold 9 base units"));
            s.check(l.qtyAlreadyReturned == 0, QStringLiteral("repo: nothing returned yet"));
            s.check(l.unitMrp == QStringLiteral("8.00"), QStringLiteral("repo: unit MRP 8.00"));
        }
        s.check(itemId > 0, QStringLiteral("repo: sale_item id from repo"));

        // Process a return through the service, then re-look-up.
        ReturnService svc(db, userId);
        const ReturnResult r = svc.commitReturn(itemId, 4, QStringLiteral("EXPIRED"), true);
        s.check(r.ok, QStringLiteral("repo: return committed"));

        const ReturnSaleHeader hdr2 = repo.lookupByReceipt(sale.receiptNumber);
        s.check(!hdr2.lines.isEmpty() && hdr2.lines.first().qtyAlreadyReturned == 4,
                QStringLiteral("repo: qtyAlreadyReturned now 4"));
        s.check(hdr2.status == QStringLiteral("REFUNDED_PARTIAL"),
                QStringLiteral("repo: status REFUNDED_PARTIAL after return"));

        // listRecent contains this return, newest first, with the right fields.
        const QVector<ReturnRow> recent = repo.listRecent(50);
        bool seen = false;
        for (const ReturnRow &row : recent) {
            if (row.id == r.returnId) {
                seen = true;
                s.check(row.returnNumber == r.returnNumber,
                        QStringLiteral("repo: listRecent return number matches"));
                s.check(row.qtyReturned == 4, QStringLiteral("repo: listRecent qty 4"));
                s.check(row.refundAmount == QStringLiteral("32.00"),
                        QStringLiteral("repo: listRecent refund 4 x 8.00 = 32.00"));
                s.check(row.status == QStringLiteral("APPROVED_RESTOCK"),
                        QStringLiteral("repo: listRecent status APPROVED_RESTOCK"));
                s.check(row.receiptNumber == sale.receiptNumber,
                        QStringLiteral("repo: listRecent links original receipt"));
            }
        }
        s.check(seen, QStringLiteral("repo: listRecent surfaces the new return"));

        // Receipt lookup tolerates an unpadded numeric suffix (INV-...-N).
        const int dash = sale.receiptNumber.lastIndexOf(QLatin1Char('-'));
        if (dash > 0) {
            const QString head = sale.receiptNumber.left(dash);
            const QString tail = sale.receiptNumber.mid(dash + 1);
            bool numeric = !tail.isEmpty();
            for (const QChar c : tail)
                if (!c.isDigit()) numeric = false;
            if (numeric) {
                const QString unpadded = QStringLiteral("%1-%2").arg(head).arg(tail.toInt());
                const ReturnSaleHeader hu = repo.lookupByReceipt(unpadded);
                s.check(hu.found && hu.saleId == sale.saleId,
                        QStringLiteral("repo: unpadded receipt suffix still matches"));
            }
        }

        // Unknown receipt → not found.
        const ReturnSaleHeader none
            = repo.lookupByReceipt(QStringLiteral("TRET_NO_SUCH_RECEIPT_999"));
        s.check(!none.found, QStringLiteral("repo: unknown receipt not found"));
        const ReturnSaleHeader blank = repo.lookupByReceipt(QString());
        s.check(!blank.found, QStringLiteral("repo: blank receipt not found"));
    }

    // ====================================================================
    // 9) Refund-amount math sweep: refund = qty * unitMrp rounded HALF_UP to
    //    scale 2, matching Money. Verify across a range of MRPs and quantities.
    // ====================================================================
    {
        struct Case
        {
            QString mrp;
            int qty;
        };
        const QVector<Case> cases = {
            {QStringLiteral("0.01"), 1},  {QStringLiteral("0.01"), 3},
            {QStringLiteral("1.99"), 2},  {QStringLiteral("2.50"), 4},
            {QStringLiteral("3.33"), 3},  {QStringLiteral("9.99"), 5},
            {QStringLiteral("12.50"), 2}, {QStringLiteral("100.00"), 1},
        };
        for (const Case &c : cases) {
            const qint64 medId = makeMedicine(db, userId);
            [[maybe_unused]] const qint64 batchId = makeBatch(db, userId, medId, 50, c.mrp, future);
            (void)batchId;
            const SaleResult sale = sellOne(db, userId, medId, c.qty + 1, c.mrp);
            if (!sale.ok) {
                s.check(false, QStringLiteral("math: sale ok mrp=%1 qty=%2").arg(c.mrp).arg(c.qty));
                continue;
            }
            const qint64 itemId = firstSaleItemId(db, sale.saleId);
            ReturnService svc(db, userId);
            const ReturnResult r = svc.commitReturn(itemId, c.qty, QStringLiteral("OTHER"), true);
            const QString expected
                = Money::fromString(c.mrp).mul(c.qty).toString(Money::ScaleMoney);
            s.check(r.ok, QStringLiteral("math: return ok mrp=%1 qty=%2").arg(c.mrp).arg(c.qty));
            s.check(
                r.refundAmount == expected,
                QStringLiteral("math: refund %1 x %2 = %3").arg(c.qty).arg(c.mrp).arg(expected));
        }
    }

    // ====================================================================
    // 10) TWO-STEP — initiate(): files a PENDING_REVIEW return + a zero-delta
    //     RETURN_QUARANTINE movement; stock is NOT changed yet; the sale flips
    //     to PENDING_REVIEW.
    // ====================================================================
    {
        const qint64 medId = makeMedicine(db, userId);
        [[maybe_unused]] const qint64 batchId
            = makeBatch(db, userId, medId, 50, QStringLiteral("2.50"), future);
        const SaleResult sale = sellOne(db, userId, medId, 10, QStringLiteral("2.50"));
        s.check(sale.ok, QStringLiteral("init: sale committed"));
        const qint64 itemId = firstSaleItemId(db, sale.saleId);
        s.check(itemId > 0, QStringLiteral("init: sale_item resolved"));

        const int qtyAfterSale = batchQty(db, batchId); // 40
        const int quarBefore = movementCount(db, batchId, QStringLiteral("RETURN_QUARANTINE"));

        ReturnService svc(db, userId);
        const ReturnResult r = svc.initiate(itemId, 3, QStringLiteral("DAMAGED"));
        s.check(r.ok, QStringLiteral("init: initiate ok"));
        s.check(r.returnId > 0, QStringLiteral("init: returnId assigned"));
        s.check(r.saleStatus == QStringLiteral("PENDING_REVIEW"),
                QStringLiteral("init: sale status PENDING_REVIEW"));
        // The ORIGINAL sale stays COMPLETED while the return is only pending —
        // PENDING_REVIEW is a returns status, not a sales status, and the sale
        // is recomputed to REFUNDED_* only once a return reaches a final state.
        s.check(saleStatus(db, sale.saleId) == QStringLiteral("COMPLETED"),
                QStringLiteral("init: DB sale stays COMPLETED while return pending"));

        // Stock unchanged at initiate time.
        s.check(batchQty(db, batchId) == qtyAfterSale,
                QStringLiteral("init: on-hand UNCHANGED at initiate"));

        // One RETURN_QUARANTINE movement, zero-delta.
        s.check(movementCount(db, batchId, QStringLiteral("RETURN_QUARANTINE")) == quarBefore + 1,
                QStringLiteral("init: one RETURN_QUARANTINE movement added"));
        QSqlQuery mv(db);
        mv.prepare(
            QStringLiteral("SELECT qty_delta FROM inventory_movements WHERE batch_id = ? "
                           " AND movement_type = 'RETURN_QUARANTINE' ORDER BY id DESC LIMIT 1"));
        mv.addBindValue(batchId);
        s.check(mv.exec() && mv.next() && mv.value(0).toInt() == 0,
                QStringLiteral("init: RETURN_QUARANTINE qty_delta 0"));

        s.check(returnStatus(db, r.returnId) == QStringLiteral("PENDING_REVIEW"),
                QStringLiteral("init: returns row status PENDING_REVIEW"));

        // ----------------------------------------------------------------
        // 11) adjudicate(APPROVED_RESTOCK): stock += qty, RETURN_RESTOCK row,
        //     returns.status flips, sale recomputes to a REFUNDED_* status.
        // ----------------------------------------------------------------
        const int restockBefore = movementCount(db, batchId, QStringLiteral("RETURN_RESTOCK"));
        const ReturnOpResult adj = svc.adjudicate(r.returnId, QStringLiteral("APPROVED_RESTOCK"));
        s.check(adj.ok, QStringLiteral("adj: adjudicate APPROVED_RESTOCK ok"));
        s.check(batchQty(db, batchId) == qtyAfterSale + 3,
                QStringLiteral("adj: on-hand restocked +3"));
        s.check(movementCount(db, batchId, QStringLiteral("RETURN_RESTOCK")) == restockBefore + 1,
                QStringLiteral("adj: one RETURN_RESTOCK movement added"));
        s.check(returnStatus(db, r.returnId) == QStringLiteral("APPROVED_RESTOCK"),
                QStringLiteral("adj: returns status APPROVED_RESTOCK"));
        const QString st = saleStatus(db, sale.saleId);
        s.check(st == QStringLiteral("REFUNDED_PARTIAL") || st == QStringLiteral("REFUNDED_FULL"),
                QStringLiteral("adj: sale status REFUNDED_PARTIAL/FULL"));
    }

    // ====================================================================
    // 12) adjudicate(RETURN_TO_SUPPLIER): stock stays out (unchanged from the
    //     post-initiate value), a WRITE_OFF movement records the disposition;
    //     then markSupplierReturnSent() closes the loop and is idempotent-once.
    // ====================================================================
    {
        const qint64 medId = makeMedicine(db, userId);
        [[maybe_unused]] const qint64 batchId
            = makeBatch(db, userId, medId, 50, QStringLiteral("3.00"), future);
        const SaleResult sale = sellOne(db, userId, medId, 8, QStringLiteral("3.00"));
        s.check(sale.ok, QStringLiteral("supplier: sale committed"));
        const qint64 itemId = firstSaleItemId(db, sale.saleId);

        ReturnService svc(db, userId);
        const ReturnResult r = svc.initiate(itemId, 4, QStringLiteral("DAMAGED"));
        s.check(r.ok, QStringLiteral("supplier: initiate ok"));
        const int qtyPostInitiate = batchQty(db, batchId);
        const int writeoffBefore = movementCount(db, batchId, QStringLiteral("WRITE_OFF"));

        const ReturnOpResult adj = svc.adjudicate(r.returnId, QStringLiteral("RETURN_TO_SUPPLIER"));
        s.check(adj.ok, QStringLiteral("supplier: adjudicate RETURN_TO_SUPPLIER ok"));
        s.check(batchQty(db, batchId) == qtyPostInitiate,
                QStringLiteral("supplier: on-hand UNCHANGED (goods stay out)"));
        s.check(movementCount(db, batchId, QStringLiteral("WRITE_OFF")) == writeoffBefore + 1,
                QStringLiteral("supplier: one WRITE_OFF movement added"));
        s.check(returnStatus(db, r.returnId) == QStringLiteral("RETURN_TO_SUPPLIER"),
                QStringLiteral("supplier: returns status RETURN_TO_SUPPLIER"));

        // Close the supplier-credit loop.
        const ReturnOpResult sent
            = svc.markSupplierReturnSent(r.returnId, QStringLiteral("CN-123"));
        s.check(sent.ok, QStringLiteral("supplier: markSupplierReturnSent ok"));
        QSqlQuery rq(db);
        rq.prepare(QStringLiteral(
            "SELECT supplier_settled_at, supplier_reference FROM returns WHERE id = ?"));
        rq.addBindValue(r.returnId);
        s.check(rq.exec() && rq.next(), QStringLiteral("supplier: returns row readable"));
        s.check(!rq.value(0).isNull() && !rq.value(0).toString().isEmpty(),
                QStringLiteral("supplier: supplier_settled_at not null"));
        s.check(rq.value(1).toString() == QStringLiteral("CN-123"),
                QStringLiteral("supplier: supplier_reference CN-123"));

        // Marking again is refused (already sent).
        const ReturnOpResult again
            = svc.markSupplierReturnSent(r.returnId, QStringLiteral("CN-999"));
        s.check(!again.ok, QStringLiteral("supplier: second markSupplierReturnSent refused"));
    }

    // ====================================================================
    // 13) adjudicate() on an already-final return is refused.
    // ====================================================================
    {
        const qint64 medId = makeMedicine(db, userId);
        [[maybe_unused]] const qint64 batchId
            = makeBatch(db, userId, medId, 20, QStringLiteral("1.00"), future);
        (void)batchId;
        const SaleResult sale = sellOne(db, userId, medId, 5, QStringLiteral("1.00"));
        const qint64 itemId = firstSaleItemId(db, sale.saleId);

        ReturnService svc(db, userId);
        const ReturnResult r = svc.initiate(itemId, 2, QStringLiteral("OTHER"));
        s.check(r.ok, QStringLiteral("final-guard: initiate ok"));
        const ReturnOpResult adj1 = svc.adjudicate(r.returnId, QStringLiteral("WRITE_OFF"));
        s.check(adj1.ok, QStringLiteral("final-guard: first adjudicate ok"));
        const ReturnOpResult adj2 = svc.adjudicate(r.returnId, QStringLiteral("APPROVED_RESTOCK"));
        s.check(!adj2.ok, QStringLiteral("final-guard: re-adjudicate on final return refused"));
    }

    // ====================================================================
    // 14) Controlled (NARCOTIC) medicine: initiate without a witness is refused;
    //     initiate with a valid MANAGER witness (≠ operator) is accepted.
    // ====================================================================
    {
        const qint64 managerId = makeStaff(db, userId, QStringLiteral("MANAGER"));
        s.check(managerId > 0, QStringLiteral("ctrl: manager witness created"));

        // No-witness attempt.
        {
            const qint64 medId = makeNarcoticMedicine(db, userId);
            s.check(medId > 0, QStringLiteral("ctrl: narcotic medicine created"));
            [[maybe_unused]] const qint64 batchId
                = makeBatch(db, userId, medId, 50, QStringLiteral("50.00"), future);
            (void)batchId;
            // Sell with the two-person rule satisfied so the SALE goes through.
            SaleService sale(db, userId);
            SaleInput in;
            SaleLineInput li;
            li.medicineId = medId;
            li.qtySoldDisplay = 5;
            li.soldUnitLabel = QStringLiteral("TABLET");
            li.soldUnitFactor = 1;
            li.unitMrp = QStringLiteral("50.00");
            in.items << li;
            in.paymentMode = QStringLiteral("CASH");
            in.amountTendered = QStringLiteral("100000");
            in.prescriberLicense = QStringLiteral("LIC-555");
            in.controlledWitnessUserId = managerId;
            const SaleResult sr = sale.commit(in);
            s.check(sr.ok, QStringLiteral("ctrl: controlled sale committed"));
            const qint64 itemId = firstSaleItemId(db, sr.saleId);

            ReturnService svc(db, userId);
            const ReturnResult noWit = svc.initiate(itemId, 2, QStringLiteral("ADVERSE_REACTION"));
            s.check(!noWit.ok, QStringLiteral("ctrl: initiate without witness refused"));
            s.check(noWit.error.contains(QStringLiteral("witness"), Qt::CaseInsensitive),
                    QStringLiteral("ctrl: error mentions witness"));

            // With a valid manager witness (≠ operator) it is accepted.
            const ReturnResult wit
                = svc.initiate(itemId, 2, QStringLiteral("ADVERSE_REACTION"), QString(), managerId);
            s.check(wit.ok, QStringLiteral("ctrl: initiate with manager witness ok"));
        }
    }

    // ====================================================================
    // 15) Restock guard: a quarantined batch cannot be APPROVED_RESTOCK.
    // ====================================================================
    {
        const qint64 medId = makeMedicine(db, userId);
        [[maybe_unused]] const qint64 batchId
            = makeBatch(db, userId, medId, 30, QStringLiteral("4.00"), future);
        const SaleResult sale = sellOne(db, userId, medId, 6, QStringLiteral("4.00"));
        s.check(sale.ok, QStringLiteral("quar: sale committed"));
        const qint64 itemId = firstSaleItemId(db, sale.saleId);
        s.check(saleItemBatchId(db, itemId) == batchId,
                QStringLiteral("quar: sale_item bound to batch"));

        ReturnService svc(db, userId);
        const ReturnResult r = svc.initiate(itemId, 3, QStringLiteral("DAMAGED"));
        s.check(r.ok, QStringLiteral("quar: initiate ok"));

        // Quarantine the batch, then try to restock.
        QSqlQuery up(db);
        up.prepare(QStringLiteral("UPDATE batches SET is_quarantined = 1 WHERE id = ?"));
        up.addBindValue(batchId);
        s.check(up.exec(), QStringLiteral("quar: batch marked quarantined"));

        const int qtyBefore = batchQty(db, batchId);
        const ReturnOpResult adj = svc.adjudicate(r.returnId, QStringLiteral("APPROVED_RESTOCK"));
        s.check(!adj.ok, QStringLiteral("quar: restock onto quarantined batch refused"));
        s.check(adj.error.contains(QStringLiteral("quarantin"), Qt::CaseInsensitive),
                QStringLiteral("quar: error mentions quarantined"));
        s.check(batchQty(db, batchId) == qtyBefore,
                QStringLiteral("quar: on-hand unchanged after refused restock"));
    }

    return s;
}

} // namespace pharmadesk_tests
