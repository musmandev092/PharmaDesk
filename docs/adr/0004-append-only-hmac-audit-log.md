# ADR-0004: Append-only, HMAC-chained audit log

**Status:** Accepted (append-only implemented; HMAC chain scaffolded, not yet populated)

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
- **−/open:** the HMAC chain is **not yet populated** (columns default to `''`). Until then
  the log is append-only but not cryptographically tamper-*evident*. Implementing it is
  additive; the **key location & rotation policy needs owner sign-off** before the chain
  verifier becomes a release gate. Tracked as finding H2 in DEBT.md / `audit/05`.
