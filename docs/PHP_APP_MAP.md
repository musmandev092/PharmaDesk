# PHP App Structure Map (Phase 0 deliverable)

Source of truth: `../pos` (plain PHP 8.4 + Postgres 17, Caddy, Docker) and shared
schema/triggers at `../db`. This map is the porting checklist — each screen and
service below becomes a Qt Widgets screen / C++ class.

## Architecture of the PHP app
- **Front controller:** `pos/public/index.php` → `pos/backend/Router.php` with the
  route table in `pos/frontend/routes.php`. POST routes auto-verify CSRF.
- **Backend core** (`pos/backend/`): `Db.php` (PDO/Postgres), `Auth.php`,
  `Session.php`, `Audit.php`, `Money.php` (decimal math), `View.php`, `Router.php`,
  `RateLimit.php`, `Csrf.php`, `Csp.php`, `PinPolicy.php`.
- **Domain services** (`pos/backend/services/`) — port these to plain C++ classes:
  | Service | Responsibility |
  |---|---|
  | `Sale.php` | Build + commit a sale: validate cart, FEFO-allocate, decrement stock, write `sales`/`sale_items`/`inventory_movements`, totals, change. **Critical path.** |
  | `SaleCalculator.php` | Line/total math (scale-2 money, HALF_UP, round-once). |
  | `Money.php` | Decimal fixed-point (scale-2 money, scale-4 cost). |
  | `Fefo.php` | First-Expired-First-Out batch allocation; skips quarantined/expired/empty; throws `InsufficientStockException`. |
  | `Returns.php` | Refunds/returns workflow + restock/quarantine/write-off. |
  | `Grn.php` | Goods-receipt notes: receive stock, create batches, blended cost. |
  | `CostBlender.php` | Weighted-average cost across batches (scale-4). |
  | `StockAdjuster.php` | Manual stock adjustments with reason + movement ledger. |
  | `Reconciler.php` | Cashier session cash reconciliation. |
  | `ZReport.php` | End-of-session Z-report (HMAC-signed). |
  | `Sessions.php` | Cashier session open/close. |
  | `MedicineSearch.php` / `DrugLookup.php` | POS product search / lookup. |
  | `Fefo`, `ManagerOverride.php` | Manager PIN override for discounts/voids. |
  | `QrParser.php` | GS1 / Digital Link / HIBC barcode parsing (large). |
  | `EscPos.php` / `Cups.php` / `Printer.php` | Thermal receipt printing → port to `QPrinter`/`QPainter` (Phase 5). |
  | `Csv.php` | CSV export for reports. |

## Screens (from `pos/frontend/routes.php`) → Qt phases
- **Auth:** `/login` (PIN), `/logout`, `/account/change-pin` → Phase 6.
- **Home:** `/` dashboard (`pages/home.php`).
- **POS (Phase 2 core):** `/pos` terminal, `/pos/session`, `/pos/history`,
  `/pos/returns`, `/pos/returns/history`.
- **Inventory (Phase 3):** `/inventory` dashboard, `/inventory/medicines` (CRUD),
  `/inventory/stock`, `/inventory/adjustments`.
- **Purchasing (Phase 4):** `/inventory/grn` list + `/inventory/grn/new|{id}`,
  `/suppliers`.
- **Reports (Phase 5):** dashboard, sales, pl, top-sellers, refunds, tax,
  compliance, expiry, low-stock, velocity, audit, narcotic-register.
- **Sessions:** `/sessions/{id}/z-report`, `/zreport/verify/{id}`.
- **Admin/Ops (Phase 6):** users, returns, supplier-returns, sessions, printer,
  system, settings.
- **Internal API (fold into C++ services, not HTTP):** drug-lookup,
  medicine-search, medicine-alternatives, barcode-parse, print-z-report.

## Database (`../db/schema/init.sql`, 14 tables + enums)
Tables: `branches`, `users`, `pin_history`, `suppliers`, `medicines`, `batches`,
`grn_documents`, `grn_lines`, `sales`, `sale_items`, `returns`,
`stock_adjustments`, `inventory_movements`, `cashier_sessions`, `audit_log`,
`settings`, `rate_limits`.

Postgres enums to port as `TEXT + CHECK`: `UserRole`, `MedicineForm` (~55 values),
`ControlledSchedule`, `TaxCode`, `GrnStatus`, `PaymentMode`, `SaleStatus`,
`ReturnReason`, `ReturnStatus`, `AdjustmentReason`, `MovementType`, `SessionStatus`.

DB invariants enforced by triggers (`../db/triggers/`) — reproduce in C++/SQLite:
- `audit_log_immutable.sql` — append-only audit, HMAC chain.
- non-negative `current_qty`/`received_qty`/`foc_qty` CHECKs on `batches`.
- partial-unique medicines, supplier-returns settling, concurrency hardening.

## Key business rules already confirmed (port exactly + test)
1. **Money:** scale-2 money / scale-4 cost, HALF_UP at storage; line total =
   `round(subtotal − discount + tax)` rounded once (see `SaleCalculator.php`).
2. **FEFO:** allocate from valid batches sorted by `expiry_date ASC, id ASC`;
   exclude quarantined/expired/zero-qty/past-expiry; throw if short.
3. **Stock ledger:** every stock change writes an `inventory_movements` row with
   `qty_before`/`qty_after`; all inside one transaction.
4. **Units:** medicines have purchase_unit/base_unit/units_per_purchase; sales
   record `qty_in_base_units` + `sold_unit_factor` + `qty_sold_display`.
