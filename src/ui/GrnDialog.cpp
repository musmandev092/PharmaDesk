#include "ui/GrnDialog.h"

#include "data/SupplierRepository.h"
#include "domain/CostBlender.h"
#include "domain/Money.h"
#include "ui/GrnLineDialog.h"
#include "ui/UiUtil.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

GrnDialog::GrnDialog(QSqlDatabase db, qint64 userId, QWidget *parent)
    : QDialog(parent), m_db(std::move(db)), m_userId(userId)
{
    setWindowTitle(QStringLiteral("New goods-receipt note"));
    setModal(true);
    setMinimumWidth(560);
    resize(620, 560);

    m_supplier = new QComboBox(this);
    SupplierRepository srepo(m_db);
    for (const SupplierRow &s : srepo.list()) {
        if (s.isActive) {
            m_supplier->addItem(s.name, s.id);
        }
    }

    m_invoiceNumber = new QLineEdit(this);
    m_invoiceNumber->setMaxLength(60);
    m_invoiceDate = new QDateEdit(this);
    m_invoiceDate->setCalendarPopup(true);
    m_invoiceDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_invoiceDate->setDate(QDate::currentDate());

    auto *form = new QFormLayout;
    form->setSpacing(8);
    form->addRow(QStringLiteral("Supplier *"), m_supplier);
    form->addRow(QStringLiteral("Invoice number"), m_invoiceNumber);
    form->addRow(QStringLiteral("Invoice date"), m_invoiceDate);

    m_lines = new QTableWidget(this);
    m_lines->setColumnCount(6);
    m_lines->setHorizontalHeaderLabels({QStringLiteral("Medicine"), QStringLiteral("Batch"),
                                        QStringLiteral("Expiry"), QStringLiteral("Paid+Free"),
                                        QStringLiteral("Cost/pack"), QStringLiteral("Line total")});
    // Match header alignment to the column data: keep Medicine/Batch/Expiry
    // left, right-align the numeric/money headers (Paid+Free, Cost/pack, Line
    // total) so they line up with their right-aligned cells.
    UiUtil::rightAlign(m_lines->horizontalHeaderItem(3));
    UiUtil::rightAlign(m_lines->horizontalHeaderItem(4));
    UiUtil::rightAlign(m_lines->horizontalHeaderItem(5));
    m_lines->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_lines->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_lines->verticalHeader()->setVisible(false);
    m_lines->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    auto *addBtn = new QPushButton(QStringLiteral("Add line"), this);
    auto *rmBtn = new QPushButton(QStringLiteral("Remove line"), this);
    rmBtn->setProperty("variant", "secondary");
    m_subtotal = new QLabel(QStringLiteral("Subtotal: PKR 0.00"), this);
    m_subtotal->setObjectName(QStringLiteral("h2"));

    auto *lineBtns = new QHBoxLayout;
    lineBtns->setSpacing(8);
    lineBtns->addWidget(addBtn);
    lineBtns->addWidget(rmBtn);
    lineBtns->addStretch();
    lineBtns->addWidget(m_subtotal);

    auto *buttons = new QDialogButtonBox(this);
    auto *postBtn = buttons->addButton(QStringLiteral("Post GRN"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addLayout(form);
    layout->addWidget(m_lines, 1);
    layout->addLayout(lineBtns);
    layout->addWidget(buttons);

    connect(addBtn, &QPushButton::clicked, this, &GrnDialog::addLine);
    connect(rmBtn, &QPushButton::clicked, this, &GrnDialog::removeLine);
    connect(postBtn, &QPushButton::clicked, this, &GrnDialog::postGrn);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (m_supplier->count() == 0) {
        m_supplier->setEnabled(false);
        m_supplier->addItem(QStringLiteral("(add a supplier first)"), QVariant());
    }
    renderLines();

    // Critical for scanners: never let a stray Enter post the GRN. Posting must
    // be an explicit click on "Post GRN".
    UiUtil::makeScanSafe(this);
}

void GrnDialog::addLine()
{
    GrnLineDialog dlg(m_db, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_lineInputs.push_back(dlg.line());
        m_lineNames.push_back(dlg.medicineName());
        renderLines();
    }
}

void GrnDialog::removeLine()
{
    const int row = m_lines->currentRow();
    if (row >= 0 && row < m_lineInputs.size()) {
        m_lineInputs.remove(row);
        m_lineNames.removeAt(row);
        renderLines();
    }
}

void GrnDialog::renderLines()
{
    UiUtil::beginFill(m_lines);
    m_lines->setRowCount(m_lineInputs.size());
    Money subtotal;
    for (int i = 0; i < m_lineInputs.size(); ++i) {
        const GrnLineInput &l = m_lineInputs.at(i);
        const QString lineTotal = CostBlender::lineTotal(l.paidQty, l.unitCost);
        subtotal = subtotal + Money::fromString(lineTotal);
        m_lines->setItem(i, 0, new QTableWidgetItem(m_lineNames.value(i)));
        m_lines->setItem(i, 1, new QTableWidgetItem(l.batchNumber));

        auto *expiryItem = new QTableWidgetItem(l.expiry.toString(Qt::ISODate));
        UiUtil::rightAlign(expiryItem);
        m_lines->setItem(i, 2, expiryItem);

        auto *qtyItem
            = new QTableWidgetItem(QStringLiteral("%1 + %2").arg(l.paidQty).arg(l.focQty));
        UiUtil::rightAlign(qtyItem);
        m_lines->setItem(i, 3, qtyItem);

        auto *costItem = new QTableWidgetItem(Money::fromString(l.unitCost).fmt(Money::ScaleCost));
        UiUtil::rightAlign(costItem);
        m_lines->setItem(i, 4, costItem);

        auto *totalItem = new QTableWidgetItem(Money::fromString(lineTotal).fmt());
        UiUtil::rightAlign(totalItem);
        m_lines->setItem(i, 5, totalItem);
    }
    if (m_lineInputs.isEmpty()) {
        UiUtil::emptyState(m_lines,
                           QStringLiteral("No lines yet. Use \"Add line\" to receive stock."));
    }
    m_subtotal->setText(QStringLiteral("Subtotal: PKR %1").arg(subtotal.fmt()));
}

void GrnDialog::postGrn()
{
    if (!m_supplier->isEnabled() || !m_supplier->currentData().isValid()) {
        QMessageBox::warning(this, windowTitle(), QStringLiteral("Choose a supplier."));
        return;
    }
    if (m_lineInputs.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), QStringLiteral("Add at least one line."));
        return;
    }

    GrnService service(m_db, m_userId);
    const GrnResult r = service.post(m_supplier->currentData().toLongLong(), m_lineInputs,
                                     m_invoiceNumber->text().trimmed(), m_invoiceDate->date());
    if (!r.ok) {
        QMessageBox::warning(this, windowTitle(), r.error);
        return;
    }
    QMessageBox::information(this, windowTitle(),
                             QStringLiteral("Posted %1 — subtotal PKR %2.\nStock updated.")
                                 .arg(r.grnNumber, Money::fromString(r.subtotal).fmt()));
    QDialog::accept();
}
