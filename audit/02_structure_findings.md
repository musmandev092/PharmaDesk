# 02 — Structure Findings

_Phase 0. God-files, long functions, duplication, dead code, leaky layering._

## God-files / god-classes

| Symbol | Where | Size | Smell |
|---|---|---|---|
| `ReturnService` | `service/ReturnService.cpp` | 893 lines | 6 public ops (initiate/adjudicate/commitReturn/voidSale/markSupplierReturnSent/recompute) + private helpers all in one TU. |
| `AdminPage` | `ui/AdminPage.cpp` | 845 lines, 24 methods | Builds Users/Pharmacy/Backup/Settings tabs inline; mixes UI construction, validation, persistence calls. |
| `ReturnsPage` | `ui/ReturnsPage.cpp` | 821 lines, 19 methods | Process/Adjudicate/History tabs inline. |
| `AnalyticsRepository` | `data/AnalyticsRepository.cpp` | 644 lines | ~7 distinct reports (P&L, top-sellers, tax, refunds, velocity, compliance, narcotic) in one class. |
| `SaleService::commit` | `service/SaleService.cpp:54` | **397-line function** | Validate → controlled-gate → discount-auth → FEFO → decrement → ledger → audit → commit, all inline. The single most important function in the app. |

## Longest functions (≥100 lines, heuristic)

| Lines | Location | Kind |
|---|---|---|
| 397 | `SaleService::commit` (`service/SaleService.cpp:54`) | business |
| 236 | `GrnService::post` body (`service/GrnService.cpp:50`) | business |
| 199 | `MainWindow::MainWindow` (`MainWindow.cpp:31`) | UI wiring |
| 196 | `PosTerminalPage` ctor (`ui/PosTerminalPage.cpp:41`) | UI build |
| 191 | `ReturnService::commitReturn` (`ReturnService.cpp:575`) | business |
| 175 | `ReportsPage` ctor (`ui/ReportsPage.cpp:32`) | UI build |
| 159 | `MedicineDialog` ctor (`ui/MedicineDialog.cpp:20`) | UI build |
| 157 | `ReturnService::adjudicate` (`ReturnService.cpp:344`) | business |
| 157 | `ReturnService::initiate` (`ReturnService.cpp:186`) | business |
| 147 | `main` (`main.cpp:23`) | bootstrap |
| 134 | `StockAdjuster::adjust` (`StockAdjuster.cpp:81`) | business |
| 127 | `ReturnService::voidSale` (`ReturnService.cpp:767`) | business |
| 123 | `PosTerminalPage::completeSale` (`ui/PosTerminalPage.cpp:421`) | business-in-UI |

Two patterns dominate: **(a) long Qt constructors** that build an entire screen inline
(low risk, mechanical to extract into `buildX()` helpers), and **(b) long business
functions** in services (`commit`, `post`, return ops) that interleave validation, math,
and persistence (higher value to decompose — these are the trust-core paths).

## Leaky layering

See `01_dependency_graph.md`. Summary: `data/→service/` (L1, High), `domain/→data/` (L2),
UI reaching into `data/`/`QSqlQuery` directly (L3/L4). Business logic occasionally lives in
UI (`PosTerminalPage::completeSale`, 123 lines) rather than a service.

## Duplication

- **Document-number generation** (`nextReceiptNumber`/GRN/return/adjustment) repeats the
  `count(*)+1` → uniqueness re-check → random-suffix fallback pattern across
  `SaleService`, `GrnService`, `ReturnService`, `StockAdjuster`. Candidate for one helper.
- **`SELECT current_qty` → compute before/after → write ledger row** repeats at 8 stock-
  mutation sites (sale, adjust, 3× restock, 2× GRN, batch-add). A `recordMovement(...)`
  helper would remove the copy-paste and centralize the `qty_before/qty_after` invariant.
- **Report-widget construction** (`*ReportWidget` ctors) share date-range + table-build
  boilerplate.
- **Money percentage** `ratioMul(x, fromUnits(1000000), y)` repeated in
  `AnalyticsRepository` (3×) — a `percentOf()` helper.

## Dead / unreferenced code

- `tests/barcode_test.cpp` duplicates `tests/cases/barcode_tests.cpp` coverage (two
  barcode targets). Not dead, but redundant.
- `service/ThermalPrinter.h` — header-only interface; verify it has a live implementor.
- `Cups{RawPrinter,Status}` have **no tests** and are hardware-coupled; not dead but
  unexercised in CI.
- No large blocks of commented-out code found (clean in that respect).

## Positive structural notes

- Clear 4-layer package layout; domain is *almost* a pure leaf (one violation, L2).
- Money is a proper fixed-point type, not float (see 06).
- Every stock mutation is transactional and writes an `inventory_movements` ledger row
  (see 06) — the strongest part of the codebase.
- Tests use **real SQLite + real repositories** (no data-layer mocking).
