#include "framework/TestStats.h"
#include "domain/Money.h"

namespace pharmadesk_tests {

TestStats run_money_tests(QSqlDatabase db, qint64 userId)
{
    (void)db;
    (void)userId;
    TestStats s;
    s.module = QStringLiteral("money");

    s.check(Money::fromString(QStringLiteral("0")).toString(4) == QStringLiteral("0.0000"),
            QStringLiteral("parse roundtrip s4 0 -> 0.0000"));
    s.check(Money::fromString(QStringLiteral("1")).toString(4) == QStringLiteral("1.0000"),
            QStringLiteral("parse roundtrip s4 1 -> 1.0000"));
    s.check(Money::fromString(QStringLiteral("10")).toString(4) == QStringLiteral("10.0000"),
            QStringLiteral("parse roundtrip s4 10 -> 10.0000"));
    s.check(Money::fromString(QStringLiteral("100")).toString(4) == QStringLiteral("100.0000"),
            QStringLiteral("parse roundtrip s4 100 -> 100.0000"));
    s.check(Money::fromString(QStringLiteral("0.5")).toString(4) == QStringLiteral("0.5000"),
            QStringLiteral("parse roundtrip s4 0.5 -> 0.5000"));
    s.check(Money::fromString(QStringLiteral("0.25")).toString(4) == QStringLiteral("0.2500"),
            QStringLiteral("parse roundtrip s4 0.25 -> 0.2500"));
    s.check(Money::fromString(QStringLiteral("0.1")).toString(4) == QStringLiteral("0.1000"),
            QStringLiteral("parse roundtrip s4 0.1 -> 0.1000"));
    s.check(Money::fromString(QStringLiteral("0.0001")).toString(4) == QStringLiteral("0.0001"),
            QStringLiteral("parse roundtrip s4 0.0001 -> 0.0001"));
    s.check(Money::fromString(QStringLiteral("0.1234")).toString(4) == QStringLiteral("0.1234"),
            QStringLiteral("parse roundtrip s4 0.1234 -> 0.1234"));
    s.check(Money::fromString(QStringLiteral("12.3456")).toString(4) == QStringLiteral("12.3456"),
            QStringLiteral("parse roundtrip s4 12.3456 -> 12.3456"));
    s.check(Money::fromString(QStringLiteral("10.5")).toString(4) == QStringLiteral("10.5000"),
            QStringLiteral("parse roundtrip s4 10.5 -> 10.5000"));
    s.check(Money::fromString(QStringLiteral("10.50")).toString(4) == QStringLiteral("10.5000"),
            QStringLiteral("parse roundtrip s4 10.50 -> 10.5000"));
    s.check(Money::fromString(QStringLiteral("10.5000")).toString(4) == QStringLiteral("10.5000"),
            QStringLiteral("parse roundtrip s4 10.5000 -> 10.5000"));
    s.check(Money::fromString(QStringLiteral("123.4567")).toString(4) == QStringLiteral("123.4567"),
            QStringLiteral("parse roundtrip s4 123.4567 -> 123.4567"));
    s.check(Money::fromString(QStringLiteral("-0.5")).toString(4) == QStringLiteral("-0.5000"),
            QStringLiteral("parse roundtrip s4 -0.5 -> -0.5000"));
    s.check(Money::fromString(QStringLiteral("-1")).toString(4) == QStringLiteral("-1.0000"),
            QStringLiteral("parse roundtrip s4 -1 -> -1.0000"));
    s.check(Money::fromString(QStringLiteral("-10.25")).toString(4) == QStringLiteral("-10.2500"),
            QStringLiteral("parse roundtrip s4 -10.25 -> -10.2500"));
    s.check(Money::fromString(QStringLiteral("-123.4567")).toString(4)
                == QStringLiteral("-123.4567"),
            QStringLiteral("parse roundtrip s4 -123.4567 -> -123.4567"));
    s.check(Money::fromString(QStringLiteral("1000.0001")).toString(4)
                == QStringLiteral("1000.0001"),
            QStringLiteral("parse roundtrip s4 1000.0001 -> 1000.0001"));
    s.check(Money::fromString(QStringLiteral("999.9999")).toString(4) == QStringLiteral("999.9999"),
            QStringLiteral("parse roundtrip s4 999.9999 -> 999.9999"));
    s.check(Money::fromString(QStringLiteral("0.0005")).toString(4) == QStringLiteral("0.0005"),
            QStringLiteral("parse roundtrip s4 0.0005 -> 0.0005"));
    s.check(Money::fromString(QStringLiteral("-0.0005")).toString(4) == QStringLiteral("-0.0005"),
            QStringLiteral("parse roundtrip s4 -0.0005 -> -0.0005"));
    s.check(Money::fromString(QStringLiteral("42")).toString(4) == QStringLiteral("42.0000"),
            QStringLiteral("parse roundtrip s4 42 -> 42.0000"));
    s.check(Money::fromString(QStringLiteral("42.0")).toString(4) == QStringLiteral("42.0000"),
            QStringLiteral("parse roundtrip s4 42.0 -> 42.0000"));
    s.check(Money::fromString(QStringLiteral("42.00")).toString(4) == QStringLiteral("42.0000"),
            QStringLiteral("parse roundtrip s4 42.00 -> 42.0000"));
    s.check(Money::fromString(QStringLiteral("42.000")).toString(4) == QStringLiteral("42.0000"),
            QStringLiteral("parse roundtrip s4 42.000 -> 42.0000"));
    s.check(Money::fromString(QStringLiteral("42.0000")).toString(4) == QStringLiteral("42.0000"),
            QStringLiteral("parse roundtrip s4 42.0000 -> 42.0000"));
    s.check(Money::fromString(QStringLiteral("7.654")).toString(4) == QStringLiteral("7.6540"),
            QStringLiteral("parse roundtrip s4 7.654 -> 7.6540"));
    s.check(Money::fromString(QStringLiteral("-7.654")).toString(4) == QStringLiteral("-7.6540"),
            QStringLiteral("parse roundtrip s4 -7.654 -> -7.6540"));
    s.check(Money::fromString(QStringLiteral("3.14159")).toString(4) == QStringLiteral("3.1416"),
            QStringLiteral("parse roundtrip s4 3.14159 -> 3.1416"));
    s.check(Money::fromString(QStringLiteral("-3.14159")).toString(4) == QStringLiteral("-3.1416"),
            QStringLiteral("parse roundtrip s4 -3.14159 -> -3.1416"));
    s.check(Money::fromString(QStringLiteral("100000.0000")).toString(4)
                == QStringLiteral("100000.0000"),
            QStringLiteral("parse roundtrip s4 100000.0000 -> 100000.0000"));
    s.check(Money::fromString(QStringLiteral("-100000.0000")).toString(4)
                == QStringLiteral("-100000.0000"),
            QStringLiteral("parse roundtrip s4 -100000.0000 -> -100000.0000"));
    s.check(Money::fromString(QStringLiteral("0.9999")).toString(4) == QStringLiteral("0.9999"),
            QStringLiteral("parse roundtrip s4 0.9999 -> 0.9999"));
    s.check(Money::fromString(QStringLiteral("-0.9999")).toString(4) == QStringLiteral("-0.9999"),
            QStringLiteral("parse roundtrip s4 -0.9999 -> -0.9999"));
    s.check(Money::fromString(QStringLiteral("250.7500")).toString(4) == QStringLiteral("250.7500"),
            QStringLiteral("parse roundtrip s4 250.7500 -> 250.7500"));
    s.check(Money::fromString(QStringLiteral("-250.7500")).toString(4)
                == QStringLiteral("-250.7500"),
            QStringLiteral("parse roundtrip s4 -250.7500 -> -250.7500"));
    s.check(Money::fromString(QStringLiteral("1.1")).toString(4) == QStringLiteral("1.1000"),
            QStringLiteral("parse roundtrip s4 1.1 -> 1.1000"));
    s.check(Money::fromString(QStringLiteral("2.2")).toString(4) == QStringLiteral("2.2000"),
            QStringLiteral("parse roundtrip s4 2.2 -> 2.2000"));
    s.check(Money::fromString(QStringLiteral("3.3")).toString(4) == QStringLiteral("3.3000"),
            QStringLiteral("parse roundtrip s4 3.3 -> 3.3000"));
    s.check(Money::fromString(QStringLiteral("-4.4")).toString(4) == QStringLiteral("-4.4000"),
            QStringLiteral("parse roundtrip s4 -4.4 -> -4.4000"));
    s.check(Money::fromString(QStringLiteral("5.5")).toString(4) == QStringLiteral("5.5000"),
            QStringLiteral("parse roundtrip s4 5.5 -> 5.5000"));
    s.check(Money::fromString(QStringLiteral("0")).toString(2) == QStringLiteral("0.00"),
            QStringLiteral("parse roundtrip s2 0 -> 0.00"));
    s.check(Money::fromString(QStringLiteral("1")).toString(2) == QStringLiteral("1.00"),
            QStringLiteral("parse roundtrip s2 1 -> 1.00"));
    s.check(Money::fromString(QStringLiteral("10.5")).toString(2) == QStringLiteral("10.50"),
            QStringLiteral("parse roundtrip s2 10.5 -> 10.50"));
    s.check(Money::fromString(QStringLiteral("10.50")).toString(2) == QStringLiteral("10.50"),
            QStringLiteral("parse roundtrip s2 10.50 -> 10.50"));
    s.check(Money::fromString(QStringLiteral("100.00")).toString(2) == QStringLiteral("100.00"),
            QStringLiteral("parse roundtrip s2 100.00 -> 100.00"));
    s.check(Money::fromString(QStringLiteral("0.5")).toString(2) == QStringLiteral("0.50"),
            QStringLiteral("parse roundtrip s2 0.5 -> 0.50"));
    s.check(Money::fromString(QStringLiteral("0.25")).toString(2) == QStringLiteral("0.25"),
            QStringLiteral("parse roundtrip s2 0.25 -> 0.25"));
    s.check(Money::fromString(QStringLiteral("-0.25")).toString(2) == QStringLiteral("-0.25"),
            QStringLiteral("parse roundtrip s2 -0.25 -> -0.25"));
    s.check(Money::fromString(QStringLiteral("12.34")).toString(2) == QStringLiteral("12.34"),
            QStringLiteral("parse roundtrip s2 12.34 -> 12.34"));
    s.check(Money::fromString(QStringLiteral("12.345")).toString(2) == QStringLiteral("12.35"),
            QStringLiteral("parse roundtrip s2 12.345 -> 12.35"));
    s.check(Money::fromString(QStringLiteral("12.344")).toString(2) == QStringLiteral("12.34"),
            QStringLiteral("parse roundtrip s2 12.344 -> 12.34"));
    s.check(Money::fromString(QStringLiteral("999.995")).toString(2) == QStringLiteral("1000.00"),
            QStringLiteral("parse roundtrip s2 999.995 -> 1000.00"));
    s.check(Money::fromString(QStringLiteral("999.994")).toString(2) == QStringLiteral("999.99"),
            QStringLiteral("parse roundtrip s2 999.994 -> 999.99"));
    s.check(Money::fromString(QStringLiteral("-999.995")).toString(2) == QStringLiteral("-1000.00"),
            QStringLiteral("parse roundtrip s2 -999.995 -> -1000.00"));
    s.check(Money::fromString(QStringLiteral("0.005")).toString(2) == QStringLiteral("0.01"),
            QStringLiteral("parse roundtrip s2 0.005 -> 0.01"));
    s.check(Money::fromString(QStringLiteral("0.004")).toString(2) == QStringLiteral("0.00"),
            QStringLiteral("parse roundtrip s2 0.004 -> 0.00"));
    s.check(Money::fromString(QStringLiteral("-0.005")).toString(2) == QStringLiteral("-0.01"),
            QStringLiteral("parse roundtrip s2 -0.005 -> -0.01"));
    s.check(Money::fromString(QStringLiteral("-0.004")).toString(2) == QStringLiteral("0.00"),
            QStringLiteral("parse roundtrip s2 -0.004 -> 0.00"));
    s.check(Money::fromString(QStringLiteral("1234.56")).toString(2) == QStringLiteral("1234.56"),
            QStringLiteral("parse roundtrip s2 1234.56 -> 1234.56"));
    s.check(Money::fromString(QStringLiteral("-1234.56")).toString(2) == QStringLiteral("-1234.56"),
            QStringLiteral("parse roundtrip s2 -1234.56 -> -1234.56"));
    s.check(Money::fromString(QStringLiteral("0.01")).toString(2) == QStringLiteral("0.01"),
            QStringLiteral("parse roundtrip s2 0.01 -> 0.01"));
    s.check(Money::fromString(QStringLiteral("0.99")).toString(2) == QStringLiteral("0.99"),
            QStringLiteral("parse roundtrip s2 0.99 -> 0.99"));
    s.check(Money::fromString(QStringLiteral("-0.99")).toString(2) == QStringLiteral("-0.99"),
            QStringLiteral("parse roundtrip s2 -0.99 -> -0.99"));
    s.check(Money::fromString(QStringLiteral("50")).toString(2) == QStringLiteral("50.00"),
            QStringLiteral("parse roundtrip s2 50 -> 50.00"));
    s.check(Money::fromString(QStringLiteral("50.00")).toString(2) == QStringLiteral("50.00"),
            QStringLiteral("parse roundtrip s2 50.00 -> 50.00"));
    s.check(Money::fromString(QStringLiteral("7.5")).toString(2) == QStringLiteral("7.50"),
            QStringLiteral("parse roundtrip s2 7.5 -> 7.50"));
    s.check(Money::fromString(QStringLiteral("7.55")).toString(2) == QStringLiteral("7.55"),
            QStringLiteral("parse roundtrip s2 7.55 -> 7.55"));
    s.check(Money::fromString(QStringLiteral("7.555")).toString(2) == QStringLiteral("7.56"),
            QStringLiteral("parse roundtrip s2 7.555 -> 7.56"));
    s.check(Money::fromString(QStringLiteral("7.554")).toString(2) == QStringLiteral("7.55"),
            QStringLiteral("parse roundtrip s2 7.554 -> 7.55"));
    s.check(Money::fromString(QStringLiteral("1.00004")).toString(4) == QStringLiteral("1.0000"),
            QStringLiteral("parse >4dp s4 1.00004 -> 1.0000"));
    s.check(Money::fromString(QStringLiteral("1.00005")).toString(4) == QStringLiteral("1.0001"),
            QStringLiteral("parse >4dp s4 1.00005 -> 1.0001"));
    s.check(Money::fromString(QStringLiteral("1.00006")).toString(4) == QStringLiteral("1.0001"),
            QStringLiteral("parse >4dp s4 1.00006 -> 1.0001"));
    s.check(Money::fromString(QStringLiteral("1.000049")).toString(4) == QStringLiteral("1.0000"),
            QStringLiteral("parse >4dp s4 1.000049 -> 1.0000"));
    s.check(Money::fromString(QStringLiteral("1.000051")).toString(4) == QStringLiteral("1.0001"),
            QStringLiteral("parse >4dp s4 1.000051 -> 1.0001"));
    s.check(Money::fromString(QStringLiteral("1.000099999")).toString(4)
                == QStringLiteral("1.0001"),
            QStringLiteral("parse >4dp s4 1.000099999 -> 1.0001"));
    s.check(Money::fromString(QStringLiteral("1.99995")).toString(4) == QStringLiteral("2.0000"),
            QStringLiteral("parse >4dp s4 1.99995 -> 2.0000"));
    s.check(Money::fromString(QStringLiteral("1.99994")).toString(4) == QStringLiteral("1.9999"),
            QStringLiteral("parse >4dp s4 1.99994 -> 1.9999"));
    s.check(Money::fromString(QStringLiteral("-1.00005")).toString(4) == QStringLiteral("-1.0001"),
            QStringLiteral("parse >4dp s4 -1.00005 -> -1.0001"));
    s.check(Money::fromString(QStringLiteral("-1.00004")).toString(4) == QStringLiteral("-1.0000"),
            QStringLiteral("parse >4dp s4 -1.00004 -> -1.0000"));
    s.check(Money::fromString(QStringLiteral("-1.99995")).toString(4) == QStringLiteral("-2.0000"),
            QStringLiteral("parse >4dp s4 -1.99995 -> -2.0000"));
    s.check(Money::fromString(QStringLiteral("0.12345")).toString(4) == QStringLiteral("0.1235"),
            QStringLiteral("parse >4dp s4 0.12345 -> 0.1235"));
    s.check(Money::fromString(QStringLiteral("0.12344")).toString(4) == QStringLiteral("0.1234"),
            QStringLiteral("parse >4dp s4 0.12344 -> 0.1234"));
    s.check(Money::fromString(QStringLiteral("0.123456789")).toString(4)
                == QStringLiteral("0.1235"),
            QStringLiteral("parse >4dp s4 0.123456789 -> 0.1235"));
    s.check(Money::fromString(QStringLiteral("9.99995")).toString(4) == QStringLiteral("10.0000"),
            QStringLiteral("parse >4dp s4 9.99995 -> 10.0000"));
    s.check(Money::fromString(QStringLiteral("9.99994")).toString(4) == QStringLiteral("9.9999"),
            QStringLiteral("parse >4dp s4 9.99994 -> 9.9999"));
    s.check(Money::fromString(QStringLiteral("-9.99995")).toString(4) == QStringLiteral("-10.0000"),
            QStringLiteral("parse >4dp s4 -9.99995 -> -10.0000"));
    s.check(Money::fromString(QStringLiteral("123.456789")).toString(4)
                == QStringLiteral("123.4568"),
            QStringLiteral("parse >4dp s4 123.456789 -> 123.4568"));
    s.check(Money::fromString(QStringLiteral("-123.456789")).toString(4)
                == QStringLiteral("-123.4568"),
            QStringLiteral("parse >4dp s4 -123.456789 -> -123.4568"));
    s.check(Money::fromString(QStringLiteral("0.00005")).toString(4) == QStringLiteral("0.0001"),
            QStringLiteral("parse >4dp s4 0.00005 -> 0.0001"));
    s.check(Money::fromString(QStringLiteral("0.00004")).toString(4) == QStringLiteral("0.0000"),
            QStringLiteral("parse >4dp s4 0.00004 -> 0.0000"));
    s.check(Money::fromString(QStringLiteral("-0.00005")).toString(4) == QStringLiteral("-0.0001"),
            QStringLiteral("parse >4dp s4 -0.00005 -> -0.0001"));
    s.check(Money::fromString(QStringLiteral("-0.00004")).toString(4) == QStringLiteral("0.0000"),
            QStringLiteral("parse >4dp s4 -0.00004 -> 0.0000"));
    s.check(Money::fromString(QStringLiteral("10.55555")).toString(4) == QStringLiteral("10.5556"),
            QStringLiteral("parse >4dp s4 10.55555 -> 10.5556"));
    s.check(Money::fromString(QStringLiteral("10.55554")).toString(4) == QStringLiteral("10.5555"),
            QStringLiteral("parse >4dp s4 10.55554 -> 10.5555"));
    s.check(Money::fromString(QStringLiteral("-10.55555")).toString(4)
                == QStringLiteral("-10.5556"),
            QStringLiteral("parse >4dp s4 -10.55555 -> -10.5556"));
    s.check(Money::fromString(QStringLiteral("0.004")).toString(2) == QStringLiteral("0.00"),
            QStringLiteral("HALF_UP s2 0.004 -> 0.00"));
    s.check(Money::fromString(QStringLiteral("-0.004")).toString(2) == QStringLiteral("0.00"),
            QStringLiteral("HALF_UP s2 -0.004 -> 0.00"));
    s.check(Money::fromString(QStringLiteral("0.005")).toString(2) == QStringLiteral("0.01"),
            QStringLiteral("HALF_UP s2 0.005 -> 0.01"));
    s.check(Money::fromString(QStringLiteral("-0.005")).toString(2) == QStringLiteral("-0.01"),
            QStringLiteral("HALF_UP s2 -0.005 -> -0.01"));
    s.check(Money::fromString(QStringLiteral("0.006")).toString(2) == QStringLiteral("0.01"),
            QStringLiteral("HALF_UP s2 0.006 -> 0.01"));
    s.check(Money::fromString(QStringLiteral("-0.006")).toString(2) == QStringLiteral("-0.01"),
            QStringLiteral("HALF_UP s2 -0.006 -> -0.01"));
    s.check(Money::fromString(QStringLiteral("0.124")).toString(2) == QStringLiteral("0.12"),
            QStringLiteral("HALF_UP s2 0.124 -> 0.12"));
    s.check(Money::fromString(QStringLiteral("-0.124")).toString(2) == QStringLiteral("-0.12"),
            QStringLiteral("HALF_UP s2 -0.124 -> -0.12"));
    s.check(Money::fromString(QStringLiteral("0.125")).toString(2) == QStringLiteral("0.13"),
            QStringLiteral("HALF_UP s2 0.125 -> 0.13"));
    s.check(Money::fromString(QStringLiteral("-0.125")).toString(2) == QStringLiteral("-0.13"),
            QStringLiteral("HALF_UP s2 -0.125 -> -0.13"));
    s.check(Money::fromString(QStringLiteral("0.126")).toString(2) == QStringLiteral("0.13"),
            QStringLiteral("HALF_UP s2 0.126 -> 0.13"));
    s.check(Money::fromString(QStringLiteral("-0.126")).toString(2) == QStringLiteral("-0.13"),
            QStringLiteral("HALF_UP s2 -0.126 -> -0.13"));
    s.check(Money::fromString(QStringLiteral("0.494")).toString(2) == QStringLiteral("0.49"),
            QStringLiteral("HALF_UP s2 0.494 -> 0.49"));
    s.check(Money::fromString(QStringLiteral("-0.494")).toString(2) == QStringLiteral("-0.49"),
            QStringLiteral("HALF_UP s2 -0.494 -> -0.49"));
    s.check(Money::fromString(QStringLiteral("0.495")).toString(2) == QStringLiteral("0.50"),
            QStringLiteral("HALF_UP s2 0.495 -> 0.50"));
    s.check(Money::fromString(QStringLiteral("-0.495")).toString(2) == QStringLiteral("-0.50"),
            QStringLiteral("HALF_UP s2 -0.495 -> -0.50"));
    s.check(Money::fromString(QStringLiteral("0.496")).toString(2) == QStringLiteral("0.50"),
            QStringLiteral("HALF_UP s2 0.496 -> 0.50"));
    s.check(Money::fromString(QStringLiteral("-0.496")).toString(2) == QStringLiteral("-0.50"),
            QStringLiteral("HALF_UP s2 -0.496 -> -0.50"));
    s.check(Money::fromString(QStringLiteral("0.504")).toString(2) == QStringLiteral("0.50"),
            QStringLiteral("HALF_UP s2 0.504 -> 0.50"));
    s.check(Money::fromString(QStringLiteral("-0.504")).toString(2) == QStringLiteral("-0.50"),
            QStringLiteral("HALF_UP s2 -0.504 -> -0.50"));
    s.check(Money::fromString(QStringLiteral("0.505")).toString(2) == QStringLiteral("0.51"),
            QStringLiteral("HALF_UP s2 0.505 -> 0.51"));
    s.check(Money::fromString(QStringLiteral("-0.505")).toString(2) == QStringLiteral("-0.51"),
            QStringLiteral("HALF_UP s2 -0.505 -> -0.51"));
    s.check(Money::fromString(QStringLiteral("0.506")).toString(2) == QStringLiteral("0.51"),
            QStringLiteral("HALF_UP s2 0.506 -> 0.51"));
    s.check(Money::fromString(QStringLiteral("-0.506")).toString(2) == QStringLiteral("-0.51"),
            QStringLiteral("HALF_UP s2 -0.506 -> -0.51"));
    s.check(Money::fromString(QStringLiteral("0.994")).toString(2) == QStringLiteral("0.99"),
            QStringLiteral("HALF_UP s2 0.994 -> 0.99"));
    s.check(Money::fromString(QStringLiteral("-0.994")).toString(2) == QStringLiteral("-0.99"),
            QStringLiteral("HALF_UP s2 -0.994 -> -0.99"));
    s.check(Money::fromString(QStringLiteral("0.995")).toString(2) == QStringLiteral("1.00"),
            QStringLiteral("HALF_UP s2 0.995 -> 1.00"));
    s.check(Money::fromString(QStringLiteral("-0.995")).toString(2) == QStringLiteral("-1.00"),
            QStringLiteral("HALF_UP s2 -0.995 -> -1.00"));
    s.check(Money::fromString(QStringLiteral("0.996")).toString(2) == QStringLiteral("1.00"),
            QStringLiteral("HALF_UP s2 0.996 -> 1.00"));
    s.check(Money::fromString(QStringLiteral("-0.996")).toString(2) == QStringLiteral("-1.00"),
            QStringLiteral("HALF_UP s2 -0.996 -> -1.00"));
    s.check(Money::fromString(QStringLiteral("1.004")).toString(2) == QStringLiteral("1.00"),
            QStringLiteral("HALF_UP s2 1.004 -> 1.00"));
    s.check(Money::fromString(QStringLiteral("-1.004")).toString(2) == QStringLiteral("-1.00"),
            QStringLiteral("HALF_UP s2 -1.004 -> -1.00"));
    s.check(Money::fromString(QStringLiteral("1.005")).toString(2) == QStringLiteral("1.01"),
            QStringLiteral("HALF_UP s2 1.005 -> 1.01"));
    s.check(Money::fromString(QStringLiteral("-1.005")).toString(2) == QStringLiteral("-1.01"),
            QStringLiteral("HALF_UP s2 -1.005 -> -1.01"));
    s.check(Money::fromString(QStringLiteral("1.006")).toString(2) == QStringLiteral("1.01"),
            QStringLiteral("HALF_UP s2 1.006 -> 1.01"));
    s.check(Money::fromString(QStringLiteral("-1.006")).toString(2) == QStringLiteral("-1.01"),
            QStringLiteral("HALF_UP s2 -1.006 -> -1.01"));
    s.check(Money::fromString(QStringLiteral("1.124")).toString(2) == QStringLiteral("1.12"),
            QStringLiteral("HALF_UP s2 1.124 -> 1.12"));
    s.check(Money::fromString(QStringLiteral("-1.124")).toString(2) == QStringLiteral("-1.12"),
            QStringLiteral("HALF_UP s2 -1.124 -> -1.12"));
    s.check(Money::fromString(QStringLiteral("1.125")).toString(2) == QStringLiteral("1.13"),
            QStringLiteral("HALF_UP s2 1.125 -> 1.13"));
    s.check(Money::fromString(QStringLiteral("-1.125")).toString(2) == QStringLiteral("-1.13"),
            QStringLiteral("HALF_UP s2 -1.125 -> -1.13"));
    s.check(Money::fromString(QStringLiteral("1.126")).toString(2) == QStringLiteral("1.13"),
            QStringLiteral("HALF_UP s2 1.126 -> 1.13"));
    s.check(Money::fromString(QStringLiteral("-1.126")).toString(2) == QStringLiteral("-1.13"),
            QStringLiteral("HALF_UP s2 -1.126 -> -1.13"));
    s.check(Money::fromString(QStringLiteral("1.494")).toString(2) == QStringLiteral("1.49"),
            QStringLiteral("HALF_UP s2 1.494 -> 1.49"));
    s.check(Money::fromString(QStringLiteral("-1.494")).toString(2) == QStringLiteral("-1.49"),
            QStringLiteral("HALF_UP s2 -1.494 -> -1.49"));
    s.check(Money::fromString(QStringLiteral("1.495")).toString(2) == QStringLiteral("1.50"),
            QStringLiteral("HALF_UP s2 1.495 -> 1.50"));
    s.check(Money::fromString(QStringLiteral("-1.495")).toString(2) == QStringLiteral("-1.50"),
            QStringLiteral("HALF_UP s2 -1.495 -> -1.50"));
    s.check(Money::fromString(QStringLiteral("1.496")).toString(2) == QStringLiteral("1.50"),
            QStringLiteral("HALF_UP s2 1.496 -> 1.50"));
    s.check(Money::fromString(QStringLiteral("-1.496")).toString(2) == QStringLiteral("-1.50"),
            QStringLiteral("HALF_UP s2 -1.496 -> -1.50"));
    s.check(Money::fromString(QStringLiteral("1.504")).toString(2) == QStringLiteral("1.50"),
            QStringLiteral("HALF_UP s2 1.504 -> 1.50"));
    s.check(Money::fromString(QStringLiteral("-1.504")).toString(2) == QStringLiteral("-1.50"),
            QStringLiteral("HALF_UP s2 -1.504 -> -1.50"));
    s.check(Money::fromString(QStringLiteral("1.505")).toString(2) == QStringLiteral("1.51"),
            QStringLiteral("HALF_UP s2 1.505 -> 1.51"));
    s.check(Money::fromString(QStringLiteral("-1.505")).toString(2) == QStringLiteral("-1.51"),
            QStringLiteral("HALF_UP s2 -1.505 -> -1.51"));
    s.check(Money::fromString(QStringLiteral("1.506")).toString(2) == QStringLiteral("1.51"),
            QStringLiteral("HALF_UP s2 1.506 -> 1.51"));
    s.check(Money::fromString(QStringLiteral("-1.506")).toString(2) == QStringLiteral("-1.51"),
            QStringLiteral("HALF_UP s2 -1.506 -> -1.51"));
    s.check(Money::fromString(QStringLiteral("1.994")).toString(2) == QStringLiteral("1.99"),
            QStringLiteral("HALF_UP s2 1.994 -> 1.99"));
    s.check(Money::fromString(QStringLiteral("-1.994")).toString(2) == QStringLiteral("-1.99"),
            QStringLiteral("HALF_UP s2 -1.994 -> -1.99"));
    s.check(Money::fromString(QStringLiteral("1.995")).toString(2) == QStringLiteral("2.00"),
            QStringLiteral("HALF_UP s2 1.995 -> 2.00"));
    s.check(Money::fromString(QStringLiteral("-1.995")).toString(2) == QStringLiteral("-2.00"),
            QStringLiteral("HALF_UP s2 -1.995 -> -2.00"));
    s.check(Money::fromString(QStringLiteral("1.996")).toString(2) == QStringLiteral("2.00"),
            QStringLiteral("HALF_UP s2 1.996 -> 2.00"));
    s.check(Money::fromString(QStringLiteral("-1.996")).toString(2) == QStringLiteral("-2.00"),
            QStringLiteral("HALF_UP s2 -1.996 -> -2.00"));
    s.check(Money::fromString(QStringLiteral("2.004")).toString(2) == QStringLiteral("2.00"),
            QStringLiteral("HALF_UP s2 2.004 -> 2.00"));
    s.check(Money::fromString(QStringLiteral("-2.004")).toString(2) == QStringLiteral("-2.00"),
            QStringLiteral("HALF_UP s2 -2.004 -> -2.00"));
    s.check(Money::fromString(QStringLiteral("2.005")).toString(2) == QStringLiteral("2.01"),
            QStringLiteral("HALF_UP s2 2.005 -> 2.01"));
    s.check(Money::fromString(QStringLiteral("-2.005")).toString(2) == QStringLiteral("-2.01"),
            QStringLiteral("HALF_UP s2 -2.005 -> -2.01"));
    s.check(Money::fromString(QStringLiteral("2.006")).toString(2) == QStringLiteral("2.01"),
            QStringLiteral("HALF_UP s2 2.006 -> 2.01"));
    s.check(Money::fromString(QStringLiteral("-2.006")).toString(2) == QStringLiteral("-2.01"),
            QStringLiteral("HALF_UP s2 -2.006 -> -2.01"));
    s.check(Money::fromString(QStringLiteral("2.124")).toString(2) == QStringLiteral("2.12"),
            QStringLiteral("HALF_UP s2 2.124 -> 2.12"));
    s.check(Money::fromString(QStringLiteral("-2.124")).toString(2) == QStringLiteral("-2.12"),
            QStringLiteral("HALF_UP s2 -2.124 -> -2.12"));
    s.check(Money::fromString(QStringLiteral("2.125")).toString(2) == QStringLiteral("2.13"),
            QStringLiteral("HALF_UP s2 2.125 -> 2.13"));
    s.check(Money::fromString(QStringLiteral("-2.125")).toString(2) == QStringLiteral("-2.13"),
            QStringLiteral("HALF_UP s2 -2.125 -> -2.13"));
    s.check(Money::fromString(QStringLiteral("2.126")).toString(2) == QStringLiteral("2.13"),
            QStringLiteral("HALF_UP s2 2.126 -> 2.13"));
    s.check(Money::fromString(QStringLiteral("-2.126")).toString(2) == QStringLiteral("-2.13"),
            QStringLiteral("HALF_UP s2 -2.126 -> -2.13"));
    s.check(Money::fromString(QStringLiteral("2.494")).toString(2) == QStringLiteral("2.49"),
            QStringLiteral("HALF_UP s2 2.494 -> 2.49"));
    s.check(Money::fromString(QStringLiteral("-2.494")).toString(2) == QStringLiteral("-2.49"),
            QStringLiteral("HALF_UP s2 -2.494 -> -2.49"));
    s.check(Money::fromString(QStringLiteral("2.495")).toString(2) == QStringLiteral("2.50"),
            QStringLiteral("HALF_UP s2 2.495 -> 2.50"));
    s.check(Money::fromString(QStringLiteral("-2.495")).toString(2) == QStringLiteral("-2.50"),
            QStringLiteral("HALF_UP s2 -2.495 -> -2.50"));
    s.check(Money::fromString(QStringLiteral("2.496")).toString(2) == QStringLiteral("2.50"),
            QStringLiteral("HALF_UP s2 2.496 -> 2.50"));
    s.check(Money::fromString(QStringLiteral("-2.496")).toString(2) == QStringLiteral("-2.50"),
            QStringLiteral("HALF_UP s2 -2.496 -> -2.50"));
    s.check(Money::fromString(QStringLiteral("2.504")).toString(2) == QStringLiteral("2.50"),
            QStringLiteral("HALF_UP s2 2.504 -> 2.50"));
    s.check(Money::fromString(QStringLiteral("-2.504")).toString(2) == QStringLiteral("-2.50"),
            QStringLiteral("HALF_UP s2 -2.504 -> -2.50"));
    s.check(Money::fromString(QStringLiteral("2.505")).toString(2) == QStringLiteral("2.51"),
            QStringLiteral("HALF_UP s2 2.505 -> 2.51"));
    s.check(Money::fromString(QStringLiteral("-2.505")).toString(2) == QStringLiteral("-2.51"),
            QStringLiteral("HALF_UP s2 -2.505 -> -2.51"));
    s.check(Money::fromString(QStringLiteral("2.506")).toString(2) == QStringLiteral("2.51"),
            QStringLiteral("HALF_UP s2 2.506 -> 2.51"));
    s.check(Money::fromString(QStringLiteral("-2.506")).toString(2) == QStringLiteral("-2.51"),
            QStringLiteral("HALF_UP s2 -2.506 -> -2.51"));
    s.check(Money::fromString(QStringLiteral("2.994")).toString(2) == QStringLiteral("2.99"),
            QStringLiteral("HALF_UP s2 2.994 -> 2.99"));
    s.check(Money::fromString(QStringLiteral("-2.994")).toString(2) == QStringLiteral("-2.99"),
            QStringLiteral("HALF_UP s2 -2.994 -> -2.99"));
    s.check(Money::fromString(QStringLiteral("2.995")).toString(2) == QStringLiteral("3.00"),
            QStringLiteral("HALF_UP s2 2.995 -> 3.00"));
    s.check(Money::fromString(QStringLiteral("-2.995")).toString(2) == QStringLiteral("-3.00"),
            QStringLiteral("HALF_UP s2 -2.995 -> -3.00"));
    s.check(Money::fromString(QStringLiteral("2.996")).toString(2) == QStringLiteral("3.00"),
            QStringLiteral("HALF_UP s2 2.996 -> 3.00"));
    s.check(Money::fromString(QStringLiteral("-2.996")).toString(2) == QStringLiteral("-3.00"),
            QStringLiteral("HALF_UP s2 -2.996 -> -3.00"));
    s.check(Money::fromString(QStringLiteral("5.004")).toString(2) == QStringLiteral("5.00"),
            QStringLiteral("HALF_UP s2 5.004 -> 5.00"));
    s.check(Money::fromString(QStringLiteral("-5.004")).toString(2) == QStringLiteral("-5.00"),
            QStringLiteral("HALF_UP s2 -5.004 -> -5.00"));
    s.check(Money::fromString(QStringLiteral("5.005")).toString(2) == QStringLiteral("5.01"),
            QStringLiteral("HALF_UP s2 5.005 -> 5.01"));
    s.check(Money::fromString(QStringLiteral("-5.005")).toString(2) == QStringLiteral("-5.01"),
            QStringLiteral("HALF_UP s2 -5.005 -> -5.01"));
    s.check(Money::fromString(QStringLiteral("5.006")).toString(2) == QStringLiteral("5.01"),
            QStringLiteral("HALF_UP s2 5.006 -> 5.01"));
    s.check(Money::fromString(QStringLiteral("-5.006")).toString(2) == QStringLiteral("-5.01"),
            QStringLiteral("HALF_UP s2 -5.006 -> -5.01"));
    s.check(Money::fromString(QStringLiteral("5.124")).toString(2) == QStringLiteral("5.12"),
            QStringLiteral("HALF_UP s2 5.124 -> 5.12"));
    s.check(Money::fromString(QStringLiteral("-5.124")).toString(2) == QStringLiteral("-5.12"),
            QStringLiteral("HALF_UP s2 -5.124 -> -5.12"));
    s.check(Money::fromString(QStringLiteral("5.125")).toString(2) == QStringLiteral("5.13"),
            QStringLiteral("HALF_UP s2 5.125 -> 5.13"));
    s.check(Money::fromString(QStringLiteral("-5.125")).toString(2) == QStringLiteral("-5.13"),
            QStringLiteral("HALF_UP s2 -5.125 -> -5.13"));
    s.check(Money::fromString(QStringLiteral("5.126")).toString(2) == QStringLiteral("5.13"),
            QStringLiteral("HALF_UP s2 5.126 -> 5.13"));
    s.check(Money::fromString(QStringLiteral("-5.126")).toString(2) == QStringLiteral("-5.13"),
            QStringLiteral("HALF_UP s2 -5.126 -> -5.13"));
    s.check(Money::fromString(QStringLiteral("5.494")).toString(2) == QStringLiteral("5.49"),
            QStringLiteral("HALF_UP s2 5.494 -> 5.49"));
    s.check(Money::fromString(QStringLiteral("-5.494")).toString(2) == QStringLiteral("-5.49"),
            QStringLiteral("HALF_UP s2 -5.494 -> -5.49"));
    s.check(Money::fromString(QStringLiteral("5.495")).toString(2) == QStringLiteral("5.50"),
            QStringLiteral("HALF_UP s2 5.495 -> 5.50"));
    s.check(Money::fromString(QStringLiteral("-5.495")).toString(2) == QStringLiteral("-5.50"),
            QStringLiteral("HALF_UP s2 -5.495 -> -5.50"));
    s.check(Money::fromString(QStringLiteral("5.496")).toString(2) == QStringLiteral("5.50"),
            QStringLiteral("HALF_UP s2 5.496 -> 5.50"));
    s.check(Money::fromString(QStringLiteral("-5.496")).toString(2) == QStringLiteral("-5.50"),
            QStringLiteral("HALF_UP s2 -5.496 -> -5.50"));
    s.check(Money::fromString(QStringLiteral("5.504")).toString(2) == QStringLiteral("5.50"),
            QStringLiteral("HALF_UP s2 5.504 -> 5.50"));
    s.check(Money::fromString(QStringLiteral("-5.504")).toString(2) == QStringLiteral("-5.50"),
            QStringLiteral("HALF_UP s2 -5.504 -> -5.50"));
    s.check(Money::fromString(QStringLiteral("5.505")).toString(2) == QStringLiteral("5.51"),
            QStringLiteral("HALF_UP s2 5.505 -> 5.51"));
    s.check(Money::fromString(QStringLiteral("-5.505")).toString(2) == QStringLiteral("-5.51"),
            QStringLiteral("HALF_UP s2 -5.505 -> -5.51"));
    s.check(Money::fromString(QStringLiteral("5.506")).toString(2) == QStringLiteral("5.51"),
            QStringLiteral("HALF_UP s2 5.506 -> 5.51"));
    s.check(Money::fromString(QStringLiteral("-5.506")).toString(2) == QStringLiteral("-5.51"),
            QStringLiteral("HALF_UP s2 -5.506 -> -5.51"));
    s.check(Money::fromString(QStringLiteral("5.994")).toString(2) == QStringLiteral("5.99"),
            QStringLiteral("HALF_UP s2 5.994 -> 5.99"));
    s.check(Money::fromString(QStringLiteral("-5.994")).toString(2) == QStringLiteral("-5.99"),
            QStringLiteral("HALF_UP s2 -5.994 -> -5.99"));
    s.check(Money::fromString(QStringLiteral("5.995")).toString(2) == QStringLiteral("6.00"),
            QStringLiteral("HALF_UP s2 5.995 -> 6.00"));
    s.check(Money::fromString(QStringLiteral("-5.995")).toString(2) == QStringLiteral("-6.00"),
            QStringLiteral("HALF_UP s2 -5.995 -> -6.00"));
    s.check(Money::fromString(QStringLiteral("5.996")).toString(2) == QStringLiteral("6.00"),
            QStringLiteral("HALF_UP s2 5.996 -> 6.00"));
    s.check(Money::fromString(QStringLiteral("-5.996")).toString(2) == QStringLiteral("-6.00"),
            QStringLiteral("HALF_UP s2 -5.996 -> -6.00"));
    s.check(Money::fromString(QStringLiteral("10.004")).toString(2) == QStringLiteral("10.00"),
            QStringLiteral("HALF_UP s2 10.004 -> 10.00"));
    s.check(Money::fromString(QStringLiteral("-10.004")).toString(2) == QStringLiteral("-10.00"),
            QStringLiteral("HALF_UP s2 -10.004 -> -10.00"));
    s.check(Money::fromString(QStringLiteral("10.005")).toString(2) == QStringLiteral("10.01"),
            QStringLiteral("HALF_UP s2 10.005 -> 10.01"));
    s.check(Money::fromString(QStringLiteral("-10.005")).toString(2) == QStringLiteral("-10.01"),
            QStringLiteral("HALF_UP s2 -10.005 -> -10.01"));
    s.check(Money::fromString(QStringLiteral("10.006")).toString(2) == QStringLiteral("10.01"),
            QStringLiteral("HALF_UP s2 10.006 -> 10.01"));
    s.check(Money::fromString(QStringLiteral("-10.006")).toString(2) == QStringLiteral("-10.01"),
            QStringLiteral("HALF_UP s2 -10.006 -> -10.01"));
    s.check(Money::fromString(QStringLiteral("10.124")).toString(2) == QStringLiteral("10.12"),
            QStringLiteral("HALF_UP s2 10.124 -> 10.12"));
    s.check(Money::fromString(QStringLiteral("-10.124")).toString(2) == QStringLiteral("-10.12"),
            QStringLiteral("HALF_UP s2 -10.124 -> -10.12"));
    s.check(Money::fromString(QStringLiteral("10.125")).toString(2) == QStringLiteral("10.13"),
            QStringLiteral("HALF_UP s2 10.125 -> 10.13"));
    s.check(Money::fromString(QStringLiteral("-10.125")).toString(2) == QStringLiteral("-10.13"),
            QStringLiteral("HALF_UP s2 -10.125 -> -10.13"));
    s.check(Money::fromString(QStringLiteral("10.126")).toString(2) == QStringLiteral("10.13"),
            QStringLiteral("HALF_UP s2 10.126 -> 10.13"));
    s.check(Money::fromString(QStringLiteral("-10.126")).toString(2) == QStringLiteral("-10.13"),
            QStringLiteral("HALF_UP s2 -10.126 -> -10.13"));
    s.check(Money::fromString(QStringLiteral("10.494")).toString(2) == QStringLiteral("10.49"),
            QStringLiteral("HALF_UP s2 10.494 -> 10.49"));
    s.check(Money::fromString(QStringLiteral("-10.494")).toString(2) == QStringLiteral("-10.49"),
            QStringLiteral("HALF_UP s2 -10.494 -> -10.49"));
    s.check(Money::fromString(QStringLiteral("10.495")).toString(2) == QStringLiteral("10.50"),
            QStringLiteral("HALF_UP s2 10.495 -> 10.50"));
    s.check(Money::fromString(QStringLiteral("-10.495")).toString(2) == QStringLiteral("-10.50"),
            QStringLiteral("HALF_UP s2 -10.495 -> -10.50"));
    s.check(Money::fromString(QStringLiteral("10.496")).toString(2) == QStringLiteral("10.50"),
            QStringLiteral("HALF_UP s2 10.496 -> 10.50"));
    s.check(Money::fromString(QStringLiteral("-10.496")).toString(2) == QStringLiteral("-10.50"),
            QStringLiteral("HALF_UP s2 -10.496 -> -10.50"));
    s.check(Money::fromString(QStringLiteral("10.504")).toString(2) == QStringLiteral("10.50"),
            QStringLiteral("HALF_UP s2 10.504 -> 10.50"));
    s.check(Money::fromString(QStringLiteral("-10.504")).toString(2) == QStringLiteral("-10.50"),
            QStringLiteral("HALF_UP s2 -10.504 -> -10.50"));
    s.check(Money::fromString(QStringLiteral("10.505")).toString(2) == QStringLiteral("10.51"),
            QStringLiteral("HALF_UP s2 10.505 -> 10.51"));
    s.check(Money::fromString(QStringLiteral("-10.505")).toString(2) == QStringLiteral("-10.51"),
            QStringLiteral("HALF_UP s2 -10.505 -> -10.51"));
    s.check(Money::fromString(QStringLiteral("10.506")).toString(2) == QStringLiteral("10.51"),
            QStringLiteral("HALF_UP s2 10.506 -> 10.51"));
    s.check(Money::fromString(QStringLiteral("-10.506")).toString(2) == QStringLiteral("-10.51"),
            QStringLiteral("HALF_UP s2 -10.506 -> -10.51"));
    s.check(Money::fromString(QStringLiteral("10.994")).toString(2) == QStringLiteral("10.99"),
            QStringLiteral("HALF_UP s2 10.994 -> 10.99"));
    s.check(Money::fromString(QStringLiteral("-10.994")).toString(2) == QStringLiteral("-10.99"),
            QStringLiteral("HALF_UP s2 -10.994 -> -10.99"));
    s.check(Money::fromString(QStringLiteral("10.995")).toString(2) == QStringLiteral("11.00"),
            QStringLiteral("HALF_UP s2 10.995 -> 11.00"));
    s.check(Money::fromString(QStringLiteral("-10.995")).toString(2) == QStringLiteral("-11.00"),
            QStringLiteral("HALF_UP s2 -10.995 -> -11.00"));
    s.check(Money::fromString(QStringLiteral("10.996")).toString(2) == QStringLiteral("11.00"),
            QStringLiteral("HALF_UP s2 10.996 -> 11.00"));
    s.check(Money::fromString(QStringLiteral("-10.996")).toString(2) == QStringLiteral("-11.00"),
            QStringLiteral("HALF_UP s2 -10.996 -> -11.00"));
    s.check(Money::fromString(QStringLiteral("99.004")).toString(2) == QStringLiteral("99.00"),
            QStringLiteral("HALF_UP s2 99.004 -> 99.00"));
    s.check(Money::fromString(QStringLiteral("-99.004")).toString(2) == QStringLiteral("-99.00"),
            QStringLiteral("HALF_UP s2 -99.004 -> -99.00"));
    s.check(Money::fromString(QStringLiteral("99.005")).toString(2) == QStringLiteral("99.01"),
            QStringLiteral("HALF_UP s2 99.005 -> 99.01"));
    s.check(Money::fromString(QStringLiteral("-99.005")).toString(2) == QStringLiteral("-99.01"),
            QStringLiteral("HALF_UP s2 -99.005 -> -99.01"));
    s.check(Money::fromString(QStringLiteral("99.006")).toString(2) == QStringLiteral("99.01"),
            QStringLiteral("HALF_UP s2 99.006 -> 99.01"));
    s.check(Money::fromString(QStringLiteral("-99.006")).toString(2) == QStringLiteral("-99.01"),
            QStringLiteral("HALF_UP s2 -99.006 -> -99.01"));
    s.check(Money::fromString(QStringLiteral("99.124")).toString(2) == QStringLiteral("99.12"),
            QStringLiteral("HALF_UP s2 99.124 -> 99.12"));
    s.check(Money::fromString(QStringLiteral("-99.124")).toString(2) == QStringLiteral("-99.12"),
            QStringLiteral("HALF_UP s2 -99.124 -> -99.12"));
    s.check(Money::fromString(QStringLiteral("99.125")).toString(2) == QStringLiteral("99.13"),
            QStringLiteral("HALF_UP s2 99.125 -> 99.13"));
    s.check(Money::fromString(QStringLiteral("-99.125")).toString(2) == QStringLiteral("-99.13"),
            QStringLiteral("HALF_UP s2 -99.125 -> -99.13"));
    s.check(Money::fromString(QStringLiteral("99.126")).toString(2) == QStringLiteral("99.13"),
            QStringLiteral("HALF_UP s2 99.126 -> 99.13"));
    s.check(Money::fromString(QStringLiteral("-99.126")).toString(2) == QStringLiteral("-99.13"),
            QStringLiteral("HALF_UP s2 -99.126 -> -99.13"));
    s.check(Money::fromString(QStringLiteral("99.494")).toString(2) == QStringLiteral("99.49"),
            QStringLiteral("HALF_UP s2 99.494 -> 99.49"));
    s.check(Money::fromString(QStringLiteral("-99.494")).toString(2) == QStringLiteral("-99.49"),
            QStringLiteral("HALF_UP s2 -99.494 -> -99.49"));
    s.check(Money::fromString(QStringLiteral("99.495")).toString(2) == QStringLiteral("99.50"),
            QStringLiteral("HALF_UP s2 99.495 -> 99.50"));
    s.check(Money::fromString(QStringLiteral("-99.495")).toString(2) == QStringLiteral("-99.50"),
            QStringLiteral("HALF_UP s2 -99.495 -> -99.50"));
    s.check(Money::fromString(QStringLiteral("99.496")).toString(2) == QStringLiteral("99.50"),
            QStringLiteral("HALF_UP s2 99.496 -> 99.50"));
    s.check(Money::fromString(QStringLiteral("-99.496")).toString(2) == QStringLiteral("-99.50"),
            QStringLiteral("HALF_UP s2 -99.496 -> -99.50"));
    s.check(Money::fromString(QStringLiteral("99.504")).toString(2) == QStringLiteral("99.50"),
            QStringLiteral("HALF_UP s2 99.504 -> 99.50"));
    s.check(Money::fromString(QStringLiteral("-99.504")).toString(2) == QStringLiteral("-99.50"),
            QStringLiteral("HALF_UP s2 -99.504 -> -99.50"));
    s.check(Money::fromString(QStringLiteral("99.505")).toString(2) == QStringLiteral("99.51"),
            QStringLiteral("HALF_UP s2 99.505 -> 99.51"));
    s.check(Money::fromString(QStringLiteral("-99.505")).toString(2) == QStringLiteral("-99.51"),
            QStringLiteral("HALF_UP s2 -99.505 -> -99.51"));
    s.check(Money::fromString(QStringLiteral("99.506")).toString(2) == QStringLiteral("99.51"),
            QStringLiteral("HALF_UP s2 99.506 -> 99.51"));
    s.check(Money::fromString(QStringLiteral("-99.506")).toString(2) == QStringLiteral("-99.51"),
            QStringLiteral("HALF_UP s2 -99.506 -> -99.51"));
    s.check(Money::fromString(QStringLiteral("99.994")).toString(2) == QStringLiteral("99.99"),
            QStringLiteral("HALF_UP s2 99.994 -> 99.99"));
    s.check(Money::fromString(QStringLiteral("-99.994")).toString(2) == QStringLiteral("-99.99"),
            QStringLiteral("HALF_UP s2 -99.994 -> -99.99"));
    s.check(Money::fromString(QStringLiteral("99.995")).toString(2) == QStringLiteral("100.00"),
            QStringLiteral("HALF_UP s2 99.995 -> 100.00"));
    s.check(Money::fromString(QStringLiteral("-99.995")).toString(2) == QStringLiteral("-100.00"),
            QStringLiteral("HALF_UP s2 -99.995 -> -100.00"));
    s.check(Money::fromString(QStringLiteral("99.996")).toString(2) == QStringLiteral("100.00"),
            QStringLiteral("HALF_UP s2 99.996 -> 100.00"));
    s.check(Money::fromString(QStringLiteral("-99.996")).toString(2) == QStringLiteral("-100.00"),
            QStringLiteral("HALF_UP s2 -99.996 -> -100.00"));
    s.check(Money::fromString(QStringLiteral("123.004")).toString(2) == QStringLiteral("123.00"),
            QStringLiteral("HALF_UP s2 123.004 -> 123.00"));
    s.check(Money::fromString(QStringLiteral("-123.004")).toString(2) == QStringLiteral("-123.00"),
            QStringLiteral("HALF_UP s2 -123.004 -> -123.00"));
    s.check(Money::fromString(QStringLiteral("123.005")).toString(2) == QStringLiteral("123.01"),
            QStringLiteral("HALF_UP s2 123.005 -> 123.01"));
    s.check(Money::fromString(QStringLiteral("-123.005")).toString(2) == QStringLiteral("-123.01"),
            QStringLiteral("HALF_UP s2 -123.005 -> -123.01"));
    s.check(Money::fromString(QStringLiteral("123.006")).toString(2) == QStringLiteral("123.01"),
            QStringLiteral("HALF_UP s2 123.006 -> 123.01"));
    s.check(Money::fromString(QStringLiteral("-123.006")).toString(2) == QStringLiteral("-123.01"),
            QStringLiteral("HALF_UP s2 -123.006 -> -123.01"));
    s.check(Money::fromString(QStringLiteral("123.124")).toString(2) == QStringLiteral("123.12"),
            QStringLiteral("HALF_UP s2 123.124 -> 123.12"));
    s.check(Money::fromString(QStringLiteral("-123.124")).toString(2) == QStringLiteral("-123.12"),
            QStringLiteral("HALF_UP s2 -123.124 -> -123.12"));
    s.check(Money::fromString(QStringLiteral("123.125")).toString(2) == QStringLiteral("123.13"),
            QStringLiteral("HALF_UP s2 123.125 -> 123.13"));
    s.check(Money::fromString(QStringLiteral("-123.125")).toString(2) == QStringLiteral("-123.13"),
            QStringLiteral("HALF_UP s2 -123.125 -> -123.13"));
    s.check(Money::fromString(QStringLiteral("123.126")).toString(2) == QStringLiteral("123.13"),
            QStringLiteral("HALF_UP s2 123.126 -> 123.13"));
    s.check(Money::fromString(QStringLiteral("-123.126")).toString(2) == QStringLiteral("-123.13"),
            QStringLiteral("HALF_UP s2 -123.126 -> -123.13"));
    s.check(Money::fromString(QStringLiteral("123.494")).toString(2) == QStringLiteral("123.49"),
            QStringLiteral("HALF_UP s2 123.494 -> 123.49"));
    s.check(Money::fromString(QStringLiteral("-123.494")).toString(2) == QStringLiteral("-123.49"),
            QStringLiteral("HALF_UP s2 -123.494 -> -123.49"));
    s.check(Money::fromString(QStringLiteral("123.495")).toString(2) == QStringLiteral("123.50"),
            QStringLiteral("HALF_UP s2 123.495 -> 123.50"));
    s.check(Money::fromString(QStringLiteral("-123.495")).toString(2) == QStringLiteral("-123.50"),
            QStringLiteral("HALF_UP s2 -123.495 -> -123.50"));
    s.check(Money::fromString(QStringLiteral("123.496")).toString(2) == QStringLiteral("123.50"),
            QStringLiteral("HALF_UP s2 123.496 -> 123.50"));
    s.check(Money::fromString(QStringLiteral("-123.496")).toString(2) == QStringLiteral("-123.50"),
            QStringLiteral("HALF_UP s2 -123.496 -> -123.50"));
    s.check(Money::fromString(QStringLiteral("123.504")).toString(2) == QStringLiteral("123.50"),
            QStringLiteral("HALF_UP s2 123.504 -> 123.50"));
    s.check(Money::fromString(QStringLiteral("-123.504")).toString(2) == QStringLiteral("-123.50"),
            QStringLiteral("HALF_UP s2 -123.504 -> -123.50"));
    s.check(Money::fromString(QStringLiteral("123.505")).toString(2) == QStringLiteral("123.51"),
            QStringLiteral("HALF_UP s2 123.505 -> 123.51"));
    s.check(Money::fromString(QStringLiteral("-123.505")).toString(2) == QStringLiteral("-123.51"),
            QStringLiteral("HALF_UP s2 -123.505 -> -123.51"));
    s.check(Money::fromString(QStringLiteral("123.506")).toString(2) == QStringLiteral("123.51"),
            QStringLiteral("HALF_UP s2 123.506 -> 123.51"));
    s.check(Money::fromString(QStringLiteral("-123.506")).toString(2) == QStringLiteral("-123.51"),
            QStringLiteral("HALF_UP s2 -123.506 -> -123.51"));
    s.check(Money::fromString(QStringLiteral("123.994")).toString(2) == QStringLiteral("123.99"),
            QStringLiteral("HALF_UP s2 123.994 -> 123.99"));
    s.check(Money::fromString(QStringLiteral("-123.994")).toString(2) == QStringLiteral("-123.99"),
            QStringLiteral("HALF_UP s2 -123.994 -> -123.99"));
    s.check(Money::fromString(QStringLiteral("123.995")).toString(2) == QStringLiteral("124.00"),
            QStringLiteral("HALF_UP s2 123.995 -> 124.00"));
    s.check(Money::fromString(QStringLiteral("-123.995")).toString(2) == QStringLiteral("-124.00"),
            QStringLiteral("HALF_UP s2 -123.995 -> -124.00"));
    s.check(Money::fromString(QStringLiteral("123.996")).toString(2) == QStringLiteral("124.00"),
            QStringLiteral("HALF_UP s2 123.996 -> 124.00"));
    s.check(Money::fromString(QStringLiteral("-123.996")).toString(2) == QStringLiteral("-124.00"),
            QStringLiteral("HALF_UP s2 -123.996 -> -124.00"));
    s.check(Money::fromString(QStringLiteral("1000.004")).toString(2) == QStringLiteral("1000.00"),
            QStringLiteral("HALF_UP s2 1000.004 -> 1000.00"));
    s.check(Money::fromString(QStringLiteral("-1000.004")).toString(2)
                == QStringLiteral("-1000.00"),
            QStringLiteral("HALF_UP s2 -1000.004 -> -1000.00"));
    s.check(Money::fromString(QStringLiteral("1000.005")).toString(2) == QStringLiteral("1000.01"),
            QStringLiteral("HALF_UP s2 1000.005 -> 1000.01"));
    s.check(Money::fromString(QStringLiteral("-1000.005")).toString(2)
                == QStringLiteral("-1000.01"),
            QStringLiteral("HALF_UP s2 -1000.005 -> -1000.01"));
    s.check(Money::fromString(QStringLiteral("1000.006")).toString(2) == QStringLiteral("1000.01"),
            QStringLiteral("HALF_UP s2 1000.006 -> 1000.01"));
    s.check(Money::fromString(QStringLiteral("-1000.006")).toString(2)
                == QStringLiteral("-1000.01"),
            QStringLiteral("HALF_UP s2 -1000.006 -> -1000.01"));
    s.check(Money::fromString(QStringLiteral("1000.124")).toString(2) == QStringLiteral("1000.12"),
            QStringLiteral("HALF_UP s2 1000.124 -> 1000.12"));
    s.check(Money::fromString(QStringLiteral("-1000.124")).toString(2)
                == QStringLiteral("-1000.12"),
            QStringLiteral("HALF_UP s2 -1000.124 -> -1000.12"));
    s.check(Money::fromString(QStringLiteral("1000.125")).toString(2) == QStringLiteral("1000.13"),
            QStringLiteral("HALF_UP s2 1000.125 -> 1000.13"));
    s.check(Money::fromString(QStringLiteral("-1000.125")).toString(2)
                == QStringLiteral("-1000.13"),
            QStringLiteral("HALF_UP s2 -1000.125 -> -1000.13"));
    s.check(Money::fromString(QStringLiteral("1000.126")).toString(2) == QStringLiteral("1000.13"),
            QStringLiteral("HALF_UP s2 1000.126 -> 1000.13"));
    s.check(Money::fromString(QStringLiteral("-1000.126")).toString(2)
                == QStringLiteral("-1000.13"),
            QStringLiteral("HALF_UP s2 -1000.126 -> -1000.13"));
    s.check(Money::fromString(QStringLiteral("1000.494")).toString(2) == QStringLiteral("1000.49"),
            QStringLiteral("HALF_UP s2 1000.494 -> 1000.49"));
    s.check(Money::fromString(QStringLiteral("-1000.494")).toString(2)
                == QStringLiteral("-1000.49"),
            QStringLiteral("HALF_UP s2 -1000.494 -> -1000.49"));
    s.check(Money::fromString(QStringLiteral("1000.495")).toString(2) == QStringLiteral("1000.50"),
            QStringLiteral("HALF_UP s2 1000.495 -> 1000.50"));
    s.check(Money::fromString(QStringLiteral("-1000.495")).toString(2)
                == QStringLiteral("-1000.50"),
            QStringLiteral("HALF_UP s2 -1000.495 -> -1000.50"));
    s.check(Money::fromString(QStringLiteral("1000.496")).toString(2) == QStringLiteral("1000.50"),
            QStringLiteral("HALF_UP s2 1000.496 -> 1000.50"));
    s.check(Money::fromString(QStringLiteral("-1000.496")).toString(2)
                == QStringLiteral("-1000.50"),
            QStringLiteral("HALF_UP s2 -1000.496 -> -1000.50"));
    s.check(Money::fromString(QStringLiteral("1000.504")).toString(2) == QStringLiteral("1000.50"),
            QStringLiteral("HALF_UP s2 1000.504 -> 1000.50"));
    s.check(Money::fromString(QStringLiteral("-1000.504")).toString(2)
                == QStringLiteral("-1000.50"),
            QStringLiteral("HALF_UP s2 -1000.504 -> -1000.50"));
    s.check(Money::fromString(QStringLiteral("1000.505")).toString(2) == QStringLiteral("1000.51"),
            QStringLiteral("HALF_UP s2 1000.505 -> 1000.51"));
    s.check(Money::fromString(QStringLiteral("-1000.505")).toString(2)
                == QStringLiteral("-1000.51"),
            QStringLiteral("HALF_UP s2 -1000.505 -> -1000.51"));
    s.check(Money::fromString(QStringLiteral("1000.506")).toString(2) == QStringLiteral("1000.51"),
            QStringLiteral("HALF_UP s2 1000.506 -> 1000.51"));
    s.check(Money::fromString(QStringLiteral("-1000.506")).toString(2)
                == QStringLiteral("-1000.51"),
            QStringLiteral("HALF_UP s2 -1000.506 -> -1000.51"));
    s.check(Money::fromString(QStringLiteral("1000.994")).toString(2) == QStringLiteral("1000.99"),
            QStringLiteral("HALF_UP s2 1000.994 -> 1000.99"));
    s.check(Money::fromString(QStringLiteral("-1000.994")).toString(2)
                == QStringLiteral("-1000.99"),
            QStringLiteral("HALF_UP s2 -1000.994 -> -1000.99"));
    s.check(Money::fromString(QStringLiteral("1000.995")).toString(2) == QStringLiteral("1001.00"),
            QStringLiteral("HALF_UP s2 1000.995 -> 1001.00"));
    s.check(Money::fromString(QStringLiteral("-1000.995")).toString(2)
                == QStringLiteral("-1001.00"),
            QStringLiteral("HALF_UP s2 -1000.995 -> -1001.00"));
    s.check(Money::fromString(QStringLiteral("1000.996")).toString(2) == QStringLiteral("1001.00"),
            QStringLiteral("HALF_UP s2 1000.996 -> 1001.00"));
    s.check(Money::fromString(QStringLiteral("-1000.996")).toString(2)
                == QStringLiteral("-1001.00"),
            QStringLiteral("HALF_UP s2 -1000.996 -> -1001.00"));
    s.check(Money::fromString(QStringLiteral("0.0004")).toString(4) == QStringLiteral("0.0004"),
            QStringLiteral("HALF_UP s4 0.0004 -> 0.0004"));
    s.check(Money::fromString(QStringLiteral("-0.0004")).toString(4) == QStringLiteral("-0.0004"),
            QStringLiteral("HALF_UP s4 -0.0004 -> -0.0004"));
    s.check(Money::fromString(QStringLiteral("0.0005")).toString(4) == QStringLiteral("0.0005"),
            QStringLiteral("HALF_UP s4 0.0005 -> 0.0005"));
    s.check(Money::fromString(QStringLiteral("-0.0005")).toString(4) == QStringLiteral("-0.0005"),
            QStringLiteral("HALF_UP s4 -0.0005 -> -0.0005"));
    s.check(Money::fromString(QStringLiteral("0.0006")).toString(4) == QStringLiteral("0.0006"),
            QStringLiteral("HALF_UP s4 0.0006 -> 0.0006"));
    s.check(Money::fromString(QStringLiteral("-0.0006")).toString(4) == QStringLiteral("-0.0006"),
            QStringLiteral("HALF_UP s4 -0.0006 -> -0.0006"));
    s.check(Money::fromString(QStringLiteral("0.1234")).toString(4) == QStringLiteral("0.1234"),
            QStringLiteral("HALF_UP s4 0.1234 -> 0.1234"));
    s.check(Money::fromString(QStringLiteral("-0.1234")).toString(4) == QStringLiteral("-0.1234"),
            QStringLiteral("HALF_UP s4 -0.1234 -> -0.1234"));
    s.check(Money::fromString(QStringLiteral("0.1235")).toString(4) == QStringLiteral("0.1235"),
            QStringLiteral("HALF_UP s4 0.1235 -> 0.1235"));
    s.check(Money::fromString(QStringLiteral("-0.1235")).toString(4) == QStringLiteral("-0.1235"),
            QStringLiteral("HALF_UP s4 -0.1235 -> -0.1235"));
    s.check(Money::fromString(QStringLiteral("0.1236")).toString(4) == QStringLiteral("0.1236"),
            QStringLiteral("HALF_UP s4 0.1236 -> 0.1236"));
    s.check(Money::fromString(QStringLiteral("-0.1236")).toString(4) == QStringLiteral("-0.1236"),
            QStringLiteral("HALF_UP s4 -0.1236 -> -0.1236"));
    s.check(Money::fromString(QStringLiteral("0.9994")).toString(4) == QStringLiteral("0.9994"),
            QStringLiteral("HALF_UP s4 0.9994 -> 0.9994"));
    s.check(Money::fromString(QStringLiteral("-0.9994")).toString(4) == QStringLiteral("-0.9994"),
            QStringLiteral("HALF_UP s4 -0.9994 -> -0.9994"));
    s.check(Money::fromString(QStringLiteral("0.9995")).toString(4) == QStringLiteral("0.9995"),
            QStringLiteral("HALF_UP s4 0.9995 -> 0.9995"));
    s.check(Money::fromString(QStringLiteral("-0.9995")).toString(4) == QStringLiteral("-0.9995"),
            QStringLiteral("HALF_UP s4 -0.9995 -> -0.9995"));
    s.check(Money::fromString(QStringLiteral("0.9996")).toString(4) == QStringLiteral("0.9996"),
            QStringLiteral("HALF_UP s4 0.9996 -> 0.9996"));
    s.check(Money::fromString(QStringLiteral("-0.9996")).toString(4) == QStringLiteral("-0.9996"),
            QStringLiteral("HALF_UP s4 -0.9996 -> -0.9996"));
    s.check(Money::fromString(QStringLiteral("0.5554")).toString(4) == QStringLiteral("0.5554"),
            QStringLiteral("HALF_UP s4 0.5554 -> 0.5554"));
    s.check(Money::fromString(QStringLiteral("-0.5554")).toString(4) == QStringLiteral("-0.5554"),
            QStringLiteral("HALF_UP s4 -0.5554 -> -0.5554"));
    s.check(Money::fromString(QStringLiteral("0.5555")).toString(4) == QStringLiteral("0.5555"),
            QStringLiteral("HALF_UP s4 0.5555 -> 0.5555"));
    s.check(Money::fromString(QStringLiteral("-0.5555")).toString(4) == QStringLiteral("-0.5555"),
            QStringLiteral("HALF_UP s4 -0.5555 -> -0.5555"));
    s.check(Money::fromString(QStringLiteral("0.5556")).toString(4) == QStringLiteral("0.5556"),
            QStringLiteral("HALF_UP s4 0.5556 -> 0.5556"));
    s.check(Money::fromString(QStringLiteral("-0.5556")).toString(4) == QStringLiteral("-0.5556"),
            QStringLiteral("HALF_UP s4 -0.5556 -> -0.5556"));
    s.check(Money::fromString(QStringLiteral("0.2504")).toString(4) == QStringLiteral("0.2504"),
            QStringLiteral("HALF_UP s4 0.2504 -> 0.2504"));
    s.check(Money::fromString(QStringLiteral("-0.2504")).toString(4) == QStringLiteral("-0.2504"),
            QStringLiteral("HALF_UP s4 -0.2504 -> -0.2504"));
    s.check(Money::fromString(QStringLiteral("0.2505")).toString(4) == QStringLiteral("0.2505"),
            QStringLiteral("HALF_UP s4 0.2505 -> 0.2505"));
    s.check(Money::fromString(QStringLiteral("-0.2505")).toString(4) == QStringLiteral("-0.2505"),
            QStringLiteral("HALF_UP s4 -0.2505 -> -0.2505"));
    s.check(Money::fromString(QStringLiteral("0.2506")).toString(4) == QStringLiteral("0.2506"),
            QStringLiteral("HALF_UP s4 0.2506 -> 0.2506"));
    s.check(Money::fromString(QStringLiteral("-0.2506")).toString(4) == QStringLiteral("-0.2506"),
            QStringLiteral("HALF_UP s4 -0.2506 -> -0.2506"));
    s.check(Money::fromString(QStringLiteral("1.0004")).toString(4) == QStringLiteral("1.0004"),
            QStringLiteral("HALF_UP s4 1.0004 -> 1.0004"));
    s.check(Money::fromString(QStringLiteral("-1.0004")).toString(4) == QStringLiteral("-1.0004"),
            QStringLiteral("HALF_UP s4 -1.0004 -> -1.0004"));
    s.check(Money::fromString(QStringLiteral("1.0005")).toString(4) == QStringLiteral("1.0005"),
            QStringLiteral("HALF_UP s4 1.0005 -> 1.0005"));
    s.check(Money::fromString(QStringLiteral("-1.0005")).toString(4) == QStringLiteral("-1.0005"),
            QStringLiteral("HALF_UP s4 -1.0005 -> -1.0005"));
    s.check(Money::fromString(QStringLiteral("1.0006")).toString(4) == QStringLiteral("1.0006"),
            QStringLiteral("HALF_UP s4 1.0006 -> 1.0006"));
    s.check(Money::fromString(QStringLiteral("-1.0006")).toString(4) == QStringLiteral("-1.0006"),
            QStringLiteral("HALF_UP s4 -1.0006 -> -1.0006"));
    s.check(Money::fromString(QStringLiteral("1.1234")).toString(4) == QStringLiteral("1.1234"),
            QStringLiteral("HALF_UP s4 1.1234 -> 1.1234"));
    s.check(Money::fromString(QStringLiteral("-1.1234")).toString(4) == QStringLiteral("-1.1234"),
            QStringLiteral("HALF_UP s4 -1.1234 -> -1.1234"));
    s.check(Money::fromString(QStringLiteral("1.1235")).toString(4) == QStringLiteral("1.1235"),
            QStringLiteral("HALF_UP s4 1.1235 -> 1.1235"));
    s.check(Money::fromString(QStringLiteral("-1.1235")).toString(4) == QStringLiteral("-1.1235"),
            QStringLiteral("HALF_UP s4 -1.1235 -> -1.1235"));
    s.check(Money::fromString(QStringLiteral("1.1236")).toString(4) == QStringLiteral("1.1236"),
            QStringLiteral("HALF_UP s4 1.1236 -> 1.1236"));
    s.check(Money::fromString(QStringLiteral("-1.1236")).toString(4) == QStringLiteral("-1.1236"),
            QStringLiteral("HALF_UP s4 -1.1236 -> -1.1236"));
    s.check(Money::fromString(QStringLiteral("1.9994")).toString(4) == QStringLiteral("1.9994"),
            QStringLiteral("HALF_UP s4 1.9994 -> 1.9994"));
    s.check(Money::fromString(QStringLiteral("-1.9994")).toString(4) == QStringLiteral("-1.9994"),
            QStringLiteral("HALF_UP s4 -1.9994 -> -1.9994"));
    s.check(Money::fromString(QStringLiteral("1.9995")).toString(4) == QStringLiteral("1.9995"),
            QStringLiteral("HALF_UP s4 1.9995 -> 1.9995"));
    s.check(Money::fromString(QStringLiteral("-1.9995")).toString(4) == QStringLiteral("-1.9995"),
            QStringLiteral("HALF_UP s4 -1.9995 -> -1.9995"));
    s.check(Money::fromString(QStringLiteral("1.9996")).toString(4) == QStringLiteral("1.9996"),
            QStringLiteral("HALF_UP s4 1.9996 -> 1.9996"));
    s.check(Money::fromString(QStringLiteral("-1.9996")).toString(4) == QStringLiteral("-1.9996"),
            QStringLiteral("HALF_UP s4 -1.9996 -> -1.9996"));
    s.check(Money::fromString(QStringLiteral("1.5554")).toString(4) == QStringLiteral("1.5554"),
            QStringLiteral("HALF_UP s4 1.5554 -> 1.5554"));
    s.check(Money::fromString(QStringLiteral("-1.5554")).toString(4) == QStringLiteral("-1.5554"),
            QStringLiteral("HALF_UP s4 -1.5554 -> -1.5554"));
    s.check(Money::fromString(QStringLiteral("1.5555")).toString(4) == QStringLiteral("1.5555"),
            QStringLiteral("HALF_UP s4 1.5555 -> 1.5555"));
    s.check(Money::fromString(QStringLiteral("-1.5555")).toString(4) == QStringLiteral("-1.5555"),
            QStringLiteral("HALF_UP s4 -1.5555 -> -1.5555"));
    s.check(Money::fromString(QStringLiteral("1.5556")).toString(4) == QStringLiteral("1.5556"),
            QStringLiteral("HALF_UP s4 1.5556 -> 1.5556"));
    s.check(Money::fromString(QStringLiteral("-1.5556")).toString(4) == QStringLiteral("-1.5556"),
            QStringLiteral("HALF_UP s4 -1.5556 -> -1.5556"));
    s.check(Money::fromString(QStringLiteral("1.2504")).toString(4) == QStringLiteral("1.2504"),
            QStringLiteral("HALF_UP s4 1.2504 -> 1.2504"));
    s.check(Money::fromString(QStringLiteral("-1.2504")).toString(4) == QStringLiteral("-1.2504"),
            QStringLiteral("HALF_UP s4 -1.2504 -> -1.2504"));
    s.check(Money::fromString(QStringLiteral("1.2505")).toString(4) == QStringLiteral("1.2505"),
            QStringLiteral("HALF_UP s4 1.2505 -> 1.2505"));
    s.check(Money::fromString(QStringLiteral("-1.2505")).toString(4) == QStringLiteral("-1.2505"),
            QStringLiteral("HALF_UP s4 -1.2505 -> -1.2505"));
    s.check(Money::fromString(QStringLiteral("1.2506")).toString(4) == QStringLiteral("1.2506"),
            QStringLiteral("HALF_UP s4 1.2506 -> 1.2506"));
    s.check(Money::fromString(QStringLiteral("-1.2506")).toString(4) == QStringLiteral("-1.2506"),
            QStringLiteral("HALF_UP s4 -1.2506 -> -1.2506"));
    s.check(Money::fromString(QStringLiteral("7.0004")).toString(4) == QStringLiteral("7.0004"),
            QStringLiteral("HALF_UP s4 7.0004 -> 7.0004"));
    s.check(Money::fromString(QStringLiteral("-7.0004")).toString(4) == QStringLiteral("-7.0004"),
            QStringLiteral("HALF_UP s4 -7.0004 -> -7.0004"));
    s.check(Money::fromString(QStringLiteral("7.0005")).toString(4) == QStringLiteral("7.0005"),
            QStringLiteral("HALF_UP s4 7.0005 -> 7.0005"));
    s.check(Money::fromString(QStringLiteral("-7.0005")).toString(4) == QStringLiteral("-7.0005"),
            QStringLiteral("HALF_UP s4 -7.0005 -> -7.0005"));
    s.check(Money::fromString(QStringLiteral("7.0006")).toString(4) == QStringLiteral("7.0006"),
            QStringLiteral("HALF_UP s4 7.0006 -> 7.0006"));
    s.check(Money::fromString(QStringLiteral("-7.0006")).toString(4) == QStringLiteral("-7.0006"),
            QStringLiteral("HALF_UP s4 -7.0006 -> -7.0006"));
    s.check(Money::fromString(QStringLiteral("7.1234")).toString(4) == QStringLiteral("7.1234"),
            QStringLiteral("HALF_UP s4 7.1234 -> 7.1234"));
    s.check(Money::fromString(QStringLiteral("-7.1234")).toString(4) == QStringLiteral("-7.1234"),
            QStringLiteral("HALF_UP s4 -7.1234 -> -7.1234"));
    s.check(Money::fromString(QStringLiteral("7.1235")).toString(4) == QStringLiteral("7.1235"),
            QStringLiteral("HALF_UP s4 7.1235 -> 7.1235"));
    s.check(Money::fromString(QStringLiteral("-7.1235")).toString(4) == QStringLiteral("-7.1235"),
            QStringLiteral("HALF_UP s4 -7.1235 -> -7.1235"));
    s.check(Money::fromString(QStringLiteral("7.1236")).toString(4) == QStringLiteral("7.1236"),
            QStringLiteral("HALF_UP s4 7.1236 -> 7.1236"));
    s.check(Money::fromString(QStringLiteral("-7.1236")).toString(4) == QStringLiteral("-7.1236"),
            QStringLiteral("HALF_UP s4 -7.1236 -> -7.1236"));
    s.check(Money::fromString(QStringLiteral("7.9994")).toString(4) == QStringLiteral("7.9994"),
            QStringLiteral("HALF_UP s4 7.9994 -> 7.9994"));
    s.check(Money::fromString(QStringLiteral("-7.9994")).toString(4) == QStringLiteral("-7.9994"),
            QStringLiteral("HALF_UP s4 -7.9994 -> -7.9994"));
    s.check(Money::fromString(QStringLiteral("7.9995")).toString(4) == QStringLiteral("7.9995"),
            QStringLiteral("HALF_UP s4 7.9995 -> 7.9995"));
    s.check(Money::fromString(QStringLiteral("-7.9995")).toString(4) == QStringLiteral("-7.9995"),
            QStringLiteral("HALF_UP s4 -7.9995 -> -7.9995"));
    s.check(Money::fromString(QStringLiteral("7.9996")).toString(4) == QStringLiteral("7.9996"),
            QStringLiteral("HALF_UP s4 7.9996 -> 7.9996"));
    s.check(Money::fromString(QStringLiteral("-7.9996")).toString(4) == QStringLiteral("-7.9996"),
            QStringLiteral("HALF_UP s4 -7.9996 -> -7.9996"));
    s.check(Money::fromString(QStringLiteral("7.5554")).toString(4) == QStringLiteral("7.5554"),
            QStringLiteral("HALF_UP s4 7.5554 -> 7.5554"));
    s.check(Money::fromString(QStringLiteral("-7.5554")).toString(4) == QStringLiteral("-7.5554"),
            QStringLiteral("HALF_UP s4 -7.5554 -> -7.5554"));
    s.check(Money::fromString(QStringLiteral("7.5555")).toString(4) == QStringLiteral("7.5555"),
            QStringLiteral("HALF_UP s4 7.5555 -> 7.5555"));
    s.check(Money::fromString(QStringLiteral("-7.5555")).toString(4) == QStringLiteral("-7.5555"),
            QStringLiteral("HALF_UP s4 -7.5555 -> -7.5555"));
    s.check(Money::fromString(QStringLiteral("7.5556")).toString(4) == QStringLiteral("7.5556"),
            QStringLiteral("HALF_UP s4 7.5556 -> 7.5556"));
    s.check(Money::fromString(QStringLiteral("-7.5556")).toString(4) == QStringLiteral("-7.5556"),
            QStringLiteral("HALF_UP s4 -7.5556 -> -7.5556"));
    s.check(Money::fromString(QStringLiteral("7.2504")).toString(4) == QStringLiteral("7.2504"),
            QStringLiteral("HALF_UP s4 7.2504 -> 7.2504"));
    s.check(Money::fromString(QStringLiteral("-7.2504")).toString(4) == QStringLiteral("-7.2504"),
            QStringLiteral("HALF_UP s4 -7.2504 -> -7.2504"));
    s.check(Money::fromString(QStringLiteral("7.2505")).toString(4) == QStringLiteral("7.2505"),
            QStringLiteral("HALF_UP s4 7.2505 -> 7.2505"));
    s.check(Money::fromString(QStringLiteral("-7.2505")).toString(4) == QStringLiteral("-7.2505"),
            QStringLiteral("HALF_UP s4 -7.2505 -> -7.2505"));
    s.check(Money::fromString(QStringLiteral("7.2506")).toString(4) == QStringLiteral("7.2506"),
            QStringLiteral("HALF_UP s4 7.2506 -> 7.2506"));
    s.check(Money::fromString(QStringLiteral("-7.2506")).toString(4) == QStringLiteral("-7.2506"),
            QStringLiteral("HALF_UP s4 -7.2506 -> -7.2506"));
    s.check(Money::fromString(QStringLiteral("42.0004")).toString(4) == QStringLiteral("42.0004"),
            QStringLiteral("HALF_UP s4 42.0004 -> 42.0004"));
    s.check(Money::fromString(QStringLiteral("-42.0004")).toString(4) == QStringLiteral("-42.0004"),
            QStringLiteral("HALF_UP s4 -42.0004 -> -42.0004"));
    s.check(Money::fromString(QStringLiteral("42.0005")).toString(4) == QStringLiteral("42.0005"),
            QStringLiteral("HALF_UP s4 42.0005 -> 42.0005"));
    s.check(Money::fromString(QStringLiteral("-42.0005")).toString(4) == QStringLiteral("-42.0005"),
            QStringLiteral("HALF_UP s4 -42.0005 -> -42.0005"));
    s.check(Money::fromString(QStringLiteral("42.0006")).toString(4) == QStringLiteral("42.0006"),
            QStringLiteral("HALF_UP s4 42.0006 -> 42.0006"));
    s.check(Money::fromString(QStringLiteral("-42.0006")).toString(4) == QStringLiteral("-42.0006"),
            QStringLiteral("HALF_UP s4 -42.0006 -> -42.0006"));
    s.check(Money::fromString(QStringLiteral("42.1234")).toString(4) == QStringLiteral("42.1234"),
            QStringLiteral("HALF_UP s4 42.1234 -> 42.1234"));
    s.check(Money::fromString(QStringLiteral("-42.1234")).toString(4) == QStringLiteral("-42.1234"),
            QStringLiteral("HALF_UP s4 -42.1234 -> -42.1234"));
    s.check(Money::fromString(QStringLiteral("42.1235")).toString(4) == QStringLiteral("42.1235"),
            QStringLiteral("HALF_UP s4 42.1235 -> 42.1235"));
    s.check(Money::fromString(QStringLiteral("-42.1235")).toString(4) == QStringLiteral("-42.1235"),
            QStringLiteral("HALF_UP s4 -42.1235 -> -42.1235"));
    s.check(Money::fromString(QStringLiteral("42.1236")).toString(4) == QStringLiteral("42.1236"),
            QStringLiteral("HALF_UP s4 42.1236 -> 42.1236"));
    s.check(Money::fromString(QStringLiteral("-42.1236")).toString(4) == QStringLiteral("-42.1236"),
            QStringLiteral("HALF_UP s4 -42.1236 -> -42.1236"));
    s.check(Money::fromString(QStringLiteral("42.9994")).toString(4) == QStringLiteral("42.9994"),
            QStringLiteral("HALF_UP s4 42.9994 -> 42.9994"));
    s.check(Money::fromString(QStringLiteral("-42.9994")).toString(4) == QStringLiteral("-42.9994"),
            QStringLiteral("HALF_UP s4 -42.9994 -> -42.9994"));
    s.check(Money::fromString(QStringLiteral("42.9995")).toString(4) == QStringLiteral("42.9995"),
            QStringLiteral("HALF_UP s4 42.9995 -> 42.9995"));
    s.check(Money::fromString(QStringLiteral("-42.9995")).toString(4) == QStringLiteral("-42.9995"),
            QStringLiteral("HALF_UP s4 -42.9995 -> -42.9995"));
    s.check(Money::fromString(QStringLiteral("42.9996")).toString(4) == QStringLiteral("42.9996"),
            QStringLiteral("HALF_UP s4 42.9996 -> 42.9996"));
    s.check(Money::fromString(QStringLiteral("-42.9996")).toString(4) == QStringLiteral("-42.9996"),
            QStringLiteral("HALF_UP s4 -42.9996 -> -42.9996"));
    s.check(Money::fromString(QStringLiteral("42.5554")).toString(4) == QStringLiteral("42.5554"),
            QStringLiteral("HALF_UP s4 42.5554 -> 42.5554"));
    s.check(Money::fromString(QStringLiteral("-42.5554")).toString(4) == QStringLiteral("-42.5554"),
            QStringLiteral("HALF_UP s4 -42.5554 -> -42.5554"));
    s.check(Money::fromString(QStringLiteral("42.5555")).toString(4) == QStringLiteral("42.5555"),
            QStringLiteral("HALF_UP s4 42.5555 -> 42.5555"));
    s.check(Money::fromString(QStringLiteral("-42.5555")).toString(4) == QStringLiteral("-42.5555"),
            QStringLiteral("HALF_UP s4 -42.5555 -> -42.5555"));
    s.check(Money::fromString(QStringLiteral("42.5556")).toString(4) == QStringLiteral("42.5556"),
            QStringLiteral("HALF_UP s4 42.5556 -> 42.5556"));
    s.check(Money::fromString(QStringLiteral("-42.5556")).toString(4) == QStringLiteral("-42.5556"),
            QStringLiteral("HALF_UP s4 -42.5556 -> -42.5556"));
    s.check(Money::fromString(QStringLiteral("42.2504")).toString(4) == QStringLiteral("42.2504"),
            QStringLiteral("HALF_UP s4 42.2504 -> 42.2504"));
    s.check(Money::fromString(QStringLiteral("-42.2504")).toString(4) == QStringLiteral("-42.2504"),
            QStringLiteral("HALF_UP s4 -42.2504 -> -42.2504"));
    s.check(Money::fromString(QStringLiteral("42.2505")).toString(4) == QStringLiteral("42.2505"),
            QStringLiteral("HALF_UP s4 42.2505 -> 42.2505"));
    s.check(Money::fromString(QStringLiteral("-42.2505")).toString(4) == QStringLiteral("-42.2505"),
            QStringLiteral("HALF_UP s4 -42.2505 -> -42.2505"));
    s.check(Money::fromString(QStringLiteral("42.2506")).toString(4) == QStringLiteral("42.2506"),
            QStringLiteral("HALF_UP s4 42.2506 -> 42.2506"));
    s.check(Money::fromString(QStringLiteral("-42.2506")).toString(4) == QStringLiteral("-42.2506"),
            QStringLiteral("HALF_UP s4 -42.2506 -> -42.2506"));
    s.check(Money::fromString(QStringLiteral("100.0004")).toString(4) == QStringLiteral("100.0004"),
            QStringLiteral("HALF_UP s4 100.0004 -> 100.0004"));
    s.check(Money::fromString(QStringLiteral("-100.0004")).toString(4)
                == QStringLiteral("-100.0004"),
            QStringLiteral("HALF_UP s4 -100.0004 -> -100.0004"));
    s.check(Money::fromString(QStringLiteral("100.0005")).toString(4) == QStringLiteral("100.0005"),
            QStringLiteral("HALF_UP s4 100.0005 -> 100.0005"));
    s.check(Money::fromString(QStringLiteral("-100.0005")).toString(4)
                == QStringLiteral("-100.0005"),
            QStringLiteral("HALF_UP s4 -100.0005 -> -100.0005"));
    s.check(Money::fromString(QStringLiteral("100.0006")).toString(4) == QStringLiteral("100.0006"),
            QStringLiteral("HALF_UP s4 100.0006 -> 100.0006"));
    s.check(Money::fromString(QStringLiteral("-100.0006")).toString(4)
                == QStringLiteral("-100.0006"),
            QStringLiteral("HALF_UP s4 -100.0006 -> -100.0006"));
    s.check(Money::fromString(QStringLiteral("100.1234")).toString(4) == QStringLiteral("100.1234"),
            QStringLiteral("HALF_UP s4 100.1234 -> 100.1234"));
    s.check(Money::fromString(QStringLiteral("-100.1234")).toString(4)
                == QStringLiteral("-100.1234"),
            QStringLiteral("HALF_UP s4 -100.1234 -> -100.1234"));
    s.check(Money::fromString(QStringLiteral("100.1235")).toString(4) == QStringLiteral("100.1235"),
            QStringLiteral("HALF_UP s4 100.1235 -> 100.1235"));
    s.check(Money::fromString(QStringLiteral("-100.1235")).toString(4)
                == QStringLiteral("-100.1235"),
            QStringLiteral("HALF_UP s4 -100.1235 -> -100.1235"));
    s.check(Money::fromString(QStringLiteral("100.1236")).toString(4) == QStringLiteral("100.1236"),
            QStringLiteral("HALF_UP s4 100.1236 -> 100.1236"));
    s.check(Money::fromString(QStringLiteral("-100.1236")).toString(4)
                == QStringLiteral("-100.1236"),
            QStringLiteral("HALF_UP s4 -100.1236 -> -100.1236"));
    s.check(Money::fromString(QStringLiteral("100.9994")).toString(4) == QStringLiteral("100.9994"),
            QStringLiteral("HALF_UP s4 100.9994 -> 100.9994"));
    s.check(Money::fromString(QStringLiteral("-100.9994")).toString(4)
                == QStringLiteral("-100.9994"),
            QStringLiteral("HALF_UP s4 -100.9994 -> -100.9994"));
    s.check(Money::fromString(QStringLiteral("100.9995")).toString(4) == QStringLiteral("100.9995"),
            QStringLiteral("HALF_UP s4 100.9995 -> 100.9995"));
    s.check(Money::fromString(QStringLiteral("-100.9995")).toString(4)
                == QStringLiteral("-100.9995"),
            QStringLiteral("HALF_UP s4 -100.9995 -> -100.9995"));
    s.check(Money::fromString(QStringLiteral("100.9996")).toString(4) == QStringLiteral("100.9996"),
            QStringLiteral("HALF_UP s4 100.9996 -> 100.9996"));
    s.check(Money::fromString(QStringLiteral("-100.9996")).toString(4)
                == QStringLiteral("-100.9996"),
            QStringLiteral("HALF_UP s4 -100.9996 -> -100.9996"));
    s.check(Money::fromString(QStringLiteral("100.5554")).toString(4) == QStringLiteral("100.5554"),
            QStringLiteral("HALF_UP s4 100.5554 -> 100.5554"));
    s.check(Money::fromString(QStringLiteral("-100.5554")).toString(4)
                == QStringLiteral("-100.5554"),
            QStringLiteral("HALF_UP s4 -100.5554 -> -100.5554"));
    s.check(Money::fromString(QStringLiteral("100.5555")).toString(4) == QStringLiteral("100.5555"),
            QStringLiteral("HALF_UP s4 100.5555 -> 100.5555"));
    s.check(Money::fromString(QStringLiteral("-100.5555")).toString(4)
                == QStringLiteral("-100.5555"),
            QStringLiteral("HALF_UP s4 -100.5555 -> -100.5555"));
    s.check(Money::fromString(QStringLiteral("100.5556")).toString(4) == QStringLiteral("100.5556"),
            QStringLiteral("HALF_UP s4 100.5556 -> 100.5556"));
    s.check(Money::fromString(QStringLiteral("-100.5556")).toString(4)
                == QStringLiteral("-100.5556"),
            QStringLiteral("HALF_UP s4 -100.5556 -> -100.5556"));
    s.check(Money::fromString(QStringLiteral("100.2504")).toString(4) == QStringLiteral("100.2504"),
            QStringLiteral("HALF_UP s4 100.2504 -> 100.2504"));
    s.check(Money::fromString(QStringLiteral("-100.2504")).toString(4)
                == QStringLiteral("-100.2504"),
            QStringLiteral("HALF_UP s4 -100.2504 -> -100.2504"));
    s.check(Money::fromString(QStringLiteral("100.2505")).toString(4) == QStringLiteral("100.2505"),
            QStringLiteral("HALF_UP s4 100.2505 -> 100.2505"));
    s.check(Money::fromString(QStringLiteral("-100.2505")).toString(4)
                == QStringLiteral("-100.2505"),
            QStringLiteral("HALF_UP s4 -100.2505 -> -100.2505"));
    s.check(Money::fromString(QStringLiteral("100.2506")).toString(4) == QStringLiteral("100.2506"),
            QStringLiteral("HALF_UP s4 100.2506 -> 100.2506"));
    s.check(Money::fromString(QStringLiteral("-100.2506")).toString(4)
                == QStringLiteral("-100.2506"),
            QStringLiteral("HALF_UP s4 -100.2506 -> -100.2506"));
    s.check((Money::fromString(QStringLiteral("1.25")) + Money::fromString(QStringLiteral("2.75")))
                    .toString(2)
                == QStringLiteral("4.00"),
            QStringLiteral("add 1.25+2.75 -> 4.00"));
    s.check(
        (Money::fromString(QStringLiteral("10.005")) + Money::fromString(QStringLiteral("0.005")))
                .toString(2)
            == QStringLiteral("10.01"),
        QStringLiteral("add 10.005+0.005 -> 10.01"));
    s.check(
        (Money::fromString(QStringLiteral("100.50")) + Money::fromString(QStringLiteral("0.50")))
                .toString(2)
            == QStringLiteral("101.00"),
        QStringLiteral("add 100.50+0.50 -> 101.00"));
    s.check(
        (Money::fromString(QStringLiteral("0.0001")) + Money::fromString(QStringLiteral("0.0002")))
                .toString(4)
            == QStringLiteral("0.0003"),
        QStringLiteral("add 0.0001+0.0002 -> 0.0003"));
    s.check((Money::fromString(QStringLiteral("-5.00")) + Money::fromString(QStringLiteral("3.00")))
                    .toString(2)
                == QStringLiteral("-2.00"),
            QStringLiteral("add -5.00+3.00 -> -2.00"));
    s.check(
        (Money::fromString(QStringLiteral("999.99")) + Money::fromString(QStringLiteral("0.01")))
                .toString(2)
            == QStringLiteral("1000.00"),
        QStringLiteral("add 999.99+0.01 -> 1000.00"));
    s.check((Money::fromString(QStringLiteral("1234.5678"))
             + Money::fromString(QStringLiteral("0.0001")))
                    .toString(4)
                == QStringLiteral("1234.5679"),
            QStringLiteral("add 1234.5678+0.0001 -> 1234.5679"));
    s.check((Money::fromString(QStringLiteral("0.1")) + Money::fromString(QStringLiteral("0.2")))
                    .toString(2)
                == QStringLiteral("0.30"),
            QStringLiteral("add 0.1+0.2 -> 0.30"));
    s.check(
        (Money::fromString(QStringLiteral("50.55")) + Money::fromString(QStringLiteral("49.45")))
                .toString(2)
            == QStringLiteral("100.00"),
        QStringLiteral("add 50.55+49.45 -> 100.00"));
    s.check((Money::fromString(QStringLiteral("-1.1111"))
             + Money::fromString(QStringLiteral("-2.2222")))
                    .toString(4)
                == QStringLiteral("-3.3333"),
            QStringLiteral("add -1.1111+-2.2222 -> -3.3333"));
    s.check((Money::fromString(QStringLiteral("4.00")) - Money::fromString(QStringLiteral("2.75")))
                    .toString(4)
                == QStringLiteral("1.2500"),
            QStringLiteral("sub 4.00-2.75 -> 1.2500"));
    s.check(
        (Money::fromString(QStringLiteral("10.01")) - Money::fromString(QStringLiteral("0.005")))
                .toString(4)
            == QStringLiteral("10.0050"),
        QStringLiteral("sub 10.01-0.005 -> 10.0050"));
    s.check(
        (Money::fromString(QStringLiteral("0.0003")) - Money::fromString(QStringLiteral("0.0002")))
                .toString(4)
            == QStringLiteral("0.0001"),
        QStringLiteral("sub 0.0003-0.0002 -> 0.0001"));
    s.check((Money::fromString(QStringLiteral("-2.00")) - Money::fromString(QStringLiteral("3.00")))
                    .toString(4)
                == QStringLiteral("-5.0000"),
            QStringLiteral("sub -2.00-3.00 -> -5.0000"));
    s.check(
        (Money::fromString(QStringLiteral("1000.00")) - Money::fromString(QStringLiteral("0.01")))
                .toString(4)
            == QStringLiteral("999.9900"),
        QStringLiteral("sub 1000.00-0.01 -> 999.9900"));
    s.check((Money::fromString(QStringLiteral("0.30")) - Money::fromString(QStringLiteral("0.2")))
                    .toString(4)
                == QStringLiteral("0.1000"),
            QStringLiteral("sub 0.30-0.2 -> 0.1000"));
    s.check(
        (Money::fromString(QStringLiteral("100.00")) - Money::fromString(QStringLiteral("49.45")))
                .toString(4)
            == QStringLiteral("50.5500"),
        QStringLiteral("sub 100.00-49.45 -> 50.5500"));
    s.check(((Money::fromString(QStringLiteral("1.11")) + Money::fromString(QStringLiteral("2.22")))
             + Money::fromString(QStringLiteral("3.33")))
                    .unitsScale4()
                == (Money::fromString(QStringLiteral("1.11"))
                    + (Money::fromString(QStringLiteral("2.22"))
                       + Money::fromString(QStringLiteral("3.33"))))
                       .unitsScale4(),
            QStringLiteral("add associativity 1.11,2.22,3.33"));
    s.check((Money::fromString(QStringLiteral("1.11")) + Money::fromString(QStringLiteral("2.22")))
                    .unitsScale4()
                == (Money::fromString(QStringLiteral("2.22"))
                    + Money::fromString(QStringLiteral("1.11")))
                       .unitsScale4(),
            QStringLiteral("add commutativity 1.11,2.22"));
    s.check(
        ((Money::fromString(QStringLiteral("0.0001")) + Money::fromString(QStringLiteral("0.0002")))
         + Money::fromString(QStringLiteral("0.0003")))
                .unitsScale4()
            == (Money::fromString(QStringLiteral("0.0001"))
                + (Money::fromString(QStringLiteral("0.0002"))
                   + Money::fromString(QStringLiteral("0.0003"))))
                   .unitsScale4(),
        QStringLiteral("add associativity 0.0001,0.0002,0.0003"));
    s.check(
        (Money::fromString(QStringLiteral("0.0001")) + Money::fromString(QStringLiteral("0.0002")))
                .unitsScale4()
            == (Money::fromString(QStringLiteral("0.0002"))
                + Money::fromString(QStringLiteral("0.0001")))
                   .unitsScale4(),
        QStringLiteral("add commutativity 0.0001,0.0002"));
    s.check(((Money::fromString(QStringLiteral("-5.5")) + Money::fromString(QStringLiteral("2.2")))
             + Money::fromString(QStringLiteral("1.1")))
                    .unitsScale4()
                == (Money::fromString(QStringLiteral("-5.5"))
                    + (Money::fromString(QStringLiteral("2.2"))
                       + Money::fromString(QStringLiteral("1.1"))))
                       .unitsScale4(),
            QStringLiteral("add associativity -5.5,2.2,1.1"));
    s.check((Money::fromString(QStringLiteral("-5.5")) + Money::fromString(QStringLiteral("2.2")))
                    .unitsScale4()
                == (Money::fromString(QStringLiteral("2.2"))
                    + Money::fromString(QStringLiteral("-5.5")))
                       .unitsScale4(),
            QStringLiteral("add commutativity -5.5,2.2"));
    s.check(
        ((Money::fromString(QStringLiteral("100.25")) + Money::fromString(QStringLiteral("50.75")))
         + Money::fromString(QStringLiteral("0.01")))
                .unitsScale4()
            == (Money::fromString(QStringLiteral("100.25"))
                + (Money::fromString(QStringLiteral("50.75"))
                   + Money::fromString(QStringLiteral("0.01"))))
                   .unitsScale4(),
        QStringLiteral("add associativity 100.25,50.75,0.01"));
    s.check(
        (Money::fromString(QStringLiteral("100.25")) + Money::fromString(QStringLiteral("50.75")))
                .unitsScale4()
            == (Money::fromString(QStringLiteral("50.75"))
                + Money::fromString(QStringLiteral("100.25")))
                   .unitsScale4(),
        QStringLiteral("add commutativity 100.25,50.75"));
    s.check(((Money::fromString(QStringLiteral("999.9999"))
              + Money::fromString(QStringLiteral("0.0001")))
             + Money::fromString(QStringLiteral("0.5")))
                    .unitsScale4()
                == (Money::fromString(QStringLiteral("999.9999"))
                    + (Money::fromString(QStringLiteral("0.0001"))
                       + Money::fromString(QStringLiteral("0.5"))))
                       .unitsScale4(),
            QStringLiteral("add associativity 999.9999,0.0001,0.5"));
    s.check((Money::fromString(QStringLiteral("999.9999"))
             + Money::fromString(QStringLiteral("0.0001")))
                    .unitsScale4()
                == (Money::fromString(QStringLiteral("0.0001"))
                    + Money::fromString(QStringLiteral("999.9999")))
                       .unitsScale4(),
            QStringLiteral("add commutativity 999.9999,0.0001"));
    s.check(Money::fromString(QStringLiteral("1.25")).mul(4).toString(4)
                == QStringLiteral("5.0000"),
            QStringLiteral("mul 1.25 x 4 -> 5.0000"));
    s.check(Money::fromString(QStringLiteral("0.0001")).mul(10000).toString(4)
                == QStringLiteral("1.0000"),
            QStringLiteral("mul 0.0001 x 10000 -> 1.0000"));
    s.check(Money::fromString(QStringLiteral("2.50")).mul(3).toString(4)
                == QStringLiteral("7.5000"),
            QStringLiteral("mul 2.50 x 3 -> 7.5000"));
    s.check(Money::fromString(QStringLiteral("10.00")).mul(7).toString(4)
                == QStringLiteral("70.0000"),
            QStringLiteral("mul 10.00 x 7 -> 70.0000"));
    s.check(Money::fromString(QStringLiteral("-1.5")).mul(4).toString(4)
                == QStringLiteral("-6.0000"),
            QStringLiteral("mul -1.5 x 4 -> -6.0000"));
    s.check(Money::fromString(QStringLiteral("0.99")).mul(100).toString(4)
                == QStringLiteral("99.0000"),
            QStringLiteral("mul 0.99 x 100 -> 99.0000"));
    s.check(Money::fromString(QStringLiteral("3.3333")).mul(3).toString(4)
                == QStringLiteral("9.9999"),
            QStringLiteral("mul 3.3333 x 3 -> 9.9999"));
    s.check(Money::fromString(QStringLiteral("100.00")).mul(12).toString(4)
                == QStringLiteral("1200.0000"),
            QStringLiteral("mul 100.00 x 12 -> 1200.0000"));
    s.check(Money::fromString(QStringLiteral("0.05")).mul(20).toString(4)
                == QStringLiteral("1.0000"),
            QStringLiteral("mul 0.05 x 20 -> 1.0000"));
    s.check(Money::fromString(QStringLiteral("-2.5")).mul(-2).toString(4)
                == QStringLiteral("5.0000"),
            QStringLiteral("mul -2.5 x -2 -> 5.0000"));
    s.check(Money::fromString(QStringLiteral("1.11")).mul(9).toString(4)
                == QStringLiteral("9.9900"),
            QStringLiteral("mul 1.11 x 9 -> 9.9900"));
    s.check(Money::fromString(QStringLiteral("0.0003")).mul(7).toString(4)
                == QStringLiteral("0.0021"),
            QStringLiteral("mul 0.0003 x 7 -> 0.0021"));
    s.check(Money::fromString(QStringLiteral("250.7500")).mul(4).toString(4)
                == QStringLiteral("1003.0000"),
            QStringLiteral("mul 250.7500 x 4 -> 1003.0000"));
    s.check(Money::fromString(QStringLiteral("7.654")).mul(6).toString(4)
                == QStringLiteral("45.9240"),
            QStringLiteral("mul 7.654 x 6 -> 45.9240"));
    s.check(Money::fromString(QStringLiteral("0")).mul(999).toString(4) == QStringLiteral("0.0000"),
            QStringLiteral("mul 0 x 999 -> 0.0000"));
    s.check(Money::fromString(QStringLiteral("1.23")).mul(5).unitsScale4()
                == (Money::fromString(QStringLiteral("1.23")).mul(2)
                    + Money::fromString(QStringLiteral("1.23")).mul(3))
                       .unitsScale4(),
            QStringLiteral("mul distributive 1.23 x 5"));
    s.check(Money::fromString(QStringLiteral("0.0007")).mul(3).unitsScale4()
                == (Money::fromString(QStringLiteral("0.0007")).mul(1)
                    + Money::fromString(QStringLiteral("0.0007")).mul(2))
                       .unitsScale4(),
            QStringLiteral("mul distributive 0.0007 x 3"));
    s.check(Money::fromString(QStringLiteral("-2.5")).mul(4).unitsScale4()
                == (Money::fromString(QStringLiteral("-2.5")).mul(2)
                    + Money::fromString(QStringLiteral("-2.5")).mul(2))
                       .unitsScale4(),
            QStringLiteral("mul distributive -2.5 x 4"));
    s.check(Money::fromString(QStringLiteral("10.0000")).divByInt(3).toString(4)
                == QStringLiteral("3.3333"),
            QStringLiteral("divByInt 10.0000 / 3 -> 3.3333"));
    s.check(Money::fromString(QStringLiteral("10.0000")).divByInt(4).toString(4)
                == QStringLiteral("2.5000"),
            QStringLiteral("divByInt 10.0000 / 4 -> 2.5000"));
    s.check(Money::fromString(QStringLiteral("1.0000")).divByInt(3).toString(4)
                == QStringLiteral("0.3333"),
            QStringLiteral("divByInt 1.0000 / 3 -> 0.3333"));
    s.check(Money::fromString(QStringLiteral("1.0000")).divByInt(6).toString(4)
                == QStringLiteral("0.1667"),
            QStringLiteral("divByInt 1.0000 / 6 -> 0.1667"));
    s.check(Money::fromString(QStringLiteral("1.0000")).divByInt(7).toString(4)
                == QStringLiteral("0.1429"),
            QStringLiteral("divByInt 1.0000 / 7 -> 0.1429"));
    s.check(Money::fromString(QStringLiteral("100.0000")).divByInt(8).toString(4)
                == QStringLiteral("12.5000"),
            QStringLiteral("divByInt 100.0000 / 8 -> 12.5000"));
    s.check(Money::fromString(QStringLiteral("0.0001")).divByInt(2).toString(4)
                == QStringLiteral("0.0001"),
            QStringLiteral("divByInt 0.0001 / 2 -> 0.0001"));
    s.check(Money::fromString(QStringLiteral("0.0003")).divByInt(2).toString(4)
                == QStringLiteral("0.0002"),
            QStringLiteral("divByInt 0.0003 / 2 -> 0.0002"));
    s.check(Money::fromString(QStringLiteral("5.0000")).divByInt(2).toString(4)
                == QStringLiteral("2.5000"),
            QStringLiteral("divByInt 5.0000 / 2 -> 2.5000"));
    s.check(Money::fromString(QStringLiteral("-10.0000")).divByInt(3).toString(4)
                == QStringLiteral("-3.3333"),
            QStringLiteral("divByInt -10.0000 / 3 -> -3.3333"));
    s.check(Money::fromString(QStringLiteral("7.0000")).divByInt(2).toString(4)
                == QStringLiteral("3.5000"),
            QStringLiteral("divByInt 7.0000 / 2 -> 3.5000"));
    s.check(Money::fromString(QStringLiteral("1.0000")).divByInt(8).toString(4)
                == QStringLiteral("0.1250"),
            QStringLiteral("divByInt 1.0000 / 8 -> 0.1250"));
    s.check(Money::fromString(QStringLiteral("9.9999")).divByInt(3).toString(4)
                == QStringLiteral("3.3333"),
            QStringLiteral("divByInt 9.9999 / 3 -> 3.3333"));
    s.check(Money::fromString(QStringLiteral("-1.0000")).divByInt(3).toString(4)
                == QStringLiteral("-0.3333"),
            QStringLiteral("divByInt -1.0000 / 3 -> -0.3333"));
    s.check(Money::fromString(QStringLiteral("2.5000")).divByInt(4).toString(4)
                == QStringLiteral("0.6250"),
            QStringLiteral("divByInt 2.5000 / 4 -> 0.6250"));
    s.check(Money::fromString(QStringLiteral("1000.0000")).divByInt(7).toString(4)
                == QStringLiteral("142.8571"),
            QStringLiteral("divByInt 1000.0000 / 7 -> 142.8571"));
    s.check(Money::fromString(QStringLiteral("0.0005")).divByInt(2).toString(4)
                == QStringLiteral("0.0003"),
            QStringLiteral("divByInt 0.0005 / 2 -> 0.0003"));
    s.check(Money::fromString(QStringLiteral("0.0006")).divByInt(4).toString(4)
                == QStringLiteral("0.0002"),
            QStringLiteral("divByInt 0.0006 / 4 -> 0.0002"));
    s.check(Money::fromString(QStringLiteral("3.3333")).divByInt(3).toString(4)
                == QStringLiteral("1.1111"),
            QStringLiteral("divByInt 3.3333 / 3 -> 1.1111"));
    s.check(Money::fromString(QStringLiteral("-7.0000")).divByInt(2).toString(4)
                == QStringLiteral("-3.5000"),
            QStringLiteral("divByInt -7.0000 / 2 -> -3.5000"));
    s.check(Money::fromString(QStringLiteral("10.0000")).divByInt(0).isZero(),
            QStringLiteral("divByInt by 0 -> zero"));
    s.check(Money::fromString(QStringLiteral("10.0000")).divByInt(-1).isZero(),
            QStringLiteral("divByInt by -1 -> zero"));
    s.check(Money::fromString(QStringLiteral("10.0000")).divByInt(-5).isZero(),
            QStringLiteral("divByInt by -5 -> zero"));
    s.check(Money::fromString(QStringLiteral("1234567.89")).fmt(2)
                == QStringLiteral("1,234,567.89"),
            QStringLiteral("fmt 1234567.89 s2 -> 1,234,567.89"));
    s.check(Money::fromString(QStringLiteral("1234567.8900")).fmt(4)
                == QStringLiteral("1,234,567.8900"),
            QStringLiteral("fmt 1234567.8900 s4 -> 1,234,567.8900"));
    s.check(Money::fromString(QStringLiteral("-1234567.89")).fmt(2)
                == QStringLiteral("-1,234,567.89"),
            QStringLiteral("fmt -1234567.89 s2 -> -1,234,567.89"));
    s.check(Money::fromString(QStringLiteral("1000.00")).fmt(2) == QStringLiteral("1,000.00"),
            QStringLiteral("fmt 1000.00 s2 -> 1,000.00"));
    s.check(Money::fromString(QStringLiteral("999.99")).fmt(2) == QStringLiteral("999.99"),
            QStringLiteral("fmt 999.99 s2 -> 999.99"));
    s.check(Money::fromString(QStringLiteral("0.50")).fmt(2) == QStringLiteral("0.50"),
            QStringLiteral("fmt 0.50 s2 -> 0.50"));
    s.check(Money::fromString(QStringLiteral("100.00")).fmt(2) == QStringLiteral("100.00"),
            QStringLiteral("fmt 100.00 s2 -> 100.00"));
    s.check(Money::fromString(QStringLiteral("12345.67")).fmt(2) == QStringLiteral("12,345.67"),
            QStringLiteral("fmt 12345.67 s2 -> 12,345.67"));
    s.check(Money::fromString(QStringLiteral("-12345.67")).fmt(2) == QStringLiteral("-12,345.67"),
            QStringLiteral("fmt -12345.67 s2 -> -12,345.67"));
    s.check(Money::fromString(QStringLiteral("1.00")).fmt(2) == QStringLiteral("1.00"),
            QStringLiteral("fmt 1.00 s2 -> 1.00"));
    s.check(Money::fromString(QStringLiteral("10.00")).fmt(2) == QStringLiteral("10.00"),
            QStringLiteral("fmt 10.00 s2 -> 10.00"));
    s.check(Money::fromString(QStringLiteral("123456789.1234")).fmt(4)
                == QStringLiteral("123,456,789.1234"),
            QStringLiteral("fmt 123456789.1234 s4 -> 123,456,789.1234"));
    s.check(Money::fromString(QStringLiteral("-0.99")).fmt(2) == QStringLiteral("-0.99"),
            QStringLiteral("fmt -0.99 s2 -> -0.99"));
    s.check(Money::fromString(QStringLiteral("1000000.00")).fmt(2)
                == QStringLiteral("1,000,000.00"),
            QStringLiteral("fmt 1000000.00 s2 -> 1,000,000.00"));
    // display() is the single source of truth for the "PKR" prefix: it must equal "PKR " + fmt().
    s.check(Money::fromString(QStringLiteral("1234567.89")).display()
                == QStringLiteral("PKR 1,234,567.89"),
            QStringLiteral("display 1234567.89 -> PKR 1,234,567.89"));
    s.check(Money::fromString(QStringLiteral("0")).display() == QStringLiteral("PKR 0.00"),
            QStringLiteral("display 0 -> PKR 0.00"));
    s.check(Money::fromString(QStringLiteral("-12345.67")).display()
                == QStringLiteral("PKR -12,345.67"),
            QStringLiteral("display -12345.67 -> PKR -12,345.67"));
    s.check(Money::fromString(QStringLiteral("1234.5678")).display(Money::ScaleCost)
                == QStringLiteral("PKR 1,234.5678"),
            QStringLiteral("display s4 1234.5678 -> PKR 1,234.5678"));
    s.check(Money::fromString(QStringLiteral("42.50")).display()
                == QStringLiteral("PKR %1").arg(Money::fromString(QStringLiteral("42.50")).fmt()),
            QStringLiteral("display == 'PKR ' + fmt()"));
    s.check(
        Money::fromString(QStringLiteral("1.00")).compare(Money::fromString(QStringLiteral("2.00")))
            == -1,
        QStringLiteral("compare 1.00 vs 2.00 = -1"));
    s.check(
        Money::fromString(QStringLiteral("2.00")).compare(Money::fromString(QStringLiteral("1.00")))
            == 1,
        QStringLiteral("compare 2.00 vs 1.00 = 1"));
    s.check(
        Money::fromString(QStringLiteral("1.00")).compare(Money::fromString(QStringLiteral("1.00")))
            == 0,
        QStringLiteral("compare 1.00 vs 1.00 = 0"));
    s.check(Money::fromString(QStringLiteral("-1.00"))
                    .compare(Money::fromString(QStringLiteral("1.00")))
                == -1,
            QStringLiteral("compare -1.00 vs 1.00 = -1"));
    s.check(Money::fromString(QStringLiteral("-2.00"))
                    .compare(Money::fromString(QStringLiteral("-1.00")))
                == -1,
            QStringLiteral("compare -2.00 vs -1.00 = -1"));
    s.check(Money::fromString(QStringLiteral("0.0001"))
                    .compare(Money::fromString(QStringLiteral("0.0002")))
                == -1,
            QStringLiteral("compare 0.0001 vs 0.0002 = -1"));
    s.check(Money::fromString(QStringLiteral("100.00"))
                    .compare(Money::fromString(QStringLiteral("99.99")))
                == 1,
            QStringLiteral("compare 100.00 vs 99.99 = 1"));
    s.check(
        Money::fromString(QStringLiteral("0.00")).compare(Money::fromString(QStringLiteral("0.00")))
            == 0,
        QStringLiteral("compare 0.00 vs 0.00 = 0"));
    s.check(Money::fromString(QStringLiteral("-0.0001"))
                    .compare(Money::fromString(QStringLiteral("0.0000")))
                == -1,
            QStringLiteral("compare -0.0001 vs 0.0000 = -1"));
    s.check(Money::fromString(QStringLiteral("1234.5678"))
                    .compare(Money::fromString(QStringLiteral("1234.5679")))
                == -1,
            QStringLiteral("compare 1234.5678 vs 1234.5679 = -1"));
    s.check(
        Money::fromString(QStringLiteral("5.5")).compare(Money::fromString(QStringLiteral("5.50")))
            == 0,
        QStringLiteral("compare 5.5 vs 5.50 = 0"));
    s.check(Money::fromString(QStringLiteral("0")).isZero(), QStringLiteral("isZero 0"));
    s.check(Money::fromString(QStringLiteral("0.00")).isZero(), QStringLiteral("isZero 0.00"));
    s.check(Money::fromString(QStringLiteral("0.0000")).isZero(), QStringLiteral("isZero 0.0000"));
    s.check(Money::fromString(QStringLiteral("-0")).isZero(), QStringLiteral("isZero -0"));
    s.check(Money::fromString(QStringLiteral("0.0")).isZero(), QStringLiteral("isZero 0.0"));
    s.check(Money::fromString(QStringLiteral("000.000")).isZero(),
            QStringLiteral("isZero 000.000"));
    s.check(!Money::fromString(QStringLiteral("0.0001")).isZero(),
            QStringLiteral("not zero 0.0001"));
    s.check(!Money::fromString(QStringLiteral("-0.0001")).isZero(),
            QStringLiteral("not zero -0.0001"));
    s.check(!Money::fromString(QStringLiteral("1")).isZero(), QStringLiteral("not zero 1"));
    s.check(!Money::fromString(QStringLiteral("100.00")).isZero(),
            QStringLiteral("not zero 100.00"));
    s.check(Money::fromString(QStringLiteral("-0.0001")).isNegative(),
            QStringLiteral("isNegative -0.0001"));
    s.check(Money::fromString(QStringLiteral("-1")).isNegative(), QStringLiteral("isNegative -1"));
    s.check(Money::fromString(QStringLiteral("-100.00")).isNegative(),
            QStringLiteral("isNegative -100.00"));
    s.check(Money::fromString(QStringLiteral("-0.5")).isNegative(),
            QStringLiteral("isNegative -0.5"));
    s.check(!Money::fromString(QStringLiteral("0")).isNegative(), QStringLiteral("not negative 0"));
    s.check(!Money::fromString(QStringLiteral("0.0001")).isNegative(),
            QStringLiteral("not negative 0.0001"));
    s.check(!Money::fromString(QStringLiteral("1")).isNegative(), QStringLiteral("not negative 1"));
    s.check(!Money::fromString(QStringLiteral("100.00")).isNegative(),
            QStringLiteral("not negative 100.00"));
    s.check(Money::fromString(QStringLiteral("99999999.9999")).toString(4)
                == QStringLiteral("99999999.9999"),
            QStringLiteral("large s4 99999999.9999 -> 99999999.9999"));
    s.check(Money::fromString(QStringLiteral("99999999.9999")).fmt(4)
                == QStringLiteral("99,999,999.9999"),
            QStringLiteral("large fmt 99999999.9999 -> 99,999,999.9999"));
    s.check(Money::fromString(QStringLiteral("-99999999.9999")).toString(4)
                == QStringLiteral("-99999999.9999"),
            QStringLiteral("large s4 -99999999.9999 -> -99999999.9999"));
    s.check(Money::fromString(QStringLiteral("-99999999.9999")).fmt(4)
                == QStringLiteral("-99,999,999.9999"),
            QStringLiteral("large fmt -99999999.9999 -> -99,999,999.9999"));
    s.check(Money::fromString(QStringLiteral("12345678.9012")).toString(4)
                == QStringLiteral("12345678.9012"),
            QStringLiteral("large s4 12345678.9012 -> 12345678.9012"));
    s.check(Money::fromString(QStringLiteral("12345678.9012")).fmt(4)
                == QStringLiteral("12,345,678.9012"),
            QStringLiteral("large fmt 12345678.9012 -> 12,345,678.9012"));
    s.check(Money::fromString(QStringLiteral("99999999.9998")).toString(4)
                == QStringLiteral("99999999.9998"),
            QStringLiteral("large s4 99999999.9998 -> 99999999.9998"));
    s.check(Money::fromString(QStringLiteral("99999999.9998")).fmt(4)
                == QStringLiteral("99,999,999.9998"),
            QStringLiteral("large fmt 99999999.9998 -> 99,999,999.9998"));
    s.check(Money::fromString(QStringLiteral("10000000.0000")).toString(4)
                == QStringLiteral("10000000.0000"),
            QStringLiteral("large s4 10000000.0000 -> 10000000.0000"));
    s.check(Money::fromString(QStringLiteral("10000000.0000")).fmt(4)
                == QStringLiteral("10,000,000.0000"),
            QStringLiteral("large fmt 10000000.0000 -> 10,000,000.0000"));
    s.check(Money::fromString(QStringLiteral("99999999.9999")).toString(4)
                == QStringLiteral("99999999.9999"),
            QStringLiteral("large s4 99999999.9999 -> 99999999.9999"));
    s.check(Money::fromString(QStringLiteral("99999999.9999")).fmt(4)
                == QStringLiteral("99,999,999.9999"),
            QStringLiteral("large fmt 99999999.9999 -> 99,999,999.9999"));
    s.check(Money::fromString(QStringLiteral("-12345678.9012")).toString(4)
                == QStringLiteral("-12345678.9012"),
            QStringLiteral("large s4 -12345678.9012 -> -12345678.9012"));
    s.check(Money::fromString(QStringLiteral("-12345678.9012")).fmt(4)
                == QStringLiteral("-12,345,678.9012"),
            QStringLiteral("large fmt -12345678.9012 -> -12,345,678.9012"));
    s.check(Money::fromString(QStringLiteral("50000000.5000")).toString(4)
                == QStringLiteral("50000000.5000"),
            QStringLiteral("large s4 50000000.5000 -> 50000000.5000"));
    s.check(Money::fromString(QStringLiteral("50000000.5000")).fmt(4)
                == QStringLiteral("50,000,000.5000"),
            QStringLiteral("large fmt 50000000.5000 -> 50,000,000.5000"));
    s.check(Money::fromString(QStringLiteral("1.0000")).unitsScale4() == 10000LL,
            QStringLiteral("unitsScale4 1.0000 == 10000"));
    s.check(Money::fromUnits(10000LL).toString(4) == QStringLiteral("1.0000"),
            QStringLiteral("fromUnits 10000 -> 1.0000"));
    s.check(Money::fromString(QStringLiteral("0.0001")).unitsScale4() == 1LL,
            QStringLiteral("unitsScale4 0.0001 == 1"));
    s.check(Money::fromUnits(1LL).toString(4) == QStringLiteral("0.0001"),
            QStringLiteral("fromUnits 1 -> 0.0001"));
    s.check(Money::fromString(QStringLiteral("-2.5000")).unitsScale4() == -25000LL,
            QStringLiteral("unitsScale4 -2.5000 == -25000"));
    s.check(Money::fromUnits(-25000LL).toString(4) == QStringLiteral("-2.5000"),
            QStringLiteral("fromUnits -25000 -> -2.5000"));
    s.check(Money::fromString(QStringLiteral("100.0000")).unitsScale4() == 1000000LL,
            QStringLiteral("unitsScale4 100.0000 == 1000000"));
    s.check(Money::fromUnits(1000000LL).toString(4) == QStringLiteral("100.0000"),
            QStringLiteral("fromUnits 1000000 -> 100.0000"));
    s.check(Money::fromString(QStringLiteral("0.0005")).unitsScale4() == 5LL,
            QStringLiteral("unitsScale4 0.0005 == 5"));
    s.check(Money::fromUnits(5LL).toString(4) == QStringLiteral("0.0005"),
            QStringLiteral("fromUnits 5 -> 0.0005"));
    s.check(Money::fromString(QStringLiteral("12.3456")).unitsScale4() == 123456LL,
            QStringLiteral("unitsScale4 12.3456 == 123456"));
    s.check(Money::fromUnits(123456LL).toString(4) == QStringLiteral("12.3456"),
            QStringLiteral("fromUnits 123456 -> 12.3456"));
    s.check(Money::fromString(QStringLiteral("")).isZero(), QStringLiteral("invalid '' -> zero"));
    s.check(Money::fromString(QStringLiteral("   ")).isZero(),
            QStringLiteral("invalid '   ' -> zero"));
    s.check(Money::fromString(QStringLiteral("abc")).isZero(),
            QStringLiteral("invalid 'abc' -> zero"));
    s.check(Money::fromString(QStringLiteral("1.2.3")).isZero(),
            QStringLiteral("invalid '1.2.3' -> zero"));
    s.check(Money::fromString(QStringLiteral("--5")).toString(4) == QStringLiteral("5.0000"),
            QStringLiteral("leading '--5' (sign stripped, -5 re-negated) -> 5.0000"));
    s.check(Money::fromString(QStringLiteral("xyz")).isZero(),
            QStringLiteral("invalid 'xyz' -> zero"));

    // ── Overflow guards: a wrap must THROW, never silently produce a wrong
    //    total (PHP bcmath is arbitrary-precision; the int64 port must detect). ──
    {
        bool threw = false;
        try {
            (void)Money::fromString(QStringLiteral("1000000000000000"));
        } // 1e15 * 1e4 > int64
        catch (const std::overflow_error &) {
            threw = true;
        }
        s.check(threw, QStringLiteral("overflow: fromString past int64/1e4 throws"));
    }
    {
        bool threw = false;
        try {
            (void)Money::fromString(QStringLiteral("1000000000")).mul(10000000000LL);
        } catch (const std::overflow_error &) {
            threw = true;
        }
        s.check(threw, QStringLiteral("overflow: mul past int64 throws"));
    }
    {
        bool ok = true;
        try {
            (void)(Money::fromString(QStringLiteral("100000.00"))
                   + Money::fromString(QStringLiteral("50000.00")));
            (void)Money::fromString(QStringLiteral("12.34")).mul(9999);
        } catch (...) {
            ok = false;
        }
        s.check(ok, QStringLiteral("overflow: normal pharmacy-scale arithmetic does NOT throw"));
    }

    // ── Parse edge: a leading-dot number (".5") keeps its fractional part. Pins
    //    the dot<0 branch in fromString (mutation testing found these uncovered). ──
    s.check(
        Money::fromString(QStringLiteral(".5")).compare(Money::fromString(QStringLiteral("0.5")))
            == 0,
        QStringLiteral("parse: \".5\" == 0.5"));
    s.check(Money::fromString(QStringLiteral(".5")).toString() == QStringLiteral("0.50"),
            QStringLiteral("parse: \".5\" -> 0.50 (not 0)"));
    s.check(Money::fromString(QStringLiteral(".25")).toString(2) == QStringLiteral("0.25"),
            QStringLiteral("parse: \".25\" -> 0.25"));

    // ── toString(scale=0): integer rupees, NO trailing dot, HALF_UP rounding.
    //    Pins the `scale > 0` branch (mutation testing found it uncovered). ──
    s.check(Money::fromString(QStringLiteral("5.00")).toString(0) == QStringLiteral("5"),
            QStringLiteral("toString(0): 5.00 -> \"5\" (no trailing dot)"));
    s.check(Money::fromString(QStringLiteral("5.6")).toString(0) == QStringLiteral("6"),
            QStringLiteral("toString(0): 5.6 -> \"6\" (HALF_UP)"));
    s.check(Money::fromString(QStringLiteral("5.4")).toString(0) == QStringLiteral("5"),
            QStringLiteral("toString(0): 5.4 -> \"5\""));

    return s;
}

} // namespace pharmadesk_tests
