#include "domain/ControlledSubstancePolicy.h"

namespace {

const QString kNone = QStringLiteral("NONE");

// The single configurable policy point: schedules that demand the full
// two-person witness + prescriber-license gate. Today that is the narcotic
// tier; SCHEDULE_G / SCHEDULE_H are controlled (flagged) but not gated.
bool isGatedSchedule(const QString &schedule)
{
    return schedule == QStringLiteral("NARCOTIC");
}

} // namespace

namespace ControlledSubstancePolicy {

bool isControlled(const QString &schedule)
{
    return !schedule.isEmpty() && schedule != kNone;
}

bool requiresWitness(const QString &schedule)
{
    return isGatedSchedule(schedule);
}

bool requiresPrescriberLicense(const QString &schedule)
{
    return isGatedSchedule(schedule);
}

bool anyControlled(const QStringList &schedules)
{
    for (const QString &s : schedules) {
        if (isControlled(s)) {
            return true;
        }
    }
    return false;
}

bool anyRequiresWitness(const QStringList &schedules)
{
    for (const QString &s : schedules) {
        if (requiresWitness(s)) {
            return true;
        }
    }
    return false;
}

bool anyRequiresPrescriberLicense(const QStringList &schedules)
{
    for (const QString &s : schedules) {
        if (requiresPrescriberLicense(s)) {
            return true;
        }
    }
    return false;
}

} // namespace ControlledSubstancePolicy
