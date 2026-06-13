#include "ui/GrnDetailDialog.h"

#include "domain/Money.h"
#include "ui/UiUtil.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QSqlQuery>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

struct GrnHeader
{
    bool found = false;
    QString grnNumber;
    QString supplierName;
    QString invoiceNumber;
    QString invoiceDate;
    QString postedAt;
    QString status;
    QString subtotal;
    QString discountTotal;
    QString taxTotal;
    QString grandTotal;
};

struct GrnLine
{
    QString brandName;
    QString genericName;
    QString baseUnit;
    QString batchNumber;
    QString expiryDate;
    int paidQty = 0;
    int focQty = 0;
    int qtyBaseUnits = 0;
    QString unitCost;
    QString mrpPerBaseUnit;
    QString lineTotal;
};

// Self-contained loaders (GrnRepository deliberately not modified).
GrnHeader loadHeader(const QSqlDatabase &db, qint64 grnId)
{
    GrnHeader h;
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT g.grn_number, COALESCE(s.name,''), COALESCE(g.invoice_number,''), "
        "       COALESCE(g.invoice_date,''), COALESCE(g.posted_at, g.created_at), g.status, "
        "       g.subtotal, g.discount_total, g.tax_total, g.grand_total "
        "  FROM grn_documents g LEFT JOIN suppliers s ON s.id = g.supplier_id "
        " WHERE g.id = ?"));
    q.addBindValue(grnId);
    if (q.exec() && q.next()) {
        h.found = true;
        h.grnNumber = q.value(0).toString();
        h.supplierName = q.value(1).toString();
        h.invoiceNumber = q.value(2).toString();
        h.invoiceDate = q.value(3).toString();
        h.postedAt = q.value(4).toString();
        h.status = q.value(5).toString();
        h.subtotal = q.value(6).toString();
        h.discountTotal = q.value(7).toString();
        h.taxTotal = q.value(8).toString();
        h.grandTotal = q.value(9).toString();
    }
    return h;
}

QVector<GrnLine> loadLines(const QSqlDatabase &db, qint64 grnId)
{
    QVector<GrnLine> out;
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT m.brand_name, m.generic_name, m.base_unit, l.batch_number, l.expiry_date, "
        "       l.paid_qty, l.foc_qty, l.qty_in_base_units, l.unit_cost, "
        "       l.mrp_per_base_unit, l.line_total "
        "  FROM grn_lines l JOIN medicines m ON m.id = l.medicine_id "
        " WHERE l.grn_id = ? ORDER BY l.id ASC"));
    q.addBindValue(grnId);
    if (q.exec()) {
        while (q.next()) {
            GrnLine r;
            r.brandName = q.value(0).toString();
            r.genericName = q.value(1).toString();
            r.baseUnit = q.value(2).toString();
            r.batchNumber = q.value(3).toString();
            r.expiryDate = q.value(4).toString();
            r.paidQty = q.value(5).toInt();
            r.focQty = q.value(6).toInt();
            r.qtyBaseUnits = q.value(7).toInt();
            r.unitCost = q.value(8).toString();
            r.mrpPerBaseUnit = q.value(9).toString();
            r.lineTotal = q.value(10).toString();
            out.push_back(r);
        }
    }
    return out;
}

} // namespace

GrnDetailDialog::GrnDetailDialog(QSqlDatabase db, qint64 grnId, QWidget *parent)
    : QDialog(parent), m_db(std::move(db)), m_grnId(grnId)
{
    setWindowTitle(QStringLiteral("Goods receipt"));
    setMinimumWidth(560);
    resize(900, 560);

    m_header = new QLabel(this);
    m_header->setObjectName(QStringLiteral("h1"));
    m_meta = new QLabel(this);
    m_meta->setObjectName(QStringLiteral("muted"));
    m_meta->setTextFormat(Qt::RichText);
    m_totals = new QLabel(this);
    m_totals->setObjectName(QStringLiteral("h2"));
    m_totals->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto *headRow = new QHBoxLayout;
    headRow->setSpacing(16);
    auto *headLeft = new QVBoxLayout;
    headLeft->setSpacing(8);
    headLeft->addWidget(m_header);
    headLeft->addWidget(m_meta);
    headRow->addLayout(headLeft, 1);
    headRow->addWidget(m_totals);

    m_lines = new QTableWidget(this);
    m_lines->setColumnCount(8);
    m_lines->setHorizontalHeaderLabels({QStringLiteral("Medicine"), QStringLiteral("Batch"),
                                        QStringLiteral("Expiry"), QStringLiteral("Paid + Free"),
                                        QStringLiteral("Qty (base)"), QStringLiteral("Unit cost"),
                                        QStringLiteral("MRP/base"), QStringLiteral("Line total")});
    m_lines->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_lines->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_lines->setSelectionMode(QAbstractItemView::SingleSelection);
    m_lines->verticalHeader()->setVisible(false);
    m_lines->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_lines->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_lines->setWordWrap(false);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addLayout(headRow);
    layout->addWidget(m_lines, 1);
    layout->addWidget(buttons);

    load();
}

