#pragma once

#include <QString>

// Cost-blending math for GRN lines. Port of
// pos/backend/services/CostBlender.php.
//
//   blended_cost_per_base_unit =
//       (unit_cost × paid_qty) / ((paid_qty + foc_qty) × units_per_purchase)
//   mrp_per_base_unit = mrp_per_purchase_unit / units_per_purchase
//
// Both at scale 4 (DECIMAL(12,4) cost_per_unit); line total at scale 2.
namespace CostBlender {

// Free-of-charge units dilute the per-unit cost (you paid for paidQty packs but
// received paid+foc). Returns "0.0000" when nothing is received.
QString blendedCostPerBaseUnit(int paidQty, int focQty, const QString &unitCost,
                               int unitsPerPurchase);

QString mrpPerBaseUnit(const QString &mrpPerPurchaseUnit, int unitsPerPurchase);

// Supplier-invoice line total in purchase units: unit_cost × paid_qty (scale 2).
QString lineTotal(int paidQty, const QString &unitCost);

} // namespace CostBlender
