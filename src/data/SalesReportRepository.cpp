#include "data/SalesReportRepository.h"

#include "domain/Money.h"

#include <QFile>
#include <QSqlQuery>
#include <QTextStream>

static const QString kDateFmt = QStringLiteral("yyyy-MM-dd");

SalesReport SalesReportRepository::report(const QDate &from, const QDate &to) const
{
    SalesReport out;
    const QString f = from.toString(kDateFmt);
    const QString t = to.toString(kDateFmt);

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT s.id, s.receipt_number, s.sold_at, s.payment_mode, s.status, "
        "       s.subtotal, s.discount_total, s.grand_total, COALESCE(u.full_name,'') "
        "  FROM sales s LEFT JOIN users u ON u.id = s.cashier_id "
        " WHERE date(s.sold_at, 'localtime') BETWEEN ? AND ? "
        " ORDER BY s.sold_at DESC LIMIT 2000"));
    q.addBindValue(f);
    q.addBindValue(t);

    Money gross, cash, card;
    if (q.exec()) {
        while (q.next()) {
            SalesReportRow r;
            r.saleId = q.value(0).toLongLong();
            r.receiptNumber = q.value(1).toString();
            r.soldAt = q.value(2).toString().left(19);
            r.paymentMode = q.value(3).toString();
            r.status = q.value(4).toString();
            r.subtotal = q.value(5).toString();
            r.discountTotal = q.value(6).toString();
            r.grandTotal = q.value(7).toString();
            r.cashierName = q.value(8).toString();
            out.rows.push_back(r);

            if (r.status == QLatin1String("VOIDED")) {
                continue;
            }
            const Money g = Money::fromString(r.grandTotal);
            gross = gross + g;
            if (r.paymentMode == QLatin1String("CASH"))
                cash = cash + g;
            else if (r.paymentMode == QLatin1String("CARD"))
                card = card + g;
        }
    }

    // Refunds in range, bucketed by the original sale's payment mode.
    Money refunds, refundsCash, refundsCard;
    int refundCount = 0;
    QSqlQuery rq(m_db);
    rq.prepare(QStringLiteral("SELECT r.refund_amount, s.payment_mode FROM returns r "
                              "JOIN sales s ON s.id = r.original_sale_id "
                              "WHERE date(r.created_at, 'localtime') BETWEEN ? AND ?"));
    rq.addBindValue(f);
    rq.addBindValue(t);
    if (rq.exec()) {
        while (rq.next()) {
            const Money amt = Money::fromString(rq.value(0).toString());
            const QString mode = rq.value(1).toString();
            refunds = refunds + amt;
            if (mode == QLatin1String("CASH"))
                refundsCash = refundsCash + amt;
            else if (mode == QLatin1String("CARD"))
                refundsCard = refundsCard + amt;
            ++refundCount;
        }
    }

    SalesSummary &s = out.summary;
    s.count = out.rows.size();
    s.gross = gross.toString();
    s.cash = cash.toString();
    s.card = card.toString();
    s.refunds = refunds.toString();
    s.refundCount = refundCount;
    s.net = (gross - refunds).toString();
    s.netCash = (cash - refundsCash).toString();
    s.netCard = (card - refundsCard).toString();
    return out;
}

bool SalesReportRepository::exportCsv(const SalesReport &report, const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&f);
    auto field = [](QString v) {
        if (v.contains(QLatin1Char(',')) || v.contains(QLatin1Char('"'))
            || v.contains(QLatin1Char('\n'))) {
            v.replace(QLatin1Char('"'), QStringLiteral("\"\""));
            return QStringLiteral("\"%1\"").arg(v);
        }
        return v;
    };
    out << "Receipt #,When,Cashier,Mode,Status,Subtotal,Discount,Grand total\n";
    for (const SalesReportRow &r : report.rows) {
        out << field(r.receiptNumber) << ',' << field(r.soldAt) << ',' << field(r.cashierName)
            << ',' << field(r.paymentMode) << ',' << field(r.status) << ',' << field(r.subtotal)
            << ',' << field(r.discountTotal) << ',' << field(r.grandTotal) << '\n';
    }
    return true;
}
