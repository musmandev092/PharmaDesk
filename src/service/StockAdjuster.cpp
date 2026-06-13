#include "service/StockAdjuster.h"

#include "data/Audit.h"
#include "domain/Money.h"

#include <QDate>
#include <QRandomGenerator>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
struct AdjustError
{
    QString message;
};

const QSet<QString> kReasons = {
    QStringLiteral("DAMAGE"),    QStringLiteral("EXPIRY_WRITEOFF"),
    QStringLiteral("SHRINKAGE"), QStringLiteral("COUNT_CORRECTION"),
    QStringLiteral("SAMPLE"),    QStringLiteral("DONATION"),
    QStringLiteral("OTHER"),
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

QString StockAdjuster::nextAdjustmentNumber()
{
    const QString today = QDate::currentDate().toString(QStringLiteral("yyyyMMdd"));
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT count(*) FROM stock_adjustments "
                          "WHERE date(created_at, 'localtime') = date('now', 'localtime')"));
    int count = 0;
    if (q.next()) count = q.value(0).toInt();

    for (int attempt = 0; attempt < 3; ++attempt) {
        const QString candidate = QStringLiteral("ADJ-%1-%2")
                                      .arg(today)
                                      .arg(count + 1 + attempt, 4, 10, QLatin1Char('0'));
        QSqlQuery chk(m_db);
        chk.prepare(
            QStringLiteral("SELECT 1 FROM stock_adjustments WHERE adjustment_number = ? LIMIT 1"));
        chk.addBindValue(candidate);
        if (chk.exec() && !chk.next()) {
            return candidate;
        }
    }
    const quint32 r = QRandomGenerator::global()->generate();
    return QStringLiteral("ADJ-%1-%2").arg(today).arg(r % 1000000u, 6, 10, QLatin1Char('0'));
}

AdjustResult StockAdjuster::adjust(qint64 batchId, int qtyDelta, const QString &reason,
                                   const QString &notes)
{
    AdjustResult res;
    bool inTxn = false;
    try {
        if (qtyDelta == 0) {
            throw AdjustError{QStringLiteral("Adjustment quantity cannot be zero.")};
        }
        if (!kReasons.contains(reason)) {
            throw AdjustError{QStringLiteral("Choose a valid adjustment reason.")};
        }
        // OTHER is a catch-all; an audit trail is worthless without a written
        // reason, so refuse it server-side rather than trusting the form.
        if (reason == QLatin1String("OTHER") && notes.trimmed().isEmpty()) {
            throw AdjustError{QStringLiteral("Notes are required when the reason is OTHER — "
                                             "explain the adjustment.")};
        }

        if (!m_db.transaction()) {
            throw AdjustError{m_db.lastError().text()};
        }
        inTxn = true;

        QSqlQuery bq(m_db);
        bq.prepare(QStringLiteral(
            "SELECT branch_id, medicine_id, current_qty, is_quarantined, cost_per_unit "
            "  FROM batches WHERE id = ?"));
        bq.addBindValue(batchId);
        if (!bq.exec() || !bq.next()) {
            throw AdjustError{QStringLiteral("Batch not found.")};
        }
        const qint64 branchId = bq.value(0).toLongLong();
        const qint64 medicineId = bq.value(1).toLongLong();
        const int qtyBefore = bq.value(2).toInt();
        const bool quarantined = bq.value(3).toInt() != 0;
        const QString costPerUnit = bq.value(4).toString();

        // Quarantined batches await QA disposition — only an expiry write-off may
        // touch them in place; otherwise release the quarantine first.
        if (quarantined && reason != QLatin1String("EXPIRY_WRITEOFF")) {
            throw AdjustError{QStringLiteral("This batch is QUARANTINED — release it before "
                                             "adjusting stock, or use an expiry write-off.")};
        }

        const int qtyAfter = qtyBefore + qtyDelta;
        if (qtyAfter < 0) {
            throw AdjustError{
                QStringLiteral("Resulting stock cannot be negative (would be %1).").arg(qtyAfter)};
        }

        const QString costImpact
            = Money::fromString(costPerUnit).mul(qAbs(qtyDelta)).toString(Money::ScaleMoney);
        const QString adjNumber = nextAdjustmentNumber();

        QSqlQuery ins(m_db);
        ins.prepare(QStringLiteral(
            "INSERT INTO stock_adjustments "
            "(branch_id, adjustment_number, medicine_id, batch_id, qty_delta, reason, notes, "
            " performed_by, cost_impact) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
        ins.addBindValue(branchId);
        ins.addBindValue(adjNumber);
        ins.addBindValue(medicineId);
        ins.addBindValue(batchId);
        ins.addBindValue(qtyDelta);
        ins.addBindValue(reason);
        ins.addBindValue(notes.trimmed().isEmpty() ? QVariant() : QVariant(notes.trimmed()));
        ins.addBindValue(m_userId);
        ins.addBindValue(costImpact);
        if (!ins.exec()) {
            throw AdjustError{ins.lastError().text()};
        }
        const qint64 adjId = ins.lastInsertId().toLongLong();

        QSqlQuery ub(m_db);
        ub.prepare(QStringLiteral(
            "UPDATE batches SET current_qty = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?"));
        ub.addBindValue(qtyAfter);
        ub.addBindValue(batchId);
        if (!ub.exec()) {
            throw AdjustError{ub.lastError().text()};
        }

        QSqlQuery mv(m_db);
        mv.prepare(QStringLiteral(
            "INSERT INTO inventory_movements "
            "(branch_id, medicine_id, batch_id, movement_type, qty_delta, qty_before, "
            " qty_after, ref_table, ref_id, performed_by) "
            "VALUES (?, ?, ?, 'ADJUSTMENT', ?, ?, ?, 'stock_adjustments', ?, ?)"));
        mv.addBindValue(branchId);
        mv.addBindValue(medicineId);
        mv.addBindValue(batchId);
        mv.addBindValue(qtyDelta);
        mv.addBindValue(qtyBefore);
        mv.addBindValue(qtyAfter);
        mv.addBindValue(adjId);
        mv.addBindValue(m_userId);
        if (!mv.exec()) {
            throw AdjustError{mv.lastError().text()};
        }

        Audit::writeOrThrow(m_db, m_userId, QStringLiteral("STOCK_ADJUSTED"),
                            QStringLiteral("stock_adjustments"), adjId, QString(),
                            QStringLiteral("{\"adjustment_number\":\"%1\",\"qty_delta\":%2,"
                                           "\"reason\":\"%3\",\"cost_impact\":\"%4\"}")
                                .arg(jesc(adjNumber))
                                .arg(qtyDelta)
                                .arg(jesc(reason), jesc(costImpact)));

        if (!m_db.commit()) {
            m_db.rollback();
            throw AdjustError{m_db.lastError().text()};
        }
        inTxn = false;

        res.ok = true;
        res.adjustmentId = adjId;
        res.adjustmentNumber = adjNumber;
        res.costImpact = costImpact;
        return res;

    } catch (const AdjustError &e) {
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
