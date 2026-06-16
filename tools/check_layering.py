#!/usr/bin/env python3
"""Layer-dependency linter for PharmaDesk (Phase 6).

Enforces the intended dependency direction  ui -> service -> data -> domain
by scanning `#include "<layer>/..."` edges in src/. domain/ is the lowest layer
(a leaf); ui/ is the top and may include anything below it.

RATCHET: any edge listed in ALLOWLIST is a known, documented baseline violation we
tolerate for now; every OTHER illegal edge fails the check (exit 1). The goal is
that no NEW reverse/skip edge can be introduced. Drain the allowlist over time.

Run from the repo root:  python3 tools/check_layering.py
"""
import os
import re
import sys

SRC = "src"
LAYERS = ("domain", "data", "service", "ui")

# What each layer is ALLOWED to include (besides its own files and non-layer
# headers like Branding.h). Lower layers must not depend on higher ones.
ALLOWED = {
    "domain": set(),                       # leaf: depends on nothing internal
    "data": {"domain"},                    # data -> domain only
    "service": {"domain", "data"},         # service -> data, domain
    "ui": {"domain", "data", "service"},   # ui -> everything below
}

# Known baseline violations (file -> included path). Documented in audit/01.
# These DO NOT fail the build; new violations not listed here DO.
ALLOWLIST = {
    # L2: DesktopIntegration is really infra glue mislabeled as domain; it reads a
    # setting via the repository. Tracked for a future move to service/.
    ("domain/DesktopIntegration.cpp", "data/SettingsRepository.h"),
}

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')


def layer_of(path):
    parts = path.replace("\\", "/").split("/")
    return parts[0] if parts and parts[0] in LAYERS else None


def main():
    violations = []
    for root, _dirs, files in os.walk(SRC):
        for name in files:
            if not name.endswith((".cpp", ".h", ".hpp")):
                continue
            full = os.path.join(root, name)
            rel = os.path.relpath(full, SRC).replace("\\", "/")
            src_layer = layer_of(rel)
            if src_layer is None:
                continue  # top-level files (main.cpp, MainWindow.*, Branding.h)
            with open(full, encoding="utf-8", errors="replace") as fh:
                for line in fh:
                    m = INCLUDE_RE.match(line)
                    if not m:
                        continue
                    inc = m.group(1)
                    inc_layer = layer_of(inc)
                    if inc_layer is None or inc_layer == src_layer:
                        continue
                    if inc_layer not in ALLOWED[src_layer]:
                        if (rel, inc) in ALLOWLIST:
                            continue
                        violations.append((rel, inc, src_layer, inc_layer))

    if violations:
        print("LAYERING VIOLATIONS (illegal include direction):\n")
        for rel, inc, sl, il in sorted(violations):
            print(f"  {rel}\n      includes {inc}  ({sl} -> {il} is not allowed)")
        print(f"\n{len(violations)} violation(s). "
              "Fix the dependency direction or, if it is a deliberate, documented "
              "baseline, add it to ALLOWLIST in tools/check_layering.py with a reason.")
        return 1

    allow = len(ALLOWLIST)
    print(f"Layering OK: ui -> service -> data -> domain respected "
          f"({allow} documented baseline exception(s) allow-listed).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
