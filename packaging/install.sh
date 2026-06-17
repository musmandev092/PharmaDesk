#!/usr/bin/env bash
# Install PharmaDesk on this PC. Run by the vendor during setup, from inside the
# extracted release tarball:  ./install.sh
#
# It drops the pre-built pharmadesk binary under your user profile, writes a
# launcher, and registers an applications-menu entry + icon. Unlike the AppImage,
# this tarball links the SYSTEM Qt6 — so Qt6 (+ libcrypt/zlib/openssl) must be
# present. The script preflights that and refuses with install hints if anything
# the binary needs is missing, rather than installing something that won't launch.
#
# No internet is required: everything shipped here is already built.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="$HERE/bin/pharmadesk"                                   # the pre-built app
ICON_SRC="$HERE/share/icons/hicolor/256x256/apps/pharmadesk.png"
TEMPLATES_SRC="$HERE/share/pharmadesk/templates"            # sample import sheets

# Everything the installer writes lives under ONE folder — the same root the app
# uses for its data, so uninstall can drop the program and leave the data behind:
#   ~/.local/share/PharmaDesk/
#     app/            the binary + sample templates        ── removed by uninstall
#     PharmaDesk/     the database, logs, license, logos    ── your data, KEPT
# (the DB sits one level DEEPER — Qt's AppDataLocation is <org>/<app> = both
#  "PharmaDesk" — so removing app/ can never touch it.)
BASE="$HOME/.local/share/PharmaDesk"
APP="$BASE/app"
BINDIR="$HOME/.local/bin"
APPS="$HOME/.local/share/applications"
ICONS="$HOME/.local/share/icons/hicolor/256x256/apps"

[ -f "$BIN" ] || { echo "!! bin/pharmadesk not found next to this script."; exit 1; }
chmod +x "$BIN" 2>/dev/null || true

# Runtime preflight: this tarball uses the SYSTEM Qt6 (it is NOT bundled, unlike
# the AppImage). Resolve the binary's shared libraries and refuse the install if
# any are missing — better an honest error now than a silent crash on launch.
echo ">> checking the runtime libraries the binary needs (system Qt6, etc.)"
if command -v ldd >/dev/null 2>&1; then
  MISSING="$(ldd "$BIN" 2>/dev/null | awk '/not found/ {print "   - "$1}')"
  if [ -n "$MISSING" ]; then
    echo "!! the following shared libraries are missing on this machine:"
    echo "$MISSING"
    echo
    echo "   PharmaDesk (tarball build) needs the system Qt6 + a few system libs."
    echo "   Install them, then re-run ./install.sh — for example:"
    echo "     Debian/Ubuntu: sudo apt install qt6-base qt6-base-dev-tools \\"
    echo "                      libqt6sql6-sqlite libgl1 libegl1 libcrypt1 zlib1g openssl"
    echo "     Fedora/RHEL:   sudo dnf install qt6-qtbase qt6-qtbase-mysql \\"
    echo "                      qt6-qtbase-sqlite mesa-libGL libxcrypt zlib openssl-libs"
    echo "   (Or use the portable AppImage build, which bundles Qt.)"
    exit 1
  fi
fi

echo ">> installing the app binary"
mkdir -p "$APP/bin"
cp -f "$BIN" "$APP/bin/pharmadesk"
chmod +x "$APP/bin/pharmadesk"
# Ship the sample import templates alongside the app so the user can find them
# (referenced from the Medicines import dialog). They are not loaded at runtime.
if [ -d "$TEMPLATES_SRC" ]; then
  mkdir -p "$APP/templates"
  cp -f "$TEMPLATES_SRC"/* "$APP/templates/" 2>/dev/null || true
fi

echo ">> writing the launcher ($BINDIR/pharmadesk)"
mkdir -p "$BINDIR"
cat > "$BINDIR/pharmadesk" <<EOF
#!/usr/bin/env bash
# PharmaDesk launcher (installed by install.sh). Runs the installed binary.
# Native file dialogs on EVERY desktop (GNOME, KDE/Plasma, Sway, …) via
# xdg-desktop-portal; falls back to Qt's plain dialog if no portal is present.
export QT_QPA_PLATFORMTHEME=xdgdesktopportal
exec "$APP/bin/pharmadesk" "\$@"
EOF
chmod +x "$BINDIR/pharmadesk"

echo ">> adding the applications-menu entry + icon"
mkdir -p "$APPS" "$ICONS"
[ -f "$ICON_SRC" ] && cp -f "$ICON_SRC" "$ICONS/pharmadesk.png"
# The .desktop basename matches the app's desktop id (setDesktopFileName =
# Branding::desktopId() = "pharmadesk"), so the desktop ties the running window
# to this launcher. Exec is absolute (not bare "pharmadesk") so the menu entry
# works even when ~/.local/bin isn't on PATH.
cat > "$APPS/pharmadesk.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=PharmaDesk
Comment=Point-of-sale, inventory, purchasing and reports for any pharmacy
Exec=$BINDIR/pharmadesk %U
Icon=pharmadesk
Categories=Office;Finance;
Terminal=false
StartupWMClass=pharmadesk
EOF
update-desktop-database "$APPS" 2>/dev/null || true
gtk-update-icon-cache "$HOME/.local/share/icons/hicolor" 2>/dev/null || true
kbuildsycoca6 2>/dev/null || kbuildsycoca5 2>/dev/null || true

echo
echo "✓ PharmaDesk installed."
echo "  Launch it from your applications menu, or run:  $BINDIR/pharmadesk"
case ":$PATH:" in *":$BINDIR:"*) : ;; *)
  echo "  NOTE: $BINDIR is not on your PATH — add it, or use the menu entry." ;;
esac
[ -d "$APP/templates" ] && echo "  Import templates are in: $APP/templates"
echo "  First launch runs the setup wizard, then asks to ACTIVATE this machine —"
echo "  send the activation request to the vendor and load back the license file."
