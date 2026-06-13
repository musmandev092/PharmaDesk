#include "data/AnalyticsRepository.h"

#include "domain/Money.h"

#include <QFile>
#include <QHash>
#include <QSet>
#include <QSqlQuery>
#include <QTextStream>

#include <algorithm>

static const QString kDateFmt = QStringLiteral("yyyy-MM-dd");

namespace {

// Money holds an int64 at scale 4. For the few ratio operations the reports
// need (proportional refund-COGS, margin %, revenue share) we work directly on
// those units with 128-bit intermediate products so nothing overflows and the
// result stays exact at scale 4 — the same scale the PHP page rounds from.
//
//   ratioMul(a, b, c) = a * b / c        (HALF_UP at scale 4)
//   percent(a, b)     = a / b * 100      (returns Money at scale 4)

Money ratioMul(const Money &a, const Money &b, const Money &c)
{
    const qint64 cu = c.unitsScale4();
    if (cu == 0) {
        return Money();
    }
    // a*b is scale-8; dividing by c (scale-4) yields scale-4. Round HALF_UP.
    const __int128 num
        = static_cast<__int128>(a.unitsScale4()) * static_cast<__int128>(b.unitsScale4());
    const __int128 den = static_cast<__int128>(cu);
    __int128 q = num / den;
    __int128 r = num % den;
    // HALF_UP on the absolute remainder.
    const __int128 twice = (r < 0 ? -r : r) * 2;
    const __int128 adb = (den < 0 ? -den : den);
    if (twice >= adb) {
        q += (num < 0) == (den < 0) ? 1 : -1;
    }
    return Money::fromUnits(static_cast<qint64>(q));
}

// margin / revenue × 100, rounded HALF_UP to 1 dp, as a display string.
QString marginPctStr(const Money &margin, const Money &revenue)
{
    if (revenue.compare(Money()) <= 0) {
        return QStringLiteral("0");
    }
    const Money pct = ratioMul(margin, Money::fromUnits(1000000 /* 100 at scale4 */), revenue);
    return pct.toString(1);
}

QString taxCodeLabel(const QString &c)
{
    if (c == QLatin1String("EXEMPT")) return QStringLiteral("Exempt (no tax)");
    if (c == QLatin1String("STANDARD_18")) return QStringLiteral("Standard GST 18%");
    if (c == QLatin1String("REDUCED")) return QStringLiteral("Reduced rate");
    if (c == QLatin1String("ZERO_RATED")) return QStringLiteral("Zero-rated");
    return c;
}

// Configured inclusive rates, scale 4 (Pakistan defaults — keep in sync with
// reports/tax.php $RATES).
Money taxRate(const QString &code)
{
    if (code == QLatin1String("STANDARD_18")) return Money::fromString(QStringLiteral("0.18"));
    if (code == QLatin1String("REDUCED")) return Money::fromString(QStringLiteral("0.05"));
    return Money(); // EXEMPT / ZERO_RATED / unknown → 0
}

QString csvField(QString v)
{
    if (v.contains(QLatin1Char(',')) || v.contains(QLatin1Char('"'))
        || v.contains(QLatin1Char('\n'))) {
        v.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QStringLiteral("\"%1\"").arg(v);
    }
    return v;
}

} // namespace

