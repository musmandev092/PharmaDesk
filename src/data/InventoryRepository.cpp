#include "data/InventoryRepository.h"

#include "domain/Money.h"

#include <QSqlQuery>

static const QString kDateFmt = QStringLiteral("yyyy-MM-dd");

InventoryTotals InventoryRepository::totals() const
{
    InventoryTotals t;

    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT count(*) FROM medicines WHERE is_active = 1 AND deleted_at IS NULL"));
    if (q.next()) t.medicinesCount = q.value(0).toInt();

    q.exec(QStringLiteral("SELECT count(*) FROM batches WHERE current_qty > 0 AND is_quarantined = "
                          "0 AND is_expired = 0"));
    if (q.next()) t.activeBatches = q.value(0).toInt();

    // Sum stock value with Money (exact) rather than SQLite float arithmetic.
    Money cost;
    Money mrp;
    q.exec(QStringLiteral("SELECT current_qty, cost_per_unit, mrp_per_unit FROM batches "
                          "WHERE current_qty > 0 AND is_quarantined = 0"));
    while (q.next()) {
        const int qty = q.value(0).toInt();
        cost = cost + Money::fromString(q.value(1).toString()).mul(qty);
        mrp = mrp + Money::fromString(q.value(2).toString()).mul(qty);
    }
    t.stockValueCost = cost.toString(Money::ScaleMoney);
    t.stockValueMrp = mrp.toString(Money::ScaleMoney);
    return t;
}

QVector<StockBatchRow> InventoryRepository::batchList(const QString &query) const
{
    const QDate today = QDate::currentDate();
    QString sql
        = QStringLiteral("SELECT b.id, m.brand_name, m.generic_name, m.base_unit, b.batch_number, "
                         "       b.expiry_date, b.current_qty, b.cost_per_unit, b.mrp_per_unit, "
                         "       b.is_quarantined, b.is_expired "
                         "  FROM batches b JOIN medicines m ON m.id = b.medicine_id");

    const QString q = query.trimmed();
    if (!q.isEmpty()) {
        sql += QStringLiteral(
            " WHERE (lower(m.brand_name) LIKE :like ESCAPE '\\' "
            "        OR lower(m.generic_name) LIKE :like ESCAPE '\\' "
            "        OR lower(m.sku) LIKE :like ESCAPE '\\' "
            "        OR lower(COALESCE(m.primary_barcode,'')) LIKE :like ESCAPE '\\' "
            "        OR lower(b.batch_number) LIKE :like ESCAPE '\\')");
    }
    sql += QStringLiteral(" ORDER BY b.expiry_date ASC, m.brand_name LIMIT 300");

    QSqlQuery sq(m_db);
    sq.prepare(sql);
    if (!q.isEmpty()) {
        QString esc = q.toLower();
        esc.replace(QLatin1Char('%'), QStringLiteral("\\%"))
            .replace(QLatin1Char('_'), QStringLiteral("\\_"));
        sq.bindValue(QStringLiteral(":like"), QStringLiteral("%%%1%%").arg(esc));
    }

    QVector<StockBatchRow> out;
    if (sq.exec()) {
        while (sq.next()) {
            StockBatchRow r;
            r.id = sq.value(0).toLongLong();
            r.brandName = sq.value(1).toString();
            r.genericName = sq.value(2).toString();
            r.baseUnit = sq.value(3).toString();
            r.batchNumber = sq.value(4).toString();
            r.expiry = QDate::fromString(sq.value(5).toString(), kDateFmt);
            r.currentQty = sq.value(6).toInt();
            r.costPerUnit = sq.value(7).toString();
            r.mrpPerUnit = sq.value(8).toString();
            r.quarantined = sq.value(9).toInt() != 0;
            r.expired = sq.value(10).toInt() != 0;
            r.days = r.expiry.isValid() ? today.daysTo(r.expiry) : 0;
            out.push_back(r);
        }
    }
    return out;
}

QVector<LowStockRow> InventoryRepository::lowStock(int limit) const
{
    QString sql
        = QStringLiteral("SELECT m.id, m.brand_name, m.generic_name, m.base_unit, m.reorder_level, "
                         "       COALESCE(SUM(b.current_qty), 0) AS on_hand "
                         "  FROM medicines m "
                         // On-hand matches PHP medicines.php: current_qty>0, not quarantined, not
                         // flagged expired. NO expiry_date>now here — that filter belongs only to
                         // the POS *sellable* path (searchForPos), not the on-hand/low-stock count.
                         "  LEFT JOIN batches b ON b.medicine_id = m.id AND b.current_qty > 0 "
                         "       AND b.is_quarantined = 0 AND b.is_expired = 0 "
                         " WHERE m.is_active = 1 AND m.deleted_at IS NULL AND m.reorder_level > 0 "
                         "   AND EXISTS (SELECT 1 FROM batches bb WHERE bb.medicine_id = m.id) "
                         " GROUP BY m.id "
                         "HAVING COALESCE(SUM(b.current_qty), 0) < m.reorder_level "
                         " ORDER BY (m.reorder_level - COALESCE(SUM(b.current_qty), 0)) DESC");
    if (limit > 0) {
        sql += QStringLiteral(" LIMIT %1").arg(limit);
    }

    QVector<LowStockRow> out;
    QSqlQuery q(m_db);
    if (q.exec(sql)) {
        while (q.next()) {
            LowStockRow r;
            r.medicineId = q.value(0).toLongLong();
            r.brandName = q.value(1).toString();
            r.genericName = q.value(2).toString();
            r.baseUnit = q.value(3).toString();
            r.reorderLevel = q.value(4).toInt();
            r.onHand = q.value(5).toInt();
            out.push_back(r);
        }
    }
    return out;
}

QVector<ExpiringRow> InventoryRepository::expiringSoon(int withinDays, int limit) const
{
    const QDate today = QDate::currentDate();
    const QString cutoff = today.addDays(withinDays).toString(kDateFmt);

    QString sql = QStringLiteral(
        "SELECT b.id, m.brand_name, b.batch_number, m.base_unit, b.expiry_date, b.current_qty "
        "  FROM batches b JOIN medicines m ON m.id = b.medicine_id "
        " WHERE b.current_qty > 0 AND b.is_quarantined = 0 AND b.is_expired = 0 "
        "   AND b.expiry_date <= :cutoff "
        " ORDER BY b.expiry_date ASC");
    if (limit > 0) {
        sql += QStringLiteral(" LIMIT %1").arg(limit);
    }

    QVector<ExpiringRow> out;
    QSqlQuery q(m_db);
    q.prepare(sql);
    q.bindValue(QStringLiteral(":cutoff"), cutoff);
    if (q.exec()) {
        while (q.next()) {
            ExpiringRow r;
            r.batchId = q.value(0).toLongLong();
            r.brandName = q.value(1).toString();
            r.batchNumber = q.value(2).toString();
            r.baseUnit = q.value(3).toString();
            r.expiry = QDate::fromString(q.value(4).toString(), kDateFmt);
            r.currentQty = q.value(5).toInt();
            r.days = r.expiry.isValid() ? today.daysTo(r.expiry) : 0;
            out.push_back(r);
        }
    }
    return out;
}
