#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVector>

// A logged-in / created user (the fields the app needs after authentication).
struct UserRecord
{
    qint64 id = -1;
    QString fullName;
    QString username;
    QString role; // ADMIN / MANAGER / CASHIER
    qint64 branchId = 1;
    bool mustRotatePin = false;
    bool valid = false;
};

// Details collected by the setup wizard to create the first admin.
struct AdminDraft
{
    QString fullName;
    QString username;
    QString pinHash;      // already bcrypt-hashed by the caller (domain concern)
    QString pharmacyName; // also written to branches.id=1 name
};

// A user row for the admin user-management screen.
struct UserAdminRow
{
    qint64 id = 0;
    QString fullName;
    QString username;
    QString role;
    bool isActive = true;
    QString lastLoginAt;
};

// Data access for the users table. createAdmin() runs as one transaction.
class UserRepository
{
public:
    explicit UserRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    // Case-insensitive existence check across non-deleted users.
    bool usernameExistsCI(const QString &username) const;

    // Transaction: ensure branch id=1 (name = pharmacyName) -> insert ADMIN
    // user -> insert pin_history row. Returns the new user id, or -1 on failure
    // (errorString() then holds the reason). must_rotate_pin = 0 because the
    // operator chose this PIN themselves.
    qint64 createAdmin(const AdminDraft &draft);

    // Loads an active, non-deleted user by username (case-insensitive). The
    // bcrypt hash is returned via outPinHash for the caller to verify.
    UserRecord findForLogin(const QString &username, QString *outPinHash) const;

    // Stamps last_login_at = now for the given user id.
    bool touchLastLogin(qint64 userId);

    // ── Admin user management (Phase 6) ────────────────────────────────────
    QVector<UserAdminRow> listUsers() const;

    // Create a staff user with the given role. pinHash already bcrypt-hashed.
    // Transaction: insert user + seed pin_history. Returns id or -1.
    qint64 createUser(const QString &fullName, const QString &username, const QString &role,
                      const QString &pinHash, bool isActive, qint64 actingUserId);

    // Update name/role/active (not the PIN). actingUserId for audit.
    bool updateUser(qint64 id, const QString &fullName, const QString &role, bool isActive,
                    qint64 actingUserId);

    // Set a new PIN (already hashed): push pin_history, trim to 5, update
    // pin_hash + pin_changed_at. requireRotation sets must_rotate_pin (an admin
    // reset forces the user to change it next login; a self-change clears it).
    bool setPin(qint64 id, const QString &pinHash, qint64 actingUserId,
                bool requireRotation = false);

    // Write a LOGIN_SUCCESS / LOGIN_FAILED audit row.
    void recordLogin(qint64 userId, bool success, const QString &username);

    // ── Brute-force PIN-login lockout (security) ───────────────────────────
    // Seconds remaining on an active, non-deleted user's lockout window (0 if
    // not currently locked or the user does not exist).
    int secondsLockedOut(const QString &username) const;

    // Record one failed login: increment failed_attempts and, once the
    // LockoutThreshold (5) is reached, set an exponential-backoff lock window
    // (30s, 60s, 120s, … capped at 15 min). No-op for unknown/inactive users.
    void registerFailedLogin(const QString &username);

    // Clear the failure counter + lock window on a successful login.
    void clearFailedLogins(qint64 userId);

    // Most-recent PIN hashes (≤ limit) for PinPolicy history checks.
    QStringList recentPinHashes(qint64 id, int limit) const;

    // Current bcrypt PIN hash (for change-PIN's verify + differs-from-current).
    QString pinHash(qint64 id) const;

    // Days since pin_changed_at (for the 90-day rotation policy). Large if unknown.
    int pinAgeDays(qint64 id) const;

    // Number of OTHER active, non-deleted ADMINs (for last-admin protection).
    int otherActiveAdmins(qint64 exceptId) const;

    // ── Manager-PIN override (Phase 1) ─────────────────────────────────────
    // Result of a manager/admin override (discount auth, controlled-substance
    // witness, void).
    struct OverrideResult
    {
        bool ok = false;
        qint64 userId = 0;
        QString fullName;
        QString role;
        QString error; // user-facing reason when ok == false
    };

    // PIN-only override: bcrypt-scan active MANAGER/ADMIN users (excluding
    // forbidUserId, e.g. the cashier), audit OVERRIDE_GRANTED / OVERRIDE_DENIED.
    // `action` is a short label ("discount", "controlled_witness", "void").
    OverrideResult verifyManagerOverride(const QString &pin, qint64 forbidUserId,
                                         const QString &action);

    // True if userId is an active, non-deleted MANAGER or ADMIN.
    bool isActiveManagerOrAdmin(qint64 userId) const;

    // Role string ("ADMIN"/"MANAGER"/"CASHIER") for a user, or empty if unknown.
    QString roleOf(qint64 userId) const;

    QString errorString() const { return m_error; }

private:
    QSqlDatabase m_db;
    mutable QString m_error;
};
