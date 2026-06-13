#include "data/UserRepository.h"

#include "data/Audit.h"
#include "domain/Bcrypt.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <algorithm>

namespace {
// Consecutive failed logins after which the account locks out.
constexpr int kLockoutThreshold = 5;
// Backoff window, in seconds: 30s on the first lock, doubling each further
// failure, capped at 15 minutes.
constexpr int kLockoutBaseSeconds = 30;
constexpr int kLockoutMaxSeconds = 900;
} // namespace

bool UserRepository::usernameExistsCI(const QString &username) const
{
    QSqlQuery q(m_db);
    // username column is COLLATE NOCASE, but be explicit so this is correct
    // regardless of the column collation.
    q.prepare(QStringLiteral(
        "SELECT 1 FROM users WHERE lower(username) = lower(?) AND deleted_at IS NULL LIMIT 1"));
    q.addBindValue(username);
    return q.exec() && q.next();
}

qint64 UserRepository::createAdmin(const AdminDraft &draft)
{
    if (!m_db.transaction()) {
        m_error = m_db.lastError().text();
        return -1;
    }

    QSqlQuery q(m_db);

    // 1) Ensure branch id=1 exists; set its name to the pharmacy name.
    q.prepare(QStringLiteral(
        "INSERT INTO branches (id, name, is_active) VALUES (1, ?, 1) "
        "ON CONFLICT(id) DO UPDATE SET name = excluded.name, updated_at = CURRENT_TIMESTAMP"));
    q.addBindValue(draft.pharmacyName.isEmpty() ? QStringLiteral("Main") : draft.pharmacyName);
    if (!q.exec()) {
        m_error = q.lastError().text();
        m_db.rollback();
        return -1;
    }

    // 2) Insert the ADMIN user.
    q.prepare(QStringLiteral("INSERT INTO users "
                             "  (full_name, username, pin_hash, role, is_active, branch_id, "
                             "   pin_changed_at, must_rotate_pin) "
                             "VALUES (?, ?, ?, 'ADMIN', 1, 1, CURRENT_TIMESTAMP, 0)"));
    q.addBindValue(draft.fullName);
    q.addBindValue(draft.username);
    q.addBindValue(draft.pinHash);
    if (!q.exec()) {
        m_error = q.lastError().text();
        m_db.rollback();
        return -1;
    }
    const qint64 userId = q.lastInsertId().toLongLong();

    // 3) Seed pin_history with the same hash.
    q.prepare(QStringLiteral("INSERT INTO pin_history (user_id, pin_hash, changed_at) "
                             "VALUES (?, ?, CURRENT_TIMESTAMP)"));
    q.addBindValue(userId);
    q.addBindValue(draft.pinHash);
    if (!q.exec()) {
        m_error = q.lastError().text();
        m_db.rollback();
        return -1;
    }

    if (!m_db.commit()) {
        m_error = m_db.lastError().text();
        m_db.rollback();
        return -1;
    }
    return userId;
}

UserRecord UserRepository::findForLogin(const QString &username, QString *outPinHash) const
{
    UserRecord rec;
    QSqlQuery q(m_db);
    q.prepare(
        QStringLiteral("SELECT id, full_name, username, pin_hash, role, branch_id, must_rotate_pin "
                       "FROM users "
                       "WHERE lower(username) = lower(?) AND is_active = 1 AND deleted_at IS NULL "
                       "LIMIT 1"));
    q.addBindValue(username);
    if (q.exec() && q.next()) {
        rec.id = q.value(0).toLongLong();
        rec.fullName = q.value(1).toString();
        rec.username = q.value(2).toString();
        if (outPinHash) {
            *outPinHash = q.value(3).toString();
        }
        rec.role = q.value(4).toString();
        rec.branchId = q.value(5).toLongLong();
        rec.mustRotatePin = q.value(6).toInt() != 0;
        rec.valid = true;
    }
    return rec;
}

