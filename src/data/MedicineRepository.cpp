#include "data/MedicineRepository.h"

#include "data/Audit.h"
#include "domain/BarcodeParser.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
// Empty string → SQL NULL.
QVariant nz(const QString &s)
{
    return s.isEmpty() ? QVariant() : QVariant(s);
}
} // namespace

QVector<MedicineRow> MedicineRepository::list(const QString &query) const
{
    QString sql = QStringLiteral(
        "SELECT m.id, m.sku, m.brand_name, m.generic_name, m.strength, m.form, "
        "       m.manufacturer, m.controlled_schedule, m.is_active, "
        "       m.base_unit, m.purchase_unit, m.units_per_purchase, "
        // On-hand matches PHP medicines.php:343 — current_qty>0, not quarantined,
        // not flagged expired. NO expiry_date>now (that's only the POS sellable
        // filter); catalog on-hand must report stock you physically hold.
        "       COALESCE((SELECT SUM(current_qty) FROM batches b "
        "                  WHERE b.medicine_id = m.id AND b.current_qty > 0 "
        "                    AND b.is_quarantined = 0 AND b.is_expired = 0), 0) AS on_hand "
        "  FROM medicines m WHERE m.deleted_at IS NULL");

    const QString q = query.trimmed();
    if (!q.isEmpty()) {
        sql += QStringLiteral(
            " AND (lower(m.brand_name) LIKE :like ESCAPE '\\' "
            "      OR lower(m.generic_name) LIKE :like ESCAPE '\\' "
            "      OR lower(m.sku) LIKE :like ESCAPE '\\' "
            "      OR lower(COALESCE(m.manufacturer,'')) LIKE :like ESCAPE '\\' "
            "      OR lower(COALESCE(m.primary_barcode,'')) LIKE :like ESCAPE '\\')");
    }
    sql += QStringLiteral(" ORDER BY m.brand_name LIMIT 200");

    QSqlQuery sq(m_db);
    sq.prepare(sql);
    if (!q.isEmpty()) {
        QString esc = q.toLower();
        esc.replace(QLatin1Char('%'), QStringLiteral("\\%"))
            .replace(QLatin1Char('_'), QStringLiteral("\\_"));
        sq.bindValue(QStringLiteral(":like"), QStringLiteral("%%%1%%").arg(esc));
    }

    QVector<MedicineRow> out;
    if (sq.exec()) {
        while (sq.next()) {
            MedicineRow r;
            r.id = sq.value(0).toLongLong();
            r.sku = sq.value(1).toString();
            r.brandName = sq.value(2).toString();
            r.genericName = sq.value(3).toString();
            r.strength = sq.value(4).toString();
            r.form = sq.value(5).toString();
            r.manufacturer = sq.value(6).toString();
            r.controlledSchedule = sq.value(7).toString();
            r.isActive = sq.value(8).toInt() != 0;
            r.baseUnit = sq.value(9).toString();
            r.purchaseUnit = sq.value(10).toString();
            r.unitsPerPurchase = sq.value(11).toInt();
            r.onHand = sq.value(12).toInt();
            out.push_back(r);
        }
    }
    return out;
}

bool MedicineRepository::find(qint64 id, MedicineDraft *out) const
{
    QSqlQuery q(m_db);
    q.prepare(
        QStringLiteral("SELECT sku, primary_barcode, brand_name, generic_name, strength, "
                       "manufacturer, therapeutic_category, form, form_custom, purchase_unit, "
                       "base_unit, units_per_purchase, controlled_schedule, tax_code_value, "
                       "reorder_level, reorder_quantity, reorder_unit, prescription_required, "
                       "is_active FROM medicines WHERE id = ? AND deleted_at IS NULL"));
    q.addBindValue(id);
    if (!q.exec() || !q.next()) {
        return false;
    }
    MedicineDraft d;
    d.sku = q.value(0).toString();
    d.primaryBarcode = q.value(1).toString();
    d.brandName = q.value(2).toString();
    d.genericName = q.value(3).toString();
    d.strength = q.value(4).toString();
    d.manufacturer = q.value(5).toString();
    d.therapeuticCategory = q.value(6).toString();
    d.form = q.value(7).toString();
    d.formCustom = q.value(8).toString();
    d.purchaseUnit = q.value(9).toString();
    d.baseUnit = q.value(10).toString();
    d.unitsPerPurchase = q.value(11).toInt();
    d.controlledSchedule = q.value(12).toString();
    d.taxCode = q.value(13).toString();
    d.reorderLevel = q.value(14).toInt();
    d.reorderQuantity = q.value(15).toInt();
    d.reorderUnit = q.value(16).toString();
    d.prescriptionRequired = q.value(17).toInt() != 0;
    d.isActive = q.value(18).toInt() != 0;
    *out = d;
    return true;
}

