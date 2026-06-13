#pragma once

#include <QDate>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

// Analytics read models for the manager reports — P&L, tax summary, and
// top-sellers. Mirrors pos/frontend/pages/reports/{pl,tax,top-sellers}.php.
//
// All monetary fields are decimal strings computed with the Money fixed-point
// type (never SQLite float). money columns in the DB are decimal-string TEXT;
// cost / unit_cost are scale-4. Date filters use date(col,'localtime') because
// timestamps are stored in UTC. VOIDED sales are excluded from every total.

// ── Profit & Loss ───────────────────────────────────────────────────────────
struct PlMedicineRow
{
    qint64 medicineId = 0;
    QString brandName;
    int units = 0;     // net units (sold − refunded)
    QString revenue;   // net of refunds, scale 2
    QString refunds;   // refund amount in range, scale 2
    QString cogs;      // net of refund-proportional COGS, scale 2
    QString margin;    // revenue − cogs, scale 2
    QString marginPct; // margin / revenue × 100, scale 1 ("0" if no revenue)
};

struct ProfitAndLoss
{
    QVector<PlMedicineRow> rows; // per-medicine breakdown, margin-desc
    QString revenue = QStringLiteral("0.00");
    QString cogs = QStringLiteral("0.00");
    QString margin = QStringLiteral("0.00");
    QString refunds = QStringLiteral("0.00");
    QString marginPct = QStringLiteral("0"); // overall, scale 1
};

// ── Tax summary ───────────────────────────────────────────────────────────
// Tax is 0 in this app's data; we group gross line totals by the medicine's
// tax_code_value bucket and apply the configured (inclusive) rate, mirroring
// reports/tax.php so the figures reconcile with the web app.
struct TaxBucketRow
{
    QString taxCode;    // EXEMPT / STANDARD_18 / REDUCED / ZERO_RATED
    QString label;      // human label
    QString ratePct;    // e.g. "18.0", scale 1
    int salesCount = 0; // distinct sales touching this bucket
    int linesCount = 0;
    QString base;  // taxable base, scale 2
    QString tax;   // tax portion, scale 2
    QString total; // gross line total, scale 2
};

struct TaxReport
{
    QVector<TaxBucketRow> rows; // gross-desc
    QString base = QStringLiteral("0.00");
    QString tax = QStringLiteral("0.00");
    QString total = QStringLiteral("0.00");
};

// ── Top sellers ───────────────────────────────────────────────────────────
struct TopSellerRow
{
    qint64 medicineId = 0;
    QString brandName;
    QString genericName;
    QString strength;
    QString baseUnit;
    int units = 0;     // net units
    QString revenue;   // net of refunds, scale 2
    QString cogs;      // net, scale 2
    QString margin;    // revenue − cogs, scale 2
    QString marginPct; // scale 1
    QString sharePct;  // share of total revenue, scale 1
};

struct TopSellers
{
    QVector<TopSellerRow> rows; // ranked by revenue desc
    QString totalRevenue = QStringLiteral("0.00");
    int totalUnits = 0;
    QString totalMargin = QStringLiteral("0.00");
};

// ── Refunds ─────────────────────────────────────────────────────────────────
// Mirrors reports/refunds.php (the per-medicine view + headline KPIs).
struct RefundMedicineRow
{
    qint64 medicineId = 0;
    QString brandName;
    int count = 0;       // number of return rows
    int qtyReturned = 0; // base units returned
    QString amount;      // refunded amount, scale 2
};

struct RefundsReport
{
    QVector<RefundMedicineRow> byMedicine; // amount desc
    int totalCount = 0;
    QString totalAmount = QStringLiteral("0.00");
    QString refundRatePct = QStringLiteral("0"); // refunds / sales grand_total × 100, scale 1
};

// ── Controlled-drug compliance ──────────────────────────────────────────────
// Mirrors reports/compliance.php — one row per controlled sale.
struct ComplianceRow
{
    QString receiptNumber;
    QString soldAt;
    QString items; // "Brand [SCHEDULE]" list, comma-joined
    QString doctorName;
    QString prescriberLicense;
    QString patientName;
    QString patientPhone;
    QString cashierName;
    QString witnessName;
};

// ── Velocity ────────────────────────────────────────────────────────────────
// Mirrors reports/velocity.php — units sold over 7/30/90-day windows.
struct VelocityRow
{
    qint64 medicineId = 0;
    QString brandName;
    QString genericName;
    int d7 = 0;
    int d30 = 0;
    int d90 = 0;
};

// ── Narcotic register ───────────────────────────────────────────────────────
// Mirrors reports/narcotic-register.php — DRAP-style ledger of NARCOTIC sales.
struct NarcoticRow
{
    QString receiptNumber;
    QString soldAt;
    QString items; // "Brand × qty unit" list
    QString doctorName;
    QString prescriberLicense;
    QString patientName;
    QString patientPhone;
    QString patientAddress;
    QString cashierName;
    QString witnessName;
    QString witnessAt;
};

class AnalyticsRepository
{
public:
    explicit AnalyticsRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    // Inclusive date range on sold_at / created_at (localtime).
    ProfitAndLoss profitAndLoss(const QDate &from, const QDate &to) const;

    // inclusive: true = price already includes tax (base = gross/(1+rate)),
    // false = tax-exclusive (tax = gross×rate, total = gross+tax). Default true,
    // matching the PHP page's Pakistan-retail assumption.
    TaxReport taxReport(const QDate &from, const QDate &to, bool inclusive = true) const;

    TopSellers topSellers(const QDate &from, const QDate &to, int limit = 100) const;

    // Per-medicine refund breakdown + headline refund rate over the range.
    RefundsReport refundsReport(const QDate &from, const QDate &to) const;

    // One row per controlled sale (has_controlled_drug or schedule ≠ NONE).
    QVector<ComplianceRow> complianceReport(const QDate &from, const QDate &to) const;

    // Units sold per medicine over 7/30/90-day windows (sorted by d30 desc).
    QVector<VelocityRow> velocityReport() const;

    // DRAP-style ledger of NARCOTIC dispenses over the range.
    QVector<NarcoticRow> narcoticRegister(const QDate &from, const QDate &to) const;

    // CSV export helpers (same quoting rule as SalesReportRepository).
    static bool exportPlCsv(const ProfitAndLoss &pl, const QString &path);
    static bool exportTopSellersCsv(const TopSellers &ts, const QString &path);

private:
    QSqlDatabase m_db;
};