ProfitAndLoss AnalyticsRepository::profitAndLoss(const QDate &from, const QDate &to) const
{
    ProfitAndLoss out;
    const QString f = from.toString(kDateFmt);
    const QString t = to.toString(kDateFmt);

    // Per-medicine sale aggregates. Revenue = Σ line_total; COGS = Σ unit_cost ×
    // qty_in_base_units — both summed with Money, not SQLite. We pull the raw
    // rows and aggregate in C++ so the money math stays exact.
    struct Agg
    {
        QString brandName;
        int units = 0;
        Money revenue;
        Money cogs;
        Money refunds;
        int refundUnits = 0;
    };
    QHash<qint64, Agg> agg;

    QSqlQuery sq(m_db);
    sq.prepare(QStringLiteral(
        "SELECT si.medicine_id, m.brand_name, si.qty_in_base_units, si.line_total, si.unit_cost "
        "  FROM sale_items si "
        "  JOIN sales s     ON s.id = si.sale_id "
        "  JOIN medicines m ON m.id = si.medicine_id "
        " WHERE date(s.sold_at, 'localtime') BETWEEN ? AND ? "
        "   AND s.status <> 'VOIDED'"));
    sq.addBindValue(f);
    sq.addBindValue(t);
    if (sq.exec()) {
        while (sq.next()) {
            const qint64 mid = sq.value(0).toLongLong();
            Agg &a = agg[mid];
            if (a.brandName.isEmpty()) a.brandName = sq.value(1).toString();
            const int qty = sq.value(2).toInt();
            a.units += qty;
            a.revenue = a.revenue + Money::fromString(sq.value(3).toString());
            a.cogs = a.cogs + Money::fromString(sq.value(4).toString()).mul(qty);
        }
    }

    // Refunds per medicine, in range, by created_at (localtime).
    QSqlQuery rq(m_db);
    rq.prepare(QStringLiteral("SELECT r.medicine_id, r.qty_returned_units, r.refund_amount "
                              "  FROM returns r "
                              " WHERE date(r.created_at, 'localtime') BETWEEN ? AND ?"));
    rq.addBindValue(f);
    rq.addBindValue(t);
    if (rq.exec()) {
        while (rq.next()) {
            const qint64 mid = rq.value(0).toLongLong();
            if (!agg.contains(mid)) {
                continue; // no matching sales in range; nothing to net against
            }
            Agg &a = agg[mid];
            a.refundUnits += rq.value(1).toInt();
            a.refunds = a.refunds + Money::fromString(rq.value(2).toString());
        }
    }

    Money totRevenue, totCogs, totMargin, totRefunds;
    for (auto it = agg.constBegin(); it != agg.constEnd(); ++it) {
        const Agg &a = it.value();
        // Net revenue, and refund-proportional COGS reduction:
        //   cogsNet = cogs − refunds × (cogs / revenue)
        const Money revenueNet = a.revenue - a.refunds;
        Money cogsNet = a.cogs;
        if (a.revenue.compare(Money()) > 0 && a.refunds.compare(Money()) > 0) {
            cogsNet = a.cogs - ratioMul(a.refunds, a.cogs, a.revenue);
        }
        const Money margin = revenueNet - cogsNet;

        PlMedicineRow r;
        r.medicineId = it.key();
        r.brandName = a.brandName;
        r.units = a.units - a.refundUnits;
        r.revenue = revenueNet.toString();
        r.refunds = a.refunds.toString();
        r.cogs = cogsNet.toString();
        r.margin = margin.toString();
        r.marginPct = marginPctStr(margin, revenueNet);
        out.rows.push_back(r);

        totRevenue = totRevenue + revenueNet;
        totCogs = totCogs + cogsNet;
        totMargin = totMargin + margin;
        totRefunds = totRefunds + a.refunds;
    }

    std::sort(out.rows.begin(), out.rows.end(), [](const PlMedicineRow &x, const PlMedicineRow &y) {
        return Money::fromString(x.margin).compare(Money::fromString(y.margin)) > 0;
    });
    if (out.rows.size() > 500) out.rows.resize(500);

    out.revenue = totRevenue.toString();
    out.cogs = totCogs.toString();
    out.margin = totMargin.toString();
    out.refunds = totRefunds.toString();
    out.marginPct = marginPctStr(totMargin, totRevenue);
    return out;
}