bool MedicineRepository::skuInUse(const QString &sku, qint64 exceptId) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT 1 FROM medicines WHERE sku = ? AND deleted_at IS NULL "
                             "AND id <> ? LIMIT 1"));
    q.addBindValue(sku);
    q.addBindValue(exceptId);
    return q.exec() && q.next();
}

bool MedicineRepository::barcodeInUse(const QString &barcode, qint64 exceptId) const
{
    if (barcode.isEmpty()) {
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(
        QStringLiteral("SELECT 1 FROM medicines WHERE primary_barcode = ? AND deleted_at IS NULL "
                       "AND id <> ? LIMIT 1"));
    q.addBindValue(barcode);
    q.addBindValue(exceptId);
    return q.exec() && q.next();
}

qint64 MedicineRepository::create(const MedicineDraft &d, qint64 userId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO medicines "
        "(sku, primary_barcode, brand_name, generic_name, manufacturer, therapeutic_category, "
        " strength, form, form_custom, purchase_unit, base_unit, units_per_purchase, "
        " controlled_schedule, tax_code_value, reorder_level, reorder_quantity, reorder_unit, "
        " prescription_required, is_active) "
        "VALUES (?,?,?,?,?,?, ?,?,?,?,?,?, ?,?,?,?,?, ?,?)"));
    q.addBindValue(d.sku);
    q.addBindValue(nz(d.primaryBarcode));
    q.addBindValue(d.brandName);
    q.addBindValue(d.genericName);
    q.addBindValue(nz(d.manufacturer));
    q.addBindValue(nz(d.therapeuticCategory));
    q.addBindValue(nz(d.strength));
    q.addBindValue(d.form);
    q.addBindValue(nz(d.formCustom));
    q.addBindValue(d.purchaseUnit);
    q.addBindValue(d.baseUnit);
    q.addBindValue(d.unitsPerPurchase);
    q.addBindValue(d.controlledSchedule);
    q.addBindValue(d.taxCode);
    q.addBindValue(d.reorderLevel);
    q.addBindValue(d.reorderQuantity);
    q.addBindValue(d.reorderUnit);
    q.addBindValue(d.prescriptionRequired ? 1 : 0);
    q.addBindValue(d.isActive ? 1 : 0);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return -1;
    }
    const qint64 id = q.lastInsertId().toLongLong();
    Audit::write(m_db, userId, QStringLiteral("MEDICINE_CREATED"), QStringLiteral("medicines"), id,
                 QString(), QStringLiteral("{\"sku\":\"%1\"}").arg(d.sku));
    return id;
}

bool MedicineRepository::update(qint64 id, const MedicineDraft &d, qint64 userId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE medicines SET sku=?, primary_barcode=?, brand_name=?, generic_name=?, "
        " manufacturer=?, therapeutic_category=?, strength=?, form=?, form_custom=?, "
        " purchase_unit=?, base_unit=?, units_per_purchase=?, controlled_schedule=?, "
        " tax_code_value=?, reorder_level=?, reorder_quantity=?, reorder_unit=?, "
        " prescription_required=?, is_active=?, updated_at=CURRENT_TIMESTAMP "
        "WHERE id=? AND deleted_at IS NULL"));
    q.addBindValue(d.sku);
    q.addBindValue(nz(d.primaryBarcode));
    q.addBindValue(d.brandName);
    q.addBindValue(d.genericName);
    q.addBindValue(nz(d.manufacturer));
    q.addBindValue(nz(d.therapeuticCategory));
    q.addBindValue(nz(d.strength));
    q.addBindValue(d.form);
    q.addBindValue(nz(d.formCustom));
    q.addBindValue(d.purchaseUnit);
    q.addBindValue(d.baseUnit);
    q.addBindValue(d.unitsPerPurchase);
    q.addBindValue(d.controlledSchedule);
    q.addBindValue(d.taxCode);
    q.addBindValue(d.reorderLevel);
    q.addBindValue(d.reorderQuantity);
    q.addBindValue(d.reorderUnit);
    q.addBindValue(d.prescriptionRequired ? 1 : 0);
    q.addBindValue(d.isActive ? 1 : 0);
    q.addBindValue(id);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    Audit::write(m_db, userId, QStringLiteral("MEDICINE_UPDATED"), QStringLiteral("medicines"), id,
                 QString(), QStringLiteral("{\"sku\":\"%1\"}").arg(d.sku));
    return true;
}

