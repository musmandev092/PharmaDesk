#include "domain/CostBlender.h"

#include "domain/Money.h"

namespace CostBlender {

QString blendedCostPerBaseUnit(int paidQty, int focQty, const QString &unitCost,
                               int unitsPerPurchase)
{
    if (paidQty < 0 || focQty < 0 || unitsPerPurchase < 1) {
        return Money().toString(Money::ScaleCost);
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
        return Money().toString(Money::ScaleCost);
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
