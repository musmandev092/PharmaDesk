#include "domain/Money.h"

#include <QStringList>
#include <cstdlib>

namespace {
qint64 pow10(int n)
{
    qint64 p = 1;
    for (int i = 0; i < n; ++i) p *= 10;
    return p;
}
} // namespace

Money Money::fromString(const QString &in)
{
    QString s = in.trimmed();
    if (s.isEmpty()) {
        return Money();
    }
    bool neg = false;
    if (s.startsWith(QLatin1Char('-'))) {
        neg = true;
        s.remove(0, 1);
    } else if (s.startsWith(QLatin1Char('+'))) {
        s.remove(0, 1);
    }

    const int dot = s.indexOf(QLatin1Char('.'));
    QString intStr = dot < 0 ? s : s.left(dot);
    QString fracStr = dot < 0 ? QString() : s.mid(dot + 1);

    if (intStr.isEmpty()) {
        intStr = QStringLiteral("0");
    }

    bool okInt = true;
    qint64 intVal = intStr.toLongLong(&okInt);
    if (!okInt) {
        return Money(); // not a number → 0 (mirrors Money::s leniency)
    }

    // Take 4 fractional digits; round HALF_UP using the 5th digit.
    QString frac4 = fracStr.left(4);
    while (frac4.size() < 4) frac4 += QLatin1Char('0');
    bool okFrac = true;
    qint64 fracVal = frac4.isEmpty() ? 0 : frac4.toLongLong(&okFrac);
    if (!okFrac) {
        return Money();
    }
    if (fracStr.size() > 4) {
        const QChar next = fracStr.at(4);
        if (next.isDigit() && next.digitValue() >= 5) {
            ++fracVal;
        }
    }

    qint64 units;
    if (__builtin_mul_overflow(intVal, static_cast<qint64>(10000), &units)
        || __builtin_add_overflow(units, fracVal, &units)) {
        throw std::overflow_error("Money: value out of representable range");
    }
    return fromUnits(neg ? -units : units);
}

Money Money::divByInt(qint64 divisor) const
{
    if (divisor <= 0) {
        return Money();
    }
    qint64 q = m_units / divisor;
    const qint64 r = m_units % divisor;
    if (std::llabs(r) * 2 >= divisor) {
        q += (m_units < 0 ? -1 : 1); // HALF_UP, away from zero
    }
    return fromUnits(q);
}

QString Money::toString(int scale) const
{
    if (scale < 0) scale = 0;
    if (scale > 4) scale = 4;

    const qint64 factor = pow10(4 - scale);
    qint64 q = m_units / factor;
    const qint64 r = m_units % factor;
    if (std::llabs(r) * 2 >= factor) {
        q += (m_units < 0 ? -1 : 1); // HALF_UP, away from zero
    }

    const bool neg = q < 0;
    const qint64 a = std::llabs(q);
    const qint64 unit = pow10(scale);
    const qint64 intPart = a / unit;
    const qint64 fracPart = a % unit;

    QString out = QString::number(intPart);
    if (scale > 0) {
        out += QLatin1Char('.') + QString::number(fracPart).rightJustified(scale, QLatin1Char('0'));
    }
    return (neg && (intPart != 0 || fracPart != 0)) ? QLatin1Char('-') + out : out;
}

QString Money::fmt(int scale) const
{
    QString plain = toString(scale);
    bool neg = plain.startsWith(QLatin1Char('-'));
    if (neg) plain.remove(0, 1);

    const int dot = plain.indexOf(QLatin1Char('.'));
    QString intPart = dot < 0 ? plain : plain.left(dot);
    const QString fracPart = dot < 0 ? QString() : plain.mid(dot);

    // Group integer digits in threes.
    QString grouped;
    int count = 0;
    for (int i = intPart.size() - 1; i >= 0; --i) {
        grouped.prepend(intPart.at(i));
        if (++count % 3 == 0 && i > 0) {
            grouped.prepend(QLatin1Char(','));
        }
    }
    return (neg ? QStringLiteral("-") : QString()) + grouped + fracPart;
}

QString Money::display(int scale) const
{
    return QStringLiteral("PKR %1").arg(fmt(scale));
}
