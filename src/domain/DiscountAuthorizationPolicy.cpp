#include "domain/DiscountAuthorizationPolicy.h"

namespace DiscountAuthorizationPolicy {

bool isManagerial(const QString &role)
{
    return role == QStringLiteral("MANAGER") || role == QStringLiteral("ADMIN");
}

bool requiresManagerOverride(const QString &actingRole, bool discountIsPositive)
{
    if (!discountIsPositive) {
        return false;
    }
    return !isManagerial(actingRole);
}

} // namespace DiscountAuthorizationPolicy
