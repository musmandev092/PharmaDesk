#include "data/BatchRepository.h"

#include "data/Audit.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

static const QString kDateFmt = QStringLiteral("yyyy-MM-dd");

qint64 BatchRepository::addStock(const StockInDraft &d, qint64 userId)
{
    if (!m_db.transaction()) {
        m_error = m_db.lastError().text();
        return -1;
    }

    QSqlQuery q(m_db);
    q.prepare(
        QStringLiteral("INSERT INTO batches "
                       "(branch_id, medicine_id, batch_number, expiry_date, received_qty, foc_qty, "
                       " current_qty, cost_per_unit, mrp_per_unit) "
                       "VALUES (1, ?, ?, ?, ?, 0, ?, ?, ?)"));
    q.addBindValue(d.medicineId);
    q.addBindValue(d.batchNumber);
    q.addBindValue(d.expiry.toString(kDateFmt));
    q.addBindValue(d.quantity);
    q.addBindValue(d.quantity);
    q.addBindValue(d.costPerUnit);
    q.addBindValue(d.mrpPerUnit);
    if (!q.exec()) {
        m_error = q.lastError().text();
        m_db.rollback();
        return -1;
    }
    const qint64 batchId = q.lastInsertId().toLongLong();

    q.prepare(QStringLiteral(
        "INSERT INTO inventory_movements "
        "(branch_id, medicine_id, batch_id, movement_type, qty_delta, qty_before, qty_after, "
        " ref_table, ref_id, performed_by) "
        "VALUES (1, ?, ?, 'GRN_RECEIPT', ?, 0, ?, 'batches', ?, ?)"));
    q.addBindValue(d.medicineId);
    q.addBindValue(batchId);
    q.addBindValue(d.quantity);
    q.addBindValue(d.quantity);
    q.addBindValue(batchId);
    q.addBindValue(userId > 0 ? QVariant(userId) : QVariant());
    if (!q.exec()) {
        m_error = q.lastError().text();
        m_db.rollback();
        return -1;
    }

    Audit::write(m_db, userId, QStringLiteral("STOCK_RECEIVED"), QStringLiteral("batches"), batchId,
                 QString(), QStringLiteral("{\"qty\":%1}").arg(d.quantity));

    if (!m_db.commit()) {
        m_error = m_db.lastError().text();
        m_db.rollback();
        return -1;
    }
    return batchId;
}

QVector<FefoBatch> BatchRepository::candidateBatchesForSale(qint64 medicineId) const
{
    QVector<FefoBatch> out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id, current_qty, cost_per_unit, mrp_per_unit, expiry_date, "
                             "       is_quarantined, is_expired "
                             "  FROM batches "
                             " WHERE medicine_id = ? AND current_qty > 0 AND is_quarantined = 0 "
                             "   AND is_expired = 0 AND expiry_date > date('now') "
                             " ORDER BY expiry_date ASC, id ASC"));
    q.addBindValue(medicineId);
    if (q.exec()) {
        while (q.next()) {
            FefoBatch b;
            b.id = q.value(0).toLongLong();
            b.currentQty = q.value(1).toInt();
            b.costPerUnit = q.value(2).toString();
            b.mrpPerUnit = q.value(3).toString();
            b.expiry = QDate::fromString(q.value(4).toString(), kDateFmt);
            b.quarantined = q.value(5).toInt() != 0;
            b.expired = q.value(6).toInt() != 0;
            out.push_back(b);
        }
    }
    return out;
}

int BatchRepository::onHand(qint64 medicineId) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(current_qty),0) FROM batches "
        "WHERE medicine_id = ? AND current_qty > 0 AND is_quarantined = 0 AND is_expired = 0"));
    q.addBindValue(medicineId);
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}
