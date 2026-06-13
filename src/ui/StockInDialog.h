#pragma once

#include <QDialog>

class QLineEdit;
class QSpinBox;
class QDateEdit;
class BatchRepository;

// Minimal stock-in: receive one batch for a medicine so it becomes sellable.
// (Full GRN with suppliers/invoices is Phase 4.) Saves via BatchRepository.
class StockInDialog : public QDialog
{
    Q_OBJECT
public:
    StockInDialog(BatchRepository *repo, qint64 userId, qint64 medicineId,
                  const QString &medicineName, QWidget *parent = nullptr);

public slots:
    void accept() override;

private:
    BatchRepository *m_repo;
    qint64 m_userId;
    qint64 m_medicineId;

    QLineEdit *m_batchNumber = nullptr;
    QDateEdit *m_expiry = nullptr;
    QSpinBox *m_qty = nullptr;
    QLineEdit *m_cost = nullptr;
    QLineEdit *m_mrp = nullptr;
};
