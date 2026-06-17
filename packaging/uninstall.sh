#!/usr/bin/env bash
# Remove PharmaDesk from this PC. Keeps your DATA (database, license, logs,
# logos) in ~/.local/share/PharmaDesk/PharmaDesk — delete that yourself only if
# you really mean to.
set -euo pipefail

BASE="$HOME/.local/share/PharmaDesk"     # app/ lives here; data is in PharmaDesk/
BINDIR="$HOME/.local/bin"
APPS="$HOME/.local/share/applications"
ICONS="$HOME/.local/share/icons/hicolor/256x256/apps"

# Remove only the installed program (app/). The database, license and logs live
# in $BASE/PharmaDesk (Qt AppDataLocation = <org>/<app>) and are left untouched.
rm -rf "$BASE/app"
rm -f "$BINDIR/pharmadesk"
rm -f "$APPS/pharmadesk.desktop"
rm -f "$ICONS/pharmadesk.png"
update-desktop-database "$APPS" 2>/dev/null || true
gtk-update-icon-cache "$HOME/.local/share/icons/hicolor" 2>/dev/null || true
kbuildsycoca6 2>/dev/null || kbuildsycoca5 2>/dev/null || true

echo "✓ PharmaDesk removed (app + launcher + menu entry)."
echo "  Your data in $BASE/PharmaDesk was NOT touched (DB, license, logs)."
echo "  To erase everything including the database:  rm -rf $BASE"
