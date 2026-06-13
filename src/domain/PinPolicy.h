#pragma once

#include <QString>
#include <QStringList>
#include <optional>

// PIN strength + reuse policy. 1:1 port of pos/backend/PinPolicy.php.
// Rules: 6-8 digits; not in a blocklist of obvious sequences; no straight
// ascending/descending run; not a reuse of the last 5 PINs (when history is
// supplied); different from the current PIN (when supplied).
namespace PinPolicy {

inline constexpr int MinLen = 6;
inline constexpr int MaxLen = 8;
inline constexpr int HistoryKeep = 5;
inline constexpr int RotationDays = 90;

// Returns std::nullopt when the PIN is acceptable, otherwise a user-facing
// error message (identical wording to the PHP version).
//
// historyHashes: most-recent bcrypt PIN hashes for this user (≤ HistoryKeep);
//                pass empty when creating a brand-new user (the wizard case).
// currentHash:   the user's current bcrypt PIN hash, or empty when none.
std::optional<QString> validate(const QString &newPin, const QStringList &historyHashes = {},
                                const QString &currentHash = QString());

} // namespace PinPolicy