bool MedicineRepository::softDelete(qint64 id, qint64 userId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE medicines SET deleted_at=CURRENT_TIMESTAMP, is_active=0, "
                             "updated_at=CURRENT_TIMESTAMP WHERE id=? AND deleted_at IS NULL"));
    q.addBindValue(id);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    Audit::write(m_db, userId, QStringLiteral("MEDICINE_DELETED"), QStringLiteral("medicines"), id);
    return true;
}

QVector<PosMedicine> MedicineRepository::searchForPos(const QString &query, int limit) const
{
    const QString q = query.trimmed();
    QVector<PosMedicine> out;
    if (q.isEmpty()) {
        return out;
    }

    QSqlQuery sq(m_db);
    sq.prepare(QStringLiteral(
        "SELECT m.id, m.brand_name, m.generic_name, m.strength, m.form, m.base_unit, "
        "       m.units_per_purchase, m.controlled_schedule, m.prescription_required, "
        "       (SELECT mrp_per_unit FROM batches b WHERE b.medicine_id = m.id AND b.current_qty > "
        "0 "
        "          AND b.is_quarantined = 0 AND b.is_expired = 0 AND b.expiry_date > date('now') "
        "          ORDER BY b.expiry_date ASC LIMIT 1) AS unit_mrp, "
        "       COALESCE((SELECT SUM(current_qty) FROM batches b WHERE b.medicine_id = m.id "
        "          AND b.current_qty > 0 AND b.is_quarantined = 0 AND b.is_expired = 0 "
        "          AND b.expiry_date > date('now')), 0) AS in_stock, "
        "       m.purchase_unit "
        "  FROM medicines m "
        " WHERE m.is_active = 1 AND m.deleted_at IS NULL "
        "   AND (lower(m.brand_name) LIKE :like ESCAPE '\\' "
        "        OR lower(m.generic_name) LIKE :like ESCAPE '\\' "
        "        OR lower(m.sku) LIKE :like ESCAPE '\\' "
        "        OR lower(COALESCE(m.manufacturer,'')) LIKE :like ESCAPE '\\' "
        "        OR lower(COALESCE(m.primary_barcode,'')) LIKE :like ESCAPE '\\' "
        "        OR m.primary_barcode IN (:g14, :g13)) "
        " ORDER BY m.brand_name ASC LIMIT :lim"));
    QString esc = q.toLower();
    esc.replace(QLatin1Char('%'), QStringLiteral("\\%"))
        .replace(QLatin1Char('_'), QStringLiteral("\\_"));
    sq.bindValue(QStringLiteral(":like"), QStringLiteral("%%%1%%").arg(esc));
    // Scanned barcode → also match primary_barcode exactly (14- and 13-digit GTIN).
    // For a non-barcode (text) query, bind SQL NULL: `primary_barcode IN (:g14,
    // :g13)` is then never true, so the barcode clause stays inert and only the
    // text LIKE matches apply. NULL (not '') is required — primary_barcode can be
    // '' (see COALESCE above), and '' IN ('', '') would match every blank row.
    const Barcode::ParsedBarcode scan = Barcode::parse(q);
    sq.bindValue(QStringLiteral(":g14"),
                 scan.isBarcode && !scan.gtin.isEmpty() ? QVariant(scan.gtin) : QVariant());
    sq.bindValue(QStringLiteral(":g13"),
                 scan.isBarcode && !scan.gtin13.isEmpty() ? QVariant(scan.gtin13) : QVariant());
    sq.bindValue(QStringLiteral(":lim"), limit);

    if (sq.exec()) {
        while (sq.next()) {
            PosMedicine p;
            p.id = sq.value(0).toLongLong();
            p.brandName = sq.value(1).toString();
            p.genericName = sq.value(2).toString();
            p.strength = sq.value(3).toString();
            p.form = sq.value(4).toString();
            p.baseUnit = sq.value(5).toString();
            p.unitsPerPurchase = sq.value(6).toInt();
            p.controlledSchedule = sq.value(7).toString();
            p.prescriptionRequired = sq.value(8).toInt() != 0;
            p.unitMrp = sq.value(9).toString();
            p.inStock = sq.value(10).toInt();
            p.purchaseUnit = sq.value(11).toString();
            out.push_back(p);
        }
    }
    return out;
}
