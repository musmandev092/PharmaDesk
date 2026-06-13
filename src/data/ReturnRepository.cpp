#include "data/ReturnRepository.h"

#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

ReturnSaleHeader ReturnRepository::lookupByReceipt(const QString &receiptNumber) const
{
    ReturnSaleHeader hdr;
    const QString raw = receiptNumber.trimmed();
    if (raw.isEmpty()) {
        return hdr;
    }

    // Build candidate forms so a typed "INV-20260518-1" matches the stored,
    // zero-padded "INV-20260518-0001" (mirrors the PHP initiate page).
    QStringList candidates;
    candidates << raw.toUpper();
    const int dash = raw.lastIndexOf(QLatin1Char('-'));
    if (dash > 0 && dash < raw.size() - 1) {
        const QString head = raw.left(dash);
        const QString tail = raw.mid(dash + 1);
        bool numeric = !tail.isEmpty();
        for (const QChar c : tail) {
            if (!c.isDigit()) {
                numeric = false;
                break;
            }
        }
        if (numeric && tail.size() < 4) {
            candidates << QStringLiteral("%1-%2").arg(
                head.toUpper(), QStringLiteral("%1").arg(tail.toInt(), 4, 10, QLatin1Char('0')));
        }
    }
    candidates.removeDuplicates();

    QStringList placeholders;
    for (int i = 0; i < candidates.size(); ++i) {
        placeholders << QStringLiteral("?");
    }

    QSqlQuery q(m_db);
    q.prepare(
        QStringLiteral(
            "SELECT s.id, s.receipt_number, s.sold_at, s.payment_mode, s.status, s.grand_total, "
            "       COALESCE(u.full_name, '') "
            "  FROM sales s LEFT JOIN users u ON u.id = s.cashier_id "
            " WHERE UPPER(s.receipt_number) IN (%1) LIMIT 1")
            .arg(placeholders.join(QStringLiteral(","))));
    for (const QString &c : candidates) {
        q.addBindValue(c);
    }
    if (!q.exec() || !q.next()) {
        return hdr;
    }

    hdr.found = true;
    hdr.saleId = q.value(0).toLongLong();
    hdr.receiptNumber = q.value(1).toString();
    hdr.soldAt = q.value(2).toString();
    hdr.paymentMode = q.value(3).toString();
    hdr.status = q.value(4).toString();
    hdr.grandTotal = q.value(5).toString();
    hdr.cashierName = q.value(6).toString();

    QSqlQuery iq(m_db);
    iq.prepare(QStringLiteral(
        "SELECT si.id, si.medicine_id, si.batch_id, si.qty_in_base_units, si.sold_unit_label, "
        "       si.qty_sold_display, si.unit_mrp, si.line_total, "
        "       m.brand_name, COALESCE(m.strength, ''), COALESCE(b.batch_number, ''), "
        "       (SELECT COALESCE(SUM(r.qty_returned_units), 0) FROM returns r "
        "          WHERE r.sale_item_id = si.id) "
        "  FROM sale_items si "
        "  JOIN medicines m ON m.id = si.medicine_id "
        "  LEFT JOIN batches b ON b.id = si.batch_id "
        " WHERE si.sale_id = ? ORDER BY si.id"));
    iq.addBindValue(hdr.saleId);
    if (iq.exec()) {
        while (iq.next()) {
            ReturnableLine l;
            l.saleItemId = iq.value(0).toLongLong();
            l.medicineId = iq.value(1).toLongLong();
            l.batchId = iq.value(2).toLongLong();
            l.qtySold = iq.value(3).toInt();
            l.soldUnitLabel = iq.value(4).toString();
            l.qtySoldDisplay = iq.value(5).toInt();
            l.unitMrp = iq.value(6).toString();
            l.lineTotal = iq.value(7).toString();
            const QString brand = iq.value(8).toString();
            const QString strength = iq.value(9).toString();
            l.name = strength.isEmpty() ? brand : (brand + QLatin1Char(' ') + strength);
            l.batchNumber = iq.value(10).toString();
            l.qtyAlreadyReturned = iq.value(11).toInt();
            hdr.lines.push_back(l);
        }
    }
    return hdr;
}

