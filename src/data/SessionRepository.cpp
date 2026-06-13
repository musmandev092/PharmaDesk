#include "data/SessionRepository.h"

#include "data/Audit.h"
#include "domain/Money.h"

#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
// Matches the PHP guard: non-negative amount, up to 2 decimal places.
bool isMoneyInput(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("^\\d+(\\.\\d{1,2})?$"));
    return re.match(s.trimmed()).hasMatch();
}

// expected_cash = opening_float + total_cash_sales - total_refunds_paid (scale 2)
QString expectedCash(const QString &openingFloat, const QString &cashSales, const QString &refunds)
{
    const Money e = Money::fromString(openingFloat) + Money::fromString(cashSales)
                    - Money::fromString(refunds);
    return e.toString(Money::ScaleMoney);
}
} // namespace

qint64 SessionRepository::openSessionId(qint64 cashierId) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id FROM cashier_sessions "
                             " WHERE cashier_id = ? AND status = 'OPEN' "
                             " ORDER BY opened_at DESC LIMIT 1"));
    q.addBindValue(cashierId);
    if (q.exec() && q.next()) {
        return q.value(0).toLongLong();
    }
    return -1;
}

qint64 SessionRepository::openSession(qint64 cashierId, const QString &openingFloat)
{
    const QString trimmed = openingFloat.trimmed();
    if (!isMoneyInput(trimmed)) {
        m_error = QStringLiteral("Opening float must be a non-negative amount.");
        return -1;
    }
    const QString floatStr = Money::fromString(trimmed).toString(Money::ScaleMoney);

    if (openSessionId(cashierId) != -1) {
        m_error = QStringLiteral(
            "You already have an open session. Close it before opening a new one.");
        return -1;
    }

    if (!m_db.transaction()) {
        m_error = m_db.lastError().text();
        return -1;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO cashier_sessions "
                             "(branch_id, cashier_id, opened_at, opening_float, total_cash_sales, "
                             " total_refunds_paid, total_sales_count, status) "
                             "VALUES (1, ?, CURRENT_TIMESTAMP, ?, '0', '0', 0, 'OPEN')"));
    q.addBindValue(cashierId);
    q.addBindValue(floatStr);
    if (!q.exec()) {
        m_error = q.lastError().text();
        m_db.rollback();
        return -1;
    }
    const qint64 id = q.lastInsertId().toLongLong();

    Audit::write(m_db, cashierId, QStringLiteral("SESSION_OPENED"),
                 QStringLiteral("cashier_sessions"), id, QString(),
                 QStringLiteral("{\"opening_float\":\"%1\"}").arg(floatStr));

    if (!m_db.commit()) {
        m_error = m_db.lastError().text();
        m_db.rollback();
        return -1;
    }
    return id;
}

