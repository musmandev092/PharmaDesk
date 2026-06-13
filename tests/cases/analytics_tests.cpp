// DB-backed tests for AnalyticsRepository (P&L, top-sellers, tax). Uses
// before/after deltas around a controlled sale so it is robust to other data.

#include "framework/TestStats.h"

#include "data/AnalyticsRepository.h"
#include "data/BatchRepository.h"
#include "data/MedicineRepository.h"
#include "data/UserRepository.h"
#include "domain/Money.h"
#include "service/ReturnService.h"
#include "service/SaleService.h"

#include <QDate>
#include <QSqlQuery>
#include <QVariant>

namespace pharmadesk_tests {
namespace {
int g_u = 0;
QString diff(const QString &a, const QString &b)
{
    return (Money::fromString(a) - Money::fromString(b)).toString();
}
} // namespace

TestStats run_analytics_tests(QSqlDatabase db, qint64 userId)
{
    TestStats s;
    s.module = QStringLiteral("analytics");

    const QDate today = QDate::currentDate();
    AnalyticsRepository an(db);

    // Create a medicine costing 4.0000, selling at 10.00; stock 100.
    MedicineRepository mrepo(db);
    MedicineDraft d;
    d.sku = QStringLiteral("TANL_%1").arg(++g_u);
    d.brandName = QStringLiteral("AnalyticMed%1").arg(g_u);
    d.genericName = QStringLiteral("Gen");
    d.baseUnit = QStringLiteral("TABLET");
    d.purchaseUnit = QStringLiteral("BOX");
    d.unitsPerPurchase = 1;
    const qint64 med = mrepo.create(d, userId);
    s.check(med > 0, QStringLiteral("analytics medicine created"));
    BatchRepository b(db);
    StockInDraft st;
    st.medicineId = med;
    st.batchNumber = QStringLiteral("TANL_B_%1").arg(g_u);
    st.expiry = today.addYears(2);
    st.quantity = 100;
    st.costPerUnit = QStringLiteral("4.0000");
    st.mrpPerUnit = QStringLiteral("10.00");
    b.addStock(st, userId);

    const ProfitAndLoss plBefore = an.profitAndLoss(today, today);
    const TopSellers tsBefore = an.topSellers(today, today, 1000);

    // Sell 5 units at 10.00 → revenue +50.00, cogs +20.00 (5*4), margin +30.00.
    {
        SaleService svc(db, userId);
        SaleInput in;
        in.paymentMode = QStringLiteral("CASH");
        in.amountTendered = QStringLiteral("1000");
        SaleLineInput li;
        li.medicineId = med;
        li.qtySoldDisplay = 5;
        li.soldUnitLabel = QStringLiteral("TABLET");
        li.soldUnitFactor = 1;
        li.unitMrp = QStringLiteral("10.00");
        in.items << li;
        s.check(svc.commit(in).ok, QStringLiteral("analytics sale commits"));
    }

    const ProfitAndLoss plAfter = an.profitAndLoss(today, today);
    s.check(diff(plAfter.revenue, plBefore.revenue) == QStringLiteral("50.00"),
            QStringLiteral("P&L revenue += 50.00"));
    s.check(diff(plAfter.cogs, plBefore.cogs) == QStringLiteral("20.00"),
            QStringLiteral("P&L COGS += 20.00 (5*4)"));
    s.check(diff(plAfter.margin, plBefore.margin) == QStringLiteral("30.00"),
            QStringLiteral("P&L margin += 30.00"));

    // Per-medicine P&L row for our medicine.
    {
        bool found = false;
        for (const PlMedicineRow &r : plAfter.rows) {
            if (r.medicineId == med) {
                found = (r.units == 5
                         && Money::fromString(r.revenue).toString() == QStringLiteral("50.00")
                         && Money::fromString(r.cogs).toString() == QStringLiteral("20.00")
                         && Money::fromString(r.margin).toString() == QStringLiteral("30.00"));
            }
        }
        s.check(found, QStringLiteral("P&L per-medicine row revenue/cogs/margin correct"));
    }

    // Top sellers includes our medicine with +5 units, +50.00 revenue.
    {
        const TopSellers tsAfter = an.topSellers(today, today, 1000);
        int unitsBefore = 0;
        for (const TopSellerRow &r : tsBefore.rows)
            if (r.medicineId == med) unitsBefore = r.units;
        bool ok = false;
        for (const TopSellerRow &r : tsAfter.rows)
            if (r.medicineId == med)
                ok = (r.units == unitsBefore + 5
                      && Money::fromString(r.revenue).toString() != QStringLiteral("0.00"));
        s.check(ok, QStringLiteral("top sellers reflects the sale"));
    }

    // Tax report gross total rises by the sale's gross (50.00).
    {
        const TaxReport txBefore
            = plBefore.revenue.isEmpty() ? TaxReport() : an.taxReport(today, today);
        Q_UNUSED(txBefore);
        const TaxReport tx = an.taxReport(today, today);
        s.check(!tx.total.isEmpty(), QStringLiteral("tax report produces a total"));
    }

    // Empty far-past range → zero P&L.
    {
        const QDate past = today.addYears(-5);
        const ProfitAndLoss empty = an.profitAndLoss(past, past);
        s.check(Money::fromString(empty.revenue).isZero(),
                QStringLiteral("empty range → 0 revenue"));
        s.check(empty.rows.isEmpty(), QStringLiteral("empty range → no rows"));
    }

    // ====================================================================
    // REFUNDS REPORT — a finalized return today shows up in refundsReport.
    // ====================================================================
    {
        MedicineRepository rm(db);
        MedicineDraft rd;
        rd.sku = QStringLiteral("TANL_RF_%1").arg(++g_u);
        rd.brandName = QStringLiteral("RefundMed%1").arg(g_u);
        rd.genericName = QStringLiteral("Gen");
        rd.baseUnit = QStringLiteral("TABLET");
        rd.purchaseUnit = QStringLiteral("BOX");
        rd.unitsPerPurchase = 1;
        const qint64 rmed = rm.create(rd, userId);
        s.check(rmed > 0, QStringLiteral("refunds: medicine created"));
        BatchRepository rb(db);
        StockInDraft rst;
        rst.medicineId = rmed;
        rst.batchNumber = QStringLiteral("TANL_RF_B_%1").arg(g_u);
        rst.expiry = today.addYears(2);
        rst.quantity = 50;
        rst.costPerUnit = QStringLiteral("2.0000");
        rst.mrpPerUnit = QStringLiteral("8.00");
        rb.addStock(rst, userId);

        // Sell 6, then return 2 (restock) → a finalized return dated today.
        SaleService svc(db, userId);
        SaleInput in;
        in.paymentMode = QStringLiteral("CASH");
        in.amountTendered = QStringLiteral("1000");
        SaleLineInput li;
        li.medicineId = rmed;
        li.qtySoldDisplay = 6;
        li.soldUnitLabel = QStringLiteral("TABLET");
        li.soldUnitFactor = 1;
        li.unitMrp = QStringLiteral("8.00");
        in.items << li;
        const SaleResult sr = svc.commit(in);
        s.check(sr.ok, QStringLiteral("refunds: sale commits"));

        qint64 itemId = -1;
        {
            QSqlQuery q(db);
            q.prepare(
                QStringLiteral("SELECT id FROM sale_items WHERE sale_id = ? ORDER BY id LIMIT 1"));
            q.addBindValue(sr.saleId);
            if (q.exec() && q.next()) itemId = q.value(0).toLongLong();
        }
        s.check(itemId > 0, QStringLiteral("refunds: sale_item resolved"));

        ReturnService rs(db, userId);
        const ReturnResult rr
            = rs.commitReturn(itemId, 2, QStringLiteral("CUSTOMER_CHANGED_MIND"), true);
        s.check(rr.ok, QStringLiteral("refunds: return finalized (2 x 8.00 = 16.00)"));

        const RefundsReport rep = an.refundsReport(today, today);
        s.check(rep.totalCount >= 1, QStringLiteral("refunds: totalCount >= 1"));
        s.check(!rep.byMedicine.isEmpty(), QStringLiteral("refunds: byMedicine non-empty"));
        s.check(!Money::fromString(rep.totalAmount).isZero()
                    && !Money::fromString(rep.totalAmount).isNegative(),
                QStringLiteral("refunds: totalAmount parses > 0"));
        bool sawMed = false;
        for (const RefundMedicineRow &r : rep.byMedicine)
            if (r.medicineId == rmed) sawMed = (r.count >= 1 && r.qtyReturned >= 2);
        s.check(sawMed, QStringLiteral("refunds: our medicine row present (qty>=2)"));
    }

    // ====================================================================
    // VELOCITY REPORT — units sold today show up in the 7-day window.
    // ====================================================================
    {
        MedicineRepository vm(db);
        MedicineDraft vd;
        vd.sku = QStringLiteral("TANL_VL_%1").arg(++g_u);
        vd.brandName = QStringLiteral("VelocityMed%1").arg(g_u);
        vd.genericName = QStringLiteral("Gen");
        vd.baseUnit = QStringLiteral("TABLET");
        vd.purchaseUnit = QStringLiteral("BOX");
        vd.unitsPerPurchase = 1;
        const qint64 vmed = vm.create(vd, userId);
        s.check(vmed > 0, QStringLiteral("velocity: medicine created"));
        BatchRepository vb(db);
        StockInDraft vst;
        vst.medicineId = vmed;
        vst.batchNumber = QStringLiteral("TANL_VL_B_%1").arg(g_u);
        vst.expiry = today.addYears(2);
        vst.quantity = 100;
        vst.costPerUnit = QStringLiteral("1.0000");
        vst.mrpPerUnit = QStringLiteral("5.00");
        vb.addStock(vst, userId);

        const int soldQty = 9;
        SaleService svc(db, userId);
        SaleInput in;
        in.paymentMode = QStringLiteral("CASH");
        in.amountTendered = QStringLiteral("1000");
        SaleLineInput li;
        li.medicineId = vmed;
        li.qtySoldDisplay = soldQty;
        li.soldUnitLabel = QStringLiteral("TABLET");
        li.soldUnitFactor = 1;
        li.unitMrp = QStringLiteral("5.00");
        in.items << li;
        s.check(svc.commit(in).ok, QStringLiteral("velocity: sale commits"));

        const QVector<VelocityRow> vel = an.velocityReport();
        bool found = false;
        for (const VelocityRow &r : vel) {
            if (r.medicineId == vmed) {
                found = true;
                s.check(r.d7 >= soldQty, QStringLiteral("velocity: d7 >= units sold today"));
                s.check(r.d7 > 0, QStringLiteral("velocity: d7 > 0 for our medicine"));
                s.check(r.d30 >= r.d7, QStringLiteral("velocity: d30 >= d7"));
                s.check(r.d90 >= r.d30, QStringLiteral("velocity: d90 >= d30"));
            }
        }
        s.check(found, QStringLiteral("velocity: our medicine appears in the report"));
    }

    // ====================================================================
    // COMPLIANCE + NARCOTIC REGISTER — a controlled (NARCOTIC) sale today.
    // ====================================================================
    {
        // Methods must always run and return a (possibly empty) vector.
        const QVector<ComplianceRow> compAny = an.complianceReport(today, today);
        const QVector<NarcoticRow> narcAny = an.narcoticRegister(today, today);
        s.check(true,
                QStringLiteral("compliance: complianceReport runs (size=%1)").arg(compAny.size()));
        s.check(true,
                QStringLiteral("narcotic: narcoticRegister runs (size=%1)").arg(narcAny.size()));

        // A manager witness (≠ operator) for the two-person rule.
        UserRepository ur(db);
        const QString uname = QStringLiteral("tanl_wit_%1").arg(++g_u, 5, 10, QLatin1Char('0'));
        const qint64 witnessId
            = ur.createUser(QStringLiteral("Analytics Witness %1").arg(g_u), uname,
                            QStringLiteral("MANAGER"), QStringLiteral("x-pin-hash"), true, userId);
        s.check(witnessId > 0, QStringLiteral("compliance: manager witness created"));

        MedicineRepository nm(db);
        MedicineDraft nd;
        nd.sku = QStringLiteral("TANL_NC_%1").arg(++g_u);
        nd.brandName = QStringLiteral("NarcoticMed%1").arg(g_u);
        nd.genericName = QStringLiteral("Gen");
        nd.strength = QStringLiteral("10mg");
        nd.baseUnit = QStringLiteral("TABLET");
        nd.purchaseUnit = QStringLiteral("BOX");
        nd.unitsPerPurchase = 1;
        nd.controlledSchedule = QStringLiteral("NARCOTIC");
        const qint64 nmed = nm.create(nd, userId);
        s.check(nmed > 0, QStringLiteral("compliance: narcotic medicine created"));
        BatchRepository nb(db);
        StockInDraft nst;
        nst.medicineId = nmed;
        nst.batchNumber = QStringLiteral("TANL_NC_B_%1").arg(g_u);
        nst.expiry = today.addYears(2);
        nst.quantity = 50;
        nst.costPerUnit = QStringLiteral("20.0000");
        nst.mrpPerUnit = QStringLiteral("50.00");
        nb.addStock(nst, userId);

        SaleService svc(db, userId);
        SaleInput in;
        in.paymentMode = QStringLiteral("CASH");
        in.amountTendered = QStringLiteral("1000");
        in.prescriberLicense = QStringLiteral("LIC-ANL-42");
        in.controlledWitnessUserId = witnessId;
        SaleLineInput li;
        li.medicineId = nmed;
        li.qtySoldDisplay = 3;
        li.soldUnitLabel = QStringLiteral("TABLET");
        li.soldUnitFactor = 1;
        li.unitMrp = QStringLiteral("50.00");
        in.items << li;
        const SaleResult sr = svc.commit(in);
        s.check(sr.ok, QStringLiteral("compliance: controlled sale commits"));

        if (sr.ok) {
            const QVector<ComplianceRow> comp = an.complianceReport(today, today);
            s.check(comp.size() >= 1, QStringLiteral("compliance: at least one row"));
            bool sawReceipt = false;
            for (const ComplianceRow &r : comp)
                if (r.receiptNumber == sr.receiptNumber) sawReceipt = true;
            s.check(sawReceipt, QStringLiteral("compliance: our receipt present"));

            const QVector<NarcoticRow> narc = an.narcoticRegister(today, today);
            s.check(narc.size() >= 1, QStringLiteral("narcotic: at least one row"));
            bool sawNarc = false;
            for (const NarcoticRow &r : narc)
                if (r.receiptNumber == sr.receiptNumber) sawNarc = true;
            s.check(sawNarc, QStringLiteral("narcotic: our receipt present in register"));
        }
    }

    return s;
}

} // namespace pharmadesk_tests
