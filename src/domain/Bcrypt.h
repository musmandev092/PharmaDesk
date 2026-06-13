#pragma once

#include <QString>

// bcrypt password hashing, backed by the OS libxcrypt (crypt_r / crypt_gensalt_rn).
// Produces standard $2b$ hashes that interoperate with PHP's password_hash
// ($2y$): hashes made here verify in PHP and PHP $2y$ hashes verify here, so
// staff PIN hashes migrated from the PHP/Postgres app keep working unchanged.
namespace Bcrypt {

// Default work factor — matches the PHP app (password_hash cost => 12).
inline constexpr int DefaultCost = 12;

// Hash a plaintext PIN/password. Returns the 60-char "$2b$..." string, or an
// empty QString on failure.
QString hash(const QString &plain, int cost = DefaultCost);

// Constant-time-ish verify of plain against a stored bcrypt hash.
bool verify(const QString &plain, const QString &storedHash);

} // namespace Bcrypt