QVector<ReturnRow> ReturnRepository::listRecent(int limit) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT r.id, r.return_number, COALESCE(s.receipt_number, ''), "
        "       m.brand_name, COALESCE(m.strength, ''), "
        "       r.qty_returned_units, r.refund_amount, r.reason, r.status, r.created_at "
        "  FROM returns r "
        "  LEFT JOIN sales s ON s.id = r.original_sale_id "
        "  JOIN medicines m ON m.id = r.medicine_id "
        " ORDER BY r.id DESC LIMIT ?"));
    q.addBindValue(limit);

    QVector<ReturnRow> out;
    if (q.exec()) {
        while (q.next()) {
            ReturnRow r;
            r.id = q.value(0).toLongLong();
            r.returnNumber = q.value(1).toString();
            r.receiptNumber = q.value(2).toString();
            const QString brand = q.value(3).toString();
            const QString strength = q.value(4).toString();
            r.medicineName = strength.isEmpty() ? brand : (brand + QLatin1Char(' ') + strength);
            r.qtyReturned = q.value(5).toInt();
            r.refundAmount = q.value(6).toString();
            r.reason = q.value(7).toString();
            r.status = q.value(8).toString();
            r.createdAt = q.value(9).toString();
            out.push_back(r);
        }
    }
    return out;
}

QVector<PendingReturn> ReturnRepository::listPending() const
{
    QVector<PendingReturn> out;
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral(
            "SELECT r.id, r.return_number, s.receipt_number, "
            "       m.brand_name || COALESCE(' ' || m.strength, ''), b.batch_number, "
            "       r.qty_returned_units, r.refund_amount, r.reason, "
            "       COALESCE(r.physical_condition,''), COALESCE(u.full_name,''), r.created_at "
            "  FROM returns r "
            "  JOIN sales s ON s.id = r.original_sale_id "
            "  JOIN medicines m ON m.id = r.medicine_id "
            "  JOIN batches b ON b.id = r.batch_id "
            "  LEFT JOIN users u ON u.id = r.initiated_by "
            " WHERE r.status = 'PENDING_REVIEW' "
            " ORDER BY r.id ASC"))) {
        while (q.next()) {
            PendingReturn r;
            r.id = q.value(0).toLongLong();
            r.returnNumber = q.value(1).toString();
            r.receiptNumber = q.value(2).toString();
            r.medicineName = q.value(3).toString();
            r.batchNumber = q.value(4).toString();
            r.qtyReturned = q.value(5).toInt();
            r.refundAmount = q.value(6).toString();
            r.reason = q.value(7).toString();
            r.physicalCondition = q.value(8).toString();
            r.initiatedBy = q.value(9).toString();
            r.createdAt = q.value(10).toString();
            out.push_back(r);
        }
    }
    return out;
}

QVector<SupplierReturn> ReturnRepository::listSupplierReturns() const
{
    QVector<SupplierReturn> out;
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral(
            "SELECT r.id, r.return_number, "
            "       m.brand_name || COALESCE(' ' || m.strength, ''), "
            "       r.qty_returned_units, r.refund_amount, COALESCE(r.adjudicated_at,''), "
            "       r.supplier_settled_at, COALESCE(r.supplier_reference,'') "
            "  FROM returns r "
            "  JOIN medicines m ON m.id = r.medicine_id "
            " WHERE r.status = 'RETURN_TO_SUPPLIER' "
            " ORDER BY (r.supplier_settled_at IS NOT NULL), r.id DESC"))) {
        while (q.next()) {
            SupplierReturn r;
            r.id = q.value(0).toLongLong();
            r.returnNumber = q.value(1).toString();
            r.medicineName = q.value(2).toString();
            r.qtyReturned = q.value(3).toInt();
            r.refundAmount = q.value(4).toString();
            r.adjudicatedAt = q.value(5).toString();
            r.settled = !q.value(6).isNull();
            r.settledAt = q.value(6).toString();
            r.supplierReference = q.value(7).toString();
            out.push_back(r);
        }
    }
    return out;
}
