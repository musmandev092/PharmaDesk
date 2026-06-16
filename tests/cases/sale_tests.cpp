// Exhaustive DB-backed tests for the sale-commit path (service/SaleService).
//
// This module creates its OWN medicines + batches with unique "TSALE_"-prefixed
// SKUs / batch numbers, then drives SaleService::commit() and verifies:
//   - money math (subtotal / grand / change) for many qty x mrp combos
//   - receipt-number format INV-YYYYMMDD-NNNN and cross-sale uniqueness
//   - stock decrements by exactly qtyBase
//   - sale_items + inventory_movements rows (counts, qty_before/after,
//     movement_type 'SALE', negative qty_delta)
//   - FEFO across multiple batches (soonest expiry drained first, split lines)
//   - pack selling (soldUnitFactor > 1)
//   - multi-line sales, CARD / OTHER payment modes
//   - validation rejections (empty cart, qty bounds, negative mrp, discount
//     bounds, CASH tendered rules, invalid payment mode) with stock unchanged
//   - insufficient stock -> ok==false, error mentions stock, stock unchanged
//
// The module does not assume a clean DB beyond what it creates and does not
// depend on other modules.

#include "framework/TestStats.h"

#include "data/BatchRepository.h"
#include "data/MedicineRepository.h"
#include "data/UserRepository.h"
#include "domain/ControlledSubstancePolicy.h"
#include "domain/DiscountAuthorizationPolicy.h"
#include "domain/SaleCalculator.h"
#include "service/SaleService.h"

#include <QDate>
#include <QSqlQuery>
#include <QString>
#include <QVariant>
#include <QVector>

namespace pharmadesk_tests {

namespace {

// A medicine + (optionally) one stock batch created for a single test.
struct TestMed
{
    qint64 id = 0;
};

// Counter so every SKU / batch number is unique across the whole run.
int g_uniq = 0;

QString uniqueSku()
{
    return QStringLiteral("TSALE_SKU_%1").arg(++g_uniq, 5, 10, QLatin1Char('0'));
}

QString uniqueBatch()
{
    return QStringLiteral("TSALE_B_%1").arg(++g_uniq, 5, 10, QLatin1Char('0'));
}

// Create a medicine with no stock; returns its id (or -1).
qint64 makeMedicine(QSqlDatabase db, qint64 userId, int unitsPerPurchase = 1)
{
    MedicineRepository repo(db);
    MedicineDraft d;
    d.sku = uniqueSku();
    d.brandName = QStringLiteral("TSale Brand");
    d.genericName = QStringLiteral("tsale-generic");
    d.strength = QStringLiteral("500mg");
    d.form = QStringLiteral("TABLET");
    d.purchaseUnit = QStringLiteral("BOX");
    d.baseUnit = QStringLiteral("TABLET");
    d.unitsPerPurchase = unitsPerPurchase;
    d.isActive = true;
    return repo.create(d, userId);
}

// Create a medicine with an explicit controlled schedule; returns its id (or -1).
qint64 makeMedicineSched(QSqlDatabase db, qint64 userId, const QString &schedule,
                         int unitsPerPurchase = 1)
{
    MedicineRepository repo(db);
    MedicineDraft d;
    d.sku = uniqueSku();
    d.brandName = QStringLiteral("TSale Brand");
    d.genericName = QStringLiteral("tsale-generic");
    d.strength = QStringLiteral("500mg");
    d.form = QStringLiteral("TABLET");
    d.purchaseUnit = QStringLiteral("BOX");
    d.baseUnit = QStringLiteral("TABLET");
    d.unitsPerPurchase = unitsPerPurchase;
    d.controlledSchedule = schedule;
    d.isActive = true;
    return repo.create(d, userId);
}

// Create an active staff user with a unique username; returns its id (or -1).
qint64 makeUser(QSqlDatabase db, qint64 actingUserId, const QString &role)
{
    UserRepository repo(db);
    const int n = ++g_uniq;
    const QString username = QStringLiteral("tsale_user_%1").arg(n, 5, 10, QLatin1Char('0'));
    const QString fullName = QStringLiteral("TSale %1 %2").arg(role).arg(n);
    return repo.createUser(fullName, username, role, QStringLiteral("x-pin-hash"),
                           /*isActive=*/true, actingUserId);
}

// Add one batch of stock to a medicine. Returns batch id (or -1).
qint64 addBatch(QSqlDatabase db, qint64 userId, qint64 medId, int qty, const QString &mrp,
                const QDate &expiry, const QString &cost = QStringLiteral("1.0000"))
{
    BatchRepository repo(db);
    StockInDraft d;
    d.medicineId = medId;
    d.batchNumber = uniqueBatch();
    d.expiry = expiry;
    d.quantity = qty;
    d.costPerUnit = cost;
    d.mrpPerUnit = mrp;
    return repo.addStock(d, userId);
}

// Convenience: create a medicine + a single far-future batch; return its id.
qint64 makeMedWithStock(QSqlDatabase db, qint64 userId, int qty, const QString &mrp,
                        int unitsPerPurchase = 1)
{
    const qint64 medId = makeMedicine(db, userId, unitsPerPurchase);
    if (medId <= 0) {
        return -1;
    }
    const QDate expiry = QDate::currentDate().addYears(2);
    if (addBatch(db, userId, medId, qty, mrp, expiry) <= 0) {
        return -1;
    }
    return medId;
}

// Build one cart line.
SaleLineInput line(qint64 medId, int qty, const QString &mrp, int factor = 1,
                   const QString &label = QStringLiteral("TABLET"))
{
    SaleLineInput l;
    l.medicineId = medId;
    l.qtySoldDisplay = qty;
    l.soldUnitLabel = label;
    l.soldUnitFactor = factor;
    l.unitMrp = mrp;
    return l;
}

// --- DB query helpers -------------------------------------------------------

int onHandOf(QSqlDatabase db, qint64 medId)
{
    return BatchRepository(db).onHand(medId);
}

int countSaleItems(QSqlDatabase db, qint64 saleId)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT count(*) FROM sale_items WHERE sale_id = ?"));
    q.addBindValue(saleId);
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return -1;
}

int countSaleMovements(QSqlDatabase db, qint64 saleId)
{
    QSqlQuery q(db);
    q.prepare(
        QStringLiteral("SELECT count(*) FROM inventory_movements "
                       "WHERE ref_table = 'sales' AND ref_id = ? AND movement_type = 'SALE'"));
    q.addBindValue(saleId);
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return -1;
}

// Sum of qty_delta across the SALE movements for a sale (should be negative).
int sumMovementDelta(QSqlDatabase db, qint64 saleId)
{
    QSqlQuery q(db);
    q.prepare(
        QStringLiteral("SELECT COALESCE(SUM(qty_delta),0) FROM inventory_movements "
                       "WHERE ref_table = 'sales' AND ref_id = ? AND movement_type = 'SALE'"));
    q.addBindValue(saleId);
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}

// True if every SALE movement for this sale has qty_delta < 0 and
// qty_after == qty_before + qty_delta.
bool movementsConsistent(QSqlDatabase db, qint64 saleId)
{
    QSqlQuery q(db);
    q.prepare(
        QStringLiteral("SELECT qty_delta, qty_before, qty_after FROM inventory_movements "
                       "WHERE ref_table = 'sales' AND ref_id = ? AND movement_type = 'SALE'"));
    q.addBindValue(saleId);
    if (!q.exec()) {
        return false;
    }
    int rows = 0;
    while (q.next()) {
        ++rows;
        const int delta = q.value(0).toInt();
        const int before = q.value(1).toInt();
        const int after = q.value(2).toInt();
        if (delta >= 0) {
            return false;
        }
        if (after != before + delta) {
            return false;
        }
    }
    return rows > 0;
}

// Total base units recorded against a sale in sale_items.
int saleItemsQtyBase(QSqlDatabase db, qint64 saleId)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(qty_in_base_units),0) FROM sale_items WHERE sale_id = ?"));
    q.addBindValue(saleId);
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return -1;
}

