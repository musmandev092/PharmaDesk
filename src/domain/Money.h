#pragma once

#include <QString>

#include <stdexcept>

// Fixed-point decimal money. Port of pos/backend/Money.php semantics: two
// scales — money = 2 dp, cost = 4 dp — with HALF_UP rounding at every storage
// boundary. Never use floating point for stored monetary values.
//
// Internally a value is held as an int64 at scale 4 (the cost/working scale),
// which covers DECIMAL(12,4); the sale path only ever adds, subtracts, or
// multiplies by an integer quantity, all exact at scale 4, then rounds to 2 at
// the storage boundary — matching the PHP results bit-for-bit.
class Money
{
public:
    static constexpr int ScaleMoney = 2;
    static constexpr int ScaleCost = 4;

    Money() = default;

    // Parse a decimal string ("10", "10.5", "10.5000", "-2.345"). Extra
    // fractional digits beyond scale 4 are rounded HALF_UP. Invalid → zero.
    static Money fromString(const QString &s);
    static Money fromUnits(qint64 unitsScale4)
    {
        Money m;
        m.m_units = unitsScale4;
        return m;
    }

    // Arithmetic is overflow-checked: PHP bcmath is arbitrary-precision, so the
    // C++ int64 port must DETECT a wrap rather than silently produce a wrong
    // total (the worst failure class for a money system). Pharmacy values never
    // come near int64/10000, so these throw only on a genuine bug/bad input —
    // services catch std::exception and roll the transaction back.
    Money operator+(const Money &o) const
    {
        qint64 r;
        if (__builtin_add_overflow(m_units, o.m_units, &r)) {
            throw std::overflow_error("Money: addition overflow");
        }
        return fromUnits(r);
    }
    Money operator-(const Money &o) const
    {
        qint64 r;
        if (__builtin_sub_overflow(m_units, o.m_units, &r)) {
            throw std::overflow_error("Money: subtraction overflow");
        }
        return fromUnits(r);
    }
    Money mul(qint64 qty) const
    {
        qint64 r;
        if (__builtin_mul_overflow(m_units, qty, &r)) {
            throw std::overflow_error("Money: multiplication overflow");
        }
        return fromUnits(r);
    }

    // Divide the value by a positive integer, HALF_UP at scale 4 (the internal
    // scale). Equivalent to the PHP CostBlender's div-at-scale-8-then-round-to-4,
    // because 8-dp truncation can't change a 4-dp HALF_UP decision. divisor ≤ 0
    // returns zero.
    Money divByInt(qint64 divisor) const;

    int compare(const Money &o) const
    {
        return m_units < o.m_units ? -1 : (m_units > o.m_units ? 1 : 0);
    }
    bool isNegative() const { return m_units < 0; }
    bool isZero() const { return m_units == 0; }

    qint64 unitsScale4() const { return m_units; }

    // Decimal string rounded HALF_UP to `scale` (no thousands separators).
    QString toString(int scale = ScaleMoney) const;
    // Same, with comma thousands separators (display only).
    QString fmt(int scale = ScaleMoney) const;

    // Display string with the "PKR" currency symbol, e.g. "PKR 1,234.50".
    // Single source of truth for currency presentation; equals "PKR " + fmt().
    QString display(int scale = ScaleMoney) const;

private:
    qint64 m_units = 0; // value * 10000
};
