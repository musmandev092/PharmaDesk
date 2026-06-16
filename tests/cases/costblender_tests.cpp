#include "framework/TestStats.h"
#include "domain/Money.h"
#include "domain/CostBlender.h"

#include <stdexcept>

namespace pharmadesk_tests {

TestStats run_costblender_tests(QSqlDatabase db, qint64 userId)
{
    (void)db;
    (void)userId;
    TestStats s;
    s.module = QStringLiteral("costblender");

    s.check(CostBlender::blendedCostPerBaseUnit(1, 0, QStringLiteral("100.0000"), 1)
                == QStringLiteral("100.0000"),
            QStringLiteral("blended p1 f0 c100.0000 upp1 -> 100.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 0, QStringLiteral("99.9900"), 1)
                == QStringLiteral("99.9900"),
            QStringLiteral("blended p1 f0 c99.9900 upp1 -> 99.9900"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 0, QStringLiteral("250.0000"), 1)
                == QStringLiteral("250.0000"),
            QStringLiteral("blended p1 f0 c250.0000 upp1 -> 250.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 0, QStringLiteral("1.0000"), 1)
                == QStringLiteral("1.0000"),
            QStringLiteral("blended p1 f0 c1.0000 upp1 -> 1.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 0, QStringLiteral("100.0000"), 10)
                == QStringLiteral("10.0000"),
            QStringLiteral("blended p1 f0 c100.0000 upp10 -> 10.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 0, QStringLiteral("99.9900"), 10)
                == QStringLiteral("9.9990"),
            QStringLiteral("blended p1 f0 c99.9900 upp10 -> 9.9990"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 0, QStringLiteral("250.0000"), 10)
                == QStringLiteral("25.0000"),
            QStringLiteral("blended p1 f0 c250.0000 upp10 -> 25.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 0, QStringLiteral("1.0000"), 10)
                == QStringLiteral("0.1000"),
            QStringLiteral("blended p1 f0 c1.0000 upp10 -> 0.1000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 0, QStringLiteral("100.0000"), 20)
                == QStringLiteral("5.0000"),
            QStringLiteral("blended p1 f0 c100.0000 upp20 -> 5.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 0, QStringLiteral("99.9900"), 20)
                == QStringLiteral("4.9995"),
            QStringLiteral("blended p1 f0 c99.9900 upp20 -> 4.9995"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 0, QStringLiteral("250.0000"), 20)
                == QStringLiteral("12.5000"),
            QStringLiteral("blended p1 f0 c250.0000 upp20 -> 12.5000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 0, QStringLiteral("1.0000"), 20)
                == QStringLiteral("0.0500"),
            QStringLiteral("blended p1 f0 c1.0000 upp20 -> 0.0500"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 0, QStringLiteral("100.0000"), 100)
                == QStringLiteral("1.0000"),
            QStringLiteral("blended p1 f0 c100.0000 upp100 -> 1.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 0, QStringLiteral("99.9900"), 100)
                == QStringLiteral("0.9999"),
            QStringLiteral("blended p1 f0 c99.9900 upp100 -> 0.9999"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 0, QStringLiteral("250.0000"), 100)
                == QStringLiteral("2.5000"),
            QStringLiteral("blended p1 f0 c250.0000 upp100 -> 2.5000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 0, QStringLiteral("1.0000"), 100)
                == QStringLiteral("0.0100"),
            QStringLiteral("blended p1 f0 c1.0000 upp100 -> 0.0100"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 1, QStringLiteral("100.0000"), 1)
                == QStringLiteral("50.0000"),
            QStringLiteral("blended p1 f1 c100.0000 upp1 -> 50.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 1, QStringLiteral("99.9900"), 1)
                == QStringLiteral("49.9950"),
            QStringLiteral("blended p1 f1 c99.9900 upp1 -> 49.9950"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 1, QStringLiteral("250.0000"), 1)
                == QStringLiteral("125.0000"),
            QStringLiteral("blended p1 f1 c250.0000 upp1 -> 125.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 1, QStringLiteral("1.0000"), 1)
                == QStringLiteral("0.5000"),
            QStringLiteral("blended p1 f1 c1.0000 upp1 -> 0.5000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 1, QStringLiteral("100.0000"), 10)
                == QStringLiteral("5.0000"),
            QStringLiteral("blended p1 f1 c100.0000 upp10 -> 5.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 1, QStringLiteral("99.9900"), 10)
                == QStringLiteral("4.9995"),
            QStringLiteral("blended p1 f1 c99.9900 upp10 -> 4.9995"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 1, QStringLiteral("250.0000"), 10)
                == QStringLiteral("12.5000"),
            QStringLiteral("blended p1 f1 c250.0000 upp10 -> 12.5000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 1, QStringLiteral("1.0000"), 10)
                == QStringLiteral("0.0500"),
            QStringLiteral("blended p1 f1 c1.0000 upp10 -> 0.0500"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 1, QStringLiteral("100.0000"), 20)
                == QStringLiteral("2.5000"),
            QStringLiteral("blended p1 f1 c100.0000 upp20 -> 2.5000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 1, QStringLiteral("99.9900"), 20)
                == QStringLiteral("2.4998"),
            QStringLiteral("blended p1 f1 c99.9900 upp20 -> 2.4998"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 1, QStringLiteral("250.0000"), 20)
                == QStringLiteral("6.2500"),
            QStringLiteral("blended p1 f1 c250.0000 upp20 -> 6.2500"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 1, QStringLiteral("1.0000"), 20)
                == QStringLiteral("0.0250"),
            QStringLiteral("blended p1 f1 c1.0000 upp20 -> 0.0250"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 1, QStringLiteral("100.0000"), 100)
                == QStringLiteral("0.5000"),
            QStringLiteral("blended p1 f1 c100.0000 upp100 -> 0.5000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 1, QStringLiteral("99.9900"), 100)
                == QStringLiteral("0.5000"),
            QStringLiteral("blended p1 f1 c99.9900 upp100 -> 0.5000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 1, QStringLiteral("250.0000"), 100)
                == QStringLiteral("1.2500"),
            QStringLiteral("blended p1 f1 c250.0000 upp100 -> 1.2500"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 1, QStringLiteral("1.0000"), 100)
                == QStringLiteral("0.0050"),
            QStringLiteral("blended p1 f1 c1.0000 upp100 -> 0.0050"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 2, QStringLiteral("100.0000"), 1)
                == QStringLiteral("33.3333"),
            QStringLiteral("blended p1 f2 c100.0000 upp1 -> 33.3333"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 2, QStringLiteral("99.9900"), 1)
                == QStringLiteral("33.3300"),
            QStringLiteral("blended p1 f2 c99.9900 upp1 -> 33.3300"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 2, QStringLiteral("250.0000"), 1)
                == QStringLiteral("83.3333"),
            QStringLiteral("blended p1 f2 c250.0000 upp1 -> 83.3333"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 2, QStringLiteral("1.0000"), 1)
                == QStringLiteral("0.3333"),
            QStringLiteral("blended p1 f2 c1.0000 upp1 -> 0.3333"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 2, QStringLiteral("100.0000"), 10)
                == QStringLiteral("3.3333"),
            QStringLiteral("blended p1 f2 c100.0000 upp10 -> 3.3333"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 2, QStringLiteral("99.9900"), 10)
                == QStringLiteral("3.3330"),
            QStringLiteral("blended p1 f2 c99.9900 upp10 -> 3.3330"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 2, QStringLiteral("250.0000"), 10)
                == QStringLiteral("8.3333"),
            QStringLiteral("blended p1 f2 c250.0000 upp10 -> 8.3333"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 2, QStringLiteral("1.0000"), 10)
                == QStringLiteral("0.0333"),
            QStringLiteral("blended p1 f2 c1.0000 upp10 -> 0.0333"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 2, QStringLiteral("100.0000"), 20)
                == QStringLiteral("1.6667"),
            QStringLiteral("blended p1 f2 c100.0000 upp20 -> 1.6667"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 2, QStringLiteral("99.9900"), 20)
                == QStringLiteral("1.6665"),
            QStringLiteral("blended p1 f2 c99.9900 upp20 -> 1.6665"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 2, QStringLiteral("250.0000"), 20)
                == QStringLiteral("4.1667"),
            QStringLiteral("blended p1 f2 c250.0000 upp20 -> 4.1667"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 2, QStringLiteral("1.0000"), 20)
                == QStringLiteral("0.0167"),
            QStringLiteral("blended p1 f2 c1.0000 upp20 -> 0.0167"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 2, QStringLiteral("100.0000"), 100)
                == QStringLiteral("0.3333"),
            QStringLiteral("blended p1 f2 c100.0000 upp100 -> 0.3333"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 2, QStringLiteral("99.9900"), 100)
                == QStringLiteral("0.3333"),
            QStringLiteral("blended p1 f2 c99.9900 upp100 -> 0.3333"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 2, QStringLiteral("250.0000"), 100)
                == QStringLiteral("0.8333"),
            QStringLiteral("blended p1 f2 c250.0000 upp100 -> 0.8333"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 2, QStringLiteral("1.0000"), 100)
                == QStringLiteral("0.0033"),
            QStringLiteral("blended p1 f2 c1.0000 upp100 -> 0.0033"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 5, QStringLiteral("100.0000"), 1)
                == QStringLiteral("16.6667"),
            QStringLiteral("blended p1 f5 c100.0000 upp1 -> 16.6667"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 5, QStringLiteral("99.9900"), 1)
                == QStringLiteral("16.6650"),
            QStringLiteral("blended p1 f5 c99.9900 upp1 -> 16.6650"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 5, QStringLiteral("250.0000"), 1)
                == QStringLiteral("41.6667"),
            QStringLiteral("blended p1 f5 c250.0000 upp1 -> 41.6667"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 5, QStringLiteral("1.0000"), 1)
                == QStringLiteral("0.1667"),
            QStringLiteral("blended p1 f5 c1.0000 upp1 -> 0.1667"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 5, QStringLiteral("100.0000"), 10)
                == QStringLiteral("1.6667"),
            QStringLiteral("blended p1 f5 c100.0000 upp10 -> 1.6667"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 5, QStringLiteral("99.9900"), 10)
                == QStringLiteral("1.6665"),
            QStringLiteral("blended p1 f5 c99.9900 upp10 -> 1.6665"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 5, QStringLiteral("250.0000"), 10)
                == QStringLiteral("4.1667"),
            QStringLiteral("blended p1 f5 c250.0000 upp10 -> 4.1667"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 5, QStringLiteral("1.0000"), 10)
                == QStringLiteral("0.0167"),
            QStringLiteral("blended p1 f5 c1.0000 upp10 -> 0.0167"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 5, QStringLiteral("100.0000"), 20)
                == QStringLiteral("0.8333"),
            QStringLiteral("blended p1 f5 c100.0000 upp20 -> 0.8333"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 5, QStringLiteral("99.9900"), 20)
                == QStringLiteral("0.8333"),
            QStringLiteral("blended p1 f5 c99.9900 upp20 -> 0.8333"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 5, QStringLiteral("250.0000"), 20)
                == QStringLiteral("2.0833"),
            QStringLiteral("blended p1 f5 c250.0000 upp20 -> 2.0833"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 5, QStringLiteral("1.0000"), 20)
                == QStringLiteral("0.0083"),
            QStringLiteral("blended p1 f5 c1.0000 upp20 -> 0.0083"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 5, QStringLiteral("100.0000"), 100)
                == QStringLiteral("0.1667"),
            QStringLiteral("blended p1 f5 c100.0000 upp100 -> 0.1667"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 5, QStringLiteral("99.9900"), 100)
                == QStringLiteral("0.1667"),
            QStringLiteral("blended p1 f5 c99.9900 upp100 -> 0.1667"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 5, QStringLiteral("250.0000"), 100)
                == QStringLiteral("0.4167"),
            QStringLiteral("blended p1 f5 c250.0000 upp100 -> 0.4167"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 5, QStringLiteral("1.0000"), 100)
                == QStringLiteral("0.0017"),
            QStringLiteral("blended p1 f5 c1.0000 upp100 -> 0.0017"));
    s.check(CostBlender::blendedCostPerBaseUnit(2, 0, QStringLiteral("100.0000"), 1)
                == QStringLiteral("100.0000"),
            QStringLiteral("blended p2 f0 c100.0000 upp1 -> 100.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(2, 0, QStringLiteral("99.9900"), 1)
                == QStringLiteral("99.9900"),
            QStringLiteral("blended p2 f0 c99.9900 upp1 -> 99.9900"));
    s.check(CostBlender::blendedCostPerBaseUnit(2, 0, QStringLiteral("250.0000"), 1)
                == QStringLiteral("250.0000"),
            QStringLiteral("blended p2 f0 c250.0000 upp1 -> 250.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(2, 0, QStringLiteral("1.0000"), 1)
                == QStringLiteral("1.0000"),
            QStringLiteral("blended p2 f0 c1.0000 upp1 -> 1.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(2, 0, QStringLiteral("100.0000"), 10)
                == QStringLiteral("10.0000"),
            QStringLiteral("blended p2 f0 c100.0000 upp10 -> 10.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(2, 0, QStringLiteral("99.9900"), 10)
                == QStringLiteral("9.9990"),
            QStringLiteral("blended p2 f0 c99.9900 upp10 -> 9.9990"));
    s.check(CostBlender::blendedCostPerBaseUnit(5, 0, QStringLiteral("100.0000"), 10)
                == CostBlender::mrpPerBaseUnit(QStringLiteral("100.0000"), 10),
            QStringLiteral("foc=0 invariant p5 c100.0000 upp10"));
    s.check(CostBlender::blendedCostPerBaseUnit(12, 0, QStringLiteral("250.0000"), 20)
                == CostBlender::mrpPerBaseUnit(QStringLiteral("250.0000"), 20),
            QStringLiteral("foc=0 invariant p12 c250.0000 upp20"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 0, QStringLiteral("99.9900"), 1)
                == CostBlender::mrpPerBaseUnit(QStringLiteral("99.9900"), 1),
            QStringLiteral("foc=0 invariant p1 c99.9900 upp1"));
    s.check(CostBlender::blendedCostPerBaseUnit(100, 0, QStringLiteral("1.0000"), 100)
                == CostBlender::mrpPerBaseUnit(QStringLiteral("1.0000"), 100),
            QStringLiteral("foc=0 invariant p100 c1.0000 upp100"));
    s.check(CostBlender::blendedCostPerBaseUnit(5, 0, QStringLiteral("100.0000"), 1)
                == QStringLiteral("100.0000"),
            QStringLiteral("blended edge p5 f0 c100.0000 upp1 -> 100.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(1, 5, QStringLiteral("100.0000"), 1)
                == QStringLiteral("16.6667"),
            QStringLiteral("blended edge p1 f5 c100.0000 upp1 -> 16.6667"));
    s.check(CostBlender::blendedCostPerBaseUnit(0, 5, QStringLiteral("100.0000"), 10)
                == QStringLiteral("0.0000"),
            QStringLiteral("blended edge p0 f5 c100.0000 upp10 -> 0.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(0, 0, QStringLiteral("100.0000"), 10)
                == QStringLiteral("0.0000"),
            QStringLiteral("blended edge p0 f0 c100.0000 upp10 -> 0.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(5, 5, QStringLiteral("100.0000"), 10)
                == QStringLiteral("5.0000"),
            QStringLiteral("blended edge p5 f5 c100.0000 upp10 -> 5.0000"));
    s.check(CostBlender::blendedCostPerBaseUnit(3, 1, QStringLiteral("99.9900"), 4)
                == QStringLiteral("18.7481"),
            QStringLiteral("blended edge p3 f1 c99.9900 upp4 -> 18.7481"));
    s.check(CostBlender::blendedCostPerBaseUnit(10, 2, QStringLiteral("50.0000"), 6)
                == QStringLiteral("6.9444"),
            QStringLiteral("blended edge p10 f2 c50.0000 upp6 -> 6.9444"));
    // PHP parity: invalid input now THROWS (was fail-open "0.0000"). The throw
    // cases are asserted at the end of this function via the throwsBlended helper.
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("100.0000"), 1)
                == QStringLiteral("100.0000"),
            QStringLiteral("mrpPerBaseUnit 100.0000/1 -> 100.0000"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("100.0000"), 2) == QStringLiteral("50.0000"),
            QStringLiteral("mrpPerBaseUnit 100.0000/2 -> 50.0000"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("100.0000"), 3) == QStringLiteral("33.3333"),
            QStringLiteral("mrpPerBaseUnit 100.0000/3 -> 33.3333"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("100.0000"), 7) == QStringLiteral("14.2857"),
            QStringLiteral("mrpPerBaseUnit 100.0000/7 -> 14.2857"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("100.0000"), 10)
                == QStringLiteral("10.0000"),
            QStringLiteral("mrpPerBaseUnit 100.0000/10 -> 10.0000"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("100.0000"), 20) == QStringLiteral("5.0000"),
            QStringLiteral("mrpPerBaseUnit 100.0000/20 -> 5.0000"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("100.0000"), 100)
                == QStringLiteral("1.0000"),
            QStringLiteral("mrpPerBaseUnit 100.0000/100 -> 1.0000"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("99.9900"), 1) == QStringLiteral("99.9900"),
            QStringLiteral("mrpPerBaseUnit 99.9900/1 -> 99.9900"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("99.9900"), 2) == QStringLiteral("49.9950"),
            QStringLiteral("mrpPerBaseUnit 99.9900/2 -> 49.9950"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("99.9900"), 3) == QStringLiteral("33.3300"),
            QStringLiteral("mrpPerBaseUnit 99.9900/3 -> 33.3300"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("99.9900"), 7) == QStringLiteral("14.2843"),
            QStringLiteral("mrpPerBaseUnit 99.9900/7 -> 14.2843"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("99.9900"), 10) == QStringLiteral("9.9990"),
            QStringLiteral("mrpPerBaseUnit 99.9900/10 -> 9.9990"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("99.9900"), 20) == QStringLiteral("4.9995"),
            QStringLiteral("mrpPerBaseUnit 99.9900/20 -> 4.9995"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("99.9900"), 100) == QStringLiteral("0.9999"),
            QStringLiteral("mrpPerBaseUnit 99.9900/100 -> 0.9999"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("250.5000"), 1)
                == QStringLiteral("250.5000"),
            QStringLiteral("mrpPerBaseUnit 250.5000/1 -> 250.5000"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("250.5000"), 2)
                == QStringLiteral("125.2500"),
            QStringLiteral("mrpPerBaseUnit 250.5000/2 -> 125.2500"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("250.5000"), 3) == QStringLiteral("83.5000"),
            QStringLiteral("mrpPerBaseUnit 250.5000/3 -> 83.5000"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("250.5000"), 7) == QStringLiteral("35.7857"),
            QStringLiteral("mrpPerBaseUnit 250.5000/7 -> 35.7857"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("250.5000"), 10)
                == QStringLiteral("25.0500"),
            QStringLiteral("mrpPerBaseUnit 250.5000/10 -> 25.0500"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("250.5000"), 20)
                == QStringLiteral("12.5250"),
            QStringLiteral("mrpPerBaseUnit 250.5000/20 -> 12.5250"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("250.5000"), 100)
                == QStringLiteral("2.5050"),
            QStringLiteral("mrpPerBaseUnit 250.5000/100 -> 2.5050"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("1.0000"), 1) == QStringLiteral("1.0000"),
            QStringLiteral("mrpPerBaseUnit 1.0000/1 -> 1.0000"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("1.0000"), 2) == QStringLiteral("0.5000"),
            QStringLiteral("mrpPerBaseUnit 1.0000/2 -> 0.5000"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("1.0000"), 3) == QStringLiteral("0.3333"),
            QStringLiteral("mrpPerBaseUnit 1.0000/3 -> 0.3333"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("1.0000"), 7) == QStringLiteral("0.1429"),
            QStringLiteral("mrpPerBaseUnit 1.0000/7 -> 0.1429"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("1.0000"), 10) == QStringLiteral("0.1000"),
            QStringLiteral("mrpPerBaseUnit 1.0000/10 -> 0.1000"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("1.0000"), 20) == QStringLiteral("0.0500"),
            QStringLiteral("mrpPerBaseUnit 1.0000/20 -> 0.0500"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("1.0000"), 100) == QStringLiteral("0.0100"),
            QStringLiteral("mrpPerBaseUnit 1.0000/100 -> 0.0100"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("333.3300"), 1)
                == QStringLiteral("333.3300"),
            QStringLiteral("mrpPerBaseUnit 333.3300/1 -> 333.3300"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("333.3300"), 2)
                == QStringLiteral("166.6650"),
            QStringLiteral("mrpPerBaseUnit 333.3300/2 -> 166.6650"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("333.3300"), 3)
                == QStringLiteral("111.1100"),
            QStringLiteral("mrpPerBaseUnit 333.3300/3 -> 111.1100"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("333.3300"), 7) == QStringLiteral("47.6186"),
            QStringLiteral("mrpPerBaseUnit 333.3300/7 -> 47.6186"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("333.3300"), 10)
                == QStringLiteral("33.3330"),
            QStringLiteral("mrpPerBaseUnit 333.3300/10 -> 33.3330"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("333.3300"), 20)
                == QStringLiteral("16.6665"),
            QStringLiteral("mrpPerBaseUnit 333.3300/20 -> 16.6665"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("333.3300"), 100)
                == QStringLiteral("3.3333"),
            QStringLiteral("mrpPerBaseUnit 333.3300/100 -> 3.3333"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("10.0000"), 1) == QStringLiteral("10.0000"),
            QStringLiteral("mrpPerBaseUnit 10.0000/1 -> 10.0000"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("10.0000"), 2) == QStringLiteral("5.0000"),
            QStringLiteral("mrpPerBaseUnit 10.0000/2 -> 5.0000"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("10.0000"), 3) == QStringLiteral("3.3333"),
            QStringLiteral("mrpPerBaseUnit 10.0000/3 -> 3.3333"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("10.0000"), 7) == QStringLiteral("1.4286"),
            QStringLiteral("mrpPerBaseUnit 10.0000/7 -> 1.4286"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("10.0000"), 10) == QStringLiteral("1.0000"),
            QStringLiteral("mrpPerBaseUnit 10.0000/10 -> 1.0000"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("10.0000"), 20) == QStringLiteral("0.5000"),
            QStringLiteral("mrpPerBaseUnit 10.0000/20 -> 0.5000"));
    s.check(CostBlender::mrpPerBaseUnit(QStringLiteral("10.0000"), 100) == QStringLiteral("0.1000"),
            QStringLiteral("mrpPerBaseUnit 10.0000/100 -> 0.1000"));
    // PHP parity: upp < 1 now THROWS (was fail-open "0.0000"); asserted below.
    s.check(CostBlender::lineTotal(5, QStringLiteral("100.0000")) == QStringLiteral("500.00"),
            QStringLiteral("lineTotal p5 c100.0000 -> 500.00"));
    s.check(CostBlender::lineTotal(1, QStringLiteral("99.9900")) == QStringLiteral("99.99"),
            QStringLiteral("lineTotal p1 c99.9900 -> 99.99"));
    s.check(CostBlender::lineTotal(12, QStringLiteral("250.0050")) == QStringLiteral("3000.06"),
            QStringLiteral("lineTotal p12 c250.0050 -> 3000.06"));
    s.check(CostBlender::lineTotal(0, QStringLiteral("100.0000")) == QStringLiteral("0.00"),
            QStringLiteral("lineTotal p0 c100.0000 -> 0.00"));
    s.check(CostBlender::lineTotal(100, QStringLiteral("1.0050")) == QStringLiteral("100.50"),
            QStringLiteral("lineTotal p100 c1.0050 -> 100.50"));
    s.check(CostBlender::lineTotal(7, QStringLiteral("3.3333")) == QStringLiteral("23.33"),
            QStringLiteral("lineTotal p7 c3.3333 -> 23.33"));
    s.check(CostBlender::lineTotal(3, QStringLiteral("0.0001")) == QStringLiteral("0.00"),
            QStringLiteral("lineTotal p3 c0.0001 -> 0.00"));
    s.check(CostBlender::lineTotal(10, QStringLiteral("12.3456")) == QStringLiteral("123.46"),
            QStringLiteral("lineTotal p10 c12.3456 -> 123.46"));

    // PHP-parity: invalid input THROWS (does not fail open to "0.0000"). Matches
    // services/CostBlender.php which raises InvalidArgumentException, so the GRN
    // post rolls back instead of persisting a bogus zero cost.
    auto throwsBlended = [](int p, int f, const QString &c, int upp) {
        try {
            CostBlender::blendedCostPerBaseUnit(p, f, c, upp);
            return false;
        } catch (const std::invalid_argument &) {
            return true;
        }
    };
    s.check(throwsBlended(-1, 0, QStringLiteral("100.0000"), 1),
            QStringLiteral("blended: negative paidQty throws"));
    s.check(throwsBlended(1, -1, QStringLiteral("100.0000"), 1),
            QStringLiteral("blended: negative focQty throws"));
    s.check(throwsBlended(1, 0, QStringLiteral("100.0000"), 0),
            QStringLiteral("blended: units_per_purchase < 1 throws"));
    // The legitimate nothing-received case still returns zero (NOT a throw).
    s.check(CostBlender::blendedCostPerBaseUnit(0, 0, QStringLiteral("100.0000"), 1)
                == QStringLiteral("0.0000"),
            QStringLiteral("blended: zero receipt -> 0.0000 (not a throw)"));
    bool mrpThrew = false;
    try {
        CostBlender::mrpPerBaseUnit(QStringLiteral("100.0000"), 0);
    } catch (const std::invalid_argument &) {
        mrpThrew = true;
    }
    s.check(mrpThrew, QStringLiteral("mrpPerBaseUnit: units_per_purchase < 1 throws"));
    return s;
}

} // namespace pharmadesk_tests
