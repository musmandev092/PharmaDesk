# 07 — Clean-Code Audit

## Summary — Grade **B**

Readable, idiomatic, consistently-named Qt/C++ with good header documentation. Held back by
a few god-files and long functions (see 02/03), localized duplication, and a handful of
error-handling shortcuts (swallowed `exec()` results, ignored audit return values).

## Strengths

- **Naming & idiom:** clear, intention-revealing names; consistent `Qt`-style; `QStringLiteral`
  for literals; `const` correctness; RAII via Qt parent/child and value types.
- **Comments:** headers explain *why* and cite the PHP behavior they mirror — high-signal,
  not noise. `Money.h`, `Database.cpp`, `SaleService.cpp` are well-documented.
- **No magic floats for money;** overflow-checked arithmetic.
- **Tests as documentation:** the suites read as behavioral specs.

## Issues (by theme)

### Long functions / god-files
See 02. `SaleService::commit` (397), `ReturnService::*`, `AdminPage`/`ReturnsPage`
tab-builders. Extract named steps (Phase 6) behind characterization tests (Phase 3).

### Error-handling shortcuts
| Finding | Location |
|---|---|
| `bumpSessionRefund` / `bumpForSale` ignore `exec()` result → silent failure. | `ReturnService.cpp:104`, `SessionRepository.cpp:187` |
| `Audit::write` return value discarded on 3 paths (best-effort audit). | `MedicineRepository.cpp:209`, `UserRepository.cpp:222`, `BatchRepository.cpp:55` |
| Several `q.exec()` results unchecked in read paths (lower risk). | various repos |

### Duplication
Doc-number generation (4×), ledger-write block (8×), report-widget ctor boilerplate,
money-percentage `ratioMul(…, fromUnits(1000000), …)` (3×). Extract helpers
(`recordMovement`, `nextDocNumber`, `percentOf`).

### Magic numbers / literals
`1000000 /* 100 at scale4 */` repeated; lockout thresholds and backoff steps inline;
qty bounds `1..9999` inline in `SaleService`. Promote to named constants.

### Consistency
Mixed authorization styles (UI PIN dialog vs. menu visibility vs. none) — unify behind the
write seam (Phase 4). Mixed audit styles (`write` vs `writeOrThrow`) — standardize on
`writeOrThrow` inside transactions.

## Lint posture (to be baselined in Phase 2)

- `clang-format`: **already clean** (`--dry-run -Werror` passes) → gate blocks immediately.
- `clang-tidy`: no `.clang-tidy` config yet → add one (bugprone/performance/readability/
  modernize subset) and baseline report-only, then ratchet.
- `cppcheck`: ~33 findings at baseline → report-only, ratchet to zero.

## The Floor Rule

No change may raise the format/tidy/cppcheck counts. Every new line is `clang-format`-clean
and introduces no new tidy/cppcheck finding; fixes are applied case-by-case with the full
test suite re-run, never blind bulk-autofix.
