#include "domain/Fefo.h"

#include <algorithm>

namespace Fefo {

QVector<FefoAllocation> allocate(const QVector<FefoBatch> &batches, int totalQtyNeeded,
                                 qint64 medicineId, const QDate &today)
{
    if (totalQtyNeeded < 1) {
        return {};
    }

    // Defensive filter (caller usually already did this in SQL): drop
    // quarantined / expired / empty / past-expiry batches.
    QVector<FefoBatch> valid;
    valid.reserve(batches.size());
    for (const FefoBatch &b : batches) {
        if (b.quarantined || b.expired || b.currentQty <= 0) {
            continue;
        }
        if (!b.expiry.isValid() || !(b.expiry > today)) {
            continue;
        }
        valid.push_back(b);
    }

    // Sort by expiry ASC, then id ASC (matches the PHP usort tie-break).
    std::sort(valid.begin(), valid.end(), [](const FefoBatch &a, const FefoBatch &b) {
        if (a.expiry != b.expiry) {
            return a.expiry < b.expiry;
        }
        return a.id < b.id;
    });

    int available = 0;
    for (const FefoBatch &b : valid) {
        available += b.currentQty;
    }

    int remaining = totalQtyNeeded;
    QVector<FefoAllocation> allocations;
    for (const FefoBatch &batch : valid) {
        if (remaining <= 0) {
            break;
        }
        const int deduction = std::min(batch.currentQty, remaining);
        allocations.push_back({batch, deduction});
        remaining -= deduction;
    }

    if (remaining > 0) {
        throw InsufficientStockException(medicineId, totalQtyNeeded, available);
    }
    return allocations;
}

void assertSortedByExpiry(const QVector<FefoBatch> &batches)
{
    bool havePrev = false;
    QDate prev;
    for (const FefoBatch &b : batches) {
        if (havePrev && b.expiry < prev) {
            throw std::runtime_error("FEFO ordering violation: batches not sorted by expiry ASC");
        }
        prev = b.expiry;
        havePrev = true;
    }
}

} // namespace Fefo
