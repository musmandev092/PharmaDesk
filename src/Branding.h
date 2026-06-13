#pragma once

#include <QString>
#include <QStringList>

// Build version (CMake project() VERSION). Guarded so header-only consumers that
// miss the compile definition still build (with a sentinel value).
#ifndef PHARMADESK_VERSION
#define PHARMADESK_VERSION "0.0.0"
#endif

// Single source of truth for the fixed PRODUCT (white-label) identity.
//
// The per-deployment pharmacy identity — display name, logo, address, tax line —
// is collected by the first-run setup wizard and stored in the `settings` table;
// it is never baked into the binary. THIS namespace holds only the neutral
// product brand (window-title fallback, data-dir/organization name, DB filename,
// packaging identifiers). Change these to re-skin the product.
namespace Branding {

// Human-facing product name (window titles, About, error dialogs, login/setup
// headers when no pharmacy name is set yet).
inline QString productName()
{
    return QStringLiteral("PharmaDesk");
}

// Build version string, e.g. "0.1.0" (used for the AppImage install/upgrade
// notice and the About box).
inline QString appVersion()
{
    return QStringLiteral(PHARMADESK_VERSION);
}

// Short product tagline (login subtitle / About card).
inline QString productTagline()
{
    return QStringLiteral("Pharmacy Management System");
}

// Developer / author credit. The per-pharmacy identity is white-label (set via
// the setup wizard); THIS is the fixed credit for whoever built the software —
// shown in the login footer and the Admin → About card. Single source of truth.
inline QString developer()
{
    return QStringLiteral("M Usman");
}
inline QString developerGithub()
{
    return QStringLiteral("github.com/mosman092");
}
inline QStringList developerEmails()
{
    return {QStringLiteral("musmaniqbalbaloch@gmail.com"), QStringLiteral("mosman092@hotmail.com")};
}

// QApplication organization name → drives QStandardPaths::AppDataLocation, i.e.
// ~/.local/share/<organizationName>/<applicationName>/. Keep stable per build so
// each white-label instance owns a distinct data directory.
inline QString organizationName()
{
    return QStringLiteral("PharmaDesk");
}

// SQLite database filename inside the app-data directory.
inline QString dbFileName()
{
    return QStringLiteral("pharmadesk.sqlite");
}

// Prefix for timestamped backup files (e.g. pharmadesk_20260608_2230.sqlite).
inline QString backupPrefix()
{
    return QStringLiteral("pharmadesk");
}

// Freedesktop desktop-entry / icon base name (pharmadesk.desktop, pharmadesk.png).
inline QString desktopId()
{
    return QStringLiteral("pharmadesk");
}
inline QString iconName()
{
    return QStringLiteral("pharmadesk");
}

} // namespace Branding
