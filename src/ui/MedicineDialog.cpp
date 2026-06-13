#include "ui/MedicineDialog.h"

#include "data/MedicineRepository.h"
#include "domain/MedicineForm.h"
#include "ui/UiUtil.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

MedicineDialog::MedicineDialog(MedicineRepository *repo, qint64 userId, qint64 editId,
                               QWidget *parent)
    : QDialog(parent), m_repo(repo), m_userId(userId), m_editId(editId)
{
    setWindowTitle(editId == 0 ? QStringLiteral("Add medicine") : QStringLiteral("Edit medicine"));
    setModal(true);
    setMinimumWidth(520);
    resize(520, 640);

    m_sku = new QLineEdit(this);
    m_sku->setMaxLength(40);
    m_barcode = new QLineEdit(this);
    m_barcode->setMaxLength(60);
    m_brand = new QLineEdit(this);
    m_brand->setMaxLength(160);
    m_generic = new QLineEdit(this);
    m_generic->setMaxLength(160);
    m_manufacturer = new QLineEdit(this);
    m_manufacturer->setMaxLength(160);
    m_strength = new QLineEdit(this);
    m_strength->setMaxLength(60);

    m_form = new QComboBox(this);
    for (const MedicineForm::Group &g : MedicineForm::groups()) {
        m_form->addItem(QStringLiteral("— %1 —").arg(g.title));
        m_form->setItemData(m_form->count() - 1, false, Qt::UserRole - 1); // disable group header
        for (const MedicineForm::Option &o : g.options) {
            m_form->addItem(o.label, o.key);
        }
    }
    m_formCustom = new QLineEdit(this);
    m_formCustom->setMaxLength(60);
    m_formCustom->setPlaceholderText(QStringLiteral("only if form is \"Other\""));
    m_therapeutic = new QLineEdit(this);
    m_therapeutic->setMaxLength(120);
    m_purchaseUnit = new QLineEdit(this);
    m_purchaseUnit->setMaxLength(40);
    m_purchaseUnit->setText(QStringLiteral("BOX"));
    m_baseUnit = new QLineEdit(this);
    m_baseUnit->setMaxLength(40);
    m_baseUnit->setText(QStringLiteral("TABLET"));
    m_unitsPerPurchase = new QSpinBox(this);
    m_unitsPerPurchase->setRange(1, 1000000);
    m_unitsPerPurchase->setValue(1);

    // Friendly labels, raw enum keys stored as item data.
    m_schedule = new QComboBox(this);
    m_schedule->addItem(QStringLiteral("None (general sale)"), QStringLiteral("NONE"));
    m_schedule->addItem(QStringLiteral("Schedule G (Rx-only)"), QStringLiteral("SCHEDULE_G"));
    m_schedule->addItem(QStringLiteral("Schedule H (Rx, no refill)"), QStringLiteral("SCHEDULE_H"));
    m_schedule->addItem(QStringLiteral("Narcotic (register entry)"), QStringLiteral("NARCOTIC"));
    m_tax = new QComboBox(this);
    m_tax->addItem(QStringLiteral("Exempt"), QStringLiteral("EXEMPT"));
    m_tax->addItem(QStringLiteral("Standard 18% GST"), QStringLiteral("STANDARD_18"));
    m_tax->addItem(QStringLiteral("Reduced rate"), QStringLiteral("REDUCED"));
    m_tax->addItem(QStringLiteral("Zero-rated"), QStringLiteral("ZERO_RATED"));

    m_reorderLevel = new QSpinBox(this);
    m_reorderLevel->setRange(0, 1000000);
    m_reorderQty = new QSpinBox(this);
    m_reorderQty->setRange(0, 1000000);
    m_reorderUnit = new QComboBox(this);
    m_reorderUnit->addItems({QStringLiteral("PURCHASE"), QStringLiteral("BASE")});

    m_rx = new QCheckBox(QStringLiteral("Prescription required"), this);
    m_active = new QCheckBox(QStringLiteral("Active (visible in POS)"), this);
    m_active->setChecked(true);

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);
    form->addRow(QStringLiteral("SKU *"), m_sku);
    form->addRow(QStringLiteral("Primary barcode"), m_barcode);
    form->addRow(QStringLiteral("Brand name *"), m_brand);
    form->addRow(QStringLiteral("Generic name *"), m_generic);
    form->addRow(QStringLiteral("Manufacturer"), m_manufacturer);
    form->addRow(QStringLiteral("Dosage form *"), m_form);
    form->addRow(QStringLiteral("Custom form"), m_formCustom);
    form->addRow(QStringLiteral("Strength"), m_strength);
    form->addRow(QStringLiteral("Therapeutic category"), m_therapeutic);
    form->addRow(QStringLiteral("Buy as (purchase unit) *"), m_purchaseUnit);
    form->addRow(QStringLiteral("Sell as (base unit) *"), m_baseUnit);
    form->addRow(QStringLiteral("Units per pack *"), m_unitsPerPurchase);
    form->addRow(QStringLiteral("Controlled schedule"), m_schedule);
    form->addRow(QStringLiteral("Tax code"), m_tax);
    form->addRow(QStringLiteral("Reorder level"), m_reorderLevel);
    form->addRow(QStringLiteral("Reorder quantity"), m_reorderQty);
    form->addRow(QStringLiteral("Reorder unit"), m_reorderUnit);
    form->addRow(QString(), m_rx);
    form->addRow(QString(), m_active);

    // Extra bottom padding so the last row isn't clipped against the footer.
    form->setContentsMargins(16, 16, 16, 24);
    auto *inner = new QWidget;
    inner->setLayout(form);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(inner);

    // GNOME footer convention: stretch, then secondary Cancel on the LEFT,
    // then primary Save added last as the RIGHT-most button.
    auto *cancelButton = new QPushButton(QStringLiteral("Cancel"), this);
    cancelButton->setProperty("variant", "secondary");
    auto *saveButton = new QPushButton(QStringLiteral("Save"), this);
    saveButton->setDefault(true);
    connect(saveButton, &QPushButton::clicked, this, &MedicineDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    auto *footer = new QHBoxLayout;
    footer->addStretch();
    footer->addWidget(cancelButton);
    footer->addWidget(saveButton);

    auto *title = new QLabel(
        m_editId == 0 ? QStringLiteral("Add medicine") : QStringLiteral("Edit medicine"), this);
    title->setObjectName(QStringLiteral("h1"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addWidget(title);
    layout->addSpacing(12);
    layout->addWidget(scroll);
    layout->addLayout(footer);
    UiUtil::makeScanSafe(this); // a scanner's Enter advances fields, never submits

    // Default form selection = TABLET.
    int idx = m_form->findData(QStringLiteral("TABLET"));
    if (idx >= 0) m_form->setCurrentIndex(idx);

    // Load existing values for edit.
    if (m_editId != 0) {
        MedicineDraft d;
        if (m_repo->find(m_editId, &d)) {
            m_sku->setText(d.sku);
            m_barcode->setText(d.primaryBarcode);
            m_brand->setText(d.brandName);
            m_generic->setText(d.genericName);
            m_manufacturer->setText(d.manufacturer);
            m_strength->setText(d.strength);
            int fi = m_form->findData(d.form);
            if (fi >= 0) m_form->setCurrentIndex(fi);
            m_formCustom->setText(d.formCustom);
            m_therapeutic->setText(d.therapeuticCategory);
            m_purchaseUnit->setText(d.purchaseUnit);
            m_baseUnit->setText(d.baseUnit);
            m_unitsPerPurchase->setValue(d.unitsPerPurchase);
            int si = m_schedule->findData(d.controlledSchedule);
            if (si >= 0) m_schedule->setCurrentIndex(si);
            int ti = m_tax->findData(d.taxCode);
            if (ti >= 0) m_tax->setCurrentIndex(ti);
            m_reorderLevel->setValue(d.reorderLevel);
            m_reorderQty->setValue(d.reorderQuantity);
            m_reorderUnit->setCurrentText(d.reorderUnit);
            m_rx->setChecked(d.prescriptionRequired);
            m_active->setChecked(d.isActive);
        }
    }
}

void MedicineDialog::accept()
{
    MedicineDraft d;
    d.sku = m_sku->text().trimmed();
    d.primaryBarcode = m_barcode->text().trimmed();
    d.brandName = m_brand->text().trimmed();
    d.genericName = m_generic->text().trimmed();
    d.manufacturer = m_manufacturer->text().trimmed();
    d.strength = m_strength->text().trimmed();
    d.form = m_form->currentData().toString();
    d.formCustom = m_formCustom->text().trimmed();
    d.therapeuticCategory = m_therapeutic->text().trimmed();
    d.purchaseUnit = m_purchaseUnit->text().trimmed();
    d.baseUnit = m_baseUnit->text().trimmed();
    d.unitsPerPurchase = m_unitsPerPurchase->value();
    d.controlledSchedule = m_schedule->currentData().toString();
    d.taxCode = m_tax->currentData().toString();
    d.reorderLevel = m_reorderLevel->value();
    d.reorderQuantity = m_reorderQty->value();
    d.reorderUnit = m_reorderUnit->currentText();
    d.prescriptionRequired = m_rx->isChecked();
    d.isActive = m_active->isChecked();

    auto fail = [this](const QString &msg) { QMessageBox::warning(this, windowTitle(), msg); };

    if (d.sku.isEmpty() || d.brandName.isEmpty() || d.genericName.isEmpty()) {
        return fail(QStringLiteral("SKU, brand, and generic name are required."));
    }
    if (d.purchaseUnit.isEmpty() || d.baseUnit.isEmpty()) {
        return fail(QStringLiteral("Purchase unit and base unit are required."));
    }
    if (!MedicineForm::isValid(d.form)) {
        return fail(QStringLiteral("Please choose a dosage form."));
    }
    if (d.form == QLatin1String("OTHER") && d.formCustom.isEmpty()) {
        return fail(QStringLiteral("Enter the custom form name (you chose \"Other\")."));
    }
    if (d.unitsPerPurchase < 1) {
        return fail(QStringLiteral("Units per pack must be at least 1."));
    }
    if (m_repo->skuInUse(d.sku, m_editId)) {
        return fail(QStringLiteral("SKU '%1' is already used by another medicine.").arg(d.sku));
    }
    if (!d.primaryBarcode.isEmpty() && m_repo->barcodeInUse(d.primaryBarcode, m_editId)) {
        return fail(QStringLiteral("Barcode '%1' is already used by another medicine.")
                        .arg(d.primaryBarcode));
    }

    bool ok = false;
    if (m_editId == 0) {
        ok = m_repo->create(d, m_userId) > 0;
    } else {
        ok = m_repo->update(m_editId, d, m_userId);
    }
    if (!ok) {
        return fail(QStringLiteral("Could not save: %1").arg(m_repo->errorString()));
    }
    QDialog::accept();
}