TaxReport AnalyticsRepository::taxReport(const QDate &from, const QDate &to, bool inclusive) const
{
    TaxReport out;
    const QString f = from.toString(kDateFmt);
    const QString t = to.toString(kDateFmt);

    struct Bucket
    {
        Money gross;
        int lines = 0;
    };
    QHash<QString, Bucket> buckets;
    QHash<QString, QSet<qint64>> salesByBucket;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT m.tax_code_value, s.id, si.line_total "
                             "  FROM sale_items si "
                             "  JOIN sales s     ON s.id = si.sale_id "
                             "  JOIN medicines m ON m.id = si.medicine_id "
                             " WHERE date(s.sold_at, 'localtime') BETWEEN ? AND ? "
                             "   AND s.status <> 'VOIDED'"));
    q.addBindValue(f);
    q.addBindValue(t);
    if (q.exec()) {
        while (q.next()) {
            const QString code = q.value(0).toString();
            Bucket &b = buckets[code];
            b.gross = b.gross + Money::fromString(q.value(2).toString());
            b.lines += 1;
            salesByBucket[code].insert(q.value(1).toLongLong());
        }
    }

    Money grandBase, grandTax, grandTotal;
    const Money one = Money::fromString(QStringLiteral("1"));
    for (auto it = buckets.constBegin(); it != buckets.constEnd(); ++it) {
        const QString &code = it.key();
        const Money &gross = it.value().gross;
        const Money rate = taxRate(code);

        Money base, tax, total;
        if (inclusive && rate.compare(Money()) > 0) {
            // base = gross / (1 + rate); tax = gross − base.
            base = ratioMul(gross, one, one + rate);
            tax = gross - base;
            total = gross;
        } else {
            // exclusive: base = gross; tax = base × rate; total = base + tax.
            base = gross;
            tax = ratioMul(base, rate, one);
            total = base + tax;
        }

        TaxBucketRow r;
        r.taxCode = code;
        r.label = taxCodeLabel(code);
        r.ratePct = ratioMul(rate, Money::fromUnits(1000000 /* 100 */), one).toString(1);
        r.salesCount = salesByBucket.value(code).size();
        r.linesCount = it.value().lines;
        r.base = base.toString();
        r.tax = tax.toString();
        r.total = total.toString();
        out.rows.push_back(r);

        grandBase = grandBase + base;
        grandTax = grandTax + tax;
        grandTotal = grandTotal + total;
    }

    std::sort(out.rows.begin(), out.rows.end(), [](const TaxBucketRow &x, const TaxBucketRow &y) {
        return Money::fromString(x.total).compare(Money::fromString(y.total)) > 0;
    });

    out.base = grandBase.toString();
    out.tax = grandTax.toString();
    out.total = grandTotal.toString();
    return out;
}

TopSellers AnalyticsRepository::topSellers(const QDate &from, const QDate &to, int limit) const
{
    TopSellers out;
    const QString f = from.toString(kDateFmt);
    const QString t = to.toString(kDateFmt);

    struct Agg
    {
        QString brand, generic, strength, baseUnit;
        int units = 0;
        Money revenue;
        Money cogs;
        Money refunds;
        int refundUnits = 0;
    };
    QHash<qint64, Agg> agg;

    QSqlQuery sq(m_db);
    sq.prepare(QStringLiteral(
        "SELECT si.medicine_id, m.brand_name, m.generic_name, COALESCE(m.strength,''), "
        "       m.base_unit, si.qty_in_base_units, si.line_total, si.unit_cost "
        "  FROM sale_items si "
        "  JOIN sales s     ON s.id = si.sale_id "
        "  JOIN medicines m ON m.id = si.medicine_id "
        " WHERE date(s.sold_at, 'localtime') BETWEEN ? AND ? "
        "   AND s.status <> 'VOIDED'"));
    sq.addBindValue(f);
    sq.addBindValue(t);
    if (sq.exec()) {
        while (sq.next()) {
            const qint64 mid = sq.value(0).toLongLong();
            Agg &a = agg[mid];
            if (a.brand.isEmpty()) {
                a.brand = sq.value(1).toString();
                a.generic = sq.value(2).toString();
                a.strength = sq.value(3).toString();
                a.baseUnit = sq.value(4).toString();
            }
            const int qty = sq.value(5).toInt();
            a.units += qty;
            a.revenue = a.revenue + Money::fromString(sq.value(6).toString());
            a.cogs = a.cogs + Money::fromString(sq.value(7).toString()).mul(qty);
        }
    }

    QSqlQuery rq(m_db);
    rq.prepare(QStringLiteral("SELECT r.medicine_id, r.qty_returned_units, r.refund_amount "
                              "  FROM returns r "
                              " WHERE date(r.created_at, 'localtime') BETWEEN ? AND ?"));
    rq.addBindValue(f);
    rq.addBindValue(t);
    if (rq.exec()) {
        while (rq.next()) {
            const qint64 mid = rq.value(0).toLongLong();
            if (!agg.contains(mid)) continue;
            Agg &a = agg[mid];
            a.refundUnits += rq.value(1).toInt();
            a.refunds = a.refunds + Money::fromString(rq.value(2).toString());
        }
    }

    Money totRevenue, totMargin;
    int totUnits = 0;
    QVector<TopSellerRow> rows;
    for (auto it = agg.constBegin(); it != agg.constEnd(); ++it) {
        const Agg &a = it.value();
        const Money revenueNet = a.revenue - a.refunds;
        Money cogsNet = a.cogs;
        if (a.revenue.compare(Money()) > 0 && a.refunds.compare(Money()) > 0) {
            cogsNet = a.cogs - ratioMul(a.refunds, a.cogs, a.revenue);
        }
        const Money margin = revenueNet - cogsNet;

        TopSellerRow r;
        r.medicineId = it.key();
        r.brandName = a.brand;
        r.genericName = a.generic;
        r.strength = a.strength;
        r.baseUnit = a.baseUnit;
        r.units = a.units - a.refundUnits;
        r.revenue = revenueNet.toString();
        r.cogs = cogsNet.toString();
        r.margin = margin.toString();
        r.marginPct = marginPctStr(margin, revenueNet);
        rows.push_back(r);

        totRevenue = totRevenue + revenueNet;
        totMargin = totMargin + margin;
        totUnits += r.units;
    }

    std::sort(rows.begin(), rows.end(), [](const TopSellerRow &x, const TopSellerRow &y) {
        return Money::fromString(x.revenue).compare(Money::fromString(y.revenue)) > 0;
    });
    if (limit > 0 && rows.size() > limit) rows.resize(limit);

    // Share of total revenue (over the full set, like the PHP page's totals).
    for (TopSellerRow &r : rows) {
        r.sharePct = marginPctStr(Money::fromString(r.revenue), totRevenue); // revenue/total×100
    }

    out.rows = rows;
    out.totalRevenue = totRevenue.toString();
    out.totalMargin = totMargin.toString();
    out.totalUnits = totUnits;
    return out;
}

