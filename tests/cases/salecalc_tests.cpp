#include "framework/TestStats.h"
#include "domain/Money.h"
#include "domain/SaleCalculator.h"

namespace pharmadesk_tests {

TestStats run_salecalc_tests(QSqlDatabase db, qint64 userId)
{
    (void)db;
    (void)userId;
    TestStats s;
    s.module = QStringLiteral("salecalc");

    s.check(SaleCalculator::lineSubtotal(0, QStringLiteral("0.00")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 0 x 0.00 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(0, QStringLiteral("0.50")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 0 x 0.50 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(0, QStringLiteral("1.25")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 0 x 1.25 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(0, QStringLiteral("9.99")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 0 x 9.99 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(0, QStringLiteral("10.00")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 0 x 10.00 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(0, QStringLiteral("100.50")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 0 x 100.50 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(0, QStringLiteral("0.0001")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 0 x 0.0001 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(0, QStringLiteral("3.3333")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 0 x 3.3333 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(0, QStringLiteral("250.7500")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 0 x 250.7500 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(0, QStringLiteral("12.34")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 0 x 12.34 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(1, QStringLiteral("0.00")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 1 x 0.00 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(1, QStringLiteral("0.50")) == QStringLiteral("0.50"),
            QStringLiteral("lineSubtotal 1 x 0.50 -> 0.50"));
    s.check(SaleCalculator::lineSubtotal(1, QStringLiteral("1.25")) == QStringLiteral("1.25"),
            QStringLiteral("lineSubtotal 1 x 1.25 -> 1.25"));
    s.check(SaleCalculator::lineSubtotal(1, QStringLiteral("9.99")) == QStringLiteral("9.99"),
            QStringLiteral("lineSubtotal 1 x 9.99 -> 9.99"));
    s.check(SaleCalculator::lineSubtotal(1, QStringLiteral("10.00")) == QStringLiteral("10.00"),
            QStringLiteral("lineSubtotal 1 x 10.00 -> 10.00"));
    s.check(SaleCalculator::lineSubtotal(1, QStringLiteral("100.50")) == QStringLiteral("100.50"),
            QStringLiteral("lineSubtotal 1 x 100.50 -> 100.50"));
    s.check(SaleCalculator::lineSubtotal(1, QStringLiteral("0.0001")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 1 x 0.0001 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(1, QStringLiteral("3.3333")) == QStringLiteral("3.33"),
            QStringLiteral("lineSubtotal 1 x 3.3333 -> 3.33"));
    s.check(SaleCalculator::lineSubtotal(1, QStringLiteral("250.7500")) == QStringLiteral("250.75"),
            QStringLiteral("lineSubtotal 1 x 250.7500 -> 250.75"));
    s.check(SaleCalculator::lineSubtotal(1, QStringLiteral("12.34")) == QStringLiteral("12.34"),
            QStringLiteral("lineSubtotal 1 x 12.34 -> 12.34"));
    s.check(SaleCalculator::lineSubtotal(2, QStringLiteral("0.00")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 2 x 0.00 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(2, QStringLiteral("0.50")) == QStringLiteral("1.00"),
            QStringLiteral("lineSubtotal 2 x 0.50 -> 1.00"));
    s.check(SaleCalculator::lineSubtotal(2, QStringLiteral("1.25")) == QStringLiteral("2.50"),
            QStringLiteral("lineSubtotal 2 x 1.25 -> 2.50"));
    s.check(SaleCalculator::lineSubtotal(2, QStringLiteral("9.99")) == QStringLiteral("19.98"),
            QStringLiteral("lineSubtotal 2 x 9.99 -> 19.98"));
    s.check(SaleCalculator::lineSubtotal(2, QStringLiteral("10.00")) == QStringLiteral("20.00"),
            QStringLiteral("lineSubtotal 2 x 10.00 -> 20.00"));
    s.check(SaleCalculator::lineSubtotal(2, QStringLiteral("100.50")) == QStringLiteral("201.00"),
            QStringLiteral("lineSubtotal 2 x 100.50 -> 201.00"));
    s.check(SaleCalculator::lineSubtotal(2, QStringLiteral("0.0001")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 2 x 0.0001 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(2, QStringLiteral("3.3333")) == QStringLiteral("6.67"),
            QStringLiteral("lineSubtotal 2 x 3.3333 -> 6.67"));
    s.check(SaleCalculator::lineSubtotal(2, QStringLiteral("250.7500")) == QStringLiteral("501.50"),
            QStringLiteral("lineSubtotal 2 x 250.7500 -> 501.50"));
    s.check(SaleCalculator::lineSubtotal(2, QStringLiteral("12.34")) == QStringLiteral("24.68"),
            QStringLiteral("lineSubtotal 2 x 12.34 -> 24.68"));
    s.check(SaleCalculator::lineSubtotal(3, QStringLiteral("0.00")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 3 x 0.00 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(3, QStringLiteral("0.50")) == QStringLiteral("1.50"),
            QStringLiteral("lineSubtotal 3 x 0.50 -> 1.50"));
    s.check(SaleCalculator::lineSubtotal(3, QStringLiteral("1.25")) == QStringLiteral("3.75"),
            QStringLiteral("lineSubtotal 3 x 1.25 -> 3.75"));
    s.check(SaleCalculator::lineSubtotal(3, QStringLiteral("9.99")) == QStringLiteral("29.97"),
            QStringLiteral("lineSubtotal 3 x 9.99 -> 29.97"));
    s.check(SaleCalculator::lineSubtotal(3, QStringLiteral("10.00")) == QStringLiteral("30.00"),
            QStringLiteral("lineSubtotal 3 x 10.00 -> 30.00"));
    s.check(SaleCalculator::lineSubtotal(3, QStringLiteral("100.50")) == QStringLiteral("301.50"),
            QStringLiteral("lineSubtotal 3 x 100.50 -> 301.50"));
    s.check(SaleCalculator::lineSubtotal(3, QStringLiteral("0.0001")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 3 x 0.0001 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(3, QStringLiteral("3.3333")) == QStringLiteral("10.00"),
            QStringLiteral("lineSubtotal 3 x 3.3333 -> 10.00"));
    s.check(SaleCalculator::lineSubtotal(3, QStringLiteral("250.7500")) == QStringLiteral("752.25"),
            QStringLiteral("lineSubtotal 3 x 250.7500 -> 752.25"));
    s.check(SaleCalculator::lineSubtotal(3, QStringLiteral("12.34")) == QStringLiteral("37.02"),
            QStringLiteral("lineSubtotal 3 x 12.34 -> 37.02"));
    s.check(SaleCalculator::lineSubtotal(5, QStringLiteral("0.00")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 5 x 0.00 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(5, QStringLiteral("0.50")) == QStringLiteral("2.50"),
            QStringLiteral("lineSubtotal 5 x 0.50 -> 2.50"));
    s.check(SaleCalculator::lineSubtotal(5, QStringLiteral("1.25")) == QStringLiteral("6.25"),
            QStringLiteral("lineSubtotal 5 x 1.25 -> 6.25"));
    s.check(SaleCalculator::lineSubtotal(5, QStringLiteral("9.99")) == QStringLiteral("49.95"),
            QStringLiteral("lineSubtotal 5 x 9.99 -> 49.95"));
    s.check(SaleCalculator::lineSubtotal(5, QStringLiteral("10.00")) == QStringLiteral("50.00"),
            QStringLiteral("lineSubtotal 5 x 10.00 -> 50.00"));
    s.check(SaleCalculator::lineSubtotal(5, QStringLiteral("100.50")) == QStringLiteral("502.50"),
            QStringLiteral("lineSubtotal 5 x 100.50 -> 502.50"));
    s.check(SaleCalculator::lineSubtotal(5, QStringLiteral("0.0001")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 5 x 0.0001 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(5, QStringLiteral("3.3333")) == QStringLiteral("16.67"),
            QStringLiteral("lineSubtotal 5 x 3.3333 -> 16.67"));
    s.check(SaleCalculator::lineSubtotal(5, QStringLiteral("250.7500"))
                == QStringLiteral("1253.75"),
            QStringLiteral("lineSubtotal 5 x 250.7500 -> 1253.75"));
    s.check(SaleCalculator::lineSubtotal(5, QStringLiteral("12.34")) == QStringLiteral("61.70"),
            QStringLiteral("lineSubtotal 5 x 12.34 -> 61.70"));
    s.check(SaleCalculator::lineSubtotal(10, QStringLiteral("0.00")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 10 x 0.00 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(10, QStringLiteral("0.50")) == QStringLiteral("5.00"),
            QStringLiteral("lineSubtotal 10 x 0.50 -> 5.00"));
    s.check(SaleCalculator::lineSubtotal(10, QStringLiteral("1.25")) == QStringLiteral("12.50"),
            QStringLiteral("lineSubtotal 10 x 1.25 -> 12.50"));
    s.check(SaleCalculator::lineSubtotal(10, QStringLiteral("9.99")) == QStringLiteral("99.90"),
            QStringLiteral("lineSubtotal 10 x 9.99 -> 99.90"));
    s.check(SaleCalculator::lineSubtotal(10, QStringLiteral("10.00")) == QStringLiteral("100.00"),
            QStringLiteral("lineSubtotal 10 x 10.00 -> 100.00"));
    s.check(SaleCalculator::lineSubtotal(10, QStringLiteral("100.50")) == QStringLiteral("1005.00"),
            QStringLiteral("lineSubtotal 10 x 100.50 -> 1005.00"));
    s.check(SaleCalculator::lineSubtotal(10, QStringLiteral("0.0001")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 10 x 0.0001 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(10, QStringLiteral("3.3333")) == QStringLiteral("33.33"),
            QStringLiteral("lineSubtotal 10 x 3.3333 -> 33.33"));
    s.check(SaleCalculator::lineSubtotal(10, QStringLiteral("250.7500"))
                == QStringLiteral("2507.50"),
            QStringLiteral("lineSubtotal 10 x 250.7500 -> 2507.50"));
    s.check(SaleCalculator::lineSubtotal(10, QStringLiteral("12.34")) == QStringLiteral("123.40"),
            QStringLiteral("lineSubtotal 10 x 12.34 -> 123.40"));
    s.check(SaleCalculator::lineSubtotal(12, QStringLiteral("0.00")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 12 x 0.00 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(12, QStringLiteral("0.50")) == QStringLiteral("6.00"),
            QStringLiteral("lineSubtotal 12 x 0.50 -> 6.00"));
    s.check(SaleCalculator::lineSubtotal(12, QStringLiteral("1.25")) == QStringLiteral("15.00"),
            QStringLiteral("lineSubtotal 12 x 1.25 -> 15.00"));
    s.check(SaleCalculator::lineSubtotal(12, QStringLiteral("9.99")) == QStringLiteral("119.88"),
            QStringLiteral("lineSubtotal 12 x 9.99 -> 119.88"));
    s.check(SaleCalculator::lineSubtotal(12, QStringLiteral("10.00")) == QStringLiteral("120.00"),
            QStringLiteral("lineSubtotal 12 x 10.00 -> 120.00"));
    s.check(SaleCalculator::lineSubtotal(12, QStringLiteral("100.50")) == QStringLiteral("1206.00"),
            QStringLiteral("lineSubtotal 12 x 100.50 -> 1206.00"));
    s.check(SaleCalculator::lineSubtotal(12, QStringLiteral("0.0001")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 12 x 0.0001 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(12, QStringLiteral("3.3333")) == QStringLiteral("40.00"),
            QStringLiteral("lineSubtotal 12 x 3.3333 -> 40.00"));
    s.check(SaleCalculator::lineSubtotal(12, QStringLiteral("250.7500"))
                == QStringLiteral("3009.00"),
            QStringLiteral("lineSubtotal 12 x 250.7500 -> 3009.00"));
    s.check(SaleCalculator::lineSubtotal(12, QStringLiteral("12.34")) == QStringLiteral("148.08"),
            QStringLiteral("lineSubtotal 12 x 12.34 -> 148.08"));
    s.check(SaleCalculator::lineSubtotal(100, QStringLiteral("0.00")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 100 x 0.00 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(100, QStringLiteral("0.50")) == QStringLiteral("50.00"),
            QStringLiteral("lineSubtotal 100 x 0.50 -> 50.00"));
    s.check(SaleCalculator::lineSubtotal(100, QStringLiteral("1.25")) == QStringLiteral("125.00"),
            QStringLiteral("lineSubtotal 100 x 1.25 -> 125.00"));
    s.check(SaleCalculator::lineSubtotal(100, QStringLiteral("9.99")) == QStringLiteral("999.00"),
            QStringLiteral("lineSubtotal 100 x 9.99 -> 999.00"));
    s.check(SaleCalculator::lineSubtotal(100, QStringLiteral("10.00")) == QStringLiteral("1000.00"),
            QStringLiteral("lineSubtotal 100 x 10.00 -> 1000.00"));
    s.check(SaleCalculator::lineSubtotal(100, QStringLiteral("100.50"))
                == QStringLiteral("10050.00"),
            QStringLiteral("lineSubtotal 100 x 100.50 -> 10050.00"));
    s.check(SaleCalculator::lineSubtotal(100, QStringLiteral("0.0001")) == QStringLiteral("0.01"),
            QStringLiteral("lineSubtotal 100 x 0.0001 -> 0.01"));
    s.check(SaleCalculator::lineSubtotal(100, QStringLiteral("3.3333")) == QStringLiteral("333.33"),
            QStringLiteral("lineSubtotal 100 x 3.3333 -> 333.33"));
    s.check(SaleCalculator::lineSubtotal(100, QStringLiteral("250.7500"))
                == QStringLiteral("25075.00"),
            QStringLiteral("lineSubtotal 100 x 250.7500 -> 25075.00"));
    s.check(SaleCalculator::lineSubtotal(100, QStringLiteral("12.34")) == QStringLiteral("1234.00"),
            QStringLiteral("lineSubtotal 100 x 12.34 -> 1234.00"));
    s.check(SaleCalculator::lineSubtotal(7, QStringLiteral("0.00")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 7 x 0.00 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(7, QStringLiteral("0.50")) == QStringLiteral("3.50"),
            QStringLiteral("lineSubtotal 7 x 0.50 -> 3.50"));
    s.check(SaleCalculator::lineSubtotal(7, QStringLiteral("1.25")) == QStringLiteral("8.75"),
            QStringLiteral("lineSubtotal 7 x 1.25 -> 8.75"));
    s.check(SaleCalculator::lineSubtotal(7, QStringLiteral("9.99")) == QStringLiteral("69.93"),
            QStringLiteral("lineSubtotal 7 x 9.99 -> 69.93"));
    s.check(SaleCalculator::lineSubtotal(7, QStringLiteral("10.00")) == QStringLiteral("70.00"),
            QStringLiteral("lineSubtotal 7 x 10.00 -> 70.00"));
    s.check(SaleCalculator::lineSubtotal(7, QStringLiteral("100.50")) == QStringLiteral("703.50"),
            QStringLiteral("lineSubtotal 7 x 100.50 -> 703.50"));
    s.check(SaleCalculator::lineSubtotal(7, QStringLiteral("0.0001")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 7 x 0.0001 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(7, QStringLiteral("3.3333")) == QStringLiteral("23.33"),
            QStringLiteral("lineSubtotal 7 x 3.3333 -> 23.33"));
    s.check(SaleCalculator::lineSubtotal(7, QStringLiteral("250.7500"))
                == QStringLiteral("1755.25"),
            QStringLiteral("lineSubtotal 7 x 250.7500 -> 1755.25"));
    s.check(SaleCalculator::lineSubtotal(7, QStringLiteral("12.34")) == QStringLiteral("86.38"),
            QStringLiteral("lineSubtotal 7 x 12.34 -> 86.38"));
    s.check(SaleCalculator::lineSubtotal(250, QStringLiteral("0.00")) == QStringLiteral("0.00"),
            QStringLiteral("lineSubtotal 250 x 0.00 -> 0.00"));
    s.check(SaleCalculator::lineSubtotal(250, QStringLiteral("0.50")) == QStringLiteral("125.00"),
            QStringLiteral("lineSubtotal 250 x 0.50 -> 125.00"));
    s.check(SaleCalculator::lineSubtotal(250, QStringLiteral("1.25")) == QStringLiteral("312.50"),
            QStringLiteral("lineSubtotal 250 x 1.25 -> 312.50"));
    s.check(SaleCalculator::lineSubtotal(250, QStringLiteral("9.99")) == QStringLiteral("2497.50"),
            QStringLiteral("lineSubtotal 250 x 9.99 -> 2497.50"));
    s.check(SaleCalculator::lineSubtotal(250, QStringLiteral("10.00")) == QStringLiteral("2500.00"),
            QStringLiteral("lineSubtotal 250 x 10.00 -> 2500.00"));
    s.check(SaleCalculator::lineSubtotal(250, QStringLiteral("100.50"))
                == QStringLiteral("25125.00"),
            QStringLiteral("lineSubtotal 250 x 100.50 -> 25125.00"));
    s.check(SaleCalculator::lineSubtotal(250, QStringLiteral("0.0001")) == QStringLiteral("0.03"),
            QStringLiteral("lineSubtotal 250 x 0.0001 -> 0.03"));
    s.check(SaleCalculator::lineSubtotal(250, QStringLiteral("3.3333")) == QStringLiteral("833.33"),
            QStringLiteral("lineSubtotal 250 x 3.3333 -> 833.33"));
    s.check(SaleCalculator::lineSubtotal(250, QStringLiteral("250.7500"))
                == QStringLiteral("62687.50"),
            QStringLiteral("lineSubtotal 250 x 250.7500 -> 62687.50"));
    s.check(SaleCalculator::lineSubtotal(250, QStringLiteral("12.34")) == QStringLiteral("3085.00"),
            QStringLiteral("lineSubtotal 250 x 12.34 -> 3085.00"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("100.00"), QStringLiteral("10.00"),
                                      QStringLiteral("5.00"))
                == QStringLiteral("95.00"),
            QStringLiteral("lineTotal 100.00-10.00+5.00 -> 95.00"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("100.00"), QStringLiteral("0"),
                                      QStringLiteral("0"))
                == QStringLiteral("100.00"),
            QStringLiteral("lineTotal 100.00-0+0 -> 100.00"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("50.50"), QStringLiteral("5.05"),
                                      QStringLiteral("2.50"))
                == QStringLiteral("47.95"),
            QStringLiteral("lineTotal 50.50-5.05+2.50 -> 47.95"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("9.99"), QStringLiteral("0"),
                                      QStringLiteral("0.50"))
                == QStringLiteral("10.49"),
            QStringLiteral("lineTotal 9.99-0+0.50 -> 10.49"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("1000.00"), QStringLiteral("100.00"),
                                      QStringLiteral("160.00"))
                == QStringLiteral("1060.00"),
            QStringLiteral("lineTotal 1000.00-100.00+160.00 -> 1060.00"));
    s.check(
        SaleCalculator::lineTotal(QStringLiteral("0.00"), QStringLiteral("0"), QStringLiteral("0"))
            == QStringLiteral("0.00"),
        QStringLiteral("lineTotal 0.00-0+0 -> 0.00"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("10.005"), QStringLiteral("0"),
                                      QStringLiteral("0"))
                == QStringLiteral("10.01"),
            QStringLiteral("lineTotal 10.005-0+0 -> 10.01"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("100.00"), QStringLiteral("100.00"),
                                      QStringLiteral("0"))
                == QStringLiteral("0.00"),
            QStringLiteral("lineTotal 100.00-100.00+0 -> 0.00"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("100.00"), QStringLiteral("150.00"),
                                      QStringLiteral("0"))
                == QStringLiteral("-50.00"),
            QStringLiteral("lineTotal 100.00-150.00+0 -> -50.00"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("250.75"), QStringLiteral("25.075"),
                                      QStringLiteral("12.5375"))
                == QStringLiteral("238.21"),
            QStringLiteral("lineTotal 250.75-25.075+12.5375 -> 238.21"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("1234.56"), QStringLiteral("34.56"),
                                      QStringLiteral("0"))
                == QStringLiteral("1200.00"),
            QStringLiteral("lineTotal 1234.56-34.56+0 -> 1200.00"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("99.99"), QStringLiteral("0"),
                                      QStringLiteral("0.01"))
                == QStringLiteral("100.00"),
            QStringLiteral("lineTotal 99.99-0+0.01 -> 100.00"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("500.00"), QStringLiteral("0"),
                                      QStringLiteral("75.00"))
                == QStringLiteral("575.00"),
            QStringLiteral("lineTotal 500.00-0+75.00 -> 575.00"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("12.34"), QStringLiteral("1.23"),
                                      QStringLiteral("0.62"))
                == QStringLiteral("11.73"),
            QStringLiteral("lineTotal 12.34-1.23+0.62 -> 11.73"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("100.00")) == QStringLiteral("100.00"),
            QStringLiteral("lineTotal default 100.00 -> 100.00"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("0.00")) == QStringLiteral("0.00"),
            QStringLiteral("lineTotal default 0.00 -> 0.00"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("999.99")) == QStringLiteral("999.99"),
            QStringLiteral("lineTotal default 999.99 -> 999.99"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("12.345")) == QStringLiteral("12.35"),
            QStringLiteral("lineTotal default 12.345 -> 12.35"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("12.344")) == QStringLiteral("12.34"),
            QStringLiteral("lineTotal default 12.344 -> 12.34"));
    s.check(SaleCalculator::change(QStringLiteral("100.00"), QStringLiteral("100.00"))
                == QStringLiteral("0.00"),
            QStringLiteral("change t100.00 g100.00 -> 0.00"));
    s.check(SaleCalculator::change(QStringLiteral("100.00"), QStringLiteral("99.99"))
                == QStringLiteral("0.01"),
            QStringLiteral("change t100.00 g99.99 -> 0.01"));
    s.check(SaleCalculator::change(QStringLiteral("50.00"), QStringLiteral("100.00"))
                == QStringLiteral("0.00"),
            QStringLiteral("change t50.00 g100.00 -> 0.00"));
    s.check(SaleCalculator::change(QStringLiteral("0.00"), QStringLiteral("0.00"))
                == QStringLiteral("0.00"),
            QStringLiteral("change t0.00 g0.00 -> 0.00"));
    s.check(SaleCalculator::change(QStringLiteral("200.00"), QStringLiteral("123.45"))
                == QStringLiteral("76.55"),
            QStringLiteral("change t200.00 g123.45 -> 76.55"));
    s.check(SaleCalculator::change(QStringLiteral("100.00"), QStringLiteral("0.00"))
                == QStringLiteral("100.00"),
            QStringLiteral("change t100.00 g0.00 -> 100.00"));
    s.check(SaleCalculator::change(QStringLiteral("99.99"), QStringLiteral("100.00"))
                == QStringLiteral("0.00"),
            QStringLiteral("change t99.99 g100.00 -> 0.00"));
    s.check(SaleCalculator::change(QStringLiteral("1000.00"), QStringLiteral("999.99"))
                == QStringLiteral("0.01"),
            QStringLiteral("change t1000.00 g999.99 -> 0.01"));
    s.check(SaleCalculator::change(QStringLiteral("500.50"), QStringLiteral("500.49"))
                == QStringLiteral("0.01"),
            QStringLiteral("change t500.50 g500.49 -> 0.01"));
    s.check(SaleCalculator::change(QStringLiteral("10.00"), QStringLiteral("10.01"))
                == QStringLiteral("0.00"),
            QStringLiteral("change t10.00 g10.01 -> 0.00"));
    s.check(SaleCalculator::change(QStringLiteral("1234.56"), QStringLiteral("234.56"))
                == QStringLiteral("1000.00"),
            QStringLiteral("change t1234.56 g234.56 -> 1000.00"));
    s.check(SaleCalculator::change(QStringLiteral("0.00"), QStringLiteral("50.00"))
                == QStringLiteral("0.00"),
            QStringLiteral("change t0.00 g50.00 -> 0.00"));
    s.check(
        SaleCalculator::lineTotal(QStringLiteral("10.00"), QStringLiteral("0"), QStringLiteral("0"))
            == QStringLiteral("10.00"),
        QStringLiteral("lineTotal grid 10.00-0+0 -> 10.00"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("10.00"), QStringLiteral("0"),
                                      QStringLiteral("0.75"))
                == QStringLiteral("10.75"),
            QStringLiteral("lineTotal grid 10.00-0+0.75 -> 10.75"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("10.00"), QStringLiteral("0"),
                                      QStringLiteral("1.60"))
                == QStringLiteral("11.60"),
            QStringLiteral("lineTotal grid 10.00-0+1.60 -> 11.60"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("10.00"), QStringLiteral("1.50"),
                                      QStringLiteral("0"))
                == QStringLiteral("8.50"),
            QStringLiteral("lineTotal grid 10.00-1.50+0 -> 8.50"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("10.00"), QStringLiteral("1.50"),
                                      QStringLiteral("0.75"))
                == QStringLiteral("9.25"),
            QStringLiteral("lineTotal grid 10.00-1.50+0.75 -> 9.25"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("10.00"), QStringLiteral("1.50"),
                                      QStringLiteral("1.60"))
                == QStringLiteral("10.10"),
            QStringLiteral("lineTotal grid 10.00-1.50+1.60 -> 10.10"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("10.00"), QStringLiteral("5.00"),
                                      QStringLiteral("0"))
                == QStringLiteral("5.00"),
            QStringLiteral("lineTotal grid 10.00-5.00+0 -> 5.00"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("10.00"), QStringLiteral("5.00"),
                                      QStringLiteral("0.75"))
                == QStringLiteral("5.75"),
            QStringLiteral("lineTotal grid 10.00-5.00+0.75 -> 5.75"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("10.00"), QStringLiteral("5.00"),
                                      QStringLiteral("1.60"))
                == QStringLiteral("6.60"),
            QStringLiteral("lineTotal grid 10.00-5.00+1.60 -> 6.60"));
    s.check(
        SaleCalculator::lineTotal(QStringLiteral("25.50"), QStringLiteral("0"), QStringLiteral("0"))
            == QStringLiteral("25.50"),
        QStringLiteral("lineTotal grid 25.50-0+0 -> 25.50"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("25.50"), QStringLiteral("0"),
                                      QStringLiteral("0.75"))
                == QStringLiteral("26.25"),
            QStringLiteral("lineTotal grid 25.50-0+0.75 -> 26.25"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("25.50"), QStringLiteral("0"),
                                      QStringLiteral("1.60"))
                == QStringLiteral("27.10"),
            QStringLiteral("lineTotal grid 25.50-0+1.60 -> 27.10"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("25.50"), QStringLiteral("1.50"),
                                      QStringLiteral("0"))
                == QStringLiteral("24.00"),
            QStringLiteral("lineTotal grid 25.50-1.50+0 -> 24.00"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("25.50"), QStringLiteral("1.50"),
                                      QStringLiteral("0.75"))
                == QStringLiteral("24.75"),
            QStringLiteral("lineTotal grid 25.50-1.50+0.75 -> 24.75"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("25.50"), QStringLiteral("1.50"),
                                      QStringLiteral("1.60"))
                == QStringLiteral("25.60"),
            QStringLiteral("lineTotal grid 25.50-1.50+1.60 -> 25.60"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("25.50"), QStringLiteral("5.00"),
                                      QStringLiteral("0"))
                == QStringLiteral("20.50"),
            QStringLiteral("lineTotal grid 25.50-5.00+0 -> 20.50"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("25.50"), QStringLiteral("5.00"),
                                      QStringLiteral("0.75"))
                == QStringLiteral("21.25"),
            QStringLiteral("lineTotal grid 25.50-5.00+0.75 -> 21.25"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("25.50"), QStringLiteral("5.00"),
                                      QStringLiteral("1.60"))
                == QStringLiteral("22.10"),
            QStringLiteral("lineTotal grid 25.50-5.00+1.60 -> 22.10"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("100.00"), QStringLiteral("0"),
                                      QStringLiteral("0"))
                == QStringLiteral("100.00"),
            QStringLiteral("lineTotal grid 100.00-0+0 -> 100.00"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("100.00"), QStringLiteral("0"),
                                      QStringLiteral("0.75"))
                == QStringLiteral("100.75"),
            QStringLiteral("lineTotal grid 100.00-0+0.75 -> 100.75"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("100.00"), QStringLiteral("0"),
                                      QStringLiteral("1.60"))
                == QStringLiteral("101.60"),
            QStringLiteral("lineTotal grid 100.00-0+1.60 -> 101.60"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("100.00"), QStringLiteral("1.50"),
                                      QStringLiteral("0"))
                == QStringLiteral("98.50"),
            QStringLiteral("lineTotal grid 100.00-1.50+0 -> 98.50"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("100.00"), QStringLiteral("1.50"),
                                      QStringLiteral("0.75"))
                == QStringLiteral("99.25"),
            QStringLiteral("lineTotal grid 100.00-1.50+0.75 -> 99.25"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("100.00"), QStringLiteral("1.50"),
                                      QStringLiteral("1.60"))
                == QStringLiteral("100.10"),
            QStringLiteral("lineTotal grid 100.00-1.50+1.60 -> 100.10"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("100.00"), QStringLiteral("5.00"),
                                      QStringLiteral("0"))
                == QStringLiteral("95.00"),
            QStringLiteral("lineTotal grid 100.00-5.00+0 -> 95.00"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("100.00"), QStringLiteral("5.00"),
                                      QStringLiteral("0.75"))
                == QStringLiteral("95.75"),
            QStringLiteral("lineTotal grid 100.00-5.00+0.75 -> 95.75"));
    s.check(SaleCalculator::lineTotal(QStringLiteral("100.00"), QStringLiteral("5.00"),
                                      QStringLiteral("1.60"))
                == QStringLiteral("96.60"),
            QStringLiteral("lineTotal grid 100.00-5.00+1.60 -> 96.60"));
    return s;
}

} // namespace pharmadesk_tests
