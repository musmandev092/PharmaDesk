# Mutation Testing (Phase 8)

Mutation testing proves the trust-core tests actually **kill** injected bugs, not just
execute lines. We use [`mull`](https://github.com/mull-project/mull) (LLVM-based) on the
trust-core translation units.

## Two ways to run it

1. **`tools/mutation_test.py` (no mull / LLVM / Docker needed) — the one that runs here.**
   A self-contained source-mutation harness: it mutates one trust-core token at a time
   (relational/equality/logical/arithmetic swaps, `return true`↔`false`), rebuilds
   `pharmadesk_tests` with the project's own toolchain, runs the suite, and classifies each
   mutant **KILLED** (suite failed — good), **SURVIVED** (suite still passed — triage), or
   **BUILD_FAIL** (didn't compile — skipped). Score = killed / (killed + survived).
   ```bash
   python3 tools/mutation_test.py                 # trust core
   python3 tools/mutation_test.py src/domain/Money.cpp
   ```
   It disables core dumps for the test child (a killed mutant often `abort()`s on an uncaught
   exception — expected; without this every kill would spawn a systemd-coredump notification).
   Per-module + overall scores and the survivor list are recorded below as they're measured.

2. **`mull`** (below) — the industry-standard LLVM mutation tester. Not usable on *this* host:
   `mull` lags LLVM and the host ships **LLVM 22** (and it isn't packaged for Arch / no usable
   Docker here), so use the pinned-LLVM container recipe on a machine that has Docker.

## How to run it (pinned-LLVM container)

Run mull in a container with an LLVM version it supports (≤ ~18). `tools/run_mutation.sh`
wraps this. Example with LLVM 17:

```bash
# Build with the matching clang and -fexperimental-new-pass-manager flags mull needs,
# then run mull-runner against the trust-core test binary.
docker run --rm -v "$PWD":/src -w /src ghcr.io/mull-project/mull-llvm-17:latest bash -lc '
  cmake -S . -B build-mull -G Ninja \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_FLAGS="-fpass-plugin=$(mull-ir-frontend --version >/dev/null; echo) -grecord-command-line -O0 -g" \
    -DCMAKE_BUILD_TYPE=Debug
  cmake --build build-mull -j --target pharmadesk_tests
  QT_QPA_PLATFORM=offscreen mull-runner-17 --reporters IDE Elements \
    --report-name trust-core build-mull/pharmadesk_tests
'
```

Scope mutation to the trust core via a `mull.yml` allowlist:

```yaml
# mull.yml
mutators:
  - cxx_all
includePaths:
  - src/domain/Money.cpp
  - src/domain/SaleCalculator.cpp
  - src/domain/CostBlender.cpp
  - src/domain/Fefo.cpp
  - src/domain/ControlledSubstancePolicy.cpp
  - src/domain/DiscountAuthorizationPolicy.cpp
```

## Triage discipline

For each surviving mutant:
- If it represents a real, observable behavior change the tests miss → **add a targeted
  test that kills it** (do not loosen assertions to hide it).
- If it is an **equivalent mutant** (no observable effect — e.g. `<=` vs `<` on an
  unreachable boundary) → **leave it and record it here / in DEBT.md** with the reason.

Record honest per-module scores (killed / total, minus equivalents) once a supported
toolchain is available. Until then this file is the tracking placeholder.
