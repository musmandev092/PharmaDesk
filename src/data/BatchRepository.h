#pragma once

#include "domain/Fefo.h"

#include <QDate>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

// Details for a minimal stock-in (a single batch). Full goods-receipt (GRN)
// with suppliers + invoices arrives in Phase 4; this lets Phase 2 put sellable
// stock on the shelf so the sale flow is testable end-to-end.
struct StockInDraft
{
    qint64 medicineId = 0;
    QString batchNumber;
    QDate expiry;
    int quantity = 0;    // base units received
    QString costPerUnit; // decimal string (scale 4)
    QString mrpPerUnit;  // decimal string (scale 2)
};

class BatchRepository
{
public:
    explicit BatchRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    // Creates a batch (current_qty = quantity) and a GRN_RECEIPT movement, in
    // one transaction. Returns new batch id or -1.
    qint64 addStock(const StockInDraft &d, qint64 userId);

    // Valid sellable batches for a medicine, FEFO-ordered (expiry ASC, id ASC).
    // Excludes quarantined / expired / empty / past-expiry batches.
    QVector<FefoBatch> candidateBatchesForSale(qint64 medicineId) const;

    int onHand(qint64 medicineId) const;

    QString errorString() const { return m_error; }

private:
    QSqlDatabase m_db;
    mutable QString m_error;
};
