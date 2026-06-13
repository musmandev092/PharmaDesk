#pragma once

#include <QDate>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

struct SalesReportRow
{
    qint64 saleId = 0;
    QString receiptNumber;
    QString soldAt;
    QString cashierName;
    QString subtotal;
    QString discountTotal;
    QString grandTotal;
    QString paymentMode;
    QString status;
};

// Net-of-refunds totals, mirroring reports/sales.php. All Money-exact.
struct SalesSummary
{
    int count = 0;
    QString gross = QStringLiteral("0.00");
    QString cash = QStringLiteral("0.00");
    QString card = QStringLiteral("0.00");
    QString refunds = QStringLiteral("0.00");
    int refundCount = 0;
    QString net = QStringLiteral("0.00");
    QString netCash = QStringLiteral("0.00");
    QString netCard = QStringLiteral("0.00");
};

struct SalesReport
{
    QVector<SalesReportRow> rows;
    SalesSummary summary;
};

class SalesReportRepository
{
public:
    explicit SalesReportRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    // Inclusive date range on sold_at. VOIDED sales excluded from totals (but
    // listed). Refunds summed from the returns table by original sale's mode.
    SalesReport report(const QDate &from, const QDate &to) const;

    // Write the rows to a CSV file. Returns false on I/O error.
    static bool exportCsv(const SalesReport &report, const QString &path);

private:
    QSqlDatabase m_db;
};
