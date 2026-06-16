# PharmaDesk Documentation

- [architecture.md](architecture.md) — layers, the write boundary, data/money invariants.
- [onboarding.md](onboarding.md) — prerequisites, build, run, tests, quality gates, release.
- [security-model.md](security-model.md) — threat model, authn/authz, crypto, audit.
- [MEDICINE_IMPORT_FORMAT.md](MEDICINE_IMPORT_FORMAT.md) — the Excel/CSV import format.
- [mutation-testing.md](mutation-testing.md) — how to run mull on the trust core.
- [PHP_APP_MAP.md](PHP_APP_MAP.md) — the surviving map of the original PHP app (the spec).
- [RUNBOOK.md](RUNBOOK.md) / [HARDENING_PLAN.md](HARDENING_PLAN.md) — ops + prior plan.

## ADRs (decision records)

- [0001](adr/0001-qt-widgets-over-qml.md) — Qt Widgets over QML.
- [0002](adr/0002-sqlite-fixed-point-money.md) — SQLite + fixed-point decimal money.
- [0003](adr/0003-authorized-audited-write-boundary.md) — authorized, audited write boundary.
- [0004](adr/0004-append-only-hmac-audit-log.md) — append-only, HMAC-chained audit log.
- [0005](adr/0005-versioning-and-release.md) — migrations, versioning, automated release.

## Audit

The full engineering audit (scores, Top-25, roadmap, per-area findings) lives in
[`../audit/`](../audit/) — start with [`FINAL_REPORT.md`](../audit/FINAL_REPORT.md). The
technical-debt register and quality-gate ratchet is [`../DEBT.md`](../DEBT.md).
