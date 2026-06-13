#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>

// A batch shown in the adjustment picker — every batch of a medicine regardless
// of state (so quarantined / expired stock can be written off).
struct AdjustableBatch
{
    qint64 id = 0;
    QString batchNumber;
    QString expiry; // ISO date
    int currentQty = 0;
    bool quarantined = false;
    bool expired = false;
};

// A row in the recent-adjustments history.
struct AdjustmentRow
{
    qint64 id = 0;
    QString number;
    QString medicineName;
    QString batchNumber;
    int qtyDelta = 0;
    QString reason;
    QString notes;
    QString costImpact; // decimal string
    QString createdAt;
    QString performedBy;
};

// Read-only access for the Stock Adjustments screen.
class StockAdjustmentRepository
{
public:
    explicit StockAdjustmentRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    // Every batch of a medicine (any state), newest-expiry last.
    QVector<AdjustableBatch> batchesForMedicine(qint64 medicineId) const;

    // Most recent adjustments (newest first).
    QVector<AdjustmentRow> listRecent(int limit = 200) const;

private:
    QSqlDatabase m_db;
};
