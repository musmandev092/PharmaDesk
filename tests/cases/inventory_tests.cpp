// DB-backed tests for InventoryRepository: stock-value totals, low-stock and
// expiring-soon queries, batch list. Uses before/after deltas so it is robust
// to data created by other modules.

#include "framework/TestStats.h"

#include "data/BatchRepository.h"
#include "data/InventoryRepository.h"
#include "data/MedicineRepository.h"
#include "domain/Money.h"

#include <QDate>

namespace pharmadesk_tests {
namespace {
int g_u = 0;
qint64 makeMed(QSqlDatabase db, qint64 userId, int reorder)
{
    MedicineRepository m(db);
    MedicineDraft d;
    d.sku = QStringLiteral("TINV_SKU_%1").arg(++g_u, 5, 10, QLatin1Char('0'));
    d.brandName = QStringLiteral("InvMed%1").arg(g_u);
    d.genericName = QStringLiteral("Gen");
    d.baseUnit = QStringLiteral("TABLET");
    d.purchaseUnit = QStringLiteral("BOX");
    d.unitsPerPurchase = 1;
    d.reorderLevel = reorder;
    return m.create(d, userId);
}
qint64 stock(QSqlDatabase db, qint64 userId, qint64 med, int qty, const QString &cost,
             const QString &mrp, const QDate &exp)
{
    BatchRepository b(db);
    StockInDraft s;
    s.medicineId = med;
    s.batchNumber = QStringLiteral("TINV_B_%1").arg(++g_u);
    s.expiry = exp;
    s.quantity = qty;
    s.costPerUnit = cost;
    s.mrpPerUnit = mrp;
    return b.addStock(s, userId);
}
QString diff(const QString &a, const QString &b)
{
    return (Money::fromString(a) - Money::fromString(b)).toString();
}
} // namespace

TestStats run_inventory_tests(QSqlDatabase db, qint64 userId)
{
    TestStats s;
    s.module = QStringLiteral("inventory");

    InventoryRepository inv(db);
    const QDate far = QDate::currentDate().addYears(2);

    // Stock-value deltas: add 100 units at cost 2.0000 / mrp 5.00 → +200 cost, +500 mrp.
    const InventoryTotals before = inv.totals();
    const qint64 m1 = makeMed(db, userId, 0);
    stock(db, userId, m1, 100, QStringLiteral("2.0000"), QStringLiteral("5.00"), far);
    const InventoryTotals after = inv.totals();
    s.check(diff(after.stockValueCost, before.stockValueCost) == QStringLiteral("200.00"),
            QStringLiteral("stock value at cost += qty*cost (200.00)"));
    s.check(diff(after.stockValueMrp, before.stockValueMrp) == QStringLiteral("500.00"),
            QStringLiteral("stock value at MRP += qty*mrp (500.00)"));
    s.check(after.medicinesCount == before.medicinesCount + 1,
            QStringLiteral("active medicines count +1"));
    s.check(after.activeBatches == before.activeBatches + 1, QStringLiteral("active batches +1"));

    // Low stock: reorder 100 but only 10 on hand → appears; another with 500 → not.
    const qint64 lowMed = makeMed(db, userId, 100);
    stock(db, userId, lowMed, 10, QStringLiteral("1.0000"), QStringLiteral("2.00"), far);
    const qint64 okMed = makeMed(db, userId, 100);
    stock(db, userId, okMed, 500, QStringLiteral("1.0000"), QStringLiteral("2.00"), far);
    {
        const QVector<LowStockRow> low = inv.lowStock(0);
        bool hasLow = false, hasOk = false;
        for (const LowStockRow &r : low) {
            if (r.medicineId == lowMed) hasLow = true;
            if (r.medicineId == okMed) hasOk = true;
        }
        s.check(hasLow, QStringLiteral("under-reorder medicine appears in low stock"));
        s.check(!hasOk, QStringLiteral("well-stocked medicine not in low stock"));
    }

    // A medicine with reorder 0 is never low-stock even at 0 on hand.
    const qint64 noReorder = makeMed(db, userId, 0);
    {
        const QVector<LowStockRow> low = inv.lowStock(0);
        bool found = false;
        for (const LowStockRow &r : low)
            if (r.medicineId == noReorder) found = true;
        s.check(!found, QStringLiteral("reorder-level 0 medicine never low-stock"));
    }

    // Expiring soon: batch in 20 days is in 60/90 windows but not 10.
    const qint64 expMed = makeMed(db, userId, 0);
    stock(db, userId, expMed, 30, QStringLiteral("1.0000"), QStringLiteral("2.00"),
          QDate::currentDate().addDays(20));
    {
        auto inWindow = [&](int days) {
            const QVector<ExpiringRow> e = inv.expiringSoon(days, 0);
            for (const ExpiringRow &r : e)
                if (r.brandName.startsWith(QStringLiteral("InvMed")) && r.days >= 0 && r.days <= 25
                    && r.currentQty == 30)
                    return true;
            return false;
        };
        s.check(inWindow(60), QStringLiteral("20-day batch in 60-day window"));
        s.check(inWindow(90), QStringLiteral("20-day batch in 90-day window"));
        s.check(!inWindow(10), QStringLiteral("20-day batch NOT in 10-day window"));
    }

    // batchList finds our just-added batch.
    {
        const QVector<StockBatchRow> rows = inv.batchList(QStringLiteral("InvMed"));
        s.check(!rows.isEmpty(), QStringLiteral("batchList returns rows for our medicines"));
        bool sorted = true;
        for (int i = 1; i < rows.size(); ++i) {
            if (rows[i].expiry.isValid() && rows[i - 1].expiry.isValid()
                && rows[i].expiry < rows[i - 1].expiry) {
                sorted = false;
                break;
            }
        }
        s.check(sorted, QStringLiteral("batchList sorted by expiry ascending"));
    }

    return s;
}

} // namespace pharmadesk_tests
