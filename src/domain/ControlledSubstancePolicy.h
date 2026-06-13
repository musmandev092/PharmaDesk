#pragma once

#include <QString>
#include <QStringList>

// Pure policy: given a medicine's controlled schedule, decide what extra
// authorization a dispense requires. Deliberately generic (not tied to any one
// country's regulator): the gated tier is a single named policy point in the
// .cpp and can later be made settings-driven. No DB, no UI — unit-testable.
namespace ControlledSubstancePolicy {

// Any schedule other than "NONE"/empty is a controlled substance.
bool isControlled(const QString &schedule);

// The strictest tier requires a second-person witness (an active manager/admin
// other than the cashier) and a prescriber license number captured at dispense.
bool requiresWitness(const QString &schedule);
bool requiresPrescriberLicense(const QString &schedule);

// Aggregate helpers over the set of schedules present in a cart.
bool anyControlled(const QStringList &schedules);
bool anyRequiresWitness(const QStringList &schedules);
bool anyRequiresPrescriberLicense(const QStringList &schedules);

} // namespace ControlledSubstancePolicy
