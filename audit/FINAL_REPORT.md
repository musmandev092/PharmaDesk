# PharmaDesk — Engineering Audit: Final Report

_Commit `12a6316`, branch `dev`. Baseline at audit start: build green, `ctest` 100% pass,
`clang-format` clean._

> **Context that shapes everything:** the PHP reference app (`../pos`) and original Postgres
> DDL (`../db`) are **absent on this machine**. The behavioral spec survives only as
> `docs/PHP_APP_MAP.md`, header comments, and a prior 409-finding audit
> (`docs/audit_findings.json`). Characterization tests (Phase 3) therefore pin **current**
> C++ behavior and become the de-facto spec.

## Scores

| Dimension | Grade | One-line justification |
|---|---|---|
| Architecture | **B** | Clean 4 layers; one reverse edge (`data→service`), thin service layer, UI-level authz. |
| Security | **C+** | Correct crypto/lockout/parameterized SQL; but UI-only authz (void-sale hole) + unimplemented audit HMAC chain. |
| Maintainability | **B** | Readable & idiomatic; held back by 5 god-files and a 397-line `commit()`. |
| Performance | **A−** | Adequate for single-PC; only missing FK indexes + wildcard search. |
| Testability | **B** | ~1,200 real-SQLite assertions / 19 modules; gaps in concurrency/rollback/policies + a float money assert. |
| Documentation | **B−** | Great inline docs; missing architecture/onboarding/security-model/ADRs. |
| Tech-Debt | **B−** | Localized & visible; no CI gates yet, audit HMAC deferred, mutation testing absent. |
| **Overall** | **B / B−** | A well-built product with concentrated, fixable risk — not a rewrite candidate. |

**Target after this engagement:** Architecture A−, Security A−, Maintainability A−,
Testability A, Docs A−, Tech-Debt A−, **Overall A−**.

## Top-25

### A. Top risks (fix-worthy)
1. **(High/Sec)** Void sale has zero authorization — any cashier can void any sale. `ReturnService::voidSale`, `ReturnsPage.cpp:629`.
2. **(High/Sec)** Audit log not tamper-evident — HMAC chain columns unpopulated. `Audit.cpp`, schema:386.
3. **(High/Sec)** Non-atomic best-effort audit on medicine update/create + user update. `MedicineRepository.cpp:209`, `UserRepository.cpp:222`.
4. **(High/Sec)** Settings changes entirely unaudited + non-atomic `setMany`. `SettingsRepository.cpp:30`.
5. **(High/Data)** SQL float arithmetic on a TEXT money column. `ReturnService.cpp:99-104`.
6. **(High/Arch)** Authorization enforced at UI only, not the service boundary (systemic).
7. **(Med/Data)** Missing FK indexes — esp. `returns.sale_item_id` (per-return scan).
8. **(Med/Data)** No `CHECK(qty_after = qty_before + qty_delta)` on `inventory_movements`.
9. **(Med/Test)** Float money assertion in `returns_tests.cpp:449` (violates project rule).
10. **(Med/Sec)** GRN posting (high-value write) has no credential prompt.
11. **(Med/Data)** `bumpSessionRefund`/`bumpForSale` swallow `exec()` failures (silent cash loss).
12. **(Med/Sec)** `OVERRIDE_GRANTED` audit committed pre-txn → can orphan on rollback.
13. **(Med/Test)** Concurrency/rollback branches of `SaleService::commit` untested.
14. **(Med/Correctness)** FEFO UTC `date('now')` vs local `QDate::currentDate()` midnight split. `SaleService.cpp:270` vs `Fefo.cpp:22`.
15. **(Med/Correctness)** `CostBlender` fails *open* (returns `0.0000`) on bad input where spec implies throw.

### B. Top fixes
16. Route every privileged write through a service `authorize() + audit() + txn` seam (keystone).
17. Implement the audit HMAC chain + off-DB key + chain verifier.
18. Wrap medicine/user/settings writes in one transaction with `writeOrThrow`.
19. Fix `bumpSessionRefund` to add via `Money`, not SQL float.
20. Additive migration: FK indexes + ledger `CHECK` constraint.
21. Replace the float money assertion with `Money`/string comparison.

### C. Top refactors
22. Extract shared sale DTOs to a neutral header → delete the `data→service` edge.
23. Decompose `SaleService::commit` (397) and `ReturnService` into named, tested steps.
24. Split `AdminPage`/`ReturnsPage` tab-builders into separate TUs (mechanical).
25. Add a header-include linter enforcing UI→service→domain (+ `data→domain`) direction.

## Roadmap (phased, each slice build+test-green before commit)

| Phase | Deliverable | Risk |
|---|---|---|
| 2 | CI (`ci.yml`): build+ctest+clang-format(blocking)+clang-tidy/cppcheck(report)+coverage+ASan/UBSan. `DEBT.md` with baseline counts + ratchet. | Low |
| 3 | Trust-core characterization tests (Money/SaleService/ReturnService/CostBlender/Fefo/policies); fix float assertion; ≥95% branch on critical set + coverage gate. | Low |
| 4 | **Keystone:** authorized+audited write seam; fix void-sale authz; atomic audit on medicine/user/settings. Tests: denied role writes nothing, authorized write atomic, failure rolls back. | Med |
| 5 | Data integrity: FK-index + ledger-CHECK migration; fix SQL-float refund; audit HMAC chain + off-DB anchor; row-scanning invariant test. | Med (migration = additive/reversible/test-guarded) |
| 6 | Decompose god-files; break `data→service` edge; include-linter layer gate; "launches every screen" smoke. | Med |
| 7 | Docs tree: architecture, onboarding, build, security model, ADRs. | Low |
| 8 | Mutation testing (mull) on trust core; honest scores; equivalent mutants documented. **Blocked:** mull vs LLVM 22 — needs pinned-LLVM container (see DEBT.md). | Low |
| 9 | **Finish line:** `build.yml` Release → CPack TGZ + sha256 + GitHub Release on `main`; `scripts/bump_version.sh`; version shown in UI from one source. | Low |

### Items flagged for owner decision (not done unsupervised)
- **Audit HMAC key management** (Phase 5): introduces a new secret/key. Will be done
  additively (chain populated forward, old rows tolerated) and test-guarded; the **key
  location & rotation policy** needs your call before enabling verification-as-a-gate.
- **mull / LLVM-22:** mutation tooling isn't installable on the host toolchain; proposed
  pinned-LLVM Docker path documented in DEBT.md rather than altering the system toolchain.
- No production data migration from the legacy Postgres app is in scope (and the source is
  absent) — called out in `docs/audit_findings.json` as the largest external gap.
