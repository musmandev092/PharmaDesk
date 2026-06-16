// Characterization tests for the pure authorization policies. These pin the
// CURRENT behavior (before any refactor) of ControlledSubstancePolicy and
// DiscountAuthorizationPolicy — including the deliberate distinctions that a
// reader might otherwise "fix" by accident:
//   * SCHEDULE_G / SCHEDULE_H are *controlled* (flagged) but NOT *gated* (no
//     witness / license) — only NARCOTIC is gated.
//   * Role and schedule matching is case-SENSITIVE and exact.
// No DB is used; the harness arguments are ignored.

#include "framework/TestStats.h"

#include "domain/ControlledSubstancePolicy.h"
#include "domain/DiscountAuthorizationPolicy.h"

namespace pharmadesk_tests {

TestStats run_policy_tests(QSqlDatabase, qint64)
{
    TestStats s;
    s.module = QStringLiteral("policy");

    using namespace ControlledSubstancePolicy;

    // isControlled: anything that is not empty and not "NONE".
    s.check(!isControlled(QString()), QStringLiteral("isControlled: empty → false"));
    s.check(!isControlled(QStringLiteral("NONE")), QStringLiteral("isControlled: NONE → false"));
    s.check(isControlled(QStringLiteral("SCHEDULE_G")), QStringLiteral("isControlled: G → true"));
    s.check(isControlled(QStringLiteral("SCHEDULE_H")), QStringLiteral("isControlled: H → true"));
    s.check(isControlled(QStringLiteral("NARCOTIC")),
            QStringLiteral("isControlled: NARCOTIC → true"));
    // Case-sensitive: "none" (lowercase) is NOT the sentinel, so it counts as controlled.
    s.check(isControlled(QStringLiteral("none")),
            QStringLiteral("isControlled: lowercase 'none' → true (exact-match sentinel)"));

    // requiresWitness / requiresPrescriberLicense: ONLY the NARCOTIC tier.
    s.check(requiresWitness(QStringLiteral("NARCOTIC")),
            QStringLiteral("requiresWitness: NARCOTIC → true"));
    s.check(!requiresWitness(QStringLiteral("SCHEDULE_G")),
            QStringLiteral("requiresWitness: G → false (controlled but not gated)"));
    s.check(!requiresWitness(QStringLiteral("SCHEDULE_H")),
            QStringLiteral("requiresWitness: H → false"));
    s.check(!requiresWitness(QStringLiteral("NONE")),
            QStringLiteral("requiresWitness: NONE → false"));
    s.check(!requiresWitness(QStringLiteral("narcotic")),
            QStringLiteral("requiresWitness: lowercase 'narcotic' → false (case-sensitive)"));
    s.check(requiresPrescriberLicense(QStringLiteral("NARCOTIC")),
            QStringLiteral("requiresPrescriberLicense: NARCOTIC → true"));
    s.check(!requiresPrescriberLicense(QStringLiteral("SCHEDULE_H")),
            QStringLiteral("requiresPrescriberLicense: H → false"));

    // Aggregates over a cart.
    s.check(!anyControlled(QStringList()), QStringLiteral("anyControlled: empty list → false"));
    s.check(!anyControlled({QStringLiteral("NONE"), QStringLiteral("NONE")}),
            QStringLiteral("anyControlled: all NONE → false"));
    s.check(anyControlled({QStringLiteral("NONE"), QStringLiteral("SCHEDULE_G")}),
            QStringLiteral("anyControlled: one G → true"));
    s.check(!anyRequiresWitness({QStringLiteral("SCHEDULE_G"), QStringLiteral("SCHEDULE_H")}),
            QStringLiteral("anyRequiresWitness: G+H → false"));
    s.check(anyRequiresWitness({QStringLiteral("NONE"), QStringLiteral("NARCOTIC")}),
            QStringLiteral("anyRequiresWitness: contains NARCOTIC → true"));
    s.check(anyRequiresPrescriberLicense({QStringLiteral("NARCOTIC")}),
            QStringLiteral("anyRequiresPrescriberLicense: NARCOTIC → true"));

    // ── DiscountAuthorizationPolicy ──────────────────────────────────────────
    using namespace DiscountAuthorizationPolicy;

    s.check(isManagerial(QStringLiteral("MANAGER")),
            QStringLiteral("isManagerial: MANAGER → true"));
    s.check(isManagerial(QStringLiteral("ADMIN")), QStringLiteral("isManagerial: ADMIN → true"));
    s.check(!isManagerial(QStringLiteral("CASHIER")),
            QStringLiteral("isManagerial: CASHIER → false"));
    s.check(!isManagerial(QString()), QStringLiteral("isManagerial: empty → false"));
    s.check(!isManagerial(QStringLiteral("manager")),
            QStringLiteral("isManagerial: lowercase 'manager' → false (case-sensitive)"));
    s.check(!isManagerial(QStringLiteral("SUPERADMIN")),
            QStringLiteral("isManagerial: unknown role → false"));

    // requiresManagerOverride: only a cashier applying a POSITIVE discount needs one.
    s.check(requiresManagerOverride(QStringLiteral("CASHIER"), true),
            QStringLiteral("override: CASHIER + positive → required"));
    s.check(!requiresManagerOverride(QStringLiteral("CASHIER"), false),
            QStringLiteral("override: CASHIER + zero discount → not required"));
    s.check(!requiresManagerOverride(QStringLiteral("MANAGER"), true),
            QStringLiteral("override: MANAGER self-authorizes"));
    s.check(!requiresManagerOverride(QStringLiteral("ADMIN"), true),
            QStringLiteral("override: ADMIN self-authorizes"));
    s.check(requiresManagerOverride(QString(), true),
            QStringLiteral("override: unknown role + positive → required (fail-closed)"));

    return s;
}

} // namespace pharmadesk_tests
