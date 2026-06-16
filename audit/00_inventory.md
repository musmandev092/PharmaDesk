# 00 — Inventory & Target Map

_Phase 0 discovery. Generated against commit `12a6316` (branch `dev`)._

## What PharmaDesk is

A native **Qt 6 / C++17** desktop pharmacy POS for a single AlmaLinux 10 PC, backed by
**SQLite** (Qt SQL `QSQLITE`). It is a port of a PHP 8.4 / PostgreSQL web app. **The PHP
source is no longer present on this machine** (`../pos` and `../db` do not exist) — the
behavioral spec now survives only as `docs/PHP_APP_MAP.md`, the PHP-citing comments in
each header, and a prior 409-finding audit (`docs/audit_findings.json`). _Implication:_
characterization tests must pin **current** C++ behavior; line-for-line PHP re-diffing is
not possible.

## Size

| Metric | Value |
|---|---|
| Source `.cpp`/`.h` (src + tests) | ~29,000 lines |
| Translation units in `pharmadesk_core` | 90 `.cpp` |
| UI widgets/pages/dialogs | 47 |
| Service classes | 11 |
| Domain (pure logic) classes | 14 |
| Data/repository classes | 22 |
| Test files | 21 (`tests/cases/*` + smoke/screenshots/barcode) |
| Test assertions (gated suite) | ~1,200 across 19 modules |

## Build targets (`CMakeLists.txt`)

| Target | Type | Purpose |
|---|---|---|
| `pharmadesk_core` | STATIC lib | All app code except `main()`. Shared by app + tests. |
| `pharmadesk` | executable | The GUI application (`src/main.cpp` + `resources.qrc`). |
| `pharmadesk_import` | executable | CLI to load the legacy medicines CSV (`tools/import_catalog.cpp`). |
| `pharmadesk_smoke` | executable | Legacy end-to-end smoke (NOT registered with CTest). |
| `pharmadesk_shots` | executable | Screenshot harness (not a test). |
| `pharmadesk_barcode_test` | executable | Standalone barcode test (CTest: `barcode`). |
| `pharmadesk_tests` | executable | Aggregate suite, 19 modules (CTest: `suite`). |

CTest registers **2** tests (`suite`, `barcode`), both headless via
`QT_QPA_PLATFORM=offscreen`, `TIMEOUT 300`. Qt6 preferred, Qt5 fallback. bcrypt via OS
`libxcrypt` (no vendored crypto).

**Baseline status at audit start:** configure + build **green** (Ninja, exit 0); `ctest`
**100% pass** (suite 12.7s, barcode 0.01s); `clang-format --dry-run -Werror` **clean**.

## Layer layout (`src/`)

```
src/
  main.cpp, MainWindow.*, Branding.h          ← composition root + shell
  domain/    14 classes   pure logic: Money, SaleCalculator, Fefo, CostBlender,
                          Bcrypt, PinPolicy, BarcodeParser, *Policy, MedicineForm,
                          AppPaths, DesktopIntegration, Log
  service/   11 classes   SaleService, ReturnService, GrnService, StockAdjuster,
                          Reconciler, BackupService, AlternativesFinder,
                          ZReportArchive, Cups{RawPrinter,Status}, ThermalPrinter.h
  data/      22 classes   Database + repositories + Audit + EscPosRenderer +
                          CatalogImporter
  ui/        47 widgets   pages, dialogs, report widgets, Theme, UiUtil; ui/setup/
```

## Largest translation units (decomposition candidates)

| Lines | File | Notes |
|---|---|---|
| 893 | `src/service/ReturnService.cpp` | initiate/adjudicate/commitReturn/voidSale — god-service |
| 845 | `src/ui/AdminPage.cpp` | 24-method god-class, multiple tabs inline |
| 821 | `src/ui/ReturnsPage.cpp` | 19-method god-class |
| 644 | `src/data/AnalyticsRepository.cpp` | many report queries |
| 595 | `src/ui/PosTerminalPage.cpp` | POS terminal |
| 538 | `src/ui/SessionsPage.cpp` | |
| 494 | `src/data/UserRepository.cpp` | auth + user mgmt + lockout |
| 450 | `src/service/SaleService.cpp` | `commit()` is a 397-line function |

## Non-source assets

`sql/schema_sqlite.sql` (baseline schema, `user_version` 0), `resources/theme.qss`
(design tokens), `medicines_export.csv` (318-row catalog seed), `packaging/`
(AppImage scripts + `.desktop` + icons), `docs/` (PHP_APP_MAP, HARDENING_PLAN,
RUNBOOK, audit_findings.json).