void GrnDetailDialog::load()
{
    const GrnHeader h = loadHeader(m_db, m_grnId);
    if (!h.found) {
        m_header->setText(QStringLiteral("GRN not found"));
        return;
    }

    m_header->setText(QStringLiteral("GRN %1").arg(h.grnNumber));

    QStringList meta;
    meta << QStringLiteral("<b>Supplier:</b> %1").arg(h.supplierName.toHtmlEscaped());
    if (!h.invoiceNumber.isEmpty()) {
        meta << QStringLiteral("<b>Invoice:</b> %1").arg(h.invoiceNumber.toHtmlEscaped());
    }
    if (!h.invoiceDate.isEmpty()) {
        meta << QStringLiteral("<b>Invoice date:</b> %1").arg(h.invoiceDate.left(10));
    }
    meta << QStringLiteral("<b>Posted:</b> %1").arg(h.postedAt.left(19));
    meta << QStringLiteral("<b>Status:</b> %1").arg(h.status);
    m_meta->setText(meta.join(QStringLiteral(" &nbsp;·&nbsp; ")));

    const Money sub = Money::fromString(h.subtotal);
    const Money disc = Money::fromString(h.discountTotal);
    const Money tax = Money::fromString(h.taxTotal);
    const Money grand = Money::fromString(h.grandTotal);
    m_totals->setText(
        QStringLiteral("Subtotal PKR %1\nDiscount PKR %2\nTax PKR %3\nGrand total PKR %4")
            .arg(sub.fmt(), disc.fmt(), tax.fmt(), grand.fmt()));

    const QVector<GrnLine> lines = loadLines(m_db, m_grnId);
    UiUtil::beginFill(m_lines);
    m_lines->setRowCount(lines.size());
    for (int i = 0; i < lines.size(); ++i) {
        const GrnLine &r = lines.at(i);
        QString med = r.brandName;
        if (!r.genericName.isEmpty()) med += QStringLiteral(" — %1").arg(r.genericName);
        m_lines->setItem(i, 0, new QTableWidgetItem(med));
        m_lines->setItem(i, 1, new QTableWidgetItem(r.batchNumber));
        m_lines->setItem(i, 2, new QTableWidgetItem(r.expiryDate.left(10)));

        auto *pf = new QTableWidgetItem(r.focQty > 0
                                            ? QStringLiteral("%1 + %2").arg(r.paidQty).arg(r.focQty)
                                            : QString::number(r.paidQty));
        UiUtil::rightAlign(pf);
        m_lines->setItem(i, 3, pf);

        auto *qty
            = new QTableWidgetItem(QStringLiteral("%1 %2").arg(r.qtyBaseUnits).arg(r.baseUnit));
        UiUtil::rightAlign(qty);
        m_lines->setItem(i, 4, qty);

        auto *cost = new QTableWidgetItem(Money::fromString(r.unitCost).display(Money::ScaleCost));
        UiUtil::rightAlign(cost);
        m_lines->setItem(i, 5, cost);

        auto *mrp
            = new QTableWidgetItem(Money::fromString(r.mrpPerBaseUnit).display(Money::ScaleCost));
        UiUtil::rightAlign(mrp);
        m_lines->setItem(i, 6, mrp);

        auto *total = new QTableWidgetItem(Money::fromString(r.lineTotal).display());
        UiUtil::rightAlign(total);
        m_lines->setItem(i, 7, total);
    }
    UiUtil::emptyState(m_lines, QStringLiteral("This GRN has no line items."));
}
