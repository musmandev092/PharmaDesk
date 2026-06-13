// DB-backed tests for goods-receipt posting (service/GrnService) + GrnRepository.

#include "framework/TestStats.h"

#include "data/BatchRepository.h"
#include "data/GrnRepository.h"
#include "data/MedicineRepository.h"
#include "data/SupplierRepository.h"
#include "domain/Money.h"
#include "service/GrnService.h"

#include <QDate>
#include <QSqlQuery>
#include <QVariant>

namespace pharmadesk_tests {
namespace {
int g_u = 0;
qint64 makeMed(QSqlDatabase db, qint64 userId, int upp)
{
    MedicineRepository m(db);
    MedicineDraft d;
    d.sku = QStringLiteral("TGRN_SKU_%1").arg(++g_u, 5, 10, QLatin1Char('0'));
    d.brandName = QStringLiteral("GrnMed");
    d.genericName = QStringLiteral("Gen");
    d.baseUnit = QStringLiteral("TABLET");
    d.purchaseUnit = QStringLiteral("BOX");
    d.unitsPerPurchase = upp;
    return m.create(d, userId);
}
GrnLineInput line(qint64 med, const QString &batch, const QDate &exp, int paid, int foc,
                  const QString &cost, const QString &mrp)
{
    GrnLineInput l;
    l.medicineId = med;
    l.batchNumber = batch;
    l.expiry = exp;
    l.paidQty = paid;
    l.focQty = foc;
    l.unitCost = cost;
    l.mrpPerPurchaseUnit = mrp;
    return l;
}
} // namespace

TestStats run_grn_tests(QSqlDatabase db, qint64 userId)
{
    TestStats s;
    s.module = QStringLiteral("grn");

    SupplierRepository sup(db);
    SupplierRow sr;
    sr.name = QStringLiteral("TGRN Supplier %1").arg(++g_u);
    const qint64 supId = sup.create(sr, userId);
    s.check(supId > 0, QStringLiteral("supplier created"));

    GrnService grn(db, userId);
    BatchRepository batches(db);
    const QDate far = QDate::currentDate().addYears(2);

    // 1) Simple post: 5 boxes of 10, cost 50/box, mrp 80/box.
    const qint64 m1 = makeMed(db, userId, 10);
    GrnResult r1 = grn.post(supId,
                            {line(m1, QStringLiteral("G1"), far, 5, 0, QStringLiteral("50.00"),
                                  QStringLiteral("80.00"))},
                            QStringLiteral("INV1"), QDate::currentDate());
    s.check(r1.ok, QStringLiteral("simple GRN posts"));
    s.check(r1.grnNumber.startsWith(QStringLiteral("GRN-")), QStringLiteral("GRN number format"));
    s.check(r1.subtotal == QStringLiteral("250.00"), QStringLiteral("subtotal 5*50 = 250.00"));
    s.check(batches.onHand(m1) == 50, QStringLiteral("on-hand 50 (5*10)"));
    {
        QVector<FefoBatch> cb = batches.candidateBatchesForSale(m1);
        s.check(cb.size() == 1, QStringLiteral("one batch created"));
        s.check(cb.size() == 1 && cb[0].costPerUnit == QStringLiteral("5.0000"),
                QStringLiteral("blended cost 5.0000/unit"));
        s.check(cb.size() == 1 && cb[0].mrpPerUnit == QStringLiteral("8.0000"),
                QStringLiteral("mrp 8.0000/unit"));
    }
    // grn_lines.batch_id populated (traceability).
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT batch_id FROM grn_lines WHERE grn_id=?"));
        q.addBindValue(r1.grnId);
        bool ok = q.exec() && q.next();
        s.check(ok && !q.value(0).isNull() && q.value(0).toLongLong() > 0,
                QStringLiteral("grn_lines.batch_id populated"));
    }
    // GRN_RECEIPT movement exists.
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT count(*) FROM inventory_movements "
            "WHERE ref_table='grn_documents' AND ref_id=? AND movement_type='GRN_RECEIPT'"));
        q.addBindValue(r1.grnId);
        s.check(q.exec() && q.next() && q.value(0).toInt() == 1,
                QStringLiteral("one GRN_RECEIPT movement"));
    }

    // 2) FOC dilution: paid 10 + free 2 boxes of 10, cost 100/box → blended.
    const qint64 m2 = makeMed(db, userId, 10);
    GrnResult r2 = grn.post(supId, {line(m2, QStringLiteral("G2"), far, 10, 2,
                                         QStringLiteral("100.00"), QStringLiteral("150.00"))});
    s.check(r2.ok, QStringLiteral("FOC GRN posts"));
    s.check(batches.onHand(m2) == 120, QStringLiteral("on-hand 120 ((10+2)*10)"));
    {
        QVector<FefoBatch> cb = batches.candidateBatchesForSale(m2);
        // (100*10)/((12)*10) = 1000/120 = 8.3333
        s.check(cb.size() == 1 && cb[0].costPerUnit == QStringLiteral("8.3333"),
                QStringLiteral("FOC-diluted blended cost 8.3333"));
    }
    s.check(r2.subtotal == QStringLiteral("1000.00"),
            QStringLiteral("FOC subtotal = paid*cost (1000.00)"));

    // 3) Multi-line subtotal.
    const qint64 m3 = makeMed(db, userId, 1);
    const qint64 m4 = makeMed(db, userId, 1);
    GrnResult r3 = grn.post(supId, {
                                       line(m3, QStringLiteral("G3"), far, 4, 0,
                                            QStringLiteral("25.00"), QStringLiteral("40.00")),
                                       line(m4, QStringLiteral("G4"), far, 6, 0,
                                            QStringLiteral("15.00"), QStringLiteral("30.00")),
                                   });
    s.check(r3.ok, QStringLiteral("multi-line GRN posts"));
    s.check(r3.subtotal == QStringLiteral("190.00"),
            QStringLiteral("subtotal 4*25 + 6*15 = 190.00"));

    // 4) Dup batch (same med+batch#+expiry) merges quantity.
    const qint64 m5 = makeMed(db, userId, 1);
    grn.post(supId, {line(m5, QStringLiteral("DUP"), far, 3, 0, QStringLiteral("10.00"),
                          QStringLiteral("20.00"))});
    grn.post(supId, {line(m5, QStringLiteral("DUP"), far, 2, 0, QStringLiteral("10.00"),
                          QStringLiteral("20.00"))});
    s.check(batches.onHand(m5) == 5, QStringLiteral("dup batch merges to 5"));
    s.check(batches.candidateBatchesForSale(m5).size() == 1,
            QStringLiteral("dup batch is a single row"));

    // 5) Validation rejections.
    const qint64 m6 = makeMed(db, userId, 1);
    s.check(!grn.post(supId, {}).ok, QStringLiteral("empty GRN rejected"));
    s.check(!grn.post(supId, {line(m6, QStringLiteral("X"), QDate::currentDate().addDays(-1), 1, 0,
                                   QStringLiteral("10.00"), QStringLiteral("20.00"))})
                 .ok,
            QStringLiteral("past-expiry rejected"));
    s.check(!grn.post(supId, {line(m6, QStringLiteral("X"), far, 0, 0, QStringLiteral("10.00"),
                                   QStringLiteral("20.00"))})
                 .ok,
            QStringLiteral("paid<1 rejected"));
    s.check(!grn.post(supId, {line(m6, QStringLiteral("X"), far, 1, 0, QStringLiteral("0"),
                                   QStringLiteral("20.00"))})
                 .ok,
            QStringLiteral("unit cost <= 0 rejected"));
    s.check(!grn.post(supId, {line(m6, QStringLiteral("X"), far, 1, 0, QStringLiteral("10.00"),
                                   QStringLiteral("0"))})
                 .ok,
            QStringLiteral("mrp <= 0 rejected"));
    s.check(!grn.post(supId, {line(m6, QString(), far, 1, 0, QStringLiteral("10.00"),
                                   QStringLiteral("20.00"))})
                 .ok,
            QStringLiteral("empty batch number rejected"));
    s.check(!grn.post(999999, {line(m6, QStringLiteral("X"), far, 1, 0, QStringLiteral("10.00"),
                                    QStringLiteral("20.00"))})
                 .ok,
            QStringLiteral("unknown supplier rejected"));
    s.check(batches.onHand(m6) == 0, QStringLiteral("no stock after rejected posts"));

    // 6) GRN list includes posted docs with POSTED status.
    {
        GrnRepository repo(db);
        const QVector<GrnDocRow> rows = repo.list(200);
        bool foundPosted = false;
        for (const GrnDocRow &g : rows) {
            if (g.grnNumber == r1.grnNumber) {
                foundPosted = (g.status == QLatin1String("POSTED") && g.lineCount == 1);
            }
        }
        s.check(foundPosted, QStringLiteral("GRN list shows posted doc with status + line count"));
    }

    return s;
}

} // namespace pharmadesk_tests
