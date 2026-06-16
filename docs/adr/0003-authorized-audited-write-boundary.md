# ADR-0003: Authorized, audited write boundary

**Status:** Accepted (partially implemented; follow-ups tracked in DEBT.md)

## Context
The most damaging systemic risk in apps like this is high-stakes writes running inline in UI
widgets with no authorization and no audit. An audit confirmed a real exploit: any cashier
could **void** (fully refund) any sale because the only gate was a confirm dialog.

## Decision
Route every privileged write through a **service/application-layer** function that, in one
DB transaction:
1. checks authorization (role/permission) at the service boundary — not only in the UI, and
2. writes an `audit_log` record via `Audit::writeOrThrow`, so a failed audit rolls back the
   business change.

Invariants to prove per operation: a **denied role writes nothing**; an **authorized write
is atomic** across all tables; a **failure rolls back**.

## Consequences
- **+** Defense in depth: UI gating is a convenience, the service is the boundary.
- **+** Who-did-what-when is captured for every privileged change, atomically.
- **−** Some repository methods that currently audit best-effort/non-atomically must be
  migrated (medicine create within the importer txn, user update, settings). Tracked in
  DEBT.md; done so far: `voidSale` authorization, `MedicineRepository::update` atomicity.
- Proven by `returns` test 6b (cashier denied + nothing written; manager allowed + audited).
