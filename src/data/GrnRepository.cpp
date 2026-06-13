#include "data/GrnRepository.h"

#include <QSqlQuery>

QVector<GrnDocRow> GrnRepository::list(int limit) const
{
    QVector<GrnDocRow> out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT g.id, g.grn_number, COALESCE(s.name,''), COALESCE(g.invoice_number,''), "
        "       COALESCE(g.posted_at, g.created_at), g.grand_total, g.status, "
        "       (SELECT count(*) FROM grn_lines l WHERE l.grn_id = g.id) "
        "  FROM grn_documents g LEFT JOIN suppliers s ON s.id = g.supplier_id "
        " ORDER BY g.id DESC LIMIT ?"));
    q.addBindValue(limit);
    if (q.exec()) {
        while (q.next()) {
            GrnDocRow r;
            r.id = q.value(0).toLongLong();
            r.grnNumber = q.value(1).toString();
            r.supplierName = q.value(2).toString();
            r.invoiceNumber = q.value(3).toString();
            r.postedAt = q.value(4).toString();
            r.grandTotal = q.value(5).toString();
            r.status = q.value(6).toString();
            r.lineCount = q.value(7).toInt();
            out.push_back(r);
        }
    }
    return out;
}
