#include "service/AlternativesFinder.h"

#include <QSqlQuery>
#include <QVariant>

QVector<AlternativeMedicine> AlternativesFinder::forMedicine(qint64 medicineId) const
{
    QVector<AlternativeMedicine> out;

    // Resolve the source medicine's generic name.
    QSqlQuery gq(m_db);
    gq.prepare(QStringLiteral("SELECT generic_name FROM medicines WHERE id = ?"));
    gq.addBindValue(medicineId);
    if (!gq.exec() || !gq.next()) {
        return out;
    }
    const QString generic = gq.value(0).toString().trimmed();
    if (generic.isEmpty()) {
        return out;
    }

    // Other active medicines of the same generic, with saleable on-hand summed
    // from valid batches, and the soonest-expiry valid batch's MRP.
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT m.id, m.brand_name, COALESCE(m.strength,''), m.form, COALESCE(m.manufacturer,''), "
        "  COALESCE((SELECT SUM(b.current_qty) FROM batches b "
        "            WHERE b.medicine_id = m.id AND b.current_qty > 0 "
        "              AND b.is_quarantined = 0 AND b.is_expired = 0 "
        "              AND b.expiry_date > date('now')), 0) AS on_hand, "
        "  (SELECT b2.mrp_per_unit FROM batches b2 "
        "     WHERE b2.medicine_id = m.id AND b2.current_qty > 0 "
        "       AND b2.is_quarantined = 0 AND b2.is_expired = 0 "
        "       AND b2.expiry_date > date('now') "
        "     ORDER BY b2.expiry_date ASC, b2.id ASC LIMIT 1) AS unit_mrp "
        "  FROM medicines m "
        " WHERE m.id <> ? AND m.is_active = 1 AND m.deleted_at IS NULL "
        "   AND LOWER(TRIM(m.generic_name)) = LOWER(?) "
        " ORDER BY on_hand DESC, m.brand_name ASC"));
    q.addBindValue(medicineId);
    q.addBindValue(generic);
    if (!q.exec()) {
        return out;
    }
    while (q.next()) {
        AlternativeMedicine a;
        a.id = q.value(0).toLongLong();
        a.brandName = q.value(1).toString();
        a.strength = q.value(2).toString();
        a.form = q.value(3).toString();
        a.manufacturer = q.value(4).toString();
        a.onHand = q.value(5).toInt();
        a.unitMrp = q.value(6).toString();
        out.push_back(a);
    }
    return out;
}
