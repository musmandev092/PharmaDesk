#include "domain/PinPolicy.h"

#include "domain/Bcrypt.h"

#include <QRegularExpression>

namespace PinPolicy {

// Verbatim from PinPolicy.php::BLOCKLIST.
static const QStringList &blocklist()
{
    static const QStringList list = {
        QStringLiteral("000000"),   QStringLiteral("111111"), QStringLiteral("222222"),
        QStringLiteral("333333"),   QStringLiteral("444444"), QStringLiteral("555555"),
        QStringLiteral("666666"),   QStringLiteral("777777"), QStringLiteral("888888"),
        QStringLiteral("999999"),   QStringLiteral("123456"), QStringLiteral("654321"),
        QStringLiteral("12345"),    QStringLiteral("54321"),  QStringLiteral("12345678"),
        QStringLiteral("87654321"),
    };
    return list;
}

// Ported from PinPolicy.php::isStraightRun — true if every adjacent pair steps
// by exactly +1 (ascending) or exactly -1 (descending).
static bool isStraightRun(const QString &pin)
{
    bool up = true;
    bool down = true;
    for (int i = 1; i < pin.size(); ++i) {
        const int d = pin.at(i).digitValue() - pin.at(i - 1).digitValue();
        if (d != 1) up = false;
        if (d != -1) down = false;
    }
    return up || down;
}

std::optional<QString> validate(const QString &newPin, const QStringList &historyHashes,
                                const QString &currentHash)
{
    const QString pin = newPin.trimmed();

    static const QRegularExpression digits(QStringLiteral("^\\d{%1,%2}$").arg(MinLen).arg(MaxLen));
    if (!digits.match(pin).hasMatch()) {
        return QStringLiteral("PIN must be %1-%2 digits.").arg(MinLen).arg(MaxLen);
    }

    if (blocklist().contains(pin)) {
        return QStringLiteral("PIN is too predictable. Pick something else.");
    }

    if (isStraightRun(pin)) {
        return QStringLiteral("PIN must not be a sequence like 1234 or 9876.");
    }

    for (const QString &h : historyHashes) {
        if (Bcrypt::verify(pin, h)) {
            return QStringLiteral("PIN was used recently. Pick one you haven't used "
                                  "in your last %1 rotations.")
                .arg(HistoryKeep);
        }
    }

    if (!currentHash.isEmpty() && Bcrypt::verify(pin, currentHash)) {
        return QStringLiteral("New PIN must be different from the current one.");
    }

    return std::nullopt;
}

} // namespace PinPolicy
