# 06 — Data Integrity Audit

## Summary — Grade **B+**

The persistence layer is the strongest part of PharmaDesk: money is fixed-point decimal
(never float in C++), `foreign_keys=ON` is enforced on every connection, every stock
mutation is transactional and writes a paired `inventory_movements` ledger row, and the
audit table is trigger-protected append-only. The gaps are a single SQL-float leak
(H5/§1.1), missing FK indexes, a few absent CHECK constraints, and the unimplemented audit
HMAC chain (H2).

## 1. Money / quantity storage

**All money & cost columns are `TEXT` decimal strings — no `REAL`/float anywhere.**
Quantities are `INTEGER` base-units. `Money` is an `int64` at scale-4 internally
(`m_units = value × 10000`), round-trips to the DB as a fixed-point string via
`fromString`/`toString` with HALF_UP, and all arithmetic is overflow-checked. This is a
correct money port and satisfies the "no float for stored money" requirement.

> The prompt's "integer minor-units" intent is met by the fixed-point decimal type — the
> values are stored as canonical decimal strings and computed as scaled int64, never as
> doubles. **Do not rewrite storage to integer cents** — cost is `DECIMAL(12,4)` (4 dp),
> which integer-paisa cannot hold, and the existing money tests pin the string form.

**One leak (High, H5):** `ReturnService::bumpSessionRefund` updates a money TEXT column
with `col = col + ?`, which SQLite evaluates as double arithmetic. Fix by reading,
adding via `Money`, and writing the string back (`SessionRepository::bumpForSale` already
does it correctly).

## 2. Foreign keys & indexes

`PRAGMA foreign_keys = ON` set on open (`Database.cpp:31`) with WAL + `synchronous=FULL` +
`busy_timeout=5000`. FK actions well-specified (RESTRICT masters, CASCADE children, SET
NULL optionals). **SQLite does not auto-index FK columns**, so several FK / hot-join
columns table-scan today:

Highest value: **`returns.sale_item_id`** — `SELECT SUM(qty_returned_units) WHERE
sale_item_id=?` runs on every return. Then `batches.supplier_id/grn_id`,
`grn_lines.medicine_id/batch_id`, `sales.cashier_session_id`,
`stock_adjustments.medicine_id`, `grn_documents.received_by`.

Proposed additive migration (Phase 5):
```sql
CREATE INDEX idx_returns_sale_item   ON returns(sale_item_id);
CREATE INDEX idx_returns_medicine    ON returns(medicine_id);
CREATE INDEX idx_returns_batch       ON returns(batch_id);
CREATE INDEX idx_batches_supplier    ON batches(supplier_id);
CREATE INDEX idx_batches_grn         ON batches(grn_id);
CREATE INDEX idx_grn_lines_medicine  ON grn_lines(medicine_id);
CREATE INDEX idx_grn_lines_batch     ON grn_lines(batch_id);
CREATE INDEX idx_sales_session       ON sales(cashier_session_id);
CREATE INDEX idx_stock_adj_medicine  ON stock_adjustments(medicine_id);
CREATE INDEX idx_grn_docs_received_by ON grn_documents(received_by);
```

## 3. Constraints

Good: CHECK enums on all status/role/payment/movement-type columns; non-negative CHECKs on
`batches` qty; UNIQUE on receipt/return/grn/adjustment numbers; partial-unique on live
`medicines.sku`/`primary_barcode`.

Gaps:
- **No `CHECK(qty_after = qty_before + qty_delta)` on `inventory_movements`** (Medium) — the
  ledger's core arithmetic invariant is unenforced. Add it (Phase 5); it backstops every
  hand-computed before/after.
- `medicines.form` (~55-value enum) enforced only in C++, not the DB (Low).
- `returns`/`stock_adjustments`/`sale_items` qty & money lack non-negative CHECKs that the
  services already enforce (Low).

_(The original Postgres DDL is absent, so "constraints dropped in the port" cannot be
diffed byte-for-byte — these are gaps vs. the C++ validation layer, not vs. Postgres.)_

## 4. Migrations

Versioned via `PRAGMA user_version` stepping (`Database.cpp:189-316`): an ordered registry
(v1–v5), `migrate()` runs `(current+1)..N`, **each migration in its own transaction** with
atomic `setSchemaVersion`+commit and rollback on failure. Fresh DB → `bootstrap()` (baseline
= v0) → `migrate()`; existing DB → `migrate()` only. Idempotent and correct.

**Additive-only; no down-migrations.** Acceptable for a single-PC till (recovery =
restore-from-backup), but worth documenting the "backup before upgrade" expectation. New
schema changes (FK indexes, the ledger CHECK) must be added as **new migrations**, never by
editing the baseline file.

## 5. Transactions

Every mutating path wraps in `transaction()`/`commit()` with rollback on error and in every
catch block: sale, all 5 return entry points, GRN post, stock adjust, reconcile,
batch add, catalog import. **Audit writes happen inside the same transaction** via
`writeOrThrow`, so a failed audit rolls back the business op — except the three non-atomic
paths in H3 (medicine update/create, user update) and unaudited settings (H4).

Two writes ignore `exec()` results (`bumpSessionRefund`, `bumpForSale`) — a failure there
silently fails to record cash without rolling back (Medium). Fix: check + throw inside the
open txn.

## 6. inventory_movements ledger

**Every one of the 8 `current_qty` mutation sites writes a paired ledger row with
`qty_before`/`qty_after` in the same transaction** (sale, adjust, 3× restock, 2× GRN, batch-
add). Zero-delta events (RETURN_QUARANTINE, WRITE_OFF) correctly recorded with delta 0.
**No path mutates `current_qty` without a ledger row.** Caveat: `qty_before` comes from a
separate pre-read rather than the guarded UPDATE; only the **sale** path is guarded
(`UPDATE … WHERE current_qty >= ?` + `numRowsAffected()==1`). The ledger CHECK (§3) plus an
invariant test (§7) backstop the unguarded restock/adjust/GRN paths.

## 7. Audit log & invariant tests

Audit table: `prev_hmac`/`row_hmac` present but unpopulated (H2). Append-only enforced by
triggers. Phase 5 adds the HMAC chain + a row-scanning **invariant test** that must return
zero rows on a healthy DB:

- INV-1 `qty_after = qty_before + qty_delta` for all movements.
- INV-2 `batches.current_qty == SUM(ledger.qty_delta)` per batch (core stock invariant).
- INV-3 no negative stock. INV-4 movement-type ↔ delta-sign consistency.
- INV-5 returns never exceed sold qty per line. INV-6 sale header = Σ line totals (in `Money`).
- INV-7 money TEXT columns have ≤4 frac digits / no sci-notation (catches H5 drift).
- INV-8 every stock-adjustment row has a matching ledger row.
- INV-9 audit triggers still installed. INV-10 `PRAGMA foreign_key_check` returns nothing.

## Severity count

High 2 (H2 HMAC, H5 SQL-float) · Medium 3 (FK indexes, ledger CHECK, swallowed exec) ·
Low 4.
