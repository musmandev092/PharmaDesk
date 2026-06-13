#include "domain/SaleCalculator.h"

namespace SaleCalculator {

QString lineSubtotal(qint64 qtyBaseUnits, const QString &unitMrp)
{
    return Money::fromString(unitMrp).mul(qtyBaseUnits).toString(Money::ScaleMoney);
}

QString lineTotal(const QString &lineSubtotal, const QString &lineDiscount, const QString &lineTax)
{
    const Money v = Money::fromString(lineSubtotal) - Money::fromString(lineDiscount)
                    + Money::fromString(lineTax);
    return v.toString(Money::ScaleMoney);
}

QString change(const QString &tendered, const QString &grandTotal)
{
    const Money diff = Money::fromString(tendered) - Money::fromString(grandTotal);
    if (diff.isNegative()) {
        return Money().toString(Money::ScaleMoney);
    }
    return diff.toString(Money::ScaleMoney);
}

} // namespace SaleCalculator
