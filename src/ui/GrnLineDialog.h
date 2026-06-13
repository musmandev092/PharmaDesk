#pragma once

#include "service/GrnService.h"

#include <QDialog>
#include <QSqlDatabase>

class QLineEdit;
class QTableWidget;
class QSpinBox;
class QDateEdit;
class QLabel;

// Build one GRN line: pick a medicine, enter batch/expiry/qtys/cost/MRP. Shows a
// live preview of blended cost-per-base-unit and line total. Exposes the line()
// on accept.
class GrnLineDialog : public QDialog
{
    Q_OBJECT
public:
    GrnLineDialog(QSqlDatabase db, QWidget *parent = nullptr);

    GrnLineInput line() const { return m_line; }
    QString medicineName() const { return m_medicineName; }

public slots:
    void accept() override;

private slots:
    void runSearch();
    void pickResult();
    void updatePreview();

private:
    QSqlDatabase m_db;
    GrnLineInput m_line;
    QString m_medicineName;
    int m_unitsPerPurchase = 1;

    QLineEdit *m_search = nullptr;
    QTableWidget *m_results = nullptr;
    QLabel *m_picked = nullptr;
    QLineEdit *m_batch = nullptr;
    QDateEdit *m_expiry = nullptr;
    QSpinBox *m_paid = nullptr;
    QSpinBox *m_foc = nullptr;
    QLineEdit *m_cost = nullptr;
    QLineEdit *m_mrp = nullptr;
    QLabel *m_preview = nullptr;
};
