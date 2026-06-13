#pragma once

#include <QColor>
#include <QString>

class QTableWidget;
class QTableWidgetItem;
class QWidget;

// Small shared UI helpers for consistent table presentation.
namespace UiUtil {

// Canonical status colours for tinting table cells and inline status text
// (expiry tiers, stock-session variance, return/GRN status, validation errors).
// These are intentionally distinct from the button/chrome design tokens in
// resources/theme.qss: a QTableWidgetItem cannot carry a QSS object name, so a
// status colour has to be applied from code. This namespace is the SINGLE SOURCE
// OF TRUTH for those colours — never hardcode these hex values at a call site.
// For stylesheet strings, use e.g. UiUtil::Palette::Danger.name().
namespace Palette {
inline const QColor Danger{QStringLiteral("#dc2828")};       // errors, expired, cancelled, negative
inline const QColor Success{QStringLiteral("#279b74")};      // ok, posted, settled, positive
inline const QColor Warning{QStringLiteral("#db7706")};      // warnings, partial, confirm-soon
inline const QColor Info{QStringLiteral("#0da2e7")};         // informational (amber-tier expiry)
inline const QColor Muted{QStringLiteral("#607085")};        // de-emphasised / secondary text
inline const QColor MutedFaint{QStringLiteral("#9aa6b5")};   // faint placeholder / unavailable text
inline const QColor BorderSubtle{QStringLiteral("#e0e5eb")}; // subtle 1px borders
} // namespace Palette

// Make a data-entry dialog safe for barcode scanners (which type the code then
// send Enter). Disables every button's auto-default so a stray Enter can't
// submit the form, and makes Enter inside any line edit advance to the next
// field instead. Call once at the end of a dialog's constructor. Do NOT use on
// dialogs where Enter-to-submit is intended (e.g. the login screen).
void makeScanSafe(QWidget *dialog);

// Right-align + vertically center a (numeric) cell. No-op if item is null.
void rightAlign(QTableWidgetItem *item);

// Colour a cell's text (e.g. status badges, low stock).
void colorItem(QTableWidgetItem *item, const QColor &color, bool bold = false);

// Call right before repopulating a QTableWidget: clears spans left by a prior
// empty-state row.
void beginFill(QTableWidget *table);

// If the table has no data rows, show a single centered muted message spanning
// all columns. Call after populating when rowCount would otherwise be 0.
void emptyState(QTableWidget *table, const QString &message);

} // namespace UiUtil
