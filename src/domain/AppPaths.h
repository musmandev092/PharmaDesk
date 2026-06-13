#pragma once

#include <QString>

// Resolves writable application-data locations. The SQLite DB and uploaded logo
// live under QStandardPaths::AppDataLocation, never inside the repo or the
// read-only AppImage.
namespace AppPaths {

// Ensures the app-data dir and logos/ subdir exist. Returns false on failure.
bool ensureDirs();

// One-time: if there's no DB at the current location but a database exists at a
// previous (pre-rename) location, copy it across so the operator keeps their
// data. Safe to call on every launch (no-op once migrated).
void migrateLegacyData();

// Absolute path to the SQLite database file.
QString dbFilePath();

// Absolute path to the logos directory (created by ensureDirs).
QString logosDir();

// Absolute path to the logs directory (created by ensureDirs).
QString logsDir();

// Restrict the app-data dir to the owner (0700) and the SQLite DB + its WAL
// sidecars to owner read/write (0600). Defence-in-depth for the local PIN
// hashes / audit trail. Safe to call every launch; best-effort (ignores errors
// on filesystems that don't support POSIX perms).
void hardenPermissions();

} // namespace AppPaths
