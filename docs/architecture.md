# PharmaDesk Architecture

A native **Qt 6 / C++17** desktop pharmacy POS for a single AlmaLinux 10 PC, backed by
**SQLite** (Qt SQL `QSQLITE`). Ported from a PHP 8.4 / PostgreSQL web app.

## Layers

```
ui/        Qt Widgets — presentation + input. Calls services for writes.
service/   use-cases — own authorization + transaction + audit (the write boundary).
data/      repositories — SQL/persistence. No upward deps. Shared DTOs in domain/.
domain/    pure logic — Money, SaleCalculator, Fefo, CostBlender, policies. A leaf.
```

Dependency direction is **ui → service → data → domain** and is enforced by
`tools/check_layering.py` (a blocking CI gate). `domain/` depends on nothing internal.

`main.cpp` is the composition root: open the DB → `bootstrap()` + `migrate()` →
authenticate (LoginDialog) → build `MainWindow`. Dependencies are injected by hand via
constructors (`QSqlDatabase`, `UserRecord`, repositories) — no global mutable state.

All app code except `main()` lives in the static library **`pharmadesk_core`**, shared by
the GUI app, the headless smoke/screenshot harnesses, and the test suite — so tests
exercise the exact code the app runs.

## The write boundary (security keystone)

Every privileged write (sale, return, void, stock adjustment, GRN post, user/medicine
changes) goes through a **service/repository** function that, in one DB transaction:
1. checks authorization (e.g. `voidSale` requires an active manager/admin — enforced in
   the service, not just the UI), and
2. writes an `audit_log` record via `Audit::writeOrThrow`, so a failed audit rolls back the
   business change.

This means a denied role writes **nothing**, an authorized write hits all tables
atomically, and any failure rolls back. See `docs/security-model.md`.

## Data & money invariants

- **Money is fixed-point decimal**, never float: an `int64` at scale-4 internally
  (`domain/Money`), stored as canonical decimal **TEXT** (scale-2 money, scale-4 cost),
  HALF_UP at storage boundaries. Never do arithmetic on money TEXT columns in SQL.
- **Every stock change is transactional and ledgered**: it writes an `inventory_movements`
  row with `qty_before`/`qty_after`; a v6 trigger enforces
  `qty_after = qty_before + qty_delta`. FEFO (`domain/Fefo`) drives batch selection
  (soonest expiry first); the sale decrement is guarded
  (`UPDATE … WHERE current_qty >= ?`) so it can never oversell.
- **Audit log is append-only** (DB triggers block UPDATE/DELETE). The HMAC tamper chain is
  scaffolded in the schema (`prev_hmac`/`row_hmac`) and is a tracked follow-up (DEBT.md).
- Invariants are checked by `tests/cases/invariant_tests.cpp` (INV-1..10) — a set a nightly
  self-check could also run.

## Persistence

SQLite with `PRAGMA foreign_keys=ON`, `journal_mode=WAL`, `synchronous=FULL` (a cash till
must survive power loss). Schema migrations are forward-only via `PRAGMA user_version`
stepping (`data/Database.cpp`), each migration in its own transaction. The baseline schema
is `sql/schema_sqlite.sql` (version 0); changes are appended as numbered migrations, never
folded back.

## Module map

- **domain/**: Money, SaleCalculator, Fefo, CostBlender, Bcrypt, PinPolicy, BarcodeParser,
  ControlledSubstancePolicy, DiscountAuthorizationPolicy, MedicineForm, SaleTypes (DTOs),
  AppPaths, DesktopIntegration, Log.
- **service/**: SaleService, ReturnService, GrnService, StockAdjuster, Reconciler,
  BackupService, AlternativesFinder, ZReportArchive, Cups{RawPrinter,Status}.
- **data/**: Database + repositories (Medicine, Batch, Inventory, Sale, Return, Grn,
  Supplier, User, Session, Settings, Analytics, SalesReport, StockAdjustment, ParkedCart,
  Audit), CatalogImporter, XlsxReader, EscPosRenderer.
- **ui/**: pages, dialogs, report widgets, Theme, UiUtil, setup wizard.

See `audit/00_inventory.md` for the full file/target map and `audit/FINAL_REPORT.md` for
scores and the roadmap. Decisions are recorded as ADRs in `docs/adr/`.