bool UserRepository::touchLastLogin(qint64 userId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE users SET last_login_at = CURRENT_TIMESTAMP WHERE id = ?"));
    q.addBindValue(userId);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

QVector<UserAdminRow> UserRepository::listUsers() const
{
    QVector<UserAdminRow> out;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT id, full_name, username, role, is_active, COALESCE(last_login_at,'') "
        "FROM users WHERE deleted_at IS NULL ORDER BY role, full_name"));
    while (q.next()) {
        UserAdminRow r;
        r.id = q.value(0).toLongLong();
        r.fullName = q.value(1).toString();
        r.username = q.value(2).toString();
        r.role = q.value(3).toString();
        r.isActive = q.value(4).toInt() != 0;
        r.lastLoginAt = q.value(5).toString().left(19);
        out.push_back(r);
    }
    return out;
}

qint64 UserRepository::createUser(const QString &fullName, const QString &username,
                                  const QString &role, const QString &pinHash, bool isActive,
                                  qint64 actingUserId)
{
    if (!m_db.transaction()) {
        m_error = m_db.lastError().text();
        return -1;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO users (full_name, username, pin_hash, role, is_active, branch_id, "
        " pin_changed_at, must_rotate_pin) VALUES (?, ?, ?, ?, ?, 1, CURRENT_TIMESTAMP, 0)"));
    q.addBindValue(fullName);
    q.addBindValue(username);
    q.addBindValue(pinHash);
    q.addBindValue(role);
    q.addBindValue(isActive ? 1 : 0);
    if (!q.exec()) {
        m_error = q.lastError().text();
        m_db.rollback();
        return -1;
    }
    const qint64 id = q.lastInsertId().toLongLong();

    q.prepare(QStringLiteral("INSERT INTO pin_history (user_id, pin_hash, changed_at) VALUES (?, "
                             "?, CURRENT_TIMESTAMP)"));
    q.addBindValue(id);
    q.addBindValue(pinHash);
    if (!q.exec()) {
        m_error = q.lastError().text();
        m_db.rollback();
        return -1;
    }

    Audit::write(m_db, actingUserId, QStringLiteral("USER_CREATED"), QStringLiteral("users"), id,
                 QString(),
                 QStringLiteral("{\"username\":\"%1\",\"role\":\"%2\"}").arg(username, role));

    if (!m_db.commit()) {
        m_error = m_db.lastError().text();
        m_db.rollback();
        return -1;
    }
    return id;
}

