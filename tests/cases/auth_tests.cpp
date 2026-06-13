// Tests for the brute-force PIN-login lockout (UserRepository).
//
// Behaviour under test:
//   - registerFailedLogin() increments a per-user counter; once 5 consecutive
//     failures accrue, the account is locked for a backoff window.
//   - secondsLockedOut() returns the remaining lock time (>0 while locked,
//     0 otherwise) for an active, non-deleted user; 0 for unknown usernames.
//   - clearFailedLogins() resets the counter + lock (called on success).
//   - Fewer than 5 failures must NOT lock the account.

#include "data/UserRepository.h"
#include "domain/Bcrypt.h"
#include "framework/TestStats.h"

#include <QSqlQuery>
#include <QString>
#include <QVariant>

namespace pharmadesk_tests {

namespace {

// Reads users.failed_attempts for a given id (returns -1 if not found).
int failedAttempts(QSqlDatabase db, qint64 id)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT failed_attempts FROM users WHERE id = ?"));
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return -1;
}

} // namespace

TestStats run_auth_tests(QSqlDatabase db, qint64 userId)
{
    TestStats s;
    s.module = QStringLiteral("auth");

    UserRepository users(db);

    // --------------------------------------------------------------------
    // 1) Five consecutive failures lock the account.
    // --------------------------------------------------------------------
    const QString username = QStringLiteral("auth_lockme");
    const qint64 id
        = users.createUser(QStringLiteral("Auth Lockout"), username, QStringLiteral("CASHIER"),
                           Bcrypt::hash(QStringLiteral("123456")), true, userId);
    s.check(id > 0, QStringLiteral("cashier created"));

    // Not locked to begin with.
    s.check(users.secondsLockedOut(username) == 0, QStringLiteral("fresh user not locked"));

    for (int i = 0; i < 5; ++i) {
        users.registerFailedLogin(username);
    }
    s.check(users.secondsLockedOut(username) > 0, QStringLiteral("locked after 5 failed attempts"));
    s.check(failedAttempts(db, id) == 5, QStringLiteral("failed_attempts == 5 after 5 failures"));

    // Unknown username is never locked.
    s.check(users.secondsLockedOut(QStringLiteral("auth_nobody_here")) == 0,
            QStringLiteral("unknown username not locked"));

    // --------------------------------------------------------------------
    // 2) clearFailedLogins() resets counter + lock (successful login path).
    // --------------------------------------------------------------------
    users.clearFailedLogins(id);
    s.check(users.secondsLockedOut(username) == 0,
            QStringLiteral("not locked after clearFailedLogins"));
    s.check(failedAttempts(db, id) == 0, QStringLiteral("failed_attempts == 0 after clear"));

    // --------------------------------------------------------------------
    // 3) A single failed login does NOT lock a fresh account.
    // --------------------------------------------------------------------
    const QString user2 = QStringLiteral("auth_oneshot");
    const qint64 id2
        = users.createUser(QStringLiteral("Auth Oneshot"), user2, QStringLiteral("CASHIER"),
                           Bcrypt::hash(QStringLiteral("123456")), true, userId);
    s.check(id2 > 0, QStringLiteral("second cashier created"));

    users.registerFailedLogin(user2);
    s.check(users.secondsLockedOut(user2) == 0, QStringLiteral("single failure does not lock"));
    s.check(failedAttempts(db, id2) == 1, QStringLiteral("failed_attempts == 1 after one failure"));

    return s;
}

} // namespace pharmadesk_tests
