#!/usr/bin/env bash
# Bump the project version in ONE place: the CMake project() VERSION.
#
# That version is the single source of truth — it is baked into the binary as
# PHARMADESK_VERSION (shown on the login screen + the footer via Branding::appVersion)
# and read by the release workflow to tag v<version> (see ADR-0005). There is no
# separate version header to keep in sync.
#
# Usage:  scripts/bump_version.sh <X.Y.Z>
#         scripts/bump_version.sh 1.2.0
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CMAKE="${ROOT}/CMakeLists.txt"

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <X.Y.Z>" >&2
  exit 2
fi
NEW="$1"
if [[ ! "$NEW" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "error: version must be X.Y.Z (e.g. 1.2.0), got '$NEW'" >&2
  exit 2
fi

OLD="$(grep -oE 'project\(pharmadesk VERSION [0-9]+\.[0-9]+\.[0-9]+' "$CMAKE" \
        | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' || true)"
if [[ -z "$OLD" ]]; then
  echo "error: could not find 'project(pharmadesk VERSION X.Y.Z' in $CMAKE" >&2
  exit 1
fi

if [[ "$OLD" == "$NEW" ]]; then
  echo "version already $NEW — nothing to do"
  exit 0
fi

# Replace only the project() VERSION token, nothing else.
sed -i -E "s/(project\(pharmadesk VERSION )[0-9]+\.[0-9]+\.[0-9]+/\1${NEW}/" "$CMAKE"

echo "bumped version: ${OLD} -> ${NEW}"
echo
echo "Next:"
echo "  git commit -am 'chore: bump version to ${NEW}'"
echo "  # merge to main → the build.yml workflow publishes release v${NEW}"
