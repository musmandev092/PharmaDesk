#include "ui/SupplierDialog.h"

#include "data/SupplierRepository.h"
#include "ui/UiUtil.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>

SupplierDialog::SupplierDialog(SupplierRepository *repo, qint64 userId, qint64 editId,
                               QWidget *parent)
    : QDialog(parent), m_repo(repo), m_userId(userId), m_editId(editId)
{
    setWindowTitle(editId == 0 ? QStringLiteral("Add supplier") : QStringLiteral("Edit supplier"));
    setModal(true);
    setMinimumWidth(440);
    resize(440, 480);

    m_name = new QLineEdit(this);
    m_name->setMaxLength(160);
    m_phone = new QLineEdit(this);
    m_phone->setMaxLength(40);
    m_ntn = new QLineEdit(this);
    m_ntn->setMaxLength(40);
    m_address = new QPlainTextEdit(this);
    m_address->setFixedHeight(56);
    m_booker = new QLineEdit(this);
    m_booker->setMaxLength(120);
    m_salesman = new QLineEdit(this);
    m_salesman->setMaxLength(120);
    m_terms = new QComboBox(this);
    m_terms->setEditable(true);
    m_terms->addItems({QStringLiteral("CASH"), QStringLiteral("CREDIT_15"),
                       QStringLiteral("CREDIT_30"), QStringLiteral("CREDIT_60")});
    m_notes = new QPlainTextEdit(this);
    m_notes->setFixedHeight(56);
    m_active = new QCheckBox(QStringLiteral("Active"), this);
    m_active->setChecked(true);

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(8);
    form->addRow(QStringLiteral("Name"), m_name);
    form->addRow(QStringLiteral("Contact phone"), m_phone);
    form->addRow(QStringLiteral("NTN / tax id"), m_ntn);
    form->addRow(QStringLiteral("Address"), m_address);
    form->addRow(QStringLiteral("Booker"), m_booker);
    form->addRow(QStringLiteral("Salesman"), m_salesman);
    form->addRow(QStringLiteral("Payment terms"), m_terms);
    form->addRow(QStringLiteral("Notes"), m_notes);
    form->addRow(QString(), m_active);

    // GNOME-convention footer: stretch, then secondary Cancel on the left and
    // primary Save as the right-most button.
    auto *cancelButton = new QPushButton(QStringLiteral("Cancel"), this);
    cancelButton->setProperty("variant", "secondary");
    auto *saveButton = new QPushButton(QStringLiteral("Save"), this);
    saveButton->setDefault(true);
    connect(saveButton, &QPushButton::clicked, this, &SupplierDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    buttons->addStretch(1);
    buttons->addWidget(cancelButton);
    buttons->addWidget(saveButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addLayout(form);
    layout->addLayout(buttons);
    UiUtil::makeScanSafe(this);

    if (m_editId != 0) {
        SupplierRow s;
        if (m_repo->find(m_editId, &s)) {
            m_name->setText(s.name);
            m_phone->setText(s.contactPhone);
            m_ntn->setText(s.ntn);
            m_address->setPlainText(s.address);
            m_booker->setText(s.bookerName);
            m_salesman->setText(s.salesmanName);
            m_terms->setCurrentText(s.paymentTerms);
            m_notes->setPlainText(s.notes);
            m_active->setChecked(s.isActive);
        }
    }
}

void SupplierDialog::accept()
{
    SupplierRow s;
    s.name = m_name->text().trimmed();
    s.contactPhone = m_phone->text().trimmed();
    s.ntn = m_ntn->text().trimmed();
    s.address = m_address->toPlainText().trimmed();
    s.bookerName = m_booker->text().trimmed();
    s.salesmanName = m_salesman->text().trimmed();
    s.paymentTerms = m_terms->currentText().trimmed();
    s.notes = m_notes->toPlainText().trimmed();
    s.isActive = m_active->isChecked();

    if (s.name.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), QStringLiteral("Supplier name is required."));
        return;
    }

    const bool ok
        = m_editId == 0 ? (m_repo->create(s, m_userId) > 0) : m_repo->update(m_editId, s, m_userId);
    if (!ok) {
        QMessageBox::warning(this, windowTitle(),
                             QStringLiteral("Could not save: %1").arg(m_repo->errorString()));
        return;
    }
    QDialog::accept();
}