bool UserRepository::updateUser(qint64 id, const QString &fullName, const QString &role,
                                bool isActive, qint64 actingUserId)
{
    // Last-active-admin protection: don't let the only admin demote/deactivate
    // themselves and lock everyone out.
    {
        QSqlQuery cur(m_db);
        cur.prepare(QStringLiteral("SELECT role, is_active FROM users WHERE id = ?"));
        cur.addBindValue(id);
        if (cur.exec() && cur.next()) {
            const bool wasActiveAdmin
                = cur.value(0).toString() == QLatin1String("ADMIN") && cur.value(1).toInt() != 0;
            const bool losingAdmin = role != QLatin1String("ADMIN") || !isActive;
            if (wasActiveAdmin && losingAdmin && otherActiveAdmins(id) == 0) {
                m_error = QStringLiteral("This is the last active administrator — keep at least "
                                         "one admin active.");
                return false;
            }
        }
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE users SET full_name = ?, role = ?, is_active = ?, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = ? AND deleted_at IS NULL"));
    q.addBindValue(fullName);
    q.addBindValue(role);
    q.addBindValue(isActive ? 1 : 0);
    q.addBindValue(id);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    Audit::write(m_db, actingUserId, QStringLiteral("USER_UPDATED"), QStringLiteral("users"), id);
    return true;
}

bool UserRepository::setPin(qint64 id, const QString &pinHash, qint64 actingUserId,
                            bool requireRotation)
{
    if (!m_db.transaction()) {
        m_error = m_db.lastError().text();
        return false;
    }
    QSqlQuery q(m_db);

    q.prepare(QStringLiteral("INSERT INTO pin_history (user_id, pin_hash, changed_at) VALUES (?, "
                             "?, CURRENT_TIMESTAMP)"));
    q.addBindValue(id);
    q.addBindValue(pinHash);
    if (!q.exec()) {
        m_error = q.lastError().text();
        m_db.rollback();
        return false;
    }

    // Trim history to the 5 newest rows for this user.
    q.prepare(QStringLiteral(
        "DELETE FROM pin_history WHERE user_id = ? AND id NOT IN "
        "(SELECT id FROM pin_history WHERE user_id = ? ORDER BY changed_at DESC LIMIT 5)"));
    q.addBindValue(id);
    q.addBindValue(id);
    if (!q.exec()) {
        m_error = q.lastError().text();
        m_db.rollback();
        return false;
    }

    q.prepare(QStringLiteral(
        "UPDATE users SET pin_hash = ?, pin_changed_at = CURRENT_TIMESTAMP, must_rotate_pin = ?, "
        " updated_at = CURRENT_TIMESTAMP WHERE id = ?"));
    q.addBindValue(pinHash);
    q.addBindValue(requireRotation ? 1 : 0);
    q.addBindValue(id);
    if (!q.exec()) {
        m_error = q.lastError().text();
        m_db.rollback();
        return false;
    }

    Audit::write(m_db, actingUserId, QStringLiteral("PIN_CHANGED"), QStringLiteral("users"), id);

    if (!m_db.commit()) {
        m_error = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}

QStringList UserRepository::recentPinHashes(qint64 id, int limit) const
{
    QStringList out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT pin_hash FROM pin_history WHERE user_id = ? ORDER BY changed_at DESC LIMIT ?"));
    q.addBindValue(id);
    q.addBindValue(limit);
    if (q.exec()) {
        while (q.next()) {
            out << q.value(0).toString();
        }
    }
    return out;
}

QString UserRepository::pinHash(qint64 id) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT pin_hash FROM users WHERE id = ?"));
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    return QString();
}

int UserRepository::pinAgeDays(qint64 id) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT CAST(julianday('now') - julianday(pin_changed_at) AS INTEGER) "
                             "FROM users WHERE id = ?"));
    q.addBindValue(id);
    if (q.exec() && q.next() && !q.value(0).isNull()) {
        return q.value(0).toInt();
    }
    // Unknown / NULL / query error → report a very large age so the 90-day
    // policy FORCES a rotation (fail-safe), honoring the header contract
    // ("Large if unknown"). pin_changed_at is NOT NULL with a CURRENT_TIMESTAMP
    // default, so this only fires for a missing user or a query failure.
    return 365000; // ~1000 years
}

void UserRepository::recordLogin(qint64 userId, bool success, const QString &username)
{
    Audit::write(m_db, success ? userId : 0,
                 success ? QStringLiteral("LOGIN_SUCCESS") : QStringLiteral("LOGIN_FAILED"),
                 QStringLiteral("users"), success ? userId : 0, QString(),
                 QStringLiteral("{\"username\":\"%1\"}").arg(username));
}

int UserRepository::secondsLockedOut(const QString &username) const
{
    QSqlQuery q(m_db);
    // SQLite computes the remaining seconds from the stored UTC timestamp; the
    // WHERE clause guarantees we only get a positive (future) value back.
    q.prepare(QStringLiteral(
        "SELECT CAST((julianday(locked_until) - julianday('now')) * 86400 AS INTEGER) + 1 "
        "FROM users "
        "WHERE lower(username) = lower(?) AND is_active = 1 AND deleted_at IS NULL "
        "AND locked_until IS NOT NULL AND locked_until > datetime('now') "
        "LIMIT 1"));
    q.addBindValue(username);
    if (q.exec() && q.next() && !q.value(0).isNull()) {
        return std::max(0, q.value(0).toInt());
    }
    return 0;
}

