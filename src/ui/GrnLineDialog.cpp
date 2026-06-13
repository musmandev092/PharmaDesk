#include "ui/GrnLineDialog.h"

#include "data/MedicineRepository.h"
#include "domain/CostBlender.h"
#include "domain/Money.h"
#include "ui/UiUtil.h"

#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

GrnLineDialog::GrnLineDialog(QSqlDatabase db, QWidget *parent)
    : QDialog(parent), m_db(std::move(db))
{
    setWindowTitle(QStringLiteral("Add GRN line"));
    setModal(true);
    setMinimumWidth(460);
    resize(460, 620);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("Search medicine to receive"));
    m_search->setClearButtonEnabled(true);

    m_results = new QTableWidget(this);
    m_results->setColumnCount(2);
    m_results->setHorizontalHeaderLabels({QStringLiteral("Medicine"), QStringLiteral("Pack")});
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_results->setSelectionMode(QAbstractItemView::SingleSelection);
    m_results->verticalHeader()->setVisible(false);
    m_results->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_results->setFixedHeight(160);

    m_picked = new QLabel(QStringLiteral("No medicine selected."), this);
    m_picked->setObjectName(QStringLiteral("muted"));

    m_batch = new QLineEdit(this);
    m_batch->setMaxLength(50);

    m_expiry = new QDateEdit(this);
    m_expiry->setCalendarPopup(true);
    m_expiry->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_expiry->setDate(QDate::currentDate().addYears(1));

    m_paid = new QSpinBox(this);
    m_paid->setRange(1, 1000000);
    m_paid->setSuffix(QStringLiteral(" packs"));
    m_foc = new QSpinBox(this);
    m_foc->setRange(0, 1000000);
    m_foc->setSuffix(QStringLiteral(" free"));

    m_cost = new QLineEdit(this);
    m_cost->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^\\d{1,9}(\\.\\d{1,4})?$")), m_cost));
    m_cost->setPlaceholderText(QStringLiteral("Cost per pack"));

    m_mrp = new QLineEdit(this);
    m_mrp->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^\\d{1,9}(\\.\\d{1,2})?$")), m_mrp));
    m_mrp->setPlaceholderText(QStringLiteral("MRP per pack"));

    m_results->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    m_preview = new QLabel(this);
    m_preview->setObjectName(QStringLiteral("muted"));
    m_preview->setWordWrap(true);

    auto *form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(8);
    form->addRow(QStringLiteral("Batch number *"), m_batch);
    form->addRow(QStringLiteral("Expiry *"), m_expiry);
    form->addRow(QStringLiteral("Paid qty *"), m_paid);
    form->addRow(QStringLiteral("Free qty"), m_foc);
    form->addRow(QStringLiteral("Cost / pack *"), m_cost);
    form->addRow(QStringLiteral("MRP / pack *"), m_mrp);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    // Force footer order regardless of platform style: Cancel (secondary) on the
    // left, primary OK/Add right-most.
    if (auto *boxLayout = qobject_cast<QHBoxLayout *>(buttons->layout())) {
        boxLayout->setDirection(QBoxLayout::LeftToRight);
        QPushButton *cancelBtn = buttons->button(QDialogButtonBox::Cancel);
        QPushButton *okBtn = buttons->button(QDialogButtonBox::Ok);
        boxLayout->removeWidget(cancelBtn);
        boxLayout->removeWidget(okBtn);
        for (int i = boxLayout->count() - 1; i >= 0; --i) {
            if (boxLayout->itemAt(i)->spacerItem()) {
                QLayoutItem *item = boxLayout->takeAt(i);
                delete item;
            }
        }
        boxLayout->addStretch(1);
        boxLayout->addWidget(cancelBtn);
        boxLayout->addWidget(okBtn);
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &GrnLineDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);
    layout->addWidget(m_search);
    layout->addWidget(m_results);
    layout->addWidget(m_picked);
    layout->addLayout(form);
    layout->addWidget(m_preview);
    layout->addWidget(buttons);

    connect(m_search, &QLineEdit::textChanged, this, &GrnLineDialog::runSearch);
    connect(m_results, &QTableWidget::itemSelectionChanged, this, &GrnLineDialog::pickResult);
    for (QLineEdit *e : {m_cost, m_mrp}) {
        connect(e, &QLineEdit::textChanged, this, &GrnLineDialog::updatePreview);
    }
    connect(m_paid, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &GrnLineDialog::updatePreview);
    connect(m_foc, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &GrnLineDialog::updatePreview);
    updatePreview();

    // A barcode scanner types the code then sends Enter. Make that Enter advance
    // focus instead of triggering OK, so a scan never closes the line half-filled.
    UiUtil::makeScanSafe(this);
}

