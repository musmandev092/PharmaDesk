#!/usr/bin/env bash
# Build pharmadesk into a single-file AppImage.
#
# IMPORTANT: an AppImage is forward-compatible only. Build on the OLDEST distro
# you must support (e.g. AlmaLinux 10) so its glibc/Qt work everywhere newer.
# Building on a bleeding-edge distro (Arch) yields an AppImage that may NOT run
# on older targets.
#
# Usage:  packaging/build-appimage.sh
# Output: PMS/PharmaDesk-x86_64.AppImage
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"   # PMS/
cd "$here"

build="$here/build-appimage"
appdir="$here/AppDir"
tools="$here/packaging/tools"
mkdir -p "$tools"

# linuxdeploy + Qt plugin extract themselves rather than needing FUSE.
export APPIMAGE_EXTRACT_AND_RUN=1

fetch() {  # fetch <url> <dest>
    [ -x "$2" ] && return 0
    echo ">> downloading $(basename "$2")"
    curl -fSL "$1" -o "$2"
    chmod +x "$2"
}
base="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous"
qbase="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous"
aibase="https://github.com/AppImage/appimagetool/releases/download/continuous"
fetch "$base/linuxdeploy-x86_64.AppImage"            "$tools/linuxdeploy-x86_64.AppImage"
fetch "$qbase/linuxdeploy-plugin-qt-x86_64.AppImage" "$tools/linuxdeploy-plugin-qt-x86_64.AppImage"
fetch "$aibase/appimagetool-x86_64.AppImage"         "$tools/appimagetool-x86_64.AppImage"

echo ">> configuring (Release, tests off)"
cmake -S "$here" -B "$build" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build "$build" -j

echo ">> installing into AppDir"
rm -rf "$appdir"
DESTDIR="$appdir" cmake --install "$build" --prefix /usr

echo ">> bundling Qt + libraries into the AppImage"
export QML_SOURCES_PATHS=""   # no QML in this app
# Bundle the offscreen platform plugin alongside xcb so the AppImage also runs
# headless (CI / smoke tests), not just on a desktop.
export EXTRA_PLATFORM_PLUGINS="libqoffscreen.so"

