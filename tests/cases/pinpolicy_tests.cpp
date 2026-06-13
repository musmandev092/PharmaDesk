// Exhaustive unit tests for the staff PIN strength + reuse policy
// (domain/PinPolicy). Pure logic, but uses Bcrypt to build history / current
// hashes for the reuse and differs-from-current rules.
//
// Rules under test (from PinPolicy.cpp, evaluated in this order):
//   1. must be 6..8 digits          → "PIN must be 6-8 digits."
//   2. not in the blocklist         → "PIN is too predictable. ..."
//   3. no straight ascending/desc.  → "PIN must not be a sequence like ..."
//   4. not a recent reuse (history) → "PIN was used recently. ..."
//   5. different from current        → "New PIN must be different ..."
//   otherwise std::nullopt (accepted).

#include "domain/Bcrypt.h"
#include "domain/PinPolicy.h"
#include "framework/TestStats.h"

#include <QString>
#include <QStringList>
#include <optional>

namespace pharmadesk_tests {

namespace {

const QString kLenMsg = QStringLiteral("PIN must be 6-8 digits.");
const QString kBlockMsg = QStringLiteral("PIN is too predictable. Pick something else.");
const QString kSeqMsg = QStringLiteral("PIN must not be a sequence like 1234 or 9876.");
const QString kReuseMsg = QStringLiteral("PIN was used recently. Pick one you haven't used "
                                         "in your last 5 rotations.");
const QString kCurrentMsg = QStringLiteral("New PIN must be different from the current one.");

bool accepted(const std::optional<QString> &r)
{
    return !r.has_value();
}
bool rejected(const std::optional<QString> &r)
{
    return r.has_value();
}
bool rejectedWith(const std::optional<QString> &r, const QString &msg)
{
    return r.has_value() && r.value() == msg;
}

} // namespace

TestStats run_pinpolicy_tests(QSqlDatabase db, qint64 userId)
{
    (void)db;
    (void)userId;
    TestStats s;
    s.module = QStringLiteral("pinpolicy");

    // --------------------------------------------------------------------
    // 1) Length rule: lengths 1..5 and 9..12 rejected with the length message;
    //    valid-shape PINs in 6..8 are NOT rejected for length.
    // --------------------------------------------------------------------
    {
        // A non-sequential, non-blocklisted digit string of arbitrary length,
        // built so it never trips the blocklist or straight-run rule.
        // Pattern "194837..." has no constant adjacent step.
        const QString pat = QStringLiteral("194837256019");
        for (int len = 1; len <= 12; ++len) {
            const QString pin = pat.left(len);
            const auto r = PinPolicy::validate(pin);
            if (len >= PinPolicy::MinLen && len <= PinPolicy::MaxLen) {
                s.check(accepted(r), QStringLiteral("length %1 accepted").arg(len));
            } else {
                s.check(rejectedWith(r, kLenMsg),
                        QStringLiteral("length %1 rejected (len msg)").arg(len));
            }
        }
    }

    // Boundary lengths 6 and 8 accepted explicitly (good PINs).
    s.check(accepted(PinPolicy::validate(QStringLiteral("194837"))),
            QStringLiteral("boundary len 6 accepted"));
    s.check(accepted(PinPolicy::validate(QStringLiteral("19483725"))),
            QStringLiteral("boundary len 8 accepted"));
    // Length 5 and 9 rejected.
    s.check(rejectedWith(PinPolicy::validate(QStringLiteral("19483")), kLenMsg),
            QStringLiteral("len 5 rejected"));
    s.check(rejectedWith(PinPolicy::validate(QStringLiteral("194837256")), kLenMsg),
            QStringLiteral("len 9 rejected"));

    // --------------------------------------------------------------------
    // 2) Non-digit content rejected (length message — regex requires \d).
    // --------------------------------------------------------------------
    {
        const char *bad[] = {
            "12a456", "abcdef", "1234 6",       "12.456",  "12-456",   "+12345",
            "12345!", " 12345", "１２３４５６", "12\t456", "abcdefgh", "12345a",
        };
        for (const char *b : bad) {
            const QString pin = QString::fromUtf8(b);
            const auto r = PinPolicy::validate(pin);
            s.check(rejected(r), QStringLiteral("non-digit rejected: [%1]").arg(pin));
        }
        // Pure-ASCII-digit-with-internal-space variants specifically hit len msg.
        s.check(rejectedWith(PinPolicy::validate(QStringLiteral("12 456")), kLenMsg),
                QStringLiteral("internal space → len msg"));
    }

    // Note: validate() trims the input, so surrounding whitespace is stripped
    // before the digit check. A trimmed-to-valid PIN is accepted.
    s.check(accepted(PinPolicy::validate(QStringLiteral("  194837  "))),
            QStringLiteral("surrounding whitespace trimmed then accepted"));

    // --------------------------------------------------------------------
    // 3) Every blocklist entry of length 6..8 is rejected. (5-digit blocklist
    //    entries are caught by the length rule first — verified separately.)
    // --------------------------------------------------------------------
    {
        const char *blocked6to8[] = {
            "000000", "111111", "222222", "333333", "444444", "555555",   "666666",
            "777777", "888888", "999999", "123456", "654321", "12345678", "87654321",
        };
        for (const char *b : blocked6to8) {
            const QString pin = QString::fromLatin1(b);
            const auto r = PinPolicy::validate(pin);
            s.check(rejected(r), QStringLiteral("blocklist rejected: %1").arg(pin));
        }
        // The all-same-digit entries are blocklisted (not straight runs), so
        // they must surface the blocklist message specifically.
        const char *sameDigit[] = {
            "000000", "111111", "222222", "333333", "444444",
            "555555", "666666", "777777", "888888", "999999",
        };
        for (const char *b : sameDigit) {
            const QString pin = QString::fromLatin1(b);
            s.check(rejectedWith(PinPolicy::validate(pin), kBlockMsg),
                    QStringLiteral("blocklist msg for repeated: %1").arg(pin));
        }
    }
    // 5-digit blocklist entries hit the length rule (too short) first.
    s.check(rejectedWith(PinPolicy::validate(QStringLiteral("12345")), kLenMsg),
            QStringLiteral("blocklist 12345 → len msg (too short)"));
    s.check(rejectedWith(PinPolicy::validate(QStringLiteral("54321")), kLenMsg),
            QStringLiteral("blocklist 54321 → len msg (too short)"));

    // --------------------------------------------------------------------
    // 4) Straight ascending runs of lengths 6, 7, 8 rejected.
    //    Build every ascending run that fits in 6..8 digits.
    // --------------------------------------------------------------------
    {
        for (int len = 6; len <= 8; ++len) {
            for (int start = 0; start + len - 1 <= 9; ++start) {
                QString pin;
                for (int k = 0; k < len; ++k) {
                    pin.append(QChar('0' + start + k));
                }
                const auto r = PinPolicy::validate(pin);
                s.check(rejected(r), QStringLiteral("ascending run rejected: %1").arg(pin));
                // 123456 / 12345678 are also blocklisted; others surface the
                // sequence message. Either way it must be rejected; check the
                // sequence message for the non-blocklisted ones.
                if (pin != QStringLiteral("123456") && pin != QStringLiteral("12345678")) {
                    s.check(rejectedWith(r, kSeqMsg),
                            QStringLiteral("ascending run seq msg: %1").arg(pin));
                }
            }
        }
    }

    // --------------------------------------------------------------------
    // 5) Straight descending runs of lengths 6, 7, 8 rejected.
    // --------------------------------------------------------------------
    {
        for (int len = 6; len <= 8; ++len) {
            for (int start = 9; start - (len - 1) >= 0; --start) {
                QString pin;
                for (int k = 0; k < len; ++k) {
                    pin.append(QChar('0' + start - k));
                }
                const auto r = PinPolicy::validate(pin);
                s.check(rejected(r), QStringLiteral("descending run rejected: %1").arg(pin));
                if (pin != QStringLiteral("654321") && pin != QStringLiteral("87654321")) {
                    s.check(rejectedWith(r, kSeqMsg),
                            QStringLiteral("descending run seq msg: %1").arg(pin));
                }
            }
        }
    }

    // Near-runs that are NOT straight (one step off) must be accepted.
    {
        const char *goodish[] = {
            "123457", "123455", "987655", "124567", "135790", "246810",
            "112345", // starts with a repeat → not a straight run
        };
        for (const char *g : goodish) {
            const QString pin = QString::fromLatin1(g);
            // Exclude any that happen to be blocklisted (none here) — assert accepted.
            s.check(accepted(PinPolicy::validate(pin)),
                    QStringLiteral("near-run accepted: %1").arg(pin));
        }
    }

    // --------------------------------------------------------------------
    // 6) A broad batch of plain "good" PINs accepted (no history/current).
    // --------------------------------------------------------------------
    {
        const char *good[] = {
            "194837",  "583920",  "748261", "905172",  "263849",   "517028",   "830491",
            "672105",  "419376",  "285094", "7391850", "62048173", "19483725", "5048261",
            "9170264", "3825016", "640917", "271804",  "9038172",  "48052619",
        };
        for (const char *g : good) {
            const QString pin = QString::fromLatin1(g);
            s.check(accepted(PinPolicy::validate(pin)),
                    QStringLiteral("good PIN accepted: %1").arg(pin));
        }
    }

    // --------------------------------------------------------------------
    // 7) Reuse rejection: a bcrypt hash of the candidate present in history.
    //    Build history with Bcrypt::hash and verify the reuse message.
    // --------------------------------------------------------------------
    {
        const char *pins[] = {"194837", "583920", "7391850", "48052619", "640917"};
        for (const char *p : pins) {
            const QString pin = QString::fromLatin1(p);
            const QString h = Bcrypt::hash(pin);
            s.check(!h.isEmpty(), QStringLiteral("bcrypt hash built: %1").arg(pin));
            // History containing exactly this PIN's hash → reuse rejection.
            const QStringList history = {h};
            const auto r = PinPolicy::validate(pin, history);
            s.check(rejectedWith(r, kReuseMsg),
                    QStringLiteral("reuse rejected (1-entry history): %1").arg(pin));
        }
    }
    {
        // Candidate buried among several other (non-matching) history hashes.
        const QString pin = QStringLiteral("285094");
        QStringList history;
        history << Bcrypt::hash(QStringLiteral("111213"));
        history << Bcrypt::hash(QStringLiteral("929495"));
        history << Bcrypt::hash(pin); // the reused one
        history << Bcrypt::hash(QStringLiteral("404142"));
        const auto r = PinPolicy::validate(pin, history);
        s.check(rejectedWith(r, kReuseMsg), QStringLiteral("reuse rejected (buried in history)"));
    }
    {
        // Non-matching history → accepted (PIN not previously used).
        const QString pin = QStringLiteral("830491");
        QStringList history;
        history << Bcrypt::hash(QStringLiteral("194837"));
        history << Bcrypt::hash(QStringLiteral("583920"));
        history << Bcrypt::hash(QStringLiteral("748261"));
        const auto r = PinPolicy::validate(pin, history);
        s.check(accepted(r), QStringLiteral("non-matching history → accepted"));
    }
    {
        // Empty history → accepted (new-user wizard case).
        s.check(accepted(PinPolicy::validate(QStringLiteral("672105"), QStringList())),
                QStringLiteral("empty history accepted"));
    }

    // --------------------------------------------------------------------
    // 8) Differs-from-current via currentHash.
    // --------------------------------------------------------------------
    {
        const char *pins[] = {"194837", "419376", "9170264", "271804"};
        for (const char *p : pins) {
            const QString pin = QString::fromLatin1(p);
            const QString cur = Bcrypt::hash(pin);
            // Same PIN as current → rejected with the "different from current" msg.
            const auto same = PinPolicy::validate(pin, QStringList(), cur);
            s.check(rejectedWith(same, kCurrentMsg),
                    QStringLiteral("same-as-current rejected: %1").arg(pin));
        }
    }
    {
        // Different PIN from current → accepted.
        const QString cur = Bcrypt::hash(QStringLiteral("194837"));
        const auto r = PinPolicy::validate(QStringLiteral("583920"), QStringList(), cur);
        s.check(accepted(r), QStringLiteral("differs-from-current accepted"));
    }
    {
        // Empty currentHash → not checked, accepted.
        s.check(accepted(PinPolicy::validate(QStringLiteral("905172"), QStringList(), QString())),
                QStringLiteral("empty currentHash accepted"));
    }

    // --------------------------------------------------------------------
    // 9) Rule ordering: length beats blocklist beats sequence beats reuse
    //    beats current. Construct inputs that would trip multiple rules.
    // --------------------------------------------------------------------
    {
        // Too short AND blocklisted (12345): length wins.
        s.check(rejectedWith(PinPolicy::validate(QStringLiteral("12345")), kLenMsg),
                QStringLiteral("ordering: short+blocklist → len"));
        // Blocklisted AND a straight run (123456): blocklist wins.
        s.check(rejectedWith(PinPolicy::validate(QStringLiteral("123456")), kBlockMsg),
                QStringLiteral("ordering: blocklist+seq → blocklist"));
        // Straight run AND in history: sequence wins (checked before history).
        const QString seqPin = QStringLiteral("234567");
        const QStringList hist = {Bcrypt::hash(seqPin)};
        s.check(rejectedWith(PinPolicy::validate(seqPin, hist), kSeqMsg),
                QStringLiteral("ordering: seq+history → seq"));
        // In history AND equals current: history (reuse) wins (checked first).
        const QString good = QStringLiteral("748261");
        const QString h = Bcrypt::hash(good);
        s.check(rejectedWith(PinPolicy::validate(good, {h}, h), kReuseMsg),
                QStringLiteral("ordering: history+current → reuse"));
    }

    // --------------------------------------------------------------------
    // 10) Combined valid scenario: good PIN, non-matching history, different
    //     current → accepted.
    // --------------------------------------------------------------------
    {
        const QString pin = QStringLiteral("640917");
        QStringList history;
        history << Bcrypt::hash(QStringLiteral("194837"));
        history << Bcrypt::hash(QStringLiteral("583920"));
        const QString cur = Bcrypt::hash(QStringLiteral("271804"));
        s.check(accepted(PinPolicy::validate(pin, history, cur)),
                QStringLiteral("combined valid scenario accepted"));
    }

    // --------------------------------------------------------------------
    // 11) Constant constants sanity (guards against accidental policy drift).
    // --------------------------------------------------------------------
    s.check(PinPolicy::MinLen == 6, QStringLiteral("MinLen == 6"));
    s.check(PinPolicy::MaxLen == 8, QStringLiteral("MaxLen == 8"));
    s.check(PinPolicy::HistoryKeep == 5, QStringLiteral("HistoryKeep == 5"));

    return s;
}

} // namespace pharmadesk_tests
