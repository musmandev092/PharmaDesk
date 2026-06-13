#pragma once

#include <QSqlDatabase>
#include <QString>

struct AdjustResult
{
    bool ok = false;
    QString error;
    qint64 adjustmentId = -1;
    QString adjustmentNumber;
    QString costImpact; // decimal string, scale 2
};

// Manual stock adjustment. Port of pos/backend/services/StockAdjuster.php.
//
// qty_delta is SIGNED — positive adds, negative removes. Refuses a result that
// would drive current_qty negative, and refuses to touch a QUARANTINED batch
// unless the reason is EXPIRY_WRITEOFF. Records the cost impact at the batch's
// current cost_per_unit, emits an ADJUSTMENT inventory_movement and a
// STOCK_ADJUSTED audit row — all in one transaction.
class StockAdjuster
{
public:
    StockAdjuster(QSqlDatabase db, qint64 userId) : m_db(std::move(db)), m_userId(userId) {}

    // reason ∈ {DAMAGE, EXPIRY_WRITEOFF, SHRINKAGE, COUNT_CORRECTION, SAMPLE,
    // DONATION, OTHER}. Notes are required when reason is OTHER.
    AdjustResult adjust(qint64 batchId, int qtyDelta, const QString &reason,
                        const QString &notes = QString());

private:
    QString nextAdjustmentNumber();

    QSqlDatabase m_db;
    qint64 m_userId;
};
