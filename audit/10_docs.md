# 10 — Documentation Audit

## Summary — Grade **B−**

Strong inline/header documentation and a useful `docs/` set, but missing the structural
docs a maintainer needs: an architecture overview, an onboarding/build guide, a written
security model, and ADRs. Phase 7 fills these.

## What exists

| Doc | Quality | Notes |
|---|---|---|
| `CLAUDE.md` | A | Excellent porting brief, money/stock guardrails, stack rules. |
| `docs/PHP_APP_MAP.md` | B+ | The surviving spec map (PHP source itself is gone). |
| `docs/HARDENING_PLAN.md` | B | Prior plan; reconcile with this audit's roadmap. |
| `docs/RUNBOOK.md` | B | Ops runbook. |
| `docs/audit_findings.json` | A | Prior 409-finding audit (2 critical, 74 high) — rich, machine-readable. |
| `README.md` | B | Present. |
| Header comments | A− | High-signal, cite PHP behavior, explain *why*. |

## Gaps (Phase 7 deliverables)

1. **Architecture overview** — the 4-layer model, the write boundary, the data/ledger/audit
   invariants, a diagram. (None today outside this audit.)
2. **Onboarding / build guide** — prerequisites (Qt6, libxcrypt, CMake, Ninja), configure/
   build/test commands, how to run a single suite, how to add a test module, the
   offscreen-platform requirement.
3. **Security model** — threat model (single-PC, insider/compliance), authn/authz design,
   the audit HMAC chain, backup/secret handling, what's in/out of scope.
4. **ADRs** — the five decisions in 04 (Widgets-over-QML, SQLite+fixed-point money, the
   authorized+audited write seam, the append-only HMAC audit log, forward-only migrations).
5. **CONTRIBUTING / quality gates** — the ratchet model, the floor rule, how DEBT.md works.
6. **Migration/upgrade doc** — "backup before upgrade", `user_version` stepping, no down-path.

## Note

The single biggest documentation risk is that **the PHP spec source is gone** — so
`docs/PHP_APP_MAP.md`, the header comments, and the new characterization tests (Phase 3) are
now the *only* record of intended behavior. Phase 3 tests therefore double as executable
documentation and must be treated as a first-class spec artifact.
