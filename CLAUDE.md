# CPHC Pharmacy — PHP → Native Qt/C++ Port

**Project:** Port the existing CPHC Pharmacy POS web app (plain PHP 8.4 + Postgres
17, Dockerized, at `../pos`) to a native Qt 6 / C++ desktop application for a
single AlmaLinux 10 PC.

**The PHP app is the specification.** Read it as the source of truth for screens,
workflows, validation rules, and the database schema. Reproduce its behavior; do
not invent new features or change business rules unless explicitly asked.

The PHP source lives at `../pos` (and shared schema/triggers at `../db`). Key
spots: `../pos/backend/services/` (domain logic), `../pos/frontend/pages/`
(screens), `../pos/frontend/assets/app.css` (design tokens), `../db/schema/init.sql`
(schema), `../db/triggers/` (DB invariants). A structure map is in
`docs/PHP_APP_MAP.md` — read it before porting any module.

---

## Corrections to the original porting brief (the source overrides the brief)

The original brief made two assumptions that the actual code contradicts. The
**code wins**:

1. **Database is PostgreSQL 17, not MySQL.** Type mapping when porting to SQLite:
   - `SERIAL` / `BIGSERIAL` → `INTEGER PRIMARY KEY AUTOINCREMENT`
   - Postgres `ENUM` types → `TEXT` + `CHECK (col IN (...))`
   - `TIMESTAMPTZ` / `DATE` → ISO-8601 `TEXT`
   - `DECIMAL(12,2)` / `DECIMAL(12,4)` → see money rule below
   - `BOOLEAN` → `INTEGER` (0/1); `JSONB` (audit before/after) → `TEXT` (JSON)

2. **Money is decimal, NOT integer paisa.** The PHP `Money` class
   (`../pos/backend/Money.php`) uses base-10 decimal math (bcmath) with two
   scales: **scale-2 for money** (subtotal, tax, discount, total, tendered,
   change) and **scale-4 for cost** (`batches.cost_per_unit`, blended cost).
   Rounding is **HALF_UP at every storage boundary**. A naive integer-paisa
   representation cannot hold the 4-dp cost values and would diverge from the PHP
   results. Port `Money` as a fixed-point decimal type that mirrors the PHP
   semantics exactly (store cost at scale 4, money at scale 4 in computation and
   round to scale 2 at storage). See `SaleCalculator.php` for the rounding order
   (subtotal − discount + tax, rounded once at the end). **Match these results
   bit-for-bit and cover them with tests.**

---

## Target stack (do not deviate without asking)

- C++17 or newer, Qt 6, Qt **Widgets** (not QML)
- CMake build — `find_package(Qt6 REQUIRED COMPONENTS Widgets Sql PrintSupport)`
  (the current `CMakeLists.txt` also falls back to Qt5 so it builds on the dev
  machine until Qt6 is installed; the AlmaLinux target uses Qt6)
- SQLite via Qt SQL (`QSQLITE` driver); enable `PRAGMA foreign_keys = ON` and
  wrap every multi-statement change in a transaction
- Package output: AppImage (built on AlmaLinux 10 itself)

## How to work

- Incrementally, one module at a time. Keep the project compiling and runnable
  after every step. No big-bang rewrites.
- Before porting a module, summarize how the PHP version behaves and confirm the
  mapping first.
- Use `QSqlTableModel` / `QSqlRelationalTableModel` bound to `QTableView` for list
  and grid screens.
- Keep clear layers: **data access (SQLite) → domain logic (plain C++ classes) →
  UI (Qt Widgets)**. Business rules stay out of the UI.

## Match the existing UI (visual fidelity)

- Keep the same look and feel as the current web app — same palette, fonts,
  spacing, button styling, layout. It should feel like the same product, native.
- Design tokens are already extracted from `../pos/frontend/assets/app.css` into
  `resources/theme.qss` (single source of truth, applied app-wide via
  `qApp->setStyleSheet`). Reuse these tokens; don't hardcode colors elsewhere.
- Honest limit: Qt Widgets render differently from HTML/CSS. Aim for a faithful
  match of colors, fonts, and feel, not a pixel-for-pixel copy. Nail the palette
  and component styling first; chase exact spacing only where it matters.

## Pharmacy guardrails (critical — medical retail system)

- Anything touching **quantities, pricing, stock decrements, batch numbers, and
  expiry dates** must be ported exactly and covered by tests comparing results
  against PHP behavior.
- Money: fixed-point decimal matching the PHP `Money` semantics (see correction 2
  above). Never floating point for stored monetary values.
- All sales and stock changes go through DB transactions; never leave stock in an
  inconsistent state on error. Mirror the `inventory_movements` ledger (every
  stock change records qty_before/qty_after) and the FEFO batch-selection logic
  (`../pos/backend/services/Fefo.php`).
- Audit trail: the PHP app has an append-only, HMAC-chained `audit_log`. Reproduce
  who-did-what-when for sales and stock adjustments (the HMAC chain can be
  simplified for single-PC SQLite, but keep the audit records).

## Don't

- Don't change the schema's *meaning* during the port (renaming for clarity is
  fine; preserve semantics).
- Don't skip tests on the money / stock / expiry paths.
- Don't try to one-shot the whole app.
