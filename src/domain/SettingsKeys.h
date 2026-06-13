#pragma once

#include <QString>

// Canonical settings keys + their defaults. Keys and default texts mirror the
// PHP app (pos/frontend/pages/admin/settings.php and the fallbacks in
// pos/backend/services/EscPos.php lines 63-70). logo_path is new to the native
// port (the PHP app hardcoded /assets/logo.jpeg).
namespace SettingsKeys {

inline const QString PharmacyName = QStringLiteral("pharmacy_name");
inline const QString PharmacyAddress = QStringLiteral("pharmacy_address");
inline const QString PharmacyPhone = QStringLiteral("pharmacy_phone");
inline const QString PharmacyNtn = QStringLiteral("pharmacy_ntn");
inline const QString ReturnPolicyText = QStringLiteral("return_policy_text");
inline const QString ReceiptFooter = QStringLiteral("receipt_footer_message");
inline const QString LogoPath = QStringLiteral("logo_path");
inline const QString Theme = QStringLiteral("theme");

// Phase 6 — automated backup.
inline const QString BackupDir = QStringLiteral("backup_dir");
inline const QString AutoBackup = QStringLiteral("auto_backup");      // "1"/"0"
inline const QString LastBackupAt = QStringLiteral("last_backup_at"); // ISO-8601

// AppImage desktop integration — last version installed/integrated, so the app
// can tell a first install from an upgrade and announce it once.
inline const QString InstalledVersion = QStringLiteral("installed_version");

// Phase 4 — thermal (ESC/POS) receipt printing via an existing CUPS queue.
inline const QString ThermalEnabled = QStringLiteral("thermal_enabled"); // "1"/"0"
inline const QString ThermalQueue = QStringLiteral("thermal_queue");     // CUPS queue name

// Defaults (verbatim from the PHP fallbacks).
inline QString defaultReturnPolicy()
{
    return QStringLiteral(
        "Medicine will be taken back within 72 hours along with original invoice.");
}
inline QString defaultReceiptFooter()
{
    return QStringLiteral("To save one life is like saving humanity.");
}

// Max lengths from the PHP $LIMITS array (settings.php).
inline constexpr int MaxPharmacyName = 160;
inline constexpr int MaxPharmacyAddress = 500;
inline constexpr int MaxPharmacyPhone = 60;
inline constexpr int MaxPharmacyNtn = 40;
inline constexpr int MaxReturnPolicy = 2000;
inline constexpr int MaxReceiptFooter = 200;

} // namespace SettingsKeys
