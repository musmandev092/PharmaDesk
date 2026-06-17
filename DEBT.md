# Technical-Debt Register & Quality Gates

This file is the **source of truth for the ratchet**. Each tracked gate has a baseline
count. The rule is simple:

> **THE FLOOR RULE — no change may RAISE any count below.** Clean up every finding your own
> change introduces. When a count reaches **0**, flip that gate from _report-only_ to
> _blocking_ in `.github/workflows/ci.yml` and record the flip here.

Baselines captured at commit `3e392b8` (branch `dev`), host: clang/clang-tidy 22.1, cppcheck 2.21.

## Gate status

| Gate | Baseline | Mode | Notes |
|---|---:|---|---|
| build + ctest | green / 2 suites pass | **BLOCKING** | Already green. |
| clang-format `--Werror` | **0** | **BLOCKING** | Tree already clean. |
| layering (include direction) | 1 baseline | **BLOCKING** | `tools/check_layering.py`; only the documented L2 (`DesktopIntegration→data`) allow-listed. New reverse/skip edges fail. |
| ASan + UBSan (test suite) | clean | **BLOCKING** | See verification note below. |
| clang-tidy | **750** | report-only | Ratchet to 0, then blocking. Breakdown below. |
| cppcheck | **9** | report-only | Ratchet to 0, then blocking. Breakdown below. |
| Trust-core LINE coverage | **99.1%** | **BLOCKING** (`--fail-under-line 95`) | Money/SaleCalculator/CostBlender/Fefo/PinPolicy + both policies. |
| Trust-core branch coverage | 69.0% | report-only | Remaining branches are defensive/exception paths; a hard branch gate would invite fake tests. |

There is no third-party dependency CVE gate: the only external libraries are OS packages
(Qt6, libxcrypt) consumed via the system package manager, not a vendored/locked dependency
tree. Dependency-CVE scanning would belong to the distro, not this repo.

## clang-tidy baseline = 750 (report-only)

| Check | Count | Drain strategy |
|---|---:|---|
| bugprone-throwing-static-initialization | 405 | One header-level static pattern counted per-TU; fix the single root (or suppress the rule with a documented reason if the static is provably safe). |
| performance-unnecessary-value-param | 121 | Pass heavy params by `const&`. Apply case-by-case, re-run tests. |
| performance-move-const-arg | 121 | Remove redundant `std::move` on const/return. |
| misc-const-correctness | 90 | Add `const` to locals. Low risk, mechanical. |
| clang-diagnostic-error | 79 | Artifacts of incomplete moc/AUTOMOC context under `run-clang-tidy`; resolve by feeding generated headers, not real code errors (build itself is clean). |
| performance-no-automatic-move | 5 | |
| other (enum-size, widening, empty-catch, …) | 29 | Triage individually; `bugprone-empty-catch` is worth a real look. |

> NOTE: counts are measured with `HeaderFilterRegex` limited to `src/`. Do **not** drain by
> loosening the check list — that games the ratchet. Drain by fixing, or by suppressing a
> rule with a written justification (per the discipline: a false-positive on correct code →
> suppress the RULE with a reason, don't contort the code).

## cppcheck baseline = 9 (report-only)

`useStlAlgorithm`, `unknownMacro`, `uninitMemberVarNoCtor`, `shadowVariable`,
`shadowFunction`, `redundantInitialization`, `noExplicitConstructor`,
`knownConditionTrueFalse`, `constParameterReference` — one each. All drainable; `uninitMemberVarNoCtor`
and `noExplicitConstructor` are worth fixing first.

## Keystone / audit-atomicity status (Phase 4)

- **voidSale authorization (H1) — FIXED.** `ReturnService::voidSale` now enforces an
  active-manager/admin check at the service boundary before any write; a denied role
  writes nothing (proven by `returns` test 6b). An authorized void still succeeds and is
  audited atomically.
- **MedicineRepository::update (H3) — FIXED.** UPDATE + `MEDICINE_UPDATED` audit now commit
  in one transaction; an audit failure rolls back the field/price change.
- **Still open (flagged, NOT yet fixed):**
  - `MedicineRepository::create` and the catalog importer: `create()` is called *inside the
    importer's* transaction, so it can't open its own (Qt has no nested transactions). During
    bulk import the audit IS atomic; the single-add-via-dialog path is not. A safe fix needs
    an "in-transaction?" flag threaded through — deferred to avoid breaking the importer.
  - `UserRepository::updateUser` (role change) non-atomic audit; `SettingsRepository::set`
    unaudited. Both are owner-review items (touch role/settings policy) — Phase 4 follow-up.

- **Audit HMAC chain (security H2) — DONE.** `Audit::write` now populates
  `prev_hmac`/`row_hmac` on every insert (`row_hmac = HMAC_SHA256(key, prev_hmac ‖ canonical)`,
  field order mirroring the PHP Postgres trigger). The per-install key is off-DB in
  `audit.key` (owner-only). `Audit::verifyChain` walks + re-derives the chain; covered by
  `audit_chain` tests (positive + tamper detection) and invariant INV-11 (whole-DB chain).
  **Key rotation — DONE:** `pharmadesk --rotate-audit-key` archives the current key and starts a
  fresh one; `verifyChain` accepts the current or any archived key, so pre-rotation rows still
  verify (planned-refresh policy in `docs/security-model.md`; tested in `audit_chain`).
- **Mutation testing (Phase 8).** `mull` is not installable on this host: it is not in the
  Arch repos/AUR and the system ships **LLVM 22**, ahead of mull's supported LLVM. Plan: run
  mull in a pinned-LLVM (≤18) Docker image against the trust-core TUs; record honest
  per-module scores and document equivalent mutants here. Until then mutation score = _not
  measured_ (not 0 — unmeasured).
- **Production data migration** from the legacy Postgres app is out of scope and its source
  is absent (see `docs/audit_findings.json`).

## Decomposition (Phase 6) — status

- **DONE:** removed the `data→service` reverse edge (sale DTOs → `domain/SaleTypes.h`);
  added `tools/check_layering.py` + a blocking CI gate so the boundary can't regress.
- **Deferred (roadmap, not a regression):** splitting the god-files (`ReturnService` 893,
  `AdminPage` 845, `ReturnsPage` 821, `AnalyticsRepository` 644, the 397-line
  `SaleService::commit`). These are behavior-bearing and were intentionally NOT split
  unsupervised (no blind large rewrites). They sit behind the Phase-3 characterization
  tests; each future split keeps its public header stable and is verified by build + tests
  + a launches-every-screen smoke run. See `audit/02`/`audit/03`.

## Ratchet log

| Date | Gate | Change |
|---|---|---|
| 2026-06-16 | all | Baselines captured; format + ASan/UBSan set BLOCKING; clang-tidy(750)/cppcheck(9) report-only. |
