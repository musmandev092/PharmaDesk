#!/usr/bin/env python3
"""Self-contained mutation testing for the trust core — no mull / LLVM / Docker.

mull won't build against this host's LLVM, so this harness measures the same thing
the honest way: it mutates one source token at a time, rebuilds `pharmadesk_tests`
with the project's own toolchain, runs the suite, and classifies the mutant:

  KILLED      — the suite FAILED (a test caught the change)        ✓ good
  SURVIVED    — the suite still PASSED (no test caught it)         ✗ triage
  BUILD_FAIL  — the mutation didn't compile (not a real mutant)    — skipped

Mutation score = killed / (killed + survived)  [build failures excluded].

Survivors are listed with file:line and the change so they can be triaged: KILL the
genuinely-killable ones with a targeted test; LEAVE equivalent mutants (no observable
effect) and record them. Do NOT add fake tests to inflate the score.

Usage:  python3 tools/mutation_test.py            # trust core
        python3 tools/mutation_test.py src/domain/Money.cpp ...
"""
from __future__ import annotations

import ctypes
import os
import re
import resource
import subprocess
import sys
import time
from pathlib import Path


def _no_core():
    # A "killed" mutant often makes the suite abort() (uncaught exception). Stop each
    # kill from spawning a core file + a systemd-coredump desktop notification.
    # RLIMIT_CORE=0 alone is ignored when core_pattern is a pipe (systemd-coredump),
    # so ALSO mark the process non-dumpable via prctl(PR_SET_DUMPABLE, 0).
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    try:
        ctypes.CDLL("libc.so.6", use_errno=True).prctl(4, 0, 0, 0, 0)  # PR_SET_DUMPABLE=4
    except Exception:
        pass

REPO = Path(__file__).resolve().parents[1]
BUILD = REPO / "build"
TEST_BIN = BUILD / "pharmadesk_tests"

TRUST_CORE = [
    "src/domain/Money.cpp",
    "src/domain/SaleCalculator.cpp",
    "src/domain/CostBlender.cpp",
    "src/domain/Fefo.cpp",
    "src/domain/ControlledSubstancePolicy.cpp",
    "src/domain/DiscountAuthorizationPolicy.cpp",
    "src/domain/PinPolicy.cpp",
]

# (pattern, replacement) token mutations. Patterns use surrounding spaces / word
# boundaries to avoid hitting pointers, templates, or substrings. Applied one
# occurrence at a time.
MUTATORS = [
    (r" <= ", " < "), (r" < ", " <= "),
    (r" >= ", " > "), (r" > ", " >= "),
    (r" == ", " != "), (r" != ", " == "),
    (r" && ", " || "), (r" \|\| ", " && "),
    (r" \+ ", " - "), (r" - ", " + "),
    (r"\breturn true\b", "return false"), (r"\breturn false\b", "return true"),
    (r" >= ", " < "), (r" <= ", " > "),
]


def run(cmd, **kw):
    return subprocess.run(cmd, cwd=REPO, capture_output=True, text=True, **kw)


def build_ok() -> bool:
    r = run(["cmake", "--build", str(BUILD), "-j", "--target", "pharmadesk_tests"])
    return r.returncode == 0


def tests_pass() -> bool:
    r = run([str(TEST_BIN)], env={**os.environ, "QT_QPA_PLATFORM": "offscreen"},
            preexec_fn=_no_core)
    return r.returncode == 0


def line_is_code(line: str) -> bool:
    s = line.strip()
    return bool(s) and not s.startswith(("//", "*", "/*"))


def gen_mutations(text: str):
    """Yield (line_idx, new_line, desc) for each applicable single-token mutation."""
    lines = text.splitlines(keepends=True)
    for i, line in enumerate(lines):
        if not line_is_code(line):
            continue
        # Don't mutate inside a string/char literal-heavy line crudely: skip lines
        # whose first token is a comment already handled; we accept some noise and
        # rely on BUILD_FAIL to drop invalid mutants.
        for pat, repl in MUTATORS:
            for m in re.finditer(pat, line):
                if repl.strip() == line[m.start():m.end()].strip():
                    continue
                new_line = line[: m.start()] + repl + line[m.end():]
                if new_line == line:
                    continue
                desc = f"{line[m.start():m.end()].strip()!r}->{repl.strip()!r}"
                yield (i, new_line, desc, m.start())


def main() -> int:
    files = sys.argv[1:] or TRUST_CORE
    if not TEST_BIN.exists() and not build_ok():
        print("baseline build failed"); return 2
    if not tests_pass():
        print("baseline tests do NOT pass — fix before mutation testing"); return 2

    overall = {"killed": 0, "survived": 0, "build_fail": 0}
    survivors = []
    t0 = time.time()
    for rel in files:
        path = REPO / rel
        original = path.read_text(encoding="utf-8")
        muts = list(gen_mutations(original))
        stats = {"killed": 0, "survived": 0, "build_fail": 0}
        print(f"\n== {rel} : {len(muts)} candidate mutants ==", flush=True)
        try:
            for (idx, new_line, desc, col) in muts:
                lines = original.splitlines(keepends=True)
                lines[idx] = new_line
                path.write_text("".join(lines), encoding="utf-8")
                if not build_ok():
                    stats["build_fail"] += 1
                    verdict = "BUILD_FAIL"
                elif tests_pass():
                    stats["survived"] += 1
                    verdict = "SURVIVED"
                    survivors.append(f"{rel}:{idx+1}  {desc}")
                else:
                    stats["killed"] += 1
                    verdict = "KILLED"
                if verdict != "BUILD_FAIL":
                    print(f"  L{idx+1:<4} {desc:<22} {verdict}", flush=True)
        finally:
            path.write_text(original, encoding="utf-8")
        valid = stats["killed"] + stats["survived"]
        score = (100.0 * stats["killed"] / valid) if valid else 0.0
        print(f"  -> killed {stats['killed']}/{valid} = {score:.0f}%  "
              f"(build_fail {stats['build_fail']})")
        for k in overall:
            overall[k] += stats[k]

    # Restore the build to a clean (un-mutated) state.
    build_ok()
    valid = overall["killed"] + overall["survived"]
    score = (100.0 * overall["killed"] / valid) if valid else 0.0
    print("\n" + "=" * 60)
    print(f"TRUST-CORE MUTATION SCORE: {overall['killed']}/{valid} = {score:.1f}%  "
          f"(build_fail {overall['build_fail']}, {time.time()-t0:.0f}s)")
    if survivors:
        print(f"\nSURVIVORS ({len(survivors)}) — triage (kill or document as equivalent):")
        for s in survivors:
            print("  " + s)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
