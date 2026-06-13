// DB-backed tests for ParkedCartRepository: park / list / load / discard.

#include "framework/TestStats.h"

#include "data/ParkedCartRepository.h"
#include "domain/Money.h"

namespace pharmadesk_tests {

TestStats run_parked_tests(QSqlDatabase db, qint64 userId)
{
    TestStats s;
    s.module = QStringLiteral("parked");

    ParkedCartRepository repo(db);
    const qint64 cashier = userId;

    // Empty cart rejected.
    s.check(repo.park(cashier, QStringLiteral("empty"), {}, userId) < 0,
            QStringLiteral("empty parked cart rejected"));

    // Park two lines: 2 × 10.00 + 3 × 5.50 = 20.00 + 16.50 = 36.50.
    QVector<ParkedItem> items;
    items.push_back(
        {101, QStringLiteral("Alpha"), QStringLiteral("TABLET"), QStringLiteral("10.00"), 2});
    items.push_back(
        {102, QStringLiteral("Beta"), QStringLiteral("BOTTLE"), QStringLiteral("5.50"), 3});
    const qint64 id = repo.park(cashier, QStringLiteral("Mr Khan"), items, userId);
    s.check(id > 0, QStringLiteral("park returns id"));

    // List shows it with itemCount + total.
    {
        const QVector<ParkedCart> carts = repo.list(cashier);
        bool ok = false;
        for (const ParkedCart &c : carts) {
            if (c.id == id) {
                ok = (c.label == QStringLiteral("Mr Khan") && c.itemCount == 2
                      && Money::fromString(c.total).toString() == QStringLiteral("36.50"));
            }
        }
        s.check(ok, QStringLiteral("list shows parked cart with itemCount 2 + total 36.50"));
    }

    // Load returns the full lines.
    {
        ParkedCart full;
        s.check(repo.load(id, &full), QStringLiteral("load succeeds"));
        s.check(full.items.size() == 2, QStringLiteral("load returns 2 items"));
        if (full.items.size() == 2) {
            s.check(full.items[0].medicineId == 101 && full.items[0].qty == 2
                        && full.items[0].unitMrp == QStringLiteral("10.00"),
                    QStringLiteral("first line round-trips"));
            s.check(full.items[1].medicineId == 102 && full.items[1].qty == 3
                        && full.items[1].baseUnit == QStringLiteral("BOTTLE"),
                    QStringLiteral("second line round-trips"));
        } else {
            s.check(false, QStringLiteral("first line round-trips"));
            s.check(false, QStringLiteral("second line round-trips"));
        }
    }

    // Discard removes it.
    s.check(repo.discard(id, userId), QStringLiteral("discard ok"));
    {
        ParkedCart gone;
        s.check(!repo.load(id, &gone), QStringLiteral("load fails after discard"));
        const QVector<ParkedCart> carts = repo.list(cashier);
        bool stillThere = false;
        for (const ParkedCart &c : carts)
            if (c.id == id) stillThere = true;
        s.check(!stillThere, QStringLiteral("discarded cart not in list"));
    }

    // A second cart with a single line.
    {
        QVector<ParkedItem> one;
        one.push_back(
            {201, QStringLiteral("Solo"), QStringLiteral("TABLET"), QStringLiteral("7.25"), 4});
        const qint64 id2 = repo.park(cashier, QString(), one, userId); // empty label allowed
        s.check(id2 > 0, QStringLiteral("park with empty label ok"));
        ParkedCart c;
        repo.load(id2, &c);
        s.check(Money::fromString(c.total).toString() == QStringLiteral("29.00"),
                QStringLiteral("single-line total 4*7.25 = 29.00"));
        repo.discard(id2, userId);
    }

    return s;
}

} // namespace pharmadesk_tests
