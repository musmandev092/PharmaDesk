# ADR-0004: Append-only, HMAC-chained audit log

**Status:** Accepted — append-only AND HMAC chain implemented (`Audit::write` /
`Audit::verifyChain`). Open: key-rotation policy.

## Context
A pharmacy's audit trail (sales, voids, stock/price/role changes, controlled dispensing)
must be tamper-evident for compliance. The source app used an HMAC-chained append-only
`audit_log`.

## Decision
- `audit_log` is **append-only at the DB level**: `BEFORE UPDATE`/`BEFORE DELETE` triggers
  `RAISE(ABORT, …)`. The signed `z_report_archive` gets the same treatment (migration v5).
- The table carries `prev_hmac`/`row_hmac` columns for a **hash chain**:
  `row_hmac = HMAC(key, canonical(row) ‖ prev_hmac)`, with the key stored **off the DB**.
- Audit rows are written inside the business transaction (`writeOrThrow`).

## Consequences
- **+** In-place edits/deletes are blocked even with raw SQLite access.
- **+** Once the chain is populated, wholesale DB-file substitution becomes detectable
  (the chain won't verify).
- **+ Implemented:** `Audit::write` populates `prev_hmac`/`row_hmac` on every insert
  (`row_hmac = HMAC_SHA256(audit.key, prev_hmac ‖ canonical(row))`); `Audit::verifyChain`
  re-derives and validates the chain. The key lives off-DB in `audit.key` (owner-only),
  generated once. Editing a row, forging a `row_hmac`, or dropping+reinserting breaks the
  chain. Covered by `audit_chain` tests + invariant INV-11.
- **− Open:** **key rotation** — a single per-install key today; rotating it invalidates
  verification of rows signed by the old key, so rotation must archive the old key with the
  rows it signed. Owner decision; documented in `docs/security-model.md`.