QString saleField(QSqlDatabase db, qint64 saleId, const QString &col)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT %1 FROM sales WHERE id = ?").arg(col));
    q.addBindValue(saleId);
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    return QString();
}

// Validate INV-YYYYMMDD-NNNN where YYYYMMDD == today and NNNN is 4+ digits.
bool receiptFormatOk(const QString &r)
{
    const QString today = QDate::currentDate().toString(QStringLiteral("yyyyMMdd"));
    const QString prefix = QStringLiteral("INV-") + today + QStringLiteral("-");
    if (!r.startsWith(prefix)) {
        return false;
    }
    const QString suffix = r.mid(prefix.size());
    if (suffix.size() < 4) {
        return false;
    }
    for (const QChar &c : suffix) {
        if (!c.isDigit()) {
            return false;
        }
    }
    return true;
}

} // namespace

TestStats run_sale_tests(QSqlDatabase db, qint64 userId)
{
    TestStats s;
    s.module = QStringLiteral("sale");

    const QDate farFuture = QDate::currentDate().addYears(2);

    // ====================================================================
    // 1) HAPPY PATH — money math for many qty x mrp combos (CASH, exact).
    //    Each combo: subtotal == grand (no discount), change exact, ok true,
    //    receipt format ok, stock decremented by qtyBase, rows written.
    // ====================================================================
    struct Combo
    {
        int qty;
        const char *mrp;
        const char *expSub;
        const char *tendered;
        const char *expChange;
    };
    const QVector<Combo> combos = {
        {1, "10.00", "10.00", "10.00", "0.00"},        {2, "10.00", "20.00", "50.00", "30.00"},
        {3, "9.99", "29.97", "30.00", "0.03"},         {5, "1.25", "6.25", "10.00", "3.75"},
        {10, "100.50", "1005.00", "1005.00", "0.00"},  {7, "12.34", "86.38", "100.00", "13.62"},
        {4, "0.50", "2.00", "5.00", "3.00"},           {12, "3.33", "39.96", "40.00", "0.04"},
        {100, "1.00", "100.00", "200.00", "100.00"},   {250, "0.50", "125.00", "125.00", "0.00"},
        {6, "250.75", "1504.50", "2000.00", "495.50"}, {1, "0.01", "0.01", "1.00", "0.99"},
    };

    for (const Combo &c : combos) {
        const QString mrp = QString::fromUtf8(c.mrp);
        const qint64 medId = makeMedWithStock(db, userId, 5000, mrp);
        s.check(medId > 0, QStringLiteral("happy: medicine created (mrp=%1)").arg(mrp));
        if (medId <= 0) {
            continue;
        }
        const int before = onHandOf(db, medId);
        s.check(before == 5000, QStringLiteral("happy: initial onHand 5000 (mrp=%1)").arg(mrp));

        SaleInput in;
        in.items = {line(medId, c.qty, mrp)};
        in.paymentMode = QStringLiteral("CASH");
        in.amountTendered = QString::fromUtf8(c.tendered);

        SaleService svc(db, userId);
        SaleResult r = svc.commit(in);

        const QString tag = QStringLiteral("qty=%1 mrp=%2").arg(c.qty).arg(mrp);
        s.check(r.ok, QStringLiteral("happy: ok (%1)").arg(tag));
        s.check(r.error.isEmpty(), QStringLiteral("happy: no error (%1)").arg(tag));
        s.check(r.saleId > 0, QStringLiteral("happy: saleId>0 (%1)").arg(tag));
        s.check(r.subtotal == QString::fromUtf8(c.expSub),
                QStringLiteral("happy: subtotal %1 (%2)").arg(QString::fromUtf8(c.expSub), tag));
        s.check(r.discountTotal == QStringLiteral("0.00"),
                QStringLiteral("happy: discount 0.00 (%1)").arg(tag));
        s.check(r.grandTotal == QString::fromUtf8(c.expSub),
                QStringLiteral("happy: grand==subtotal (%1)").arg(tag));
        s.check(r.amountTendered == QString::fromUtf8(c.tendered),
                QStringLiteral("happy: tendered echoed (%1)").arg(tag));
        s.check(r.changeReturned == QString::fromUtf8(c.expChange),
                QStringLiteral("happy: change %1 (%2)").arg(QString::fromUtf8(c.expChange), tag));
        s.check(r.paymentMode == QStringLiteral("CASH"),
                QStringLiteral("happy: payment CASH (%1)").arg(tag));
        s.check(receiptFormatOk(r.receiptNumber),
                QStringLiteral("happy: receipt format (%1)").arg(tag));

        // Stock decremented by exactly qtyBase (factor 1 here).
        const int after = onHandOf(db, medId);
        s.check(after == before - c.qty,
                QStringLiteral("happy: onHand decremented by qtyBase (%1)").arg(tag));

        // Rows written.
        s.check(countSaleItems(db, r.saleId) == 1,
                QStringLiteral("happy: 1 sale_item row (%1)").arg(tag));
        s.check(countSaleMovements(db, r.saleId) == 1,
                QStringLiteral("happy: 1 SALE movement (%1)").arg(tag));
        s.check(saleItemsQtyBase(db, r.saleId) == c.qty,
                QStringLiteral("happy: sale_items qty_base == qty (%1)").arg(tag));
        s.check(sumMovementDelta(db, r.saleId) == -c.qty,
                QStringLiteral("happy: movement delta == -qty (%1)").arg(tag));
        s.check(movementsConsistent(db, r.saleId),
                QStringLiteral("happy: movement before/after consistent (%1)").arg(tag));

        // Persisted sale row matches the result.
        s.check(saleField(db, r.saleId, QStringLiteral("receipt_number")) == r.receiptNumber,
                QStringLiteral("happy: persisted receipt matches (%1)").arg(tag));
        s.check(saleField(db, r.saleId, QStringLiteral("grand_total")) == r.grandTotal,
                QStringLiteral("happy: persisted grand matches (%1)").arg(tag));
        s.check(saleField(db, r.saleId, QStringLiteral("status")) == QStringLiteral("COMPLETED"),
                QStringLiteral("happy: status COMPLETED (%1)").arg(tag));
    }

    // ====================================================================
    // 2) DISCOUNT happy paths — grand == subtotal - discount.
    // ====================================================================
    {
        struct DCase
        {
            int qty;
            const char *mrp;
            const char *disc;
            const char *expSub;
            const char *expGrand;
            const char *tendered;
            const char *expChange;
        };
        const QVector<DCase> dcases = {
            {10, "10.00", "5.00", "100.00", "95.00", "100.00", "5.00"},
            {4, "25.00", "0.00", "100.00", "100.00", "100.00", "0.00"},
            {2, "50.00", "100.00", "100.00", "0.00", "0.00",
             "0.00"}, // discount == subtotal -> grand 0
            {3, "33.33", "0.99", "99.99", "99.00", "100.00", "1.00"},
        };
        for (const DCase &d : dcases) {
            const QString mrp = QString::fromUtf8(d.mrp);
            const qint64 medId = makeMedWithStock(db, userId, 1000, mrp);
            s.check(medId > 0, QStringLiteral("disc: med created"));
            if (medId <= 0) {
                continue;
            }
            SaleInput in;
            in.items = {line(medId, d.qty, mrp)};
            in.discountTotal = QString::fromUtf8(d.disc);
            in.paymentMode = QStringLiteral("CASH");
            in.amountTendered = QString::fromUtf8(d.tendered);
            SaleResult r = SaleService(db, userId).commit(in);
            const QString tag = QStringLiteral("disc=%1").arg(QString::fromUtf8(d.disc));
            s.check(r.ok, QStringLiteral("disc: ok (%1)").arg(tag));
            s.check(r.subtotal == QString::fromUtf8(d.expSub),
                    QStringLiteral("disc: subtotal (%1)").arg(tag));
            s.check(
                r.discountTotal
                    == QStringLiteral("%1").arg(QString::fromUtf8(d.disc) == QStringLiteral("0.00")
                                                    ? QStringLiteral("0.00")
                                                    : QString::fromUtf8(d.disc)),
                QStringLiteral("disc: discount echoed (%1)").arg(tag));
            s.check(r.grandTotal == QString::fromUtf8(d.expGrand),
                    QStringLiteral("disc: grand (%1)").arg(tag));
            s.check(r.changeReturned == QString::fromUtf8(d.expChange),
                    QStringLiteral("disc: change (%1)").arg(tag));
        }
    }

    // ====================================================================
    // 3) FEFO across multiple batches — soonest expiry drained first,
    //    line split, onHand after.
    // ====================================================================
    {
        const qint64 medId = makeMedicine(db, userId);
        s.check(medId > 0, QStringLiteral("fefo: medicine created"));
        // Three batches: nearest expiry has 30 units, mid 50, far 100.
        const qint64 bSoon = addBatch(db, userId, medId, 30, QStringLiteral("5.00"),
                                      QDate::currentDate().addDays(30));
        const qint64 bMid = addBatch(db, userId, medId, 50, QStringLiteral("5.00"),
                                     QDate::currentDate().addDays(90));
        const qint64 bFar = addBatch(db, userId, medId, 100, QStringLiteral("5.00"),
                                     QDate::currentDate().addDays(365));
        s.check(bSoon > 0 && bMid > 0 && bFar > 0, QStringLiteral("fefo: 3 batches created"));
        s.check(onHandOf(db, medId) == 180, QStringLiteral("fefo: initial onHand 180"));

        // Sell 60 -> drains soon(30) fully + mid(30 of 50). Two sale_items.
        SaleInput in;
        in.items = {line(medId, 60, QStringLiteral("5.00"))};
        in.paymentMode = QStringLiteral("CASH");
        in.amountTendered = QStringLiteral("300.00");
        SaleResult r = SaleService(db, userId).commit(in);
        s.check(r.ok, QStringLiteral("fefo: sale ok"));
        s.check(r.subtotal == QStringLiteral("300.00"), QStringLiteral("fefo: subtotal 300.00"));
        s.check(onHandOf(db, medId) == 120, QStringLiteral("fefo: onHand 120 after sale of 60"));
        s.check(countSaleItems(db, r.saleId) == 2, QStringLiteral("fefo: split into 2 sale_items"));
        s.check(countSaleMovements(db, r.saleId) == 2, QStringLiteral("fefo: 2 movements"));
        s.check(saleItemsQtyBase(db, r.saleId) == 60, QStringLiteral("fefo: total base 60"));
        s.check(sumMovementDelta(db, r.saleId) == -60, QStringLiteral("fefo: movement delta -60"));
        s.check(movementsConsistent(db, r.saleId), QStringLiteral("fefo: movements consistent"));

        // Soonest batch fully drained; mid reduced to 20; far untouched (100).
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT current_qty FROM batches WHERE id = ?"));
        q.addBindValue(bSoon);
        q.exec();
        q.next();
        s.check(q.value(0).toInt() == 0, QStringLiteral("fefo: soonest batch drained to 0"));
        q.prepare(QStringLiteral("SELECT current_qty FROM batches WHERE id = ?"));
        q.addBindValue(bMid);
        q.exec();
        q.next();
        s.check(q.value(0).toInt() == 20, QStringLiteral("fefo: mid batch reduced to 20"));
        q.prepare(QStringLiteral("SELECT current_qty FROM batches WHERE id = ?"));
        q.addBindValue(bFar);
        q.exec();
        q.next();
        s.check(q.value(0).toInt() == 100, QStringLiteral("fefo: far batch untouched 100"));

        // The first sale_item row (FEFO order) should be the soonest batch with qty 30.
        QSqlQuery siq(db);
        siq.prepare(QStringLiteral("SELECT batch_id, qty_in_base_units FROM sale_items WHERE "
                                   "sale_id = ? ORDER BY id ASC"));
        siq.addBindValue(r.saleId);
        siq.exec();
        bool firstIsSoon = false;
        bool secondIsMid = false;
        if (siq.next()) {
            firstIsSoon = (siq.value(0).toLongLong() == bSoon && siq.value(1).toInt() == 30);
        }
        if (siq.next()) {
            secondIsMid = (siq.value(0).toLongLong() == bMid && siq.value(1).toInt() == 30);
        }
        s.check(firstIsSoon, QStringLiteral("fefo: first item is soonest batch x30"));
        s.check(secondIsMid, QStringLiteral("fefo: second item is mid batch x30"));
    }

    // FEFO tie-break by id (same expiry): lower id drains first.
    {
        const qint64 medId = makeMedicine(db, userId);
        const QDate exp = QDate::currentDate().addDays(60);
        const qint64 b1 = addBatch(db, userId, medId, 10, QStringLiteral("2.00"), exp);
        const qint64 b2 = addBatch(db, userId, medId, 10, QStringLiteral("2.00"), exp);
        s.check(b1 > 0 && b2 > 0 && b1 < b2, QStringLiteral("fefo-tie: two same-expiry batches"));
        SaleInput in;
        in.items = {line(medId, 15, QStringLiteral("2.00"))};
        in.paymentMode = QStringLiteral("CASH");
        in.amountTendered = QStringLiteral("30.00");
        SaleResult r = SaleService(db, userId).commit(in);
        s.check(r.ok, QStringLiteral("fefo-tie: ok"));
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT current_qty FROM batches WHERE id = ?"));
        q.addBindValue(b1);
        q.exec();
        q.next();
        s.check(q.value(0).toInt() == 0, QStringLiteral("fefo-tie: lower-id batch drained first"));
        q.prepare(QStringLiteral("SELECT current_qty FROM batches WHERE id = ?"));
        q.addBindValue(b2);
        q.exec();
        q.next();
        s.check(q.value(0).toInt() == 5, QStringLiteral("fefo-tie: higher-id batch keeps 5"));
    }

    // ====================================================================
    // 4) PACK selling — soldUnitFactor > 1 means qtyBase = qty * factor.
    // ====================================================================
    {
        struct PCase
        {
            int qty;
            int factor;
            const char *mrp;
        };
        const QVector<PCase> pcases = {
            {2, 10, "1.50"},  // 20 base units
            {3, 5, "4.00"},   // 15 base units
            {1, 100, "0.25"}, // 100 base units
        };
        for (const PCase &p : pcases) {
            const QString mrp = QString::fromUtf8(p.mrp);
            const qint64 medId = makeMedWithStock(db, userId, 2000, mrp, p.factor);
            s.check(medId > 0, QStringLiteral("pack: med created"));
            if (medId <= 0) {
                continue;
            }
            const int before = onHandOf(db, medId);
            const int qtyBase = p.qty * p.factor;
            SaleInput in;
            in.items = {line(medId, p.qty, mrp, p.factor, QStringLiteral("BOX"))};
            in.paymentMode = QStringLiteral("CASH");
            in.amountTendered = QStringLiteral("100000.00");
            SaleResult r = SaleService(db, userId).commit(in);
            const QString tag = QStringLiteral("qty=%1xf=%2").arg(p.qty).arg(p.factor);
            s.check(r.ok, QStringLiteral("pack: ok (%1)").arg(tag));
            // Line math uses base units x mrp.
            const QString expSub = SaleCalculator::lineSubtotal(qtyBase, mrp);
            s.check(r.subtotal == expSub,
                    QStringLiteral("pack: subtotal == base*mrp (%1)").arg(tag));
            s.check(onHandOf(db, medId) == before - qtyBase,
                    QStringLiteral("pack: onHand -= qty*factor (%1)").arg(tag));
            s.check(saleItemsQtyBase(db, r.saleId) == qtyBase,
                    QStringLiteral("pack: sale_items base == qty*factor (%1)").arg(tag));
            s.check(sumMovementDelta(db, r.saleId) == -qtyBase,
                    QStringLiteral("pack: movement delta == -qty*factor (%1)").arg(tag));
        }
    }

    // ====================================================================
    // 5) MULTIPLE LINES in one sale.
    // ====================================================================
    {
        const qint64 medA = makeMedWithStock(db, userId, 500, QStringLiteral("10.00"));
        const qint64 medB = makeMedWithStock(db, userId, 500, QStringLiteral("2.50"));
        const qint64 medC = makeMedWithStock(db, userId, 500, QStringLiteral("100.00"));
        s.check(medA > 0 && medB > 0 && medC > 0, QStringLiteral("multi: 3 meds created"));
        SaleInput in;
        in.items = {line(medA, 3, QStringLiteral("10.00")),   // 30.00
                    line(medB, 4, QStringLiteral("2.50")),    // 10.00
                    line(medC, 1, QStringLiteral("100.00"))}; // 100.00
        in.paymentMode = QStringLiteral("CASH");
        in.amountTendered = QStringLiteral("200.00");
        SaleResult r = SaleService(db, userId).commit(in);
        s.check(r.ok, QStringLiteral("multi: ok"));
        s.check(r.subtotal == QStringLiteral("140.00"), QStringLiteral("multi: subtotal 140.00"));
        s.check(r.grandTotal == QStringLiteral("140.00"), QStringLiteral("multi: grand 140.00"));
        s.check(r.changeReturned == QStringLiteral("60.00"), QStringLiteral("multi: change 60.00"));
        s.check(r.lines.size() == 3, QStringLiteral("multi: 3 result lines"));
        s.check(countSaleItems(db, r.saleId) == 3, QStringLiteral("multi: 3 sale_items"));
        s.check(countSaleMovements(db, r.saleId) == 3, QStringLiteral("multi: 3 movements"));
        s.check(onHandOf(db, medA) == 497, QStringLiteral("multi: medA onHand 497"));
        s.check(onHandOf(db, medB) == 496, QStringLiteral("multi: medB onHand 496"));
        s.check(onHandOf(db, medC) == 499, QStringLiteral("multi: medC onHand 499"));
    }

    // ====================================================================
    // 6) CARD with no tendered (allowed, change empty); OTHER mode.
    // ====================================================================
    {
        const qint64 medId = makeMedWithStock(db, userId, 100, QStringLiteral("20.00"));
        SaleInput in;
        in.items = {line(medId, 2, QStringLiteral("20.00"))};
        in.paymentMode = QStringLiteral("CARD");
        // no amountTendered
        SaleResult r = SaleService(db, userId).commit(in);
        s.check(r.ok, QStringLiteral("card: ok with no tendered"));
        s.check(r.grandTotal == QStringLiteral("40.00"), QStringLiteral("card: grand 40.00"));
        s.check(r.amountTendered.isEmpty(), QStringLiteral("card: tendered empty"));
        s.check(r.changeReturned.isEmpty(), QStringLiteral("card: change empty"));
        s.check(r.paymentMode == QStringLiteral("CARD"), QStringLiteral("card: mode CARD"));
        s.check(saleField(db, r.saleId, QStringLiteral("payment_mode")) == QStringLiteral("CARD"),
                QStringLiteral("card: persisted mode CARD"));
        s.check(onHandOf(db, medId) == 98, QStringLiteral("card: onHand 98"));
    }
    {
        const qint64 medId = makeMedWithStock(db, userId, 100, QStringLiteral("15.00"));
        SaleInput in;
        in.items = {line(medId, 1, QStringLiteral("15.00"))};
        in.paymentMode = QStringLiteral("OTHER");
        SaleResult r = SaleService(db, userId).commit(in);
        s.check(r.ok, QStringLiteral("other: ok"));
        s.check(r.paymentMode == QStringLiteral("OTHER"), QStringLiteral("other: mode OTHER"));
        s.check(r.changeReturned.isEmpty(), QStringLiteral("other: change empty"));
        s.check(onHandOf(db, medId) == 99, QStringLiteral("other: onHand 99"));
    }
    // CARD with tendered supplied -> change computed.
    {
        const qint64 medId = makeMedWithStock(db, userId, 100, QStringLiteral("30.00"));
        SaleInput in;
        in.items = {line(medId, 2, QStringLiteral("30.00"))};
        in.paymentMode = QStringLiteral("CARD");
        in.amountTendered = QStringLiteral("100.00");
        SaleResult r = SaleService(db, userId).commit(in);
        s.check(r.ok, QStringLiteral("card-tendered: ok"));
        s.check(r.changeReturned == QStringLiteral("40.00"),
                QStringLiteral("card-tendered: change 40.00"));
    }

    // ====================================================================
    // 7) VALIDATION REJECTIONS — ok==false, stock unchanged, no rows.
    //    Each uses its own freshly-stocked medicine so we can assert the
    //    onHand is unchanged after the rejection.
    // ====================================================================

    // Helper to assert a rejection leaves stock untouched.
    auto expectRejected = [&](const QString &name, qint64 medId, int expectedOnHand,
                              const SaleInput &in, const QString &errSubstr = QString()) {
        const int before = onHandOf(db, medId);
        s.check(before == expectedOnHand, QStringLiteral("%1: stock baseline").arg(name));
        SaleResult r = SaleService(db, userId).commit(in);
        s.check(!r.ok, QStringLiteral("%1: rejected (ok false)").arg(name));
        s.check(r.saleId == -1, QStringLiteral("%1: no saleId").arg(name));
        s.check(!r.error.isEmpty(), QStringLiteral("%1: error message set").arg(name));
        if (!errSubstr.isEmpty()) {
            s.check(r.error.contains(errSubstr, Qt::CaseInsensitive),
                    QStringLiteral("%1: error mentions '%2'").arg(name, errSubstr));
        }
        s.check(onHandOf(db, medId) == before, QStringLiteral("%1: stock unchanged").arg(name));
    };

    // 7a) empty cart
    {
        SaleInput in;
        in.paymentMode = QStringLiteral("CASH");
        in.amountTendered = QStringLiteral("0.00");
        SaleResult r = SaleService(db, userId).commit(in);
        s.check(!r.ok, QStringLiteral("reject empty: ok false"));
        s.check(!r.error.isEmpty(), QStringLiteral("reject empty: error set"));
        s.check(r.error.contains(QStringLiteral("item"), Qt::CaseInsensitive),
                QStringLiteral("reject empty: mentions item"));
    }

    // 7b) qty < 1 (zero and negative)
    {
        const qint64 medId = makeMedWithStock(db, userId, 100, QStringLiteral("5.00"));
        SaleInput in0;
        in0.items = {line(medId, 0, QStringLiteral("5.00"))};
        in0.paymentMode = QStringLiteral("CARD");
        expectRejected(QStringLiteral("reject qty0"), medId, 100, in0);

        SaleInput inNeg;
        inNeg.items = {line(medId, -3, QStringLiteral("5.00"))};
        inNeg.paymentMode = QStringLiteral("CARD");
        expectRejected(QStringLiteral("reject qtyNeg"), medId, 100, inNeg);
    }

    // 7c) qty > 9999
    {
        const qint64 medId = makeMedWithStock(db, userId, 100, QStringLiteral("5.00"));
        SaleInput in;
        in.items = {line(medId, 10000, QStringLiteral("5.00"))};
        in.paymentMode = QStringLiteral("CARD");
        expectRejected(QStringLiteral("reject qtyTooBig"), medId, 100, in);
        // exactly 9999 is allowed by the bound check (but will fail on stock here);
        // 10000 must be rejected on the qty bound first.
    }

    // 7d) negative unitMrp
    {
        const qint64 medId = makeMedWithStock(db, userId, 100, QStringLiteral("5.00"));
        SaleInput in;
        in.items = {line(medId, 2, QStringLiteral("-5.00"))};
        in.paymentMode = QStringLiteral("CARD");
        expectRejected(QStringLiteral("reject negMrp"), medId, 100, in, QStringLiteral("negative"));
    }

    // 7e) discount negative
    {
        const qint64 medId = makeMedWithStock(db, userId, 100, QStringLiteral("10.00"));
        SaleInput in;
        in.items = {line(medId, 2, QStringLiteral("10.00"))};
        in.discountTotal = QStringLiteral("-1.00");
        in.paymentMode = QStringLiteral("CARD");
        expectRejected(QStringLiteral("reject discNeg"), medId, 100, in,
                       QStringLiteral("discount"));
    }

    // 7f) discount > subtotal
    {
        const qint64 medId = makeMedWithStock(db, userId, 100, QStringLiteral("10.00"));
        SaleInput in;
        in.items = {line(medId, 2, QStringLiteral("10.00"))}; // subtotal 20.00
        in.discountTotal = QStringLiteral("25.00");
        in.paymentMode = QStringLiteral("CARD");
        expectRejected(QStringLiteral("reject discTooBig"), medId, 100, in,
                       QStringLiteral("discount"));
    }

    // 7g) CASH without tendered
    {
        const qint64 medId = makeMedWithStock(db, userId, 100, QStringLiteral("10.00"));
        SaleInput in;
        in.items = {line(medId, 2, QStringLiteral("10.00"))};
        in.paymentMode = QStringLiteral("CASH");
        // no amountTendered
        expectRejected(QStringLiteral("reject cashNoTender"), medId, 100, in,
                       QStringLiteral("tendered"));
    }

    // 7h) CASH with insufficient tendered
    {
        const qint64 medId = makeMedWithStock(db, userId, 100, QStringLiteral("10.00"));
        SaleInput in;
        in.items = {line(medId, 2, QStringLiteral("10.00"))}; // grand 20.00
        in.paymentMode = QStringLiteral("CASH");
        in.amountTendered = QStringLiteral("19.99");
        expectRejected(QStringLiteral("reject cashShort"), medId, 100, in,
                       QStringLiteral("tendered"));
    }

    // 7i) invalid payment mode
    {
        const qint64 medId = makeMedWithStock(db, userId, 100, QStringLiteral("10.00"));
        SaleInput in;
        in.items = {line(medId, 2, QStringLiteral("10.00"))};
        in.paymentMode = QStringLiteral("CRYPTO");
        in.amountTendered = QStringLiteral("100.00");
        expectRejected(QStringLiteral("reject badMode"), medId, 100, in, QStringLiteral("payment"));
    }

    // 7j) negative tendered (with non-CASH so the tendered check fires)
    {
        const qint64 medId = makeMedWithStock(db, userId, 100, QStringLiteral("10.00"));
        SaleInput in;
        in.items = {line(medId, 1, QStringLiteral("10.00"))};
        in.paymentMode = QStringLiteral("CARD");
        in.amountTendered = QStringLiteral("-5.00");
        expectRejected(QStringLiteral("reject negTender"), medId, 100, in,
                       QStringLiteral("tendered"));
    }

    // ====================================================================
    // 8) INSUFFICIENT STOCK — ok false, error mentions stock, stock unchanged.
    // ====================================================================
    {
        const qint64 medId = makeMedWithStock(db, userId, 5, QStringLiteral("10.00"));
        SaleInput in;
        in.items = {line(medId, 10, QStringLiteral("10.00"))}; // need 10, have 5
        in.paymentMode = QStringLiteral("CARD");
        expectRejected(QStringLiteral("insufficient"), medId, 5, in, QStringLiteral("stock"));
    }
    // Insufficient on a multi-batch medicine (sum across batches still short).
    {
        const qint64 medId = makeMedicine(db, userId);
        addBatch(db, userId, medId, 3, QStringLiteral("4.00"), QDate::currentDate().addDays(40));
        addBatch(db, userId, medId, 4, QStringLiteral("4.00"), QDate::currentDate().addDays(80));
        s.check(onHandOf(db, medId) == 7, QStringLiteral("insufficient-multi: onHand 7"));
        SaleInput in;
        in.items = {line(medId, 20, QStringLiteral("4.00"))};
        in.paymentMode = QStringLiteral("CARD");
        expectRejected(QStringLiteral("insufficient-multi"), medId, 7, in, QStringLiteral("stock"));
    }
    // Medicine with no batches at all.
    {
        const qint64 medId = makeMedicine(db, userId);
        s.check(onHandOf(db, medId) == 0, QStringLiteral("insufficient-empty: onHand 0"));
        SaleInput in;
        in.items = {line(medId, 1, QStringLiteral("4.00"))};
        in.paymentMode = QStringLiteral("CARD");
        expectRejected(QStringLiteral("insufficient-empty"), medId, 0, in, QStringLiteral("stock"));
    }
    // Expired batch is not sellable -> insufficient even though qty exists.
    {
        const qint64 medId = makeMedicine(db, userId);
        // expiry in the past -> excluded by candidateBatchesForSale / FEFO query
        addBatch(db, userId, medId, 50, QStringLiteral("4.00"), QDate::currentDate().addDays(-1));
        SaleInput in;
        in.items = {line(medId, 1, QStringLiteral("4.00"))};
        in.paymentMode = QStringLiteral("CARD");
        SaleResult r = SaleService(db, userId).commit(in);
        s.check(!r.ok, QStringLiteral("expired-batch: rejected"));
        s.check(r.error.contains(QStringLiteral("stock"), Qt::CaseInsensitive),
                QStringLiteral("expired-batch: error mentions stock"));
    }

    // ====================================================================
    // 9) RECEIPT-NUMBER UNIQUENESS across several sales the same day.
    // ====================================================================
    {
        const qint64 medId = makeMedWithStock(db, userId, 10000, QStringLiteral("1.00"));
        QStringList receipts;
        const int N = 8;
        for (int i = 0; i < N; ++i) {
            SaleInput in;
            in.items = {line(medId, 1, QStringLiteral("1.00"))};
            in.paymentMode = QStringLiteral("CASH");
            in.amountTendered = QStringLiteral("1.00");
            SaleResult r = SaleService(db, userId).commit(in);
            s.check(r.ok, QStringLiteral("uniq: sale %1 ok").arg(i));
            s.check(receiptFormatOk(r.receiptNumber),
                    QStringLiteral("uniq: sale %1 receipt format").arg(i));
            s.check(!receipts.contains(r.receiptNumber),
                    QStringLiteral("uniq: sale %1 receipt unique").arg(i));
            receipts << r.receiptNumber;
        }
        s.check(receipts.size() == N, QStringLiteral("uniq: all %1 receipts collected").arg(N));
        // Confirm DB also sees them as distinct.
        QSqlQuery q(db);
        QStringList quoted;
        for (const QString &rn : receipts) {
            quoted << QStringLiteral("'%1'").arg(rn);
        }
        q.exec(QStringLiteral("SELECT COUNT(DISTINCT receipt_number) FROM sales "
                              "WHERE receipt_number IN (%1)")
                   .arg(quoted.join(QStringLiteral(","))));
        q.next();
        s.check(q.value(0).toInt() == N, QStringLiteral("uniq: DB shows N distinct receipts"));
    }

    // ====================================================================
    // 10) CUSTOMER fields persisted; receipt line name reflects medicine.
    // ====================================================================
    {
        const qint64 medId = makeMedWithStock(db, userId, 100, QStringLiteral("10.00"));
        SaleInput in;
        in.items = {line(medId, 1, QStringLiteral("10.00"))};
        in.paymentMode = QStringLiteral("CASH");
        in.amountTendered = QStringLiteral("10.00");
        in.customerName = QStringLiteral("Test Customer");
        in.customerPhone = QStringLiteral("03001234567");
        SaleResult r = SaleService(db, userId).commit(in);
        s.check(r.ok, QStringLiteral("cust: ok"));
        s.check(saleField(db, r.saleId, QStringLiteral("customer_name"))
                    == QStringLiteral("Test Customer"),
                QStringLiteral("cust: name persisted"));
        s.check(saleField(db, r.saleId, QStringLiteral("customer_phone"))
                    == QStringLiteral("03001234567"),
                QStringLiteral("cust: phone persisted"));
        s.check(r.lines.size() == 1, QStringLiteral("cust: 1 result line"));
        s.check(r.lines.at(0).name.contains(QStringLiteral("TSale Brand")),
                QStringLiteral("cust: result line name has brand"));
        s.check(r.lines.at(0).qty == 1, QStringLiteral("cust: result line qty 1"));
    }

    // ====================================================================
    // 11) qty exactly 9999 is within bounds (succeeds when stock allows).
    // ====================================================================
    {
        const qint64 medId = makeMedWithStock(db, userId, 9999, QStringLiteral("1.00"));
        SaleInput in;
        in.items = {line(medId, 9999, QStringLiteral("1.00"))};
        in.paymentMode = QStringLiteral("CASH");
        in.amountTendered = QStringLiteral("9999.00");
        SaleResult r = SaleService(db, userId).commit(in);
        s.check(r.ok, QStringLiteral("qty9999: ok at upper bound"));
        s.check(r.subtotal == QStringLiteral("9999.00"),
                QStringLiteral("qty9999: subtotal 9999.00"));
        s.check(onHandOf(db, medId) == 0, QStringLiteral("qty9999: stock fully drained"));
    }

    // ====================================================================
    // 12) CONTROLLED-SUBSTANCE GATE (NARCOTIC: license + manager/admin
    //     witness, two-person rule). `userId` is the seeded ADMIN cashier.
    // ====================================================================
    {
        // A MANAGER (valid witness) and a CASHIER (invalid witness) for the suite.
        const qint64 managerId = makeUser(db, userId, QStringLiteral("MANAGER"));
        const qint64 cashierWitnessId = makeUser(db, userId, QStringLiteral("CASHIER"));
        s.check(managerId > 0, QStringLiteral("ctrl: manager witness created"));
        s.check(cashierWitnessId > 0, QStringLiteral("ctrl: cashier (invalid witness) created"));

        // Build a fresh narcotic medicine + in-date batch per sub-case so FEFO
        // never crosses between attempts.
        auto narcoticInput = [&](qint64 medId) {
            SaleInput in;
            in.items = {line(medId, 1, QStringLiteral("50.00"))};
            in.paymentMode = QStringLiteral("CASH");
            in.amountTendered = QStringLiteral("50.00");
            return in;
        };
        auto makeNarcotic = [&]() {
            const qint64 medId = makeMedicineSched(db, userId, QStringLiteral("NARCOTIC"));
            if (medId > 0) {
                addBatch(db, userId, medId, 100, QStringLiteral("50.00"), farFuture);
            }
            return medId;
        };

        // 12.1) No prescriber license, no witness -> rejected (license required).
        {
            const qint64 medId = makeNarcotic();
            s.check(medId > 0, QStringLiteral("ctrl-1: narcotic med created"));
            SaleInput in = narcoticInput(medId);
            SaleResult r = SaleService(db, userId).commit(in);
            s.check(!r.ok, QStringLiteral("ctrl-1: rejected with no license/witness"));
            s.check(r.error.contains(QStringLiteral("license"), Qt::CaseInsensitive),
                    QStringLiteral("ctrl-1: error mentions license"));
            s.check(onHandOf(db, medId) == 100, QStringLiteral("ctrl-1: stock unchanged"));
        }

        // 12.2) License set, witness == 0 -> rejected (witness required).
        {
            const qint64 medId = makeNarcotic();
            SaleInput in = narcoticInput(medId);
            in.prescriberLicense = QStringLiteral("LIC-12345");
            in.controlledWitnessUserId = 0;
            SaleResult r = SaleService(db, userId).commit(in);
            s.check(!r.ok, QStringLiteral("ctrl-2: rejected with no witness"));
            s.check(r.error.contains(QStringLiteral("witness"), Qt::CaseInsensitive),
                    QStringLiteral("ctrl-2: error mentions witness"));
            s.check(onHandOf(db, medId) == 100, QStringLiteral("ctrl-2: stock unchanged"));
        }

        // 12.3) License set, witness == cashier -> rejected (must differ).
        {
            const qint64 medId = makeNarcotic();
            SaleInput in = narcoticInput(medId);
            in.prescriberLicense = QStringLiteral("LIC-12345");
            in.controlledWitnessUserId = userId; // the cashier
            SaleResult r = SaleService(db, userId).commit(in);
            s.check(!r.ok, QStringLiteral("ctrl-3: rejected when witness == cashier"));
            s.check(r.error.contains(QStringLiteral("different"), Qt::CaseInsensitive),
                    QStringLiteral("ctrl-3: error says witness must differ"));
            s.check(onHandOf(db, medId) == 100, QStringLiteral("ctrl-3: stock unchanged"));
        }

        // 12.4) License set, witness == a CASHIER -> rejected (must be mgr/admin).
        {
            const qint64 medId = makeNarcotic();
            SaleInput in = narcoticInput(medId);
            in.prescriberLicense = QStringLiteral("LIC-12345");
            in.controlledWitnessUserId = cashierWitnessId;
            SaleResult r = SaleService(db, userId).commit(in);
            s.check(!r.ok, QStringLiteral("ctrl-4: rejected when witness is a cashier"));
            s.check(r.error.contains(QStringLiteral("manager"), Qt::CaseInsensitive),
                    QStringLiteral("ctrl-4: error mentions manager/admin"));
            s.check(onHandOf(db, medId) == 100, QStringLiteral("ctrl-4: stock unchanged"));
        }

        // 12.5) License set, witness == MANAGER -> OK; row persists compliance.
        {
            const qint64 medId = makeNarcotic();
            SaleInput in = narcoticInput(medId);
            in.prescriberLicense = QStringLiteral("LIC-99999");
            in.controlledWitnessUserId = managerId;
            SaleResult r = SaleService(db, userId).commit(in);
            s.check(r.ok, QStringLiteral("ctrl-5: ok with license + manager witness"));
            s.check(r.error.isEmpty(), QStringLiteral("ctrl-5: no error"));
            s.check(onHandOf(db, medId) == 99, QStringLiteral("ctrl-5: stock decremented by 1"));
            s.check(saleField(db, r.saleId, QStringLiteral("has_controlled_drug"))
                        == QStringLiteral("1"),
                    QStringLiteral("ctrl-5: has_controlled_drug == 1"));
            s.check(saleField(db, r.saleId, QStringLiteral("prescriber_license_number"))
                        == QStringLiteral("LIC-99999"),
                    QStringLiteral("ctrl-5: prescriber_license_number persisted"));
            s.check(saleField(db, r.saleId, QStringLiteral("narcotic_witness_user_id"))
                        == QString::number(managerId),
                    QStringLiteral("ctrl-5: narcotic_witness_user_id == manager id"));
            s.check(!saleField(db, r.saleId, QStringLiteral("narcotic_witness_at")).isEmpty(),
                    QStringLiteral("ctrl-5: narcotic_witness_at not null"));
        }

        // 12.6) Regression: a NON-controlled sale still succeeds with no
        //       license/witness, and records has_controlled_drug == 0.
        {
            const qint64 medId = makeMedWithStock(db, userId, 100, QStringLiteral("10.00"));
            SaleInput in;
            in.items = {line(medId, 1, QStringLiteral("10.00"))};
            in.paymentMode = QStringLiteral("CASH");
            in.amountTendered = QStringLiteral("10.00");
            SaleResult r = SaleService(db, userId).commit(in);
            s.check(r.ok, QStringLiteral(
                              "ctrl-regression: non-controlled sale ok with no license/witness"));
            s.check(saleField(db, r.saleId, QStringLiteral("has_controlled_drug"))
                        == QStringLiteral("0"),
                    QStringLiteral("ctrl-regression: has_controlled_drug == 0"));
            s.check(saleField(db, r.saleId, QStringLiteral("narcotic_witness_user_id")).isEmpty(),
                    QStringLiteral("ctrl-regression: no witness recorded"));
        }
    }

    // ====================================================================
    // 13) DISCOUNT AUTHORIZATION — cashier needs a manager/admin override for
    //     any positive discount; managers/admins self-authorize.
    // ====================================================================
    {
        const qint64 cashierId = makeUser(db, userId, QStringLiteral("CASHIER"));
        const qint64 managerId = makeUser(db, userId, QStringLiteral("MANAGER"));
        s.check(cashierId > 0, QStringLiteral("disc-auth: cashier created"));
        s.check(managerId > 0, QStringLiteral("disc-auth: manager created"));

        // 13.1) Cashier + positive discount + no authorizer -> rejected.
        {
            const qint64 medId = makeMedWithStock(db, userId, 100, QStringLiteral("10.00"));
            SaleInput in;
            in.items = {line(medId, 5, QStringLiteral("10.00"))}; // subtotal 50.00
            in.discountTotal = QStringLiteral("5.00");
            in.paymentMode = QStringLiteral("CASH");
            in.amountTendered = QStringLiteral("45.00");
            SaleResult r = SaleService(db, cashierId).commit(in);
            s.check(!r.ok,
                    QStringLiteral("disc-auth-6: cashier discount rejected without override"));
            s.check(r.error.contains(QStringLiteral("override"), Qt::CaseInsensitive),
                    QStringLiteral("disc-auth-6: error mentions override"));
            s.check(onHandOf(db, medId) == 100, QStringLiteral("disc-auth-6: stock unchanged"));
        }

        // 13.2) Cashier + positive discount + manager authorizer -> OK; persisted.
        {
            const qint64 medId = makeMedWithStock(db, userId, 100, QStringLiteral("10.00"));
            SaleInput in;
            in.items = {line(medId, 5, QStringLiteral("10.00"))}; // subtotal 50.00
            in.discountTotal = QStringLiteral("5.00");
            in.discountAuthorizedBy = managerId;
            in.paymentMode = QStringLiteral("CASH");
            in.amountTendered = QStringLiteral("45.00");
            SaleResult r = SaleService(db, cashierId).commit(in);
            s.check(r.ok, QStringLiteral("disc-auth-7: cashier discount ok with manager override"));
            s.check(r.grandTotal == QStringLiteral("45.00"),
                    QStringLiteral("disc-auth-7: grand 45.00"));
            s.check(saleField(db, r.saleId, QStringLiteral("discount_authorized_by"))
                        == QString::number(managerId),
                    QStringLiteral("disc-auth-7: discount_authorized_by == manager id"));
        }

        // 13.3) Managerial cashier (the ADMIN userId) self-authorizes with no
        //       explicit authorizer.
        {
            const qint64 medId = makeMedWithStock(db, userId, 100, QStringLiteral("10.00"));
            SaleInput in;
            in.items = {line(medId, 5, QStringLiteral("10.00"))}; // subtotal 50.00
            in.discountTotal = QStringLiteral("5.00");
            in.paymentMode = QStringLiteral("CASH");
            in.amountTendered = QStringLiteral("45.00");
            SaleResult r = SaleService(db, userId).commit(in); // userId is ADMIN
            s.check(r.ok, QStringLiteral("disc-auth-8: managerial cashier self-authorizes"));
            s.check(r.grandTotal == QStringLiteral("45.00"),
                    QStringLiteral("disc-auth-8: grand 45.00"));
            s.check(saleField(db, r.saleId, QStringLiteral("discount_authorized_by"))
                        == QString::number(userId),
                    QStringLiteral("disc-auth-8: self-authorized to admin id"));
        }
    }

    // ====================================================================
    // 14) PURE POLICY asserts (no DB).
    // ====================================================================
    {
        s.check(ControlledSubstancePolicy::requiresWitness(QStringLiteral("NARCOTIC")) == true,
                QStringLiteral("policy: NARCOTIC requires witness"));
        s.check(ControlledSubstancePolicy::requiresWitness(QStringLiteral("SCHEDULE_G")) == false,
                QStringLiteral("policy: SCHEDULE_G does not require witness"));
        s.check(ControlledSubstancePolicy::isControlled(QStringLiteral("NONE")) == false,
                QStringLiteral("policy: NONE is not controlled"));

        s.check(
            DiscountAuthorizationPolicy::requiresManagerOverride(QStringLiteral("CASHIER"), true)
                == true,
            QStringLiteral("policy: CASHIER + positive discount needs override"));
        s.check(DiscountAuthorizationPolicy::requiresManagerOverride(QStringLiteral("ADMIN"), true)
                    == false,
                QStringLiteral("policy: ADMIN self-authorizes"));
        s.check(
            DiscountAuthorizationPolicy::requiresManagerOverride(QStringLiteral("CASHIER"), false)
                == false,
            QStringLiteral("policy: CASHIER + no discount needs no override"));
    }

    // ── Mid-transaction rollback atomicity ───────────────────────────────────
    // A 2-line cart where the SECOND line is short on stock must fail the WHOLE
    // sale: the first line's stock decrement, any sale row, and any movement must
    // all roll back (no partial sale). This pins the transactional guarantee that
    // a later refactor of commit() must preserve.
    {
        const qint64 medOk = makeMedWithStock(db, userId, 100, QStringLiteral("10.00"));
        const qint64 medShort = makeMedWithStock(db, userId, 1, QStringLiteral("10.00"));
        s.check(medOk > 0 && medShort > 0, QStringLiteral("rollback: meds created"));

        auto salesCount = [&]() {
            QSqlQuery q(db);
            return (q.exec(QStringLiteral("SELECT count(*) FROM sales")) && q.next())
                       ? q.value(0).toInt()
                       : -1;
        };
        const int okBefore = onHandOf(db, medOk);
        const int shortBefore = onHandOf(db, medShort);
        const int salesBefore = salesCount();

        SaleInput in;
        in.items = {line(medOk, 5, QStringLiteral("10.00")),
                    line(medShort, 50, QStringLiteral("10.00"))}; // needs 50, has 1
        in.paymentMode = QStringLiteral("CASH");
        in.amountTendered = QStringLiteral("1000.00");
        SaleResult r = SaleService(db, userId).commit(in);

        s.check(!r.ok, QStringLiteral("rollback: commit fails when 2nd line short"));
        s.check(r.error.contains(QStringLiteral("stock"), Qt::CaseInsensitive),
                QStringLiteral("rollback: error mentions stock"));
        s.check(r.saleId <= 0, QStringLiteral("rollback: no valid saleId returned"));
        s.check(onHandOf(db, medOk) == okBefore,
                QStringLiteral("rollback: first line's stock NOT decremented"));
        s.check(onHandOf(db, medShort) == shortBefore,
                QStringLiteral("rollback: short line's stock unchanged"));
        s.check(salesCount() == salesBefore,
                QStringLiteral("rollback: no sales row was committed"));
    }

    return s;
}

} // namespace pharmadesk_tests
