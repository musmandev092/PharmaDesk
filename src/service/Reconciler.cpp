#include "service/Reconciler.h"

#include "data/Audit.h"
#include "domain/Money.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
struct ReconcileError
{
    QString message;
};

QString jesc(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (const QChar c : s) {
        switch (c.unicode()) {
        case '"':
            out += QStringLiteral("\\\"");
            break;
        case '\\':
            out += QStringLiteral("\\\\");
            break;
        case '\n':
            out += QStringLiteral("\\n");
            break;
        case '\r':
            out += QStringLiteral("\\r");
            break;
        case '\t':
            out += QStringLiteral("\\t");
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}
} // namespace

ReconcileResult Reconciler::reconcile(qint64 sessionId, const QString &recountedCash,
                                      const QString &notes)
{
    ReconcileResult res;
    bool inTxn = false;
    try {
        if (!m_db.transaction()) {
            throw ReconcileError{m_db.lastError().text()};
        }
        inTxn = true;

        QSqlQuery sq(m_db);
        sq.prepare(QStringLiteral(
            "SELECT status, opening_float, total_cash_sales, total_refunds_paid, "
            "       counted_cash, COALESCE(notes,'') FROM cashier_sessions WHERE id = ?"));
        sq.addBindValue(sessionId);
        if (!sq.exec() || !sq.next()) {
            throw ReconcileError{QStringLiteral("Session not found.")};
        }
        const QString status = sq.value(0).toString();
        const Money openingFloat = Money::fromString(sq.value(1).toString());
        const Money cashSales = Money::fromString(sq.value(2).toString());
        const Money refundsPaid = Money::fromString(sq.value(3).toString());
        const QString existingCounted = sq.value(4).toString();
        const QString existingNotes = sq.value(5).toString();

        if (status != QLatin1String("CLOSED")) {
            throw ReconcileError{
                QStringLiteral("Only a CLOSED shift can be reconciled — this one is %1.")
                    .arg(status)};
        }

        // expected = opening_float + total_cash_sales − total_refunds_paid.
        const Money expected = openingFloat + cashSales - refundsPaid;

        // Recount if a new figure was supplied, else keep the cashier's count.
        const bool recount = !recountedCash.trimmed().isEmpty();
        const QString countedStr
            = recount ? Money::fromString(recountedCash).toString(Money::ScaleMoney)
                      : existingCounted;
        const Money counted = Money::fromString(countedStr);
        const QString varianceStr = (counted - expected).toString(Money::ScaleMoney);

        QString mergedNotes = existingNotes;
        if (!notes.trimmed().isEmpty()) {
            if (!mergedNotes.isEmpty()) {
                mergedNotes += QLatin1Char('\n');
            }
            mergedNotes += QStringLiteral("[reconciled] ") + notes.trimmed();
        }

        QSqlQuery uq(m_db);
        uq.prepare(QStringLiteral(
            "UPDATE cashier_sessions SET status = 'RECONCILED', counted_cash = ?, "
            "cash_variance = ?, notes = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?"));
        uq.addBindValue(countedStr.isEmpty() ? QVariant() : QVariant(countedStr));
        uq.addBindValue(varianceStr);
        uq.addBindValue(mergedNotes.isEmpty() ? QVariant() : QVariant(mergedNotes));
        uq.addBindValue(sessionId);
        if (!uq.exec()) {
            throw ReconcileError{uq.lastError().text()};
        }

        Audit::writeOrThrow(m_db, m_managerId, QStringLiteral("SESSION_RECONCILED"),
                            QStringLiteral("cashier_sessions"), sessionId,
                            QStringLiteral("{\"status\":\"CLOSED\"}"),
                            QStringLiteral("{\"status\":\"RECONCILED\",\"counted_cash\":\"%1\","
                                           "\"variance\":\"%2\"}")
                                .arg(jesc(countedStr), jesc(varianceStr)));

        if (!m_db.commit()) {
            m_db.rollback();
            throw ReconcileError{m_db.lastError().text()};
        }
        inTxn = false;

        res.ok = true;
        res.countedCash = countedStr;
        res.variance = varianceStr;
        return res;

    } catch (const ReconcileError &e) {
        if (inTxn) m_db.rollback();
        res.ok = false;
        res.error = e.message;
        return res;
    } catch (const std::exception &e) {
        if (inTxn) m_db.rollback();
        res.ok = false;
        res.error = QString::fromUtf8(e.what());
        return res;
    }
}
