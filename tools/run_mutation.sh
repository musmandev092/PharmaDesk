#!/usr/bin/env bash
# Run mull mutation testing on the trust-core TUs in a pinned-LLVM container.
#
# The host toolchain (LLVM 22) is ahead of what mull supports, so we use a
# container image that ships a supported LLVM. See docs/mutation-testing.md.
#
# Usage:  tools/run_mutation.sh [llvm_version]   (default 17)
set -euo pipefail

LLVM="${1:-17}"
IMAGE="ghcr.io/mull-project/mull-llvm-${LLVM}:latest"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker not found. Install docker (or podman) — mull needs a pinned-LLVM image" >&2
  echo "because the host LLVM is too new. See docs/mutation-testing.md." >&2
  exit 2
fi

echo ">> Running mull (LLVM ${LLVM}) on the trust core. This rebuilds with clang."
docker run --rm -v "${ROOT}":/src -w /src "${IMAGE}" bash -lc "
  set -euo pipefail
  cmake -S . -B build-mull -G Ninja \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS='-O0 -g -grecord-command-line -fpass-plugin=/usr/lib/mull-ir-frontend-${LLVM}'
  cmake --build build-mull -j\$(nproc) --target pharmadesk_tests
  QT_QPA_PLATFORM=offscreen mull-runner-${LLVM} \
    --reporters Elements --report-name trust-core \
    build-mull/pharmadesk_tests
"
echo ">> Done. Triage survivors per docs/mutation-testing.md (kill real ones; record equivalents)."
