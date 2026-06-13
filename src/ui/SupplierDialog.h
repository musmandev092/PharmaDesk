#pragma once

#include <QDialog>

class QLineEdit;
class QComboBox;
class QPlainTextEdit;
class QCheckBox;
class SupplierRepository;

// Add/edit a supplier. Saves via SupplierRepository on accept().
class SupplierDialog : public QDialog
{
    Q_OBJECT
public:
    SupplierDialog(SupplierRepository *repo, qint64 userId, qint64 editId,
                   QWidget *parent = nullptr);

public slots:
    void accept() override;

private:
    SupplierRepository *m_repo;
    qint64 m_userId;
    qint64 m_editId;

    QLineEdit *m_name = nullptr;
    QLineEdit *m_phone = nullptr;
    QLineEdit *m_ntn = nullptr;
    QPlainTextEdit *m_address = nullptr;
    QLineEdit *m_booker = nullptr;
    QLineEdit *m_salesman = nullptr;
    QComboBox *m_terms = nullptr;
    QPlainTextEdit *m_notes = nullptr;
    QCheckBox *m_active = nullptr;
};