# The Qt plugin bundles EVERY SQL driver it finds (Firebird, MySQL, ODBC, …),
# and fails if any of their client libs are missing. We only ship SQLite, so we
# present linuxdeploy a filtered plugins dir (sqldrivers = SQLite only) via a
# qmake wrapper that rewrites QT_INSTALL_PLUGINS.
realqmake="${QMAKE:-$(command -v qmake6 || command -v qmake)}"
pluginsrc="$("$realqmake" -query QT_INSTALL_PLUGINS)"
qtplugins="$tools/qtplugins"
rm -rf "$qtplugins"; mkdir -p "$qtplugins"
for d in "$pluginsrc"/*; do ln -sfn "$d" "$qtplugins/$(basename "$d")"; done
rm -f "$qtplugins/sqldrivers"
mkdir -p "$qtplugins/sqldrivers"
ln -sfn "$pluginsrc/sqldrivers/libqsqlite.so" "$qtplugins/sqldrivers/libqsqlite.so"

cat > "$tools/qmake-wrap" <<EOF
#!/usr/bin/env bash
if [ "\$1" = "-query" ] && [ "\$2" = "QT_INSTALL_PLUGINS" ]; then
    echo "$qtplugins"
elif [ "\$1" = "-query" ] && [ -z "\${2:-}" ]; then
    "$realqmake" -query | sed "s|^QT_INSTALL_PLUGINS:.*|QT_INSTALL_PLUGINS:$qtplugins|"
else
    exec "$realqmake" "\$@"
fi
EOF
chmod +x "$tools/qmake-wrap"
export QMAKE="$tools/qmake-wrap"
echo ">> using QMAKE wrapper (SQLite-only sqldrivers) over $realqmake"

# Populate the AppDir only (no --output). NO_STRIP=1: linuxdeploy's bundled strip
# can't parse modern .relr.dyn ELF sections — we strip ourselves below with the
# system strip, which handles them and shrinks the binaries substantially.
NO_STRIP=1 "$tools/linuxdeploy-x86_64.AppImage" \
    --appdir "$appdir" \
    --plugin qt \
    --desktop-file "$appdir/usr/share/applications/pharmadesk.desktop" \
    --icon-file "$appdir/usr/share/icons/hicolor/256x256/apps/pharmadesk.png"

echo ">> stripping ELF binaries (system strip)"
before=$(du -sh "$appdir" | cut -f1)
while IFS= read -r -d '' f; do
    case "$(file -b "$f" 2>/dev/null)" in
        *ELF*) strip --strip-unneeded "$f" 2>/dev/null || true ;;
    esac
done < <(find "$appdir" -type f \( -name '*.so' -o -name '*.so.*' -o -path '*/usr/bin/*' \) -print0)

echo ">> trimming bundle (drop English-app translations)"
rm -rf "$appdir/usr/translations"   # Qt .qm translation catalogs — app is English-only

# ── Minimize the ICU data library (the single biggest payload) ──────────────
# libicudata bundles ~4000 locale resource files (~32 MB); an English app needs
# only root + en + the shared Unicode data (collation, normalization, character
# properties, converters, break dictionaries). icupkg+genccode+gcc rebuild a
# ~13 MB lib. Falls back to the full library if the ICU build tools are absent.
icu_so="$(find "$appdir/usr/lib" -name 'libicudata.so.*' | head -1 || true)"
if [ -n "${icu_so:-}" ] && command -v icupkg >/dev/null 2>&1 \
   && command -v genccode >/dev/null 2>&1 && command -v gcc >/dev/null 2>&1; then
    echo ">> minimizing ICU data ($(du -h "$icu_so" | cut -f1))"
    # The extract-and-repack dance below depends on the exact ELF layout of the
    # bundled libicudata, which varies across ICU versions/distros. Run the whole
    # attempt in a guarded subshell (its own `set -e`) so ANY failure falls back
    # to shipping the full library instead of aborting the build — the AppImage
    # is just larger, never broken.
    if (
        set -e
        ver="$(basename "$icu_so" | sed -E 's/libicudata\.so\.//')"   # e.g. 78
        wd="$(mktemp -d)"
        trap 'rm -rf "$wd"' EXIT
        # Locate the icudtNN_dat blob (vaddr + size, as DECIMAL — readelf -sW
        # prints the size already 0x-prefixed, so strip any prefix before
        # strtonum) and the file offset of the section that contains it.
        read -r val sz < <(readelf -sW "$icu_so" | awk '/icudt[0-9]+_dat/{
            v=$2; s=$3; sub(/^0x/,"",v); sub(/^0x/,"",s);
            print strtonum("0x"v), strtonum("0x"s); exit }')
        [ -n "${sz:-}" ] && [ "$sz" -gt 0 ]
        off="$(readelf -SW "$icu_so" | awk -v v="$val" '
            /\] / { for (i=1;i<=NF;i++) if ($i ~ /^[0-9a-fA-F]{16}$/) {
                addr=strtonum("0x"$i); foff=strtonum("0x"$(i+1)); size=strtonum("0x"$(i+2));
                if (v>=addr && v<addr+size) { print foff + (v-addr); exit } } }')"
        [ -n "${off:-}" ] && [ "$((off % 4096))" -eq 0 ]   # page-aligned (ICU: 0x1000)
        # icupkg derives the package name from the .dat basename, so it MUST be
        # named icudtNNl (l = little-endian, as on x86_64) or it rejects the
        # items ("does not start with full/").
        pkg="icudt${ver}l"
        # Carve [off, off+sz) using fast 4 KiB blocks, then trim to the exact size.
        dd if="$icu_so" of="$wd/$pkg.dat" bs=4096 skip="$((off / 4096))" \
           count="$(((sz + 4095) / 4096))" status=none
        truncate -s "$sz" "$wd/$pkg.dat"
        python3 - "$wd" "$pkg" <<'PY'
import subprocess, re, sys
wd, pkg = sys.argv[1], sys.argv[2]
items = subprocess.check_output(["icupkg", "-l", f"{wd}/{pkg}.dat"]).decode().split()
rm = []
for it in items:
    b = it.rsplit('/', 1)[-1]
    if not b.endswith('.res'):
        continue                       # keep all shared .icu/.nrm/.cnv/.brk/.dict
    n = b[:-4]; first = n.split('_')[0]
    if first in ('en', 'root', 'und'):
        continue                       # keep english + root + undetermined
    if re.fullmatch(r'[a-z]{2,3}', first):
        rm.append(it)                  # a non-english locale family -> remove
open(wd + "/rm.txt", "w").write("\n".join(rm) + "\n")
PY
        icupkg -r "$wd/rm.txt" "$wd/$pkg.dat" "$wd/min.dat" >/dev/null 2>&1
        genccode -a gcc -e "icudt${ver}" -d "$wd" -f icumin "$wd/min.dat" >/dev/null 2>&1
        gcc -shared -fPIC -o "$wd/libicudata.so.${ver}" "$wd/icumin.S" \
            -Wl,-soname,"libicudata.so.${ver}" >/dev/null 2>&1
        nm -D "$wd/libicudata.so.${ver}" 2>/dev/null | grep -q "icudt${ver}_dat"
        cp "$wd/libicudata.so.${ver}" "$icu_so"
    ); then
        echo ">> ICU data minimized to $(du -h "$icu_so" | cut -f1)"
    else
        echo ">> ICU minimization skipped (unexpected layout/tool error) — keeping full data"
    fi
else
    echo ">> ICU tools not found — keeping full libicudata"
fi

echo ">> AppDir size: $before -> $(du -sh "$appdir" | cut -f1)"

# ── Package: max-zstd squashfs (mksquashfs from appimagetool) + AppImage runtime.
# appimagetool's default zstd level is low; we drive mksquashfs at level 22 for
# the smallest image, then prepend the type-2 runtime ourselves.
echo ">> packaging (mksquashfs zstd level 22 + AppImage runtime)"
ait="$tools/ait"
if [ ! -x "$tools/ait/usr/bin/mksquashfs" ]; then
    ( cd "$tools" && rm -rf ait squashfs-root \
      && ./appimagetool-x86_64.AppImage --appimage-extract >/dev/null 2>&1 && mv squashfs-root ait )
fi
mksq="$(find "$ait" -name mksquashfs | head -1)"
fetch "https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64" \
      "$tools/runtime-x86_64"

rm -f "$here/PharmaDesk-x86_64.AppImage" "$tools/app.sqfs"
"$mksq" "$appdir" "$tools/app.sqfs" -comp zstd -Xcompression-level 22 -b 1M -noappend -no-progress
cat "$tools/runtime-x86_64" "$tools/app.sqfs" > "$here/PharmaDesk-x86_64.AppImage"
chmod +x "$here/PharmaDesk-x86_64.AppImage"
rm -f "$tools/app.sqfs"

echo ">> done:"
ls -lh "$here"/PharmaDesk-x86_64.AppImage