RefundsReport AnalyticsRepository::refundsReport(const QDate &from, const QDate &to) const
{
    RefundsReport out;
    const QString f = from.toString(kDateFmt);
    const QString t = to.toString(kDateFmt);

    // Per-medicine breakdown over returns whose disposition counts as a
    // processed refund. amount summed with Money, never SQLite float.
    struct Agg
    {
        QString brand;
        int count = 0;
        int qty = 0;
        Money amount;
    };
    QHash<qint64, Agg> agg;

    QSqlQuery mq(m_db);
    mq.prepare(
        QStringLiteral("SELECT r.medicine_id, COALESCE(m.brand_name,''), "
                       "       r.qty_returned_units, r.refund_amount "
                       "  FROM returns r "
                       "  JOIN medicines m ON m.id = r.medicine_id "
                       " WHERE date(r.created_at, 'localtime') BETWEEN ? AND ? "
                       "   AND r.status IN ('APPROVED_RESTOCK','RETURN_TO_SUPPLIER','WRITE_OFF')"));
    mq.addBindValue(f);
    mq.addBindValue(t);
    if (mq.exec()) {
        while (mq.next()) {
            const qint64 mid = mq.value(0).toLongLong();
            Agg &a = agg[mid];
            if (a.brand.isEmpty()) a.brand = mq.value(1).toString();
            a.count += 1;
            a.qty += mq.value(2).toInt();
            a.amount = a.amount + Money::fromString(mq.value(3).toString());
        }
    }

    for (auto it = agg.constBegin(); it != agg.constEnd(); ++it) {
        const Agg &a = it.value();
        RefundMedicineRow r;
        r.medicineId = it.key();
        r.brandName = a.brand;
        r.count = a.count;
        r.qtyReturned = a.qty;
        r.amount = a.amount.toString();
        out.byMedicine.push_back(r);
    }
    std::sort(out.byMedicine.begin(), out.byMedicine.end(),
              [](const RefundMedicineRow &x, const RefundMedicineRow &y) {
                  return Money::fromString(x.amount).compare(Money::fromString(y.amount)) > 0;
              });

    // Headline: every return row in range (any disposition) for the count and
    // total; refund rate = totalAmount / gross sales × 100. Match the PHP page
    // which counts all returns, not just processed ones.
    Money totalAmount;
    int totalCount = 0;
    QSqlQuery hq(m_db);
    hq.prepare(QStringLiteral("SELECT refund_amount FROM returns "
                              " WHERE date(created_at, 'localtime') BETWEEN ? AND ?"));
    hq.addBindValue(f);
    hq.addBindValue(t);
    if (hq.exec()) {
        while (hq.next()) {
            totalCount += 1;
            totalAmount = totalAmount + Money::fromString(hq.value(0).toString());
        }
    }
    out.totalCount = totalCount;
    out.totalAmount = totalAmount.toString();

    Money salesTotal;
    QSqlQuery sq(m_db);
    sq.prepare(QStringLiteral("SELECT grand_total FROM sales "
                              " WHERE date(sold_at, 'localtime') BETWEEN ? AND ? "
                              "   AND status <> 'VOIDED'"));
    sq.addBindValue(f);
    sq.addBindValue(t);
    if (sq.exec()) {
        while (sq.next()) {
            salesTotal = salesTotal + Money::fromString(sq.value(0).toString());
        }
    }

    if (salesTotal.compare(Money()) > 0) {
        const Money pct = ratioMul(totalAmount, Money::fromUnits(1000000 /* 100 */), salesTotal);
        out.refundRatePct = pct.toString(1);
    } else {
        out.refundRatePct = QStringLiteral("0");
    }
    return out;
}

