#include "data/SaleRepository.h"

#include <QSqlQuery>

SaleResult SaleRepository::loadReceipt(qint64 saleId) const
{
    SaleResult res;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT receipt_number, subtotal, discount_total, grand_total, amount_tendered, "
        "       change_returned, payment_mode FROM sales WHERE id = ?"));
    q.addBindValue(saleId);
    if (!q.exec() || !q.next()) {
        res.ok = false;
        res.error = QStringLiteral("Sale not found.");
        return res;
    }
    res.saleId = saleId;
    res.receiptNumber = q.value(0).toString();
    res.subtotal = q.value(1).toString();
    res.discountTotal = q.value(2).toString();
    res.grandTotal = q.value(3).toString();
    res.amountTendered = q.value(4).toString();
    res.changeReturned = q.value(5).toString();
    res.paymentMode = q.value(6).toString();

    // Group sale_items by medicine for a readable receipt (a single cart line
    // may have been split across batches by FEFO).
    QSqlQuery iq(m_db);
    iq.prepare(QStringLiteral(
        "SELECT m.brand_name, COALESCE(m.strength,''), si.sold_unit_label, si.unit_mrp, "
        "       SUM(si.qty_sold_display), SUM(si.line_total) "
        "  FROM sale_items si JOIN medicines m ON m.id = si.medicine_id "
        " WHERE si.sale_id = ? "
        " GROUP BY si.medicine_id, si.sold_unit_label, si.unit_mrp "
        " ORDER BY m.brand_name"));
    iq.addBindValue(saleId);
    if (iq.exec()) {
        while (iq.next()) {
            SaleResultLine l;
            const QString brand = iq.value(0).toString();
            const QString strength = iq.value(1).toString();
            l.name = strength.isEmpty() ? brand : (brand + QLatin1Char(' ') + strength);
            l.unitLabel = iq.value(2).toString();
            l.unitMrp = iq.value(3).toString();
            l.qty = iq.value(4).toInt();
            l.lineTotal = iq.value(5).toString();
            res.lines.push_back(l);
        }
    }

    res.ok = true;
    return res;
}
