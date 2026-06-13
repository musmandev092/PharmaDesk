#pragma once

#include <QString>

// Pure policy for who may authorize a sale discount. Managers and admins
// self-authorize; a cashier must obtain a manager/admin override (a different
// user) for any positive discount. No DB, no UI — unit-testable.
namespace DiscountAuthorizationPolicy {

// True for the managerial roles that can self-authorize a discount.
bool isManagerial(const QString &role);

// True when the acting user needs a separate manager/admin override to apply
// the discount (cashier applying a positive discount).
bool requiresManagerOverride(const QString &actingRole, bool discountIsPositive);

} // namespace DiscountAuthorizationPolicy