QVector<ComplianceRow> AnalyticsRepository::complianceReport(const QDate &from,
                                                             const QDate &to) const
{
    QVector<ComplianceRow> out;
    const QString f = from.toString(kDateFmt);
    const QString t = to.toString(kDateFmt);

    // One row per controlled sale. items = the controlled lines, "Brand
    // [SCHEDULE]" joined with ", ", built with GROUP_CONCAT over a join limited
    // to controlled medicines. A sale qualifies if has_controlled_drug or it
    // has at least one controlled line.
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT s.receipt_number, s.sold_at, "
        "       COALESCE(GROUP_CONCAT(m.brand_name || ' [' || m.controlled_schedule || ']', ', '), "
        "''), "
        "       COALESCE(s.doctor_name,''), COALESCE(s.prescriber_license_number,''), "
        "       COALESCE(s.patient_name,''), COALESCE(s.patient_phone,''), "
        "       COALESCE(uc.full_name,''), COALESCE(uw.full_name,'') "
        "  FROM sales s "
        "  JOIN users uc      ON uc.id = s.cashier_id "
        "  LEFT JOIN users uw ON uw.id = s.narcotic_witness_user_id "
        "  JOIN sale_items si ON si.sale_id = s.id "
        "  JOIN medicines m   ON m.id = si.medicine_id AND m.controlled_schedule <> 'NONE' "
        " WHERE date(s.sold_at, 'localtime') BETWEEN ? AND ? "
        "   AND s.status <> 'VOIDED' "
        "   AND (s.has_controlled_drug = 1 OR EXISTS ( "
        "          SELECT 1 FROM sale_items si2 "
        "            JOIN medicines m2 ON m2.id = si2.medicine_id "
        "           WHERE si2.sale_id = s.id AND m2.controlled_schedule <> 'NONE')) "
        " GROUP BY s.id "
        " ORDER BY s.sold_at DESC"));
    q.addBindValue(f);
    q.addBindValue(t);
    if (!q.exec()) {
        return out;
    }
    while (q.next()) {
        ComplianceRow r;
        r.receiptNumber = q.value(0).toString();
        r.soldAt = q.value(1).toString();
        r.items = q.value(2).toString();
        r.doctorName = q.value(3).toString();
        r.prescriberLicense = q.value(4).toString();
        r.patientName = q.value(5).toString();
        r.patientPhone = q.value(6).toString();
        r.cashierName = q.value(7).toString();
        r.witnessName = q.value(8).toString();
        out.push_back(r);
    }
    return out;
}

QVector<VelocityRow> AnalyticsRepository::velocityReport() const
{
    QVector<VelocityRow> out;

    // Units sold per active medicine over 7/30/90-day windows, computed in a
    // single conditional-SUM query. Windows are anchored on localtime now.
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT m.id, m.brand_name, m.generic_name, "
        "  COALESCE(SUM(CASE WHEN date(s.sold_at,'localtime') >= date('now','localtime','-7 days') "
        " THEN si.qty_in_base_units ELSE 0 END), 0) AS d7, "
        "  COALESCE(SUM(CASE WHEN date(s.sold_at,'localtime') >= date('now','localtime','-30 "
        "days') THEN si.qty_in_base_units ELSE 0 END), 0) AS d30, "
        "  COALESCE(SUM(CASE WHEN date(s.sold_at,'localtime') >= date('now','localtime','-90 "
        "days') THEN si.qty_in_base_units ELSE 0 END), 0) AS d90 "
        "  FROM medicines m "
        "  LEFT JOIN sale_items si ON si.medicine_id = m.id "
        "  LEFT JOIN sales s       ON s.id = si.sale_id AND s.status <> 'VOIDED' "
        " WHERE m.is_active = 1 AND m.deleted_at IS NULL "
        " GROUP BY m.id "
        "HAVING SUM(CASE WHEN date(s.sold_at,'localtime') >= date('now','localtime','-90 days') "
        "THEN si.qty_in_base_units ELSE 0 END) > 0 "
        " ORDER BY d30 DESC"));
    if (!q.exec()) {
        return out;
    }
    while (q.next()) {
        VelocityRow r;
        r.medicineId = q.value(0).toLongLong();
        r.brandName = q.value(1).toString();
        r.genericName = q.value(2).toString();
        r.d7 = q.value(3).toInt();
        r.d30 = q.value(4).toInt();
        r.d90 = q.value(5).toInt();
        out.push_back(r);
    }
    return out;
}

