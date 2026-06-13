#pragma once

#include <QDate>
#include <QString>
#include <QVector>
#include <stdexcept>

// One candidate batch for FEFO allocation (a pre-fetched `batches` row).
struct FefoBatch
{
    qint64 id = 0;
    int currentQty = 0;
    QDate expiry;
    bool quarantined = false;
    bool expired = false;
    QString costPerUnit; // decimal string (scale 4)
    QString mrpPerUnit;  // decimal string (scale 2)
};

// One allocation: take `deduction` units from `batch`.
struct FefoAllocation
{
    FefoBatch batch;
    int deduction = 0;
};

// Raised when valid stock is short of what the sale needs.
class InsufficientStockException : public std::runtime_error
{
public:
    InsufficientStockException(qint64 medicineId, int needed, int available)
        : std::runtime_error("insufficient stock"), medicineId(medicineId), needed(needed),
          available(available)
    {}
    qint64 medicineId;
    int needed;
    int available;
};

// First-Expired-First-Out batch allocation. Port of
// pos/backend/services/Fefo.php. Pure: caller passes pre-fetched batch rows.
namespace Fefo {

// Allocate `totalQtyNeeded` base units across valid batches (soonest expiry
// first). Skips quarantined / expired / empty / past-expiry batches. Throws
// InsufficientStockException when short. `today` defaults to the current date.
QVector<FefoAllocation> allocate(const QVector<FefoBatch> &batches, int totalQtyNeeded,
                                 qint64 medicineId = 0, const QDate &today = QDate::currentDate());

// Defensive: assert the input is already sorted by expiry ASC.
void assertSortedByExpiry(const QVector<FefoBatch> &batches);

} // namespace Fefo
