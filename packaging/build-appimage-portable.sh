#!/usr/bin/env bash
# Build a PORTABLE AppImage that runs on AlmaLinux 10 and essentially every
# mainstream 2022+ Linux desktop — by compiling against an OLD glibc inside a
# container, then running the normal packaging/build-appimage.sh there.
#
# WHY: glibc is forward-compatible only and is the ONE library an AppImage can't
# bundle. Building natively on a bleeding-edge distro (e.g. Arch, glibc 2.43)
# yields an AppImage whose bundled libs demand glibc 2.43 — so it only runs on
# Arch. The fix (the AppImage project's "build on the oldest distro you support"
# rule) is to build against an old glibc. Ubuntu 22.04 ships glibc 2.35 and a
# distro Qt6, giving a ~2.35 floor: runs on AlmaLinux 9.6+/10, Ubuntu 22.04+,
# Debian 12, Fedora 36+, Arch, Mint 21+ (anything glibc >= 2.35).
#
# Need an even older floor (Ubuntu 20.04 / RHEL 8, glibc <= 2.31)? Those distros
# don't package Qt6, so you'd build Qt from source on that base — out of scope here.
#
# Usage:  packaging/build-appimage-portable.sh        # needs docker
#         BUILD_IMAGE=ubuntu:24.04 packaging/build-appimage-portable.sh
# Output: PMS/PharmaDesk-x86_64.AppImage
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
image="${BUILD_IMAGE:-ubuntu:22.04}"

command -v docker >/dev/null || { echo "error: docker is required for the portable build" >&2; exit 1; }
echo ">> portable AppImage build inside $image (old-glibc base)"

docker run --rm -i \
    -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
    -v "$here":/src -w /src \
    "$image" bash -euo pipefail -s <<'INNER'
. /etc/os-release; echo ">>> base: $PRETTY_NAME | $(ldd --version | head -1)"
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
# Toolchain + distro Qt6 (+ SQLite driver — this is a SQLite app) + the GUI/xcb
# runtime libs linuxdeploy must find on disk to bundle into the AppImage.
apt-get install -y --no-install-recommends \
    build-essential cmake file binutils python3 ca-certificates xz-utils \
    qt6-base-dev qt6-base-dev-tools libqt6sql6-sqlite \
    libgl1-mesa-dev libegl1 libxkbcommon-dev libxkbcommon-x11-0 \
    libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 \
    libxcb-render-util0 libxcb-shape0 libxcb-xinerama0 libxcb-xkb1 \
    libfontconfig1 libfreetype6 libdbus-1-3 libglib2.0-0 icu-devtools >/dev/null
export QMAKE="$(command -v qmake6)"
echo ">>> Qt $("$QMAKE" -query QT_VERSION)"
test -f "$("$QMAKE" -query QT_INSTALL_PLUGINS)/sqldrivers/libqsqlite.so" \
    || { echo "error: Qt6 SQLite driver missing" >&2; exit 1; }
export APPIMAGE_EXTRACT_AND_RUN=1      # no FUSE inside containers
cd /src
rm -rf build-appimage AppDir           # drop any stale (host-distro) CMake cache
bash packaging/build-appimage.sh
# Files were created as root in the bind mount — hand them back to the host user.
chown -R "${HOST_UID}:${HOST_GID}" \
    PharmaDesk-x86_64.AppImage build-appimage AppDir packaging/tools 2>/dev/null || true
INNER

echo ">> done: $here/PharmaDesk-x86_64.AppImage"
echo ">> verify the glibc floor with:"
echo "   ./PharmaDesk-x86_64.AppImage --appimage-extract >/dev/null && \\"
echo "   find squashfs-root -name '*.so*' -o -name pharmadesk | xargs objdump -T 2>/dev/null | \\"
echo "   grep -oE 'GLIBC_[0-9]+\\.[0-9]+' | sort -V | tail -1   # expect GLIBC_2.35"