void UserRepository::registerFailedLogin(const QString &username)
{
    if (!m_db.transaction()) {
        m_error = m_db.lastError().text();
        return;
    }

    // Find the active, non-deleted user and its current failure count.
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, failed_attempts FROM users "
        "WHERE lower(username) = lower(?) AND is_active = 1 AND deleted_at IS NULL LIMIT 1"));
    q.addBindValue(username);
    if (!q.exec() || !q.next()) {
        m_db.rollback(); // unknown / inactive user → no-op
        return;
    }
    const qint64 id = q.value(0).toLongLong();
    const int newCount = q.value(1).toInt() + 1;

    QSqlQuery up(m_db);
    if (newCount >= kLockoutThreshold) {
        // Exponential backoff: 30s, 60s, 120s, … (capped) once locked.
        const int over = newCount - kLockoutThreshold; // 0 on the first lock
        qint64 lockSeconds = kLockoutBaseSeconds;
        for (int i = 0; i < over && lockSeconds < kLockoutMaxSeconds; ++i) {
            lockSeconds *= 2;
        }
        lockSeconds = std::min<qint64>(lockSeconds, kLockoutMaxSeconds);
        up.prepare(
            QStringLiteral("UPDATE users SET failed_attempts = failed_attempts + 1, "
                           "locked_until = datetime('now', '+' || ? || ' seconds') WHERE id = ?"));
        up.addBindValue(lockSeconds);
        up.addBindValue(id);
    } else {
        up.prepare(
            QStringLiteral("UPDATE users SET failed_attempts = failed_attempts + 1 WHERE id = ?"));
        up.addBindValue(id);
    }
    if (!up.exec()) {
        m_error = up.lastError().text();
        m_db.rollback();
        return;
    }

    if (!m_db.commit()) {
        m_error = m_db.lastError().text();
        m_db.rollback();
    }
}

void UserRepository::clearFailedLogins(qint64 userId)
{
    QSqlQuery q(m_db);
    q.prepare(
        QStringLiteral("UPDATE users SET failed_attempts = 0, locked_until = NULL WHERE id = ?"));
    q.addBindValue(userId);
    if (!q.exec()) {
        m_error = q.lastError().text();
    }
}

int UserRepository::otherActiveAdmins(qint64 exceptId) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT count(*) FROM users WHERE role='ADMIN' AND is_active=1 "
                             "AND deleted_at IS NULL AND id <> ?"));
    q.addBindValue(exceptId);
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}

QString UserRepository::roleOf(qint64 userId) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT role FROM users WHERE id = ? LIMIT 1"));
    q.addBindValue(userId);
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    return QString();
}

bool UserRepository::isActiveManagerOrAdmin(qint64 userId) const
{
    QSqlQuery q(m_db);
    q.prepare(
        QStringLiteral("SELECT 1 FROM users WHERE id = ? AND is_active = 1 AND deleted_at IS NULL "
                       "AND role IN ('MANAGER','ADMIN') LIMIT 1"));
    q.addBindValue(userId);
    return q.exec() && q.next();
}

UserRepository::OverrideResult UserRepository::verifyManagerOverride(const QString &pin,
                                                                     qint64 forbidUserId,
                                                                     const QString &action)
{
    OverrideResult res;

    const QString trimmedPin = pin.trimmed();
    if (trimmedPin.isEmpty()) {
        res.error = QStringLiteral("Manager PIN required.");
        return res;
    }

    // Pull active managers/admins (optionally excluding the cashier) and
    // bcrypt-verify against each. pin_hash is not indexable, so this is a linear
    // scan — the manager set is tiny per pharmacy, so the cost is negligible.
    QString sql = QStringLiteral(
        "SELECT id, full_name, role, pin_hash FROM users "
        "WHERE role IN ('MANAGER','ADMIN') AND is_active = 1 AND deleted_at IS NULL");
    if (forbidUserId > 0) {
        sql += QStringLiteral(" AND id <> ?");
    }
    QSqlQuery q(m_db);
    q.prepare(sql);
    if (forbidUserId > 0) {
        q.addBindValue(forbidUserId);
    }
    if (!q.exec()) {
        m_error = q.lastError().text();
        res.error = QStringLiteral("Could not verify override.");
        return res;
    }

    while (q.next()) {
        const QString storedHash = q.value(3).toString();
        if (Bcrypt::verify(trimmedPin, storedHash)) {
            res.ok = true;
            res.userId = q.value(0).toLongLong();
            res.fullName = q.value(1).toString();
            res.role = q.value(2).toString();
            Audit::write(
                m_db, res.userId, QStringLiteral("OVERRIDE_GRANTED"), QStringLiteral("users"),
                res.userId, QString(),
                QStringLiteral("{\"action\":\"%1\",\"role\":\"%2\"}").arg(action, res.role));
            return res;
        }
    }

    Audit::write(m_db, 0, QStringLiteral("OVERRIDE_DENIED"), QStringLiteral("users"), 0, QString(),
                 QStringLiteral("{\"action\":\"%1\"}").arg(action));
    res.error = QStringLiteral("Manager PIN not recognised.");
    return res;
}
