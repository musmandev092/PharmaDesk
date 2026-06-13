#pragma once

#include "domain/Money.h"

#include <QString>

// Sale line/total math. Port of pos/backend/services/SaleCalculator.php — same
// scales (money=2), same HALF_UP-at-the-boundary, round-once semantics.
namespace SaleCalculator {

// qty (integer base units) × unit MRP → scale-2 line subtotal (decimal string).
QString lineSubtotal(qint64 qtyBaseUnits, const QString &unitMrp);

// subtotal − discount + tax, rounded once at scale 2.
QString lineTotal(const QString &lineSubtotal, const QString &lineDiscount = QStringLiteral("0"),
                  const QString &lineTax = QStringLiteral("0"));

// Change = max(0, tendered − grandTotal), scale 2.
QString change(const QString &tendered, const QString &grandTotal);

} // namespace SaleCalculator
