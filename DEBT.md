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
| ASan + UBSan (test suite) | clean | **BLOCKING** | See verification note below. |
| clang-tidy | **750** | report-only | Ratchet to 0, then blocking. Breakdown below. |
| cppcheck | **9** | report-only | Ratchet to 0, then blocking. Breakdown below. |
| Trust-core branch coverage | TBD (Phase 3) | report-only → gate | `fail-under` enforced once Phase-3 tests land. |

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

## Known-deferred items (with reasons)

- **Audit HMAC chain (security H2).** Schema has `prev_hmac`/`row_hmac`; not yet populated.
  Phase 5 implements it additively. Key-management policy needs owner sign-off before the
  chain verifier becomes a gate.
- **Mutation testing (Phase 8).** `mull` is not installable on this host: it is not in the
  Arch repos/AUR and the system ships **LLVM 22**, ahead of mull's supported LLVM. Plan: run
  mull in a pinned-LLVM (≤18) Docker image against the trust-core TUs; record honest
  per-module scores and document equivalent mutants here. Until then mutation score = _not
  measured_ (not 0 — unmeasured).
- **Production data migration** from the legacy Postgres app is out of scope and its source
  is absent (see `docs/audit_findings.json`).

## Ratchet log

| Date | Gate | Change |
|---|---|---|
| 2026-06-16 | all | Baselines captured; format + ASan/UBSan set BLOCKING; clang-tidy(750)/cppcheck(9) report-only. |
