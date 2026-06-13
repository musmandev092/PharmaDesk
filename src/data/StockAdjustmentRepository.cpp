#include "data/StockAdjustmentRepository.h"

#include <QSqlQuery>

QVector<AdjustableBatch> StockAdjustmentRepository::batchesForMedicine(qint64 medicineId) const
{
    QVector<AdjustableBatch> out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, batch_number, expiry_date, current_qty, is_quarantined, is_expired "
        "  FROM batches WHERE medicine_id = ? ORDER BY expiry_date ASC, id ASC"));
    q.addBindValue(medicineId);
    if (!q.exec()) {
        return out;
    }
    while (q.next()) {
        AdjustableBatch b;
        b.id = q.value(0).toLongLong();
        b.batchNumber = q.value(1).toString();
        b.expiry = q.value(2).toString();
        b.currentQty = q.value(3).toInt();
        b.quarantined = q.value(4).toInt() != 0;
        b.expired = q.value(5).toInt() != 0;
        out.push_back(b);
    }
    return out;
}

QVector<AdjustmentRow> StockAdjustmentRepository::listRecent(int limit) const
{
    QVector<AdjustmentRow> out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT a.id, a.adjustment_number, "
        "       m.brand_name || COALESCE(' ' || m.strength, ''), b.batch_number, "
        "       a.qty_delta, a.reason, COALESCE(a.notes,''), COALESCE(a.cost_impact,'0'), "
        "       a.created_at, COALESCE(u.full_name,'') "
        "  FROM stock_adjustments a "
        "  JOIN medicines m ON m.id = a.medicine_id "
        "  JOIN batches b ON b.id = a.batch_id "
        "  LEFT JOIN users u ON u.id = a.performed_by "
        " ORDER BY a.id DESC LIMIT ?"));
    q.addBindValue(limit);
    if (!q.exec()) {
        return out;
    }
    while (q.next()) {
        AdjustmentRow r;
        r.id = q.value(0).toLongLong();
        r.number = q.value(1).toString();
        r.medicineName = q.value(2).toString();
        r.batchNumber = q.value(3).toString();
        r.qtyDelta = q.value(4).toInt();
        r.reason = q.value(5).toString();
        r.notes = q.value(6).toString();
        r.costImpact = q.value(7).toString();
        r.createdAt = q.value(8).toString();
        r.performedBy = q.value(9).toString();
        out.push_back(r);
    }
    return out;
}
