#pragma once

#include <QString>

class SettingsRepository;

// First-run Freedesktop desktop integration for the AppImage build.
//
// When the binary is launched from inside an AppImage, the AppDir is mounted
// read-only and the installed .desktop/icon entries do not yet exist in the
// user's home. installIfAppImage() copies the bundled .desktop file and icon
// into ~/.local/share so the app shows up in the application menu, then nudges
// the desktop/icon caches. It is strictly user-space: it never touches CUPS,
// systemd, or anything under /etc.
//
// It also records the installed version in settings so it can tell a first
// install from an upgrade, and returns a one-line user notice to show once:
//   - first install  → "added to your applications menu"
//   - new version     → "Updated to vX"
//   - otherwise / not an AppImage → empty string (nothing to announce)
//
// Safe to call unconditionally on every startup: it is a no-op outside an
// AppImage, and silently skips when the bundled sources are absent.
namespace DesktopIntegration {

QString installIfAppImage(SettingsRepository &settings);

} // namespace DesktopIntegration
