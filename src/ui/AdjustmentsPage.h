#pragma once

#include <QSqlDatabase>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

// Stock Adjustments screen (Phase 2). Port of pos/backend/services/StockAdjuster
// + the stock-adjustment page: search a medicine, pick one of its batches, enter
// a signed quantity delta + reason (+ notes), and apply a manual adjustment that
// records an inventory_movement, a cost impact, and an audit row. A recent-
// adjustments history table sits below.
class AdjustmentsPage : public QWidget
{
    Q_OBJECT
public:
    AdjustmentsPage(QSqlDatabase db, qint64 userId, QWidget *parent = nullptr);

    void reload();

private slots:
    void searchMedicines();
    void onMedicineSelected();
    void onBatchSelected();
    void applyAdjustment();

private:
    void buildUi();
    void loadBatches(qint64 medicineId);
    void reloadRecent();
    void setStatus(const QString &text, bool error);

    qint64 selectedBatchId() const;
    int signedQty() const;

    QSqlDatabase m_db;
    qint64 m_userId = -1;

    // Medicine search
    QLineEdit *m_search = nullptr;
    QTableWidget *m_results = nullptr;

    // Batch picker
    QLabel *m_medicineLabel = nullptr;
    QComboBox *m_batchCombo = nullptr;

    // Adjustment inputs
    QComboBox *m_direction = nullptr; // Remove / Add
    QSpinBox *m_qty = nullptr;        // magnitude (>= 1)
    QComboBox *m_reason = nullptr;
    QPlainTextEdit *m_notes = nullptr;
    QPushButton *m_apply = nullptr;
    QLabel *m_status = nullptr;

    // History
    QTableWidget *m_recent = nullptr;

    qint64 m_medicineId = -1;
};
