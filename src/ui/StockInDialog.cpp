#include "ui/StockInDialog.h"

#include "data/BatchRepository.h"
#include "domain/Money.h"
#include "ui/UiUtil.h"

#include <QDateEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QVBoxLayout>

StockInDialog::StockInDialog(BatchRepository *repo, qint64 userId, qint64 medicineId,
                             const QString &medicineName, QWidget *parent)
    : QDialog(parent), m_repo(repo), m_userId(userId), m_medicineId(medicineId)
{
    setWindowTitle(QStringLiteral("Add stock"));
    setModal(true);
    setMinimumWidth(420);

    auto *heading = new QLabel(medicineName, this);
    heading->setObjectName(QStringLiteral("h2"));

    m_batchNumber = new QLineEdit(this);
    m_batchNumber->setMaxLength(50);

    m_expiry = new QDateEdit(this);
    m_expiry->setCalendarPopup(true);
    m_expiry->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_expiry->setDate(QDate::currentDate().addYears(1));

    m_qty = new QSpinBox(this);
    m_qty->setRange(1, 1000000);
    m_qty->setSuffix(QStringLiteral(" base units"));

    // Decimal validators: cost up to 4 dp, MRP up to 2 dp.
    m_cost = new QLineEdit(this);
    m_cost->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^\\d{1,9}(\\.\\d{1,4})?$")), m_cost));
    m_cost->setPlaceholderText(QStringLiteral("e.g. 1.2500"));

    m_mrp = new QLineEdit(this);
    m_mrp->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^\\d{1,9}(\\.\\d{1,2})?$")), m_mrp));
    m_mrp->setPlaceholderText(QStringLiteral("e.g. 2.50"));

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(8);
    form->addRow(QStringLiteral("Batch number *"), m_batchNumber);
    form->addRow(QStringLiteral("Expiry date *"), m_expiry);
    form->addRow(QStringLiteral("Quantity *"), m_qty);
    form->addRow(QStringLiteral("Cost per unit *"), m_cost);
    form->addRow(QStringLiteral("MRP per unit *"), m_mrp);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Cancel)->setProperty("variant", "secondary");
    connect(buttons, &QDialogButtonBox::accepted, this, &StockInDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Footer button order: Cancel (secondary) on the LEFT, primary Save on the RIGHT-most.
    auto *footer = new QHBoxLayout;
    footer->setSpacing(8);
    footer->addWidget(buttons->button(QDialogButtonBox::Cancel));
    footer->addStretch(1);
    footer->addWidget(buttons->button(QDialogButtonBox::Save));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addWidget(heading);
    layout->addLayout(form);
    layout->addLayout(footer);
    UiUtil::makeScanSafe(this);
}

void StockInDialog::accept()
{
    auto fail = [this](const QString &m) { QMessageBox::warning(this, windowTitle(), m); };

    StockInDraft d;
    d.medicineId = m_medicineId;
    d.batchNumber = m_batchNumber->text().trimmed();
    d.expiry = m_expiry->date();
    d.quantity = m_qty->value();
    d.costPerUnit = m_cost->text().trimmed();
    d.mrpPerUnit = m_mrp->text().trimmed();

    if (d.batchNumber.isEmpty()) {
        return fail(QStringLiteral("Batch number is required."));
    }
    if (!d.expiry.isValid() || d.expiry <= QDate::currentDate()) {
        return fail(QStringLiteral("Expiry must be a future date."));
    }
    if (d.costPerUnit.isEmpty() || Money::fromString(d.costPerUnit).isNegative()) {
        return fail(QStringLiteral("Enter a valid cost per unit."));
    }
    if (d.mrpPerUnit.isEmpty() || Money::fromString(d.mrpPerUnit).isNegative()) {
        return fail(QStringLiteral("Enter a valid MRP per unit."));
    }

    if (m_repo->addStock(d, m_userId) < 0) {
        return fail(QStringLiteral("Could not add stock: %1").arg(m_repo->errorString()));
    }
    QDialog::accept();
}