QVector<NarcoticRow> AnalyticsRepository::narcoticRegister(const QDate &from, const QDate &to) const
{
    QVector<NarcoticRow> out;
    const QString f = from.toString(kDateFmt);
    const QString t = to.toString(kDateFmt);

    // One row per sale with a NARCOTIC line in range. items = the narcotic
    // lines only, "Brand × qty unit" joined with ", ".
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT s.receipt_number, s.sold_at, "
        "       COALESCE(GROUP_CONCAT(m.brand_name || ' × ' || si.qty_in_base_units || ' ' || "
        "m.base_unit, ', '), ''), "
        "       COALESCE(s.doctor_name,''), COALESCE(s.prescriber_license_number,''), "
        "       COALESCE(s.patient_name,''), COALESCE(s.patient_phone,''), "
        "       COALESCE(s.patient_address,''), "
        "       COALESCE(uc.full_name,''), COALESCE(uw.full_name,''), "
        "       COALESCE(s.narcotic_witness_at,'') "
        "  FROM sales s "
        "  JOIN users uc      ON uc.id = s.cashier_id "
        "  LEFT JOIN users uw ON uw.id = s.narcotic_witness_user_id "
        "  JOIN sale_items si ON si.sale_id = s.id "
        "  JOIN medicines m   ON m.id = si.medicine_id AND m.controlled_schedule = 'NARCOTIC' "
        " WHERE date(s.sold_at, 'localtime') BETWEEN ? AND ? "
        "   AND s.status <> 'VOIDED' "
        " GROUP BY s.id "
        " ORDER BY s.sold_at DESC"));
    q.addBindValue(f);
    q.addBindValue(t);
    if (!q.exec()) {
        return out;
    }
    while (q.next()) {
        NarcoticRow r;
        r.receiptNumber = q.value(0).toString();
        r.soldAt = q.value(1).toString();
        r.items = q.value(2).toString();
        r.doctorName = q.value(3).toString();
        r.prescriberLicense = q.value(4).toString();
        r.patientName = q.value(5).toString();
        r.patientPhone = q.value(6).toString();
        r.patientAddress = q.value(7).toString();
        r.cashierName = q.value(8).toString();
        r.witnessName = q.value(9).toString();
        r.witnessAt = q.value(10).toString();
        out.push_back(r);
    }
    return out;
}

bool AnalyticsRepository::exportPlCsv(const ProfitAndLoss &pl, const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out << "Medicine,Units (net),Revenue (PKR),Refunds (PKR),COGS (PKR),Gross margin (PKR),Margin "
           "%\n";
    for (const PlMedicineRow &r : pl.rows) {
        out << csvField(r.brandName) << ',' << r.units << ',' << csvField(r.revenue) << ','
            << csvField(r.refunds) << ',' << csvField(r.cogs) << ',' << csvField(r.margin) << ','
            << csvField(r.marginPct) << '\n';
    }
    return true;
}

bool AnalyticsRepository::exportTopSellersCsv(const TopSellers &ts, const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out << "Rank,Medicine,Generic,Units (net),Revenue (PKR),COGS (PKR),Margin (PKR),Margin %\n";
    for (int i = 0; i < ts.rows.size(); ++i) {
        const TopSellerRow &r = ts.rows.at(i);
        out << (i + 1) << ',' << csvField(r.brandName) << ',' << csvField(r.genericName) << ','
            << r.units << ',' << csvField(r.revenue) << ',' << csvField(r.cogs) << ','
            << csvField(r.margin) << ',' << csvField(r.marginPct) << '\n';
    }
    return true;
}