bool SessionRepository::closeSession(qint64 sessionId, const QString &countedCash, qint64 userId)
{
    // Load + validate the session OUTSIDE the transaction (read-only checks).
    QString status;
    qint64 ownerId = 0;
    QString openingFloat, cashSales, refunds;
    {
        QSqlQuery s(m_db);
        s.prepare(QStringLiteral(
            "SELECT cashier_id, status, opening_float, total_cash_sales, total_refunds_paid "
            "  FROM cashier_sessions WHERE id = ?"));
        s.addBindValue(sessionId);
        if (!s.exec() || !s.next()) {
            m_error = QStringLiteral("Session not found.");
            return false;
        }
        ownerId = s.value(0).toLongLong();
        status = s.value(1).toString();
        openingFloat = s.value(2).toString();
        cashSales = s.value(3).toString();
        refunds = s.value(4).toString();
    }
    if (status != QLatin1String("OPEN")) {
        m_error = QStringLiteral("Session is not OPEN — cannot close.");
        return false;
    }
    // Ownership check — only the shift's own cashier can close it (mirrors PHP).
    if (ownerId != userId) {
        m_error = QStringLiteral("Permission denied: only the shift's own cashier can close it.");
        return false;
    }

    const QString trimmed = countedCash.trimmed();
    if (!isMoneyInput(trimmed)) {
        m_error = QStringLiteral("Counted cash must be a non-negative amount.");
        return false;
    }

    const QString expected = expectedCash(openingFloat, cashSales, refunds);
    const QString counted = Money::fromString(trimmed).toString(Money::ScaleMoney);
    const QString variance
        = (Money::fromString(counted) - Money::fromString(expected)).toString(Money::ScaleMoney);

    if (!m_db.transaction()) {
        m_error = m_db.lastError().text();
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare(
        QStringLiteral("UPDATE cashier_sessions "
                       "   SET closed_at = CURRENT_TIMESTAMP, "
                       "       expected_cash_in_drawer = ?, counted_cash = ?, cash_variance = ?, "
                       "       status = 'CLOSED', updated_at = CURRENT_TIMESTAMP "
                       " WHERE id = ? AND status = 'OPEN'"));
    q.addBindValue(expected);
    q.addBindValue(counted);
    q.addBindValue(variance);
    q.addBindValue(sessionId);
    if (!q.exec()) {
        m_error = q.lastError().text();
        m_db.rollback();
        return false;
    }
    if (q.numRowsAffected() != 1) {
        m_error = QStringLiteral("Session is not OPEN — cannot close.");
        m_db.rollback();
        return false;
    }

    Audit::write(
        m_db, userId, QStringLiteral("SESSION_CLOSED"), QStringLiteral("cashier_sessions"),
        sessionId, QString(),
        QStringLiteral("{\"expected_cash\":\"%1\",\"counted_cash\":\"%2\",\"variance\":\"%3\"}")
            .arg(expected, counted, variance));

    if (!m_db.commit()) {
        m_error = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}

bool SessionRepository::bumpForSale(qint64 sessionId, const QString &grandTotal,
                                    const QString &paymentMode)
{
    if (sessionId <= 0) {
        return true; // no open session linked — nothing to bump
    }

    // Read current cash total so the add happens through Money (decimal-exact),
    // not via SQL string concatenation. Caller is expected to already be inside
    // the sale transaction.
    QString currentCash = QStringLiteral("0");
    {
        QSqlQuery s(m_db);
        s.prepare(QStringLiteral(
            "SELECT total_cash_sales FROM cashier_sessions WHERE id = ? AND status = 'OPEN'"));
        s.addBindValue(sessionId);
        if (!s.exec() || !s.next()) {
            // Session missing or not OPEN — treat as no-op (don't fail the sale).
            return true;
        }
        currentCash = s.value(0).toString();
    }

    QString newCash = currentCash;
    if (paymentMode == QLatin1String("CASH")) {
        newCash = (Money::fromString(currentCash) + Money::fromString(grandTotal))
                      .toString(Money::ScaleMoney);
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE cashier_sessions "
                             "   SET total_sales_count = total_sales_count + 1, "
                             "       total_cash_sales = ?, updated_at = CURRENT_TIMESTAMP "
                             " WHERE id = ? AND status = 'OPEN'"));
    q.addBindValue(newCash);
    q.addBindValue(sessionId);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

QVector<SessionRow> SessionRepository::list(int limit) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT s.id, COALESCE(u.full_name, '—'), s.opened_at, s.closed_at, "
                             "       s.opening_float, s.total_cash_sales, s.total_sales_count, "
                             "       s.counted_cash, s.cash_variance, s.status "
                             "  FROM cashier_sessions s "
                             "  LEFT JOIN users u ON u.id = s.cashier_id "
                             " ORDER BY s.opened_at DESC, s.id DESC LIMIT ?"));
    q.addBindValue(limit);

    QVector<SessionRow> out;
    if (q.exec()) {
        while (q.next()) {
            SessionRow r;
            r.id = q.value(0).toLongLong();
            r.cashierName = q.value(1).toString();
            r.openedAt = q.value(2).toString();
            r.closedAt = q.value(3).toString();
            r.openingFloat = q.value(4).toString();
            r.totalCashSales = q.value(5).toString();
            r.totalSalesCount = q.value(6).toInt();
            r.countedCash = q.value(7).toString();
            r.cashVariance = q.value(8).toString();
            r.status = q.value(9).toString();
            out.push_back(r);
        }
    }
    return out;
}

SessionSummary SessionRepository::sessionSummary(qint64 sessionId) const
{
    SessionSummary out;
    QSqlQuery q(m_db);
    q.prepare(
        QStringLiteral("SELECT s.id, COALESCE(u.full_name, '—'), s.opened_at, s.closed_at, "
                       "       s.opening_float, s.total_cash_sales, s.total_refunds_paid, "
                       "       s.total_sales_count, s.counted_cash, s.cash_variance, s.status "
                       "  FROM cashier_sessions s "
                       "  LEFT JOIN users u ON u.id = s.cashier_id "
                       " WHERE s.id = ?"));
    q.addBindValue(sessionId);
    if (!q.exec() || !q.next()) {
        return out;
    }
    out.valid = true;
    out.id = q.value(0).toLongLong();
    out.cashierName = q.value(1).toString();
    out.openedAt = q.value(2).toString();
    out.closedAt = q.value(3).toString();
    out.openingFloat = q.value(4).toString();
    out.totalCashSales = q.value(5).toString();
    out.totalRefundsPaid = q.value(6).toString();
    out.totalSalesCount = q.value(7).toInt();
    out.countedCash = q.value(8).toString();
    out.cashVariance = q.value(9).toString();
    out.status = q.value(10).toString();
    out.expectedCash = expectedCash(out.openingFloat, out.totalCashSales, out.totalRefundsPaid);
    return out;
}