void GrnLineDialog::runSearch()
{
    MedicineRepository repo(m_db);
    const QVector<MedicineRow> rows = repo.list(m_search->text());
    UiUtil::beginFill(m_results);
    m_results->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const MedicineRow &r = rows.at(i);
        auto *name = new QTableWidgetItem(
            QStringLiteral("%1 %2\n%3").arg(r.brandName, r.strength, r.genericName));
        name->setData(Qt::UserRole, r.id);
        name->setData(Qt::UserRole + 1, r.brandName + QLatin1Char(' ') + r.strength);
        name->setData(Qt::UserRole + 2, r.unitsPerPurchase);
        m_results->setItem(i, 0, name);
        auto *pack = new QTableWidgetItem(
            QStringLiteral("%1 %2 / %3").arg(r.unitsPerPurchase).arg(r.baseUnit, r.purchaseUnit));
        UiUtil::rightAlign(pack);
        m_results->setItem(i, 1, pack);
    }
    m_results->resizeRowsToContents();
    if (rows.isEmpty()) {
        UiUtil::emptyState(m_results, m_search->text().trimmed().isEmpty()
                                          ? QStringLiteral("Search for a medicine to receive.")
                                          : QStringLiteral("No matching medicines."));
    }

    // Scanner-friendly: a scanned barcode usually matches exactly one medicine —
    // select it automatically so the operator can go straight to batch entry.
    if (rows.size() == 1) {
        m_results->selectRow(0);
    }
}

void GrnLineDialog::pickResult()
{
    const int row = m_results->currentRow();
    if (row < 0 || !m_results->item(row, 0)) {
        return;
    }
    QTableWidgetItem *it = m_results->item(row, 0);
    m_line.medicineId = it->data(Qt::UserRole).toLongLong();
    m_medicineName = it->data(Qt::UserRole + 1).toString();
    m_unitsPerPurchase = it->data(Qt::UserRole + 2).toInt();
    m_picked->setText(QStringLiteral("Selected: %1  (%2 base units per pack)")
                          .arg(m_medicineName)
                          .arg(m_unitsPerPurchase));
    updatePreview();
}

void GrnLineDialog::updatePreview()
{
    if (m_line.medicineId == 0 || m_cost->text().trimmed().isEmpty()) {
        m_preview->setText(
            QStringLiteral("Pick a medicine and enter cost to preview the blended cost."));
        return;
    }
    const int totalBase = (m_paid->value() + m_foc->value()) * m_unitsPerPurchase;
    const QString blended = CostBlender::blendedCostPerBaseUnit(m_paid->value(), m_foc->value(),
                                                                m_cost->text(), m_unitsPerPurchase);
    const QString lineTotal = CostBlender::lineTotal(m_paid->value(), m_cost->text());
    QString mrpBase = m_mrp->text().trimmed().isEmpty()
                          ? QStringLiteral("—")
                          : CostBlender::mrpPerBaseUnit(m_mrp->text(), m_unitsPerPurchase);
    m_preview->setText(
        QStringLiteral(
            "Receives %1 base units · blended cost/unit %2 · MRP/unit %3 · line total PKR %4")
            .arg(totalBase)
            .arg(blended, mrpBase, Money::fromString(lineTotal).fmt()));
}

void GrnLineDialog::accept()
{
    auto fail = [this](const QString &m) { QMessageBox::warning(this, windowTitle(), m); };

    if (m_line.medicineId == 0) {
        return fail(QStringLiteral("Select a medicine."));
    }
    m_line.batchNumber = m_batch->text().trimmed();
    m_line.expiry = m_expiry->date();
    m_line.paidQty = m_paid->value();
    m_line.focQty = m_foc->value();
    m_line.unitCost = m_cost->text().trimmed();
    m_line.mrpPerPurchaseUnit = m_mrp->text().trimmed();

    if (m_line.batchNumber.isEmpty()) {
        return fail(QStringLiteral("Batch number is required."));
    }
    if (!m_line.expiry.isValid() || m_line.expiry < QDate::currentDate()) {
        return fail(QStringLiteral("Expiry cannot be in the past."));
    }
    if (m_line.unitCost.isEmpty() || Money::fromString(m_line.unitCost).compare(Money()) <= 0) {
        return fail(QStringLiteral("Cost per pack must be greater than zero."));
    }
    if (m_line.mrpPerPurchaseUnit.isEmpty()
        || Money::fromString(m_line.mrpPerPurchaseUnit).compare(Money()) <= 0) {
        return fail(QStringLiteral("MRP per pack must be greater than zero."));
    }
    QDialog::accept();
}
