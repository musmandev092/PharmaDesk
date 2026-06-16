# 08 — Test Suite Audit

## Summary — Grade **B**

A genuinely good suite for an app of this size: ~1,200 assertions across 19 modules, run
against **real SQLite + real repositories** (no data-layer mocking), headless and
deterministic. The gaps are the highest-stakes branches — concurrency/rollback in
`SaleService`, the void/witness paths, and the two authorization policies — plus a
floating-point assertion in a money test that violates the project's own rule.

## Harness

Hand-rolled, dependency-free. `tests/framework/TestStats.h` provides `check(cond, name)`;
each module is a `run_<module>_tests(QSqlDatabase, userId)` free function; `run_all.cpp`
forward-declares and tables all 19; exit non-zero on any failure. `QStandardPaths` test
mode + offscreen platform + per-module unique prefixes (`TSALE_`, `TRET_`…) and
before/after delta assertions make it isolated and order-independent. Good engineering.

## Coverage map (gated suite)

| Module under test | File | ~checks |
|---|---|---|
| `Money` | money_tests | 640 |
| `SaleCalculator` | salecalc_tests | 158 |
| `SaleService::commit` | sale_tests | 146 |
| `ReturnService` (+repo, voidSale) | returns_tests | 134 |
| `CostBlender` | costblender_tests | 136 |
| `BarcodeParser` | barcode_tests | 122 |
| `Fefo` | fefo_tests | 66 |
| `SessionRepository` + `Reconciler` | sessions_tests | 60 |
| `PinPolicy` | pinpolicy_tests | 35 |
| `AnalyticsRepository` | analytics_tests | 34 |
| `MedicineRepository` | medicine_tests | 32 |
| `StockAdjuster` | adjustment_tests | 32 |
| `GrnService` (+repo) | grn_tests | 27 |
| `EscPosRenderer`+`ZReportArchive`+`AlternativesFinder` | printing_tests | 22 |
| `Database` schema/triggers | schema_tests | 19 |
| `ParkedCartRepository` | parked_tests | 14 |
| `CatalogImporter` | catalog_tests | 12 |
| `InventoryRepository` | inventory_tests | 12 |
| `UserRepository` lockout | auth_tests | 10 |

## Gaps (ranked)

**High value, currently untested:**
1. `SaleService` **guarded-decrement concurrency branch** (`numRowsAffected()!=1` → "stock
   changed during checkout") — never exercised.
2. `SaleService` **mid-transaction rollback** — FEFO short on the 2nd item must leave NO
   partial sale/stock/movement; never forced.
3. **Exact `qty_before`/`qty_after` ledger values** asserted against real batch state — only
   indirectly checked today.
4. `ControlledSubstancePolicy` and `DiscountAuthorizationPolicy` — **no direct unit tests**
   (only 3 incidental asserts each via `sale_tests`). Witness-rejection branches
   (witness==operator, witness-not-manager) untested.
5. **Authorization** of void/medicine-edit/GRN/user-mgmt at the service layer — untested
   because (per 05) it isn't enforced there yet.
6. `Money` boundary cases: 5th-digit-only HALF_UP tail, `toLongLong` overflow on 19+ digit
   integer part, negative exact-halfway `divByInt`.
7. `CostBlender` scale-4 rounding ties; the guard-returns-`0.0000` path.

**Test-integrity defect (High):** `returns_tests.cpp:449` asserts a money total via
`CAST AS REAL + toDouble + 0.005` epsilon — **floating-point in a money test**, directly
violating CLAUDE.md. Replace with `Money`/string comparison.

**Untested modules:** `BackupService` (smoke only, not in CTest), `CupsRawPrinter`,
`CupsStatus`, `StockAdjustmentRepository` read path, `AuditRepository` read path, the entire
`ui/` layer (only the screenshot harness renders it), `MedicineForm`, `AppPaths`,
`DesktopIntegration`, `Log`.

## Flakiness risks

- Date/time coupling: receipt/return numbers and several queries use `date('now','localtime')`
  / `QDate::currentDate()` — midnight/timezone edges. Lockout uses wall-clock (asserted only
  `>0`/`==0`, currently safe). `QRandomGenerator` fallback on doc-number collision (rare,
  unasserted).

## Plan (Phases 3 & 8)

- Phase 3: add characterization tests for the High-value gaps above (concurrency, rollback,
  exact ledger values, both policies, Money boundaries), target **≥95% branch on the trust
  core**, add a CI coverage gate. Fix the float money assertion.
- Phase 8: mutation-test the trust core to prove the new tests *kill* bugs (mull — currently
  blocked by LLVM 22 toolchain; see DEBT.md).
