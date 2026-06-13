#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// MedicineForm enum, grouped for the dosage-form picker. Mirrors the
// $FORM_GROUPS list in pos/frontend/pages/inventory/medicines.php and the
// CREATE TYPE "MedicineForm" in db/schema/init.sql. Also the schedule/tax/
// reorder-unit option lists used by the medicine form.
namespace MedicineForm {

struct Option
{
    QString key;
    QString label;
};
struct Group
{
    QString title;
    QVector<Option> options;
};

// All groups (for an optgroup-style combo).
const QVector<Group> &groups();

// Flat list of valid enum keys (for validation).
QStringList allKeys();
bool isValid(const QString &key);

// Human label for a key (e.g. "TABLET" → "Tablet"); falls back to the key.
QString label(const QString &key);

} // namespace MedicineForm

namespace ControlledSchedule {
inline QStringList all()
{
    return {QStringLiteral("NONE"), QStringLiteral("SCHEDULE_G"), QStringLiteral("SCHEDULE_H"),
            QStringLiteral("NARCOTIC")};
}
} // namespace ControlledSchedule
namespace TaxCode {
inline QStringList all()
{
    return {QStringLiteral("EXEMPT"), QStringLiteral("STANDARD_18"), QStringLiteral("REDUCED"),
            QStringLiteral("ZERO_RATED")};
}
} // namespace TaxCode
