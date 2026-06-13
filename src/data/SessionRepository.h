#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>

// One row of the cashier-sessions list / day-close screen.
struct SessionRow
{
    qint64 id = 0;
    QString cashierName;
    QString openedAt;
    QString closedAt;
    QString openingFloat;   // decimal string (scale 2)
    QString totalCashSales; // decimal string (scale 2)
    int totalSalesCount = 0;
    QString countedCash;  // decimal string (scale 2) — empty if still OPEN
    QString cashVariance; // decimal string (scale 2) — empty if still OPEN
    QString status;       // OPEN / CLOSED / RECONCILED
};

// Computed summary for the open shift / Z-report header.
struct SessionSummary
{
    bool valid = false;
    qint64 id = 0;
    QString cashierName;
    QString openedAt;
    QString closedAt;
    QString openingFloat;     // scale 2
    QString totalCashSales;   // scale 2
    QString totalRefundsPaid; // scale 2
    int totalSalesCount = 0;
    QString expectedCash; // opening_float + total_cash_sales - total_refunds_paid (scale 2)
    QString countedCash;  // scale 2 (empty until closed)
    QString cashVariance; // scale 2 (empty until closed)
    QString status;
};

// Data access for the cashier_sessions lifecycle. Port of
// pos/backend/services/Sessions.php. Money columns are TEXT decimal strings;
// arithmetic goes through the Money type. Every write runs in a transaction and
// is audited.
class SessionRepository
{
public:
    explicit SessionRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    // The OPEN session id for this cashier, or -1 if none.
    qint64 openSessionId(qint64 cashierId) const;

    // Open a new shift for the cashier (only if none is OPEN). One transaction +
    // SESSION_OPENED audit. Returns the new session id, or -1 on failure
    // (errorString() then holds the reason).
    qint64 openSession(qint64 cashierId, const QString &openingFloat);

    // Close the OPEN shift: stamp closed_at, store counted_cash, compute
    // expected/variance, set status = CLOSED. One transaction + SESSION_CLOSED
    // audit. userId must own the session (the cashier who opened it). Returns
    // false on failure (errorString() holds the reason).
    bool closeSession(qint64 sessionId, const QString &countedCash, qint64 userId);

    // Per-sale bump (called from SaleService inside the sale transaction):
    // increments total_sales_count, and — when CASH — adds grandTotal to
    // total_cash_sales using Money. No-op if sessionId <= 0. NOT audited (the
    // owning sale's audit row covers it). Returns false on SQL error.
    bool bumpForSale(qint64 sessionId, const QString &grandTotal, const QString &paymentMode);

    // Recent sessions across all cashiers (admin view), newest first.
    QVector<SessionRow> list(int limit = 50) const;

    // Computed summary for one session (expected_cash derived live).
    SessionSummary sessionSummary(qint64 sessionId) const;

    QString errorString() const { return m_error; }

private:
    QSqlDatabase m_db;
    mutable QString m_error;
};
