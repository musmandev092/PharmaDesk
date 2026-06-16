#include "domain/CostBlender.h"

#include "domain/Money.h"

#include <stdexcept>

namespace CostBlender {

QString blendedCostPerBaseUnit(int paidQty, int focQty, const QString &unitCost,
                               int unitsPerPurchase)
{
    // Match the PHP spec (services/CostBlender.php): invalid quantities are a
    // programming/data error that must abort the GRN post, NOT silently produce a
    // zero blended cost (which would corrupt COGS). The legitimate
    // nothing-received case (totalBaseUnits == 0) still returns "0.0000" below.
    if (paidQty < 0 || focQty < 0 || unitsPerPurchase < 1) {
        throw std::invalid_argument("paid_qty/foc_qty must be >= 0 and units_per_purchase >= 1");
    }
    const qint64 totalBaseUnits = static_cast<qint64>(paidQty + focQty) * unitsPerPurchase;
    if (totalBaseUnits == 0) {
        return Money().toString(Money::ScaleCost);
    }
    const Money numerator = Money::fromString(unitCost).mul(paidQty);
    return numerator.divByInt(totalBaseUnits).toString(Money::ScaleCost);
}

QString mrpPerBaseUnit(const QString &mrpPerPurchaseUnit, int unitsPerPurchase)
{
    if (unitsPerPurchase < 1) {
        throw std::invalid_argument("units_per_purchase must be >= 1");
    }
    return Money::fromString(mrpPerPurchaseUnit)
        .divByInt(unitsPerPurchase)
        .toString(Money::ScaleCost);
}

QString lineTotal(int paidQty, const QString &unitCost)
{
    return Money::fromString(unitCost).mul(paidQty).toString(Money::ScaleMoney);
}

} // namespace CostBlender
