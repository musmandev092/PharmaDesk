#pragma once

#include <QDialog>

class QLineEdit;
class QComboBox;
class QSpinBox;
class QCheckBox;
class MedicineRepository;

// Add/edit a medicine. Validates (required fields, lengths, valid enums,
// units ≥ 1, live sku/barcode uniqueness) and saves via MedicineRepository on
// accept(). Mirrors the validation in pos/frontend/pages/inventory/medicines.php.
class MedicineDialog : public QDialog
{
    Q_OBJECT
public:
    // editId == 0 → create; otherwise edit that medicine.
    MedicineDialog(MedicineRepository *repo, qint64 userId, qint64 editId,
                   QWidget *parent = nullptr);

public slots:
    void accept() override;

private:
    MedicineRepository *m_repo;
    qint64 m_userId;
    qint64 m_editId;

    QLineEdit *m_sku = nullptr;
    QLineEdit *m_barcode = nullptr;
    QLineEdit *m_brand = nullptr;
    QLineEdit *m_generic = nullptr;
    QLineEdit *m_manufacturer = nullptr;
    QLineEdit *m_strength = nullptr;
    QComboBox *m_form = nullptr;
    QLineEdit *m_formCustom = nullptr;
    QLineEdit *m_therapeutic = nullptr;
    QLineEdit *m_purchaseUnit = nullptr;
    QLineEdit *m_baseUnit = nullptr;
    QSpinBox *m_unitsPerPurchase = nullptr;
    QComboBox *m_schedule = nullptr;
    QComboBox *m_tax = nullptr;
    QSpinBox *m_reorderLevel = nullptr;
    QSpinBox *m_reorderQty = nullptr;
    QComboBox *m_reorderUnit = nullptr;
    QCheckBox *m_rx = nullptr;
    QCheckBox *m_active = nullptr;
};
