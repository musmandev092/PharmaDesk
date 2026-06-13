// Exhaustive unit tests for First-Expired-First-Out batch allocation
// (domain/Fefo). Pure: no DB / userId needed. Covers ordering, spill across
// batches, exact deductions, skipping invalid batches, tie-break by id,
// InsufficientStockException, and the needed<1 / empty / single-batch edges.

#include "domain/Fefo.h"
#include "framework/TestStats.h"

#include <QDate>
#include <QVector>

namespace pharmadesk_tests {

namespace {

// Convenience builder for a valid (sellable) batch: future expiry, positive qty,
// not quarantined / expired.
FefoBatch mk(qint64 id, int qty, const QDate &expiry, bool quarantined = false,
             bool expired = false)
{
    FefoBatch b;
    b.id = id;
    b.currentQty = qty;
    b.expiry = expiry;
    b.quarantined = quarantined;
    b.expired = expired;
    b.costPerUnit = QStringLiteral("1.0000");
    b.mrpPerUnit = QStringLiteral("2.00");
    return b;
}

// Sum of all deductions in an allocation result.
int sumDeductions(const QVector<FefoAllocation> &allocs)
{
    int total = 0;
    for (const FefoAllocation &a : allocs) {
        total += a.deduction;
    }
    return total;
}

// True if the allocation batch-ids appear in soonest-expiry-first order
// (ties broken by ascending id).
bool isFefoOrdered(const QVector<FefoAllocation> &allocs)
{
    for (int i = 1; i < allocs.size(); ++i) {
        const FefoBatch &prev = allocs.at(i - 1).batch;
        const FefoBatch &cur = allocs.at(i).batch;
        if (cur.expiry < prev.expiry) {
            return false;
        }
        if (cur.expiry == prev.expiry && cur.id < prev.id) {
            return false;
        }
    }
    return true;
}

} // namespace

TestStats run_fefo_tests(QSqlDatabase db, qint64 userId)
{
    (void)db;
    (void)userId;
    TestStats s;
    s.module = QStringLiteral("fefo");

    const QDate today = QDate::currentDate();

    // --------------------------------------------------------------------
    // 1) needed < 1 → always empty, never throws, for many batch shapes.
    // --------------------------------------------------------------------
    {
        QVector<FefoBatch> batches = {mk(1, 100, today.addDays(30))};
        for (int needed = -5; needed <= 0; ++needed) {
            const auto a = Fefo::allocate(batches, needed, 7, today);
            s.check(a.isEmpty(), QStringLiteral("needed<1 empty (needed=%1)").arg(needed));
            s.check(sumDeductions(a) == 0,
                    QStringLiteral("needed<1 zero deductions (needed=%1)").arg(needed));
        }
        // Empty batch set + needed<1.
        for (int needed = -3; needed <= 0; ++needed) {
            const auto a = Fefo::allocate({}, needed, 7, today);
            s.check(a.isEmpty(), QStringLiteral("empty set + needed<1 (needed=%1)").arg(needed));
        }
    }

    // --------------------------------------------------------------------
    // 2) Empty batch set with positive demand → InsufficientStock, avail 0.
    // --------------------------------------------------------------------
    for (int needed = 1; needed <= 5; ++needed) {
        bool threw = false;
        int caughtAvail = -1;
        int caughtNeeded = -1;
        try {
            Fefo::allocate({}, needed, 42, today);
        } catch (const InsufficientStockException &e) {
            threw = true;
            caughtAvail = e.available;
            caughtNeeded = e.needed;
        }
        s.check(threw, QStringLiteral("empty set throws (needed=%1)").arg(needed));
        s.check(caughtAvail == 0, QStringLiteral("empty set available==0 (needed=%1)").arg(needed));
        s.check(caughtNeeded == needed,
                QStringLiteral("empty set needed echoed (needed=%1)").arg(needed));
    }

    // --------------------------------------------------------------------
    // 3) Single valid batch: partial, exact, and over-demand.
    // --------------------------------------------------------------------
    for (int qty = 1; qty <= 10; ++qty) {
        QVector<FefoBatch> batches = {mk(11, qty, today.addDays(20))};
        // Partial / exact demand: 1..qty all succeed and take exactly `needed`.
        for (int needed = 1; needed <= qty; ++needed) {
            const auto a = Fefo::allocate(batches, needed, 1, today);
            s.check(a.size() == 1,
                    QStringLiteral("single batch one alloc q=%1 n=%2").arg(qty).arg(needed));
            s.check(sumDeductions(a) == needed,
                    QStringLiteral("single batch exact deduction q=%1 n=%2").arg(qty).arg(needed));
            s.check(!a.isEmpty() && a.first().batch.id == 11,
                    QStringLiteral("single batch correct id q=%1 n=%2").arg(qty).arg(needed));
        }
        // Over-demand by 1 → throws, available == qty.
        bool threw = false;
        int avail = -1;
        try {
            Fefo::allocate(batches, qty + 1, 1, today);
        } catch (const InsufficientStockException &e) {
            threw = true;
            avail = e.available;
        }
        s.check(threw, QStringLiteral("single batch over-demand throws q=%1").arg(qty));
        s.check(avail == qty, QStringLiteral("single batch over-demand avail q=%1").arg(qty));
    }

    // --------------------------------------------------------------------
    // 4) Soonest-expiry-first ordering: shuffle expiries, assert FEFO order
    //    and that the earliest-expiry batch is consumed first.
    // --------------------------------------------------------------------
    {
        // Batches deliberately out of order in the input vector.
        QVector<FefoBatch> batches = {
            mk(1, 10, today.addDays(90)), mk(2, 10, today.addDays(10)),
            mk(3, 10, today.addDays(50)), mk(4, 10, today.addDays(30)),
            mk(5, 10, today.addDays(70)),
        };
        // Take 25 → spills batch2(10) + batch4(10) + batch3(5).
        const auto a = Fefo::allocate(batches, 25, 1, today);
        s.check(isFefoOrdered(a), QStringLiteral("order: FEFO sorted output"));
        s.check(sumDeductions(a) == 25, QStringLiteral("order: deductions sum to 25"));
        s.check(a.size() == 3, QStringLiteral("order: spans three batches"));
        s.check(a.at(0).batch.id == 2 && a.at(0).deduction == 10,
                QStringLiteral("order: earliest expiry (id2) first, full 10"));
        s.check(a.at(1).batch.id == 4 && a.at(1).deduction == 10,
                QStringLiteral("order: next expiry (id4) second, full 10"));
        s.check(a.at(2).batch.id == 3 && a.at(2).deduction == 5,
                QStringLiteral("order: third (id3) takes remaining 5"));
    }

    // --------------------------------------------------------------------
    // 5) Spill across many batches with various demand levels; verify exact
    //    sums and ordering for a fixed five-batch ladder.
    // --------------------------------------------------------------------
    {
        QVector<FefoBatch> ladder = {
            mk(10, 5, today.addDays(5)),  mk(20, 5, today.addDays(15)),
            mk(30, 5, today.addDays(25)), mk(40, 5, today.addDays(35)),
            mk(50, 5, today.addDays(45)),
        };
        const int totalAvail = 25;
        for (int needed = 1; needed <= totalAvail; ++needed) {
            const auto a = Fefo::allocate(ladder, needed, 1, today);
            s.check(sumDeductions(a) == needed,
                    QStringLiteral("ladder sum==needed n=%1").arg(needed));
            s.check(isFefoOrdered(a), QStringLiteral("ladder ordered n=%1").arg(needed));
            // Number of batches touched == ceil(needed/5).
            const int expectBatches = (needed + 4) / 5;
            s.check(a.size() == expectBatches,
                    QStringLiteral("ladder batch count n=%1").arg(needed));
            // First batch always id 10 (soonest), fully consumed unless needed<5.
            s.check(a.first().batch.id == 10, QStringLiteral("ladder first id10 n=%1").arg(needed));
            s.check(a.first().deduction == (needed < 5 ? needed : 5),
                    QStringLiteral("ladder first deduction n=%1").arg(needed));
        }
        // Over-demand the whole ladder by 1 → throws, avail 25.
        bool threw = false;
        int avail = -1;
        try {
            Fefo::allocate(ladder, totalAvail + 1, 1, today);
        } catch (const InsufficientStockException &e) {
            threw = true;
            avail = e.available;
        }
        s.check(threw, QStringLiteral("ladder over-demand throws"));
        s.check(avail == totalAvail, QStringLiteral("ladder over-demand avail==25"));
    }

    // --------------------------------------------------------------------
    // 6) Skipping quarantined batches (they must not contribute to avail).
    // --------------------------------------------------------------------
    {
        QVector<FefoBatch> batches = {
            mk(1, 100, today.addDays(5), /*quarantined*/ true),
            mk(2, 7, today.addDays(10)),
        };
        const auto a = Fefo::allocate(batches, 7, 1, today);
        s.check(a.size() == 1, QStringLiteral("quarantine skipped: one alloc"));
        s.check(a.first().batch.id == 2, QStringLiteral("quarantine skipped: id2 used"));
        s.check(sumDeductions(a) == 7, QStringLiteral("quarantine skipped: exact 7"));
        bool threw = false;
        int avail = -1;
        try {
            Fefo::allocate(batches, 8, 1, today);
        } catch (const InsufficientStockException &e) {
            threw = true;
            avail = e.available;
        }
        s.check(threw, QStringLiteral("quarantine: over-demand throws"));
        s.check(avail == 7, QStringLiteral("quarantine: avail excludes quarantined"));
    }

    // --------------------------------------------------------------------
    // 7) Skipping `expired`-flagged batches.
    // --------------------------------------------------------------------
    {
        QVector<FefoBatch> batches = {
            mk(1, 100, today.addDays(5), false, /*expired*/ true),
            mk(2, 4, today.addDays(10)),
        };
        const auto a = Fefo::allocate(batches, 4, 1, today);
        s.check(a.size() == 1 && a.first().batch.id == 2, QStringLiteral("expired flag skipped"));
        s.check(sumDeductions(a) == 4, QStringLiteral("expired flag: exact 4"));
        bool threw = false;
        int avail = -1;
        try {
            Fefo::allocate(batches, 5, 1, today);
        } catch (const InsufficientStockException &e) {
            threw = true;
            avail = e.available;
        }
        s.check(threw && avail == 4, QStringLiteral("expired flag: avail excludes expired"));
    }

    // --------------------------------------------------------------------
    // 8) Skipping zero / negative qty batches.
    // --------------------------------------------------------------------
    {
        for (int badQty = -3; badQty <= 0; ++badQty) {
            QVector<FefoBatch> batches = {
                mk(1, badQty, today.addDays(5)),
                mk(2, 6, today.addDays(10)),
            };
            const auto a = Fefo::allocate(batches, 6, 1, today);
            s.check(a.size() == 1 && a.first().batch.id == 2,
                    QStringLiteral("zero/neg qty skipped (q=%1)").arg(badQty));
            s.check(sumDeductions(a) == 6, QStringLiteral("zero/neg qty exact (q=%1)").arg(badQty));
            bool threw = false;
            int avail = -1;
            try {
                Fefo::allocate(batches, 7, 1, today);
            } catch (const InsufficientStockException &e) {
                threw = true;
                avail = e.available;
            }
            s.check(threw && avail == 6, QStringLiteral("zero/neg qty avail (q=%1)").arg(badQty));
        }
    }

    // --------------------------------------------------------------------
    // 9) Skipping past-expiry batches: expiry must be strictly > today.
    //    expiry == today is NOT sellable (boundary).
    // --------------------------------------------------------------------
    {
        // expiry today and in the past are excluded.
        for (int offset = -5; offset <= 0; ++offset) {
            QVector<FefoBatch> batches = {
                mk(1, 100, today.addDays(offset)),
                mk(2, 3, today.addDays(30)),
            };
            const auto a = Fefo::allocate(batches, 3, 1, today);
            s.check(a.size() == 1 && a.first().batch.id == 2,
                    QStringLiteral("past/today expiry skipped (off=%1)").arg(offset));
            bool threw = false;
            int avail = -1;
            try {
                Fefo::allocate(batches, 4, 1, today);
            } catch (const InsufficientStockException &e) {
                threw = true;
                avail = e.available;
            }
            s.check(threw && avail == 3,
                    QStringLiteral("past/today expiry avail (off=%1)").arg(offset));
        }
        // expiry tomorrow IS sellable.
        QVector<FefoBatch> batches = {mk(1, 5, today.addDays(1))};
        const auto a = Fefo::allocate(batches, 5, 1, today);
        s.check(a.size() == 1 && sumDeductions(a) == 5, QStringLiteral("expiry tomorrow sellable"));
    }

    // --------------------------------------------------------------------
    // 10) Invalid (null) expiry is skipped.
    // --------------------------------------------------------------------
    {
        QVector<FefoBatch> batches = {
            mk(1, 100, QDate()), // invalid
            mk(2, 9, today.addDays(20)),
        };
        const auto a = Fefo::allocate(batches, 9, 1, today);
        s.check(a.size() == 1 && a.first().batch.id == 2, QStringLiteral("invalid expiry skipped"));
        s.check(sumDeductions(a) == 9, QStringLiteral("invalid expiry exact 9"));
    }

    // --------------------------------------------------------------------
    // 11) Tie-break by id when expiry is equal: lower id consumed first.
    // --------------------------------------------------------------------
    {
        const QDate same = today.addDays(40);
        // Input order scrambled; ids 3,1,2 all same expiry.
        QVector<FefoBatch> batches = {
            mk(3, 5, same),
            mk(1, 5, same),
            mk(2, 5, same),
        };
        const auto a = Fefo::allocate(batches, 7, 1, today);
        s.check(a.size() == 2, QStringLiteral("tie-break: spans two batches"));
        s.check(a.at(0).batch.id == 1 && a.at(0).deduction == 5,
                QStringLiteral("tie-break: lowest id1 first"));
        s.check(a.at(1).batch.id == 2 && a.at(1).deduction == 2,
                QStringLiteral("tie-break: next id2 remainder"));
        s.check(isFefoOrdered(a), QStringLiteral("tie-break: ordered"));
    }

    // Tie-break combined with distinct earlier expiry.
    {
        const QDate eq = today.addDays(20);
        QVector<FefoBatch> batches = {
            mk(9, 4, today.addDays(5)),
            mk(7, 4, eq),
            mk(3, 4, eq),
            mk(5, 4, today.addDays(60)),
        };
        const auto a = Fefo::allocate(batches, 12, 1, today);
        s.check(a.size() == 3, QStringLiteral("tie+distinct: three batches"));
        s.check(a.at(0).batch.id == 9, QStringLiteral("tie+distinct: earliest id9"));
        s.check(a.at(1).batch.id == 3, QStringLiteral("tie+distinct: tie low id3 next"));
        s.check(a.at(2).batch.id == 7, QStringLiteral("tie+distinct: tie high id7 last"));
        s.check(sumDeductions(a) == 12, QStringLiteral("tie+distinct: sum 12"));
    }

    // --------------------------------------------------------------------
    // 12) All-invalid batch sets → throw with available 0.
    // --------------------------------------------------------------------
    {
        QVector<FefoBatch> batches = {
            mk(1, 10, today.addDays(5), true, false), // quarantined
            mk(2, 10, today.addDays(5), false, true), // expired flag
            mk(3, 0, today.addDays(5)),               // zero qty
            mk(4, 10, today.addDays(-1)),             // past expiry
            mk(5, 10, QDate()),                       // invalid expiry
        };
        for (int needed = 1; needed <= 5; ++needed) {
            bool threw = false;
            int avail = -1;
            try {
                Fefo::allocate(batches, needed, 1, today);
            } catch (const InsufficientStockException &e) {
                threw = true;
                avail = e.available;
            }
            s.check(threw, QStringLiteral("all-invalid throws n=%1").arg(needed));
            s.check(avail == 0, QStringLiteral("all-invalid avail==0 n=%1").arg(needed));
        }
    }

    // --------------------------------------------------------------------
    // 13) Mix of valid + invalid: only valid contribute, ordered correctly.
    // --------------------------------------------------------------------
    {
        QVector<FefoBatch> batches = {
            mk(100, 50, today.addDays(2), true), // quarantined (skip)
            mk(101, 8, today.addDays(8)),        // valid
            mk(102, 0, today.addDays(1)),        // zero (skip)
            mk(103, 6, today.addDays(4)),        // valid (earlier)
            mk(104, 99, today.addDays(-10)),     // past (skip)
            mk(105, 10, today.addDays(12)),      // valid (latest)
        };
        // Valid total = 8+6+10 = 24. Order by expiry: 103(d4), 101(d8), 105(d12).
        const auto a = Fefo::allocate(batches, 20, 1, today);
        s.check(sumDeductions(a) == 20, QStringLiteral("mix: sum 20"));
        s.check(isFefoOrdered(a), QStringLiteral("mix: ordered"));
        s.check(a.at(0).batch.id == 103 && a.at(0).deduction == 6,
                QStringLiteral("mix: id103 first full 6"));
        s.check(a.at(1).batch.id == 101 && a.at(1).deduction == 8,
                QStringLiteral("mix: id101 second full 8"));
        s.check(a.at(2).batch.id == 105 && a.at(2).deduction == 6,
                QStringLiteral("mix: id105 third remainder 6"));
        bool threw = false;
        int avail = -1;
        try {
            Fefo::allocate(batches, 25, 1, today);
        } catch (const InsufficientStockException &e) {
            threw = true;
            avail = e.available;
        }
        s.check(threw && avail == 24, QStringLiteral("mix: avail counts only valid (24)"));
    }

    // --------------------------------------------------------------------
    // 14) medicineId is propagated into the thrown exception.
    // --------------------------------------------------------------------
    {
        for (qint64 medId : {qint64(0), qint64(1), qint64(999), qint64(123456789)}) {
            qint64 caught = -1;
            try {
                Fefo::allocate({}, 1, medId, today);
            } catch (const InsufficientStockException &e) {
                caught = e.medicineId;
            }
            s.check(caught == medId, QStringLiteral("exception medicineId echoed (%1)").arg(medId));
        }
    }

    // --------------------------------------------------------------------
    // 15) Default-medicineId overload (medicineId omitted) still allocates and
    //     throws sensibly. Use future `today` default by passing explicit today.
    // --------------------------------------------------------------------
    {
        QVector<FefoBatch> batches = {mk(1, 3, today.addDays(10))};
        const auto a = Fefo::allocate(batches, 3, 0, today);
        s.check(sumDeductions(a) == 3, QStringLiteral("default medId allocate ok"));
    }

    // --------------------------------------------------------------------
    // 16) assertSortedByExpiry: passes when sorted, throws when not.
    // --------------------------------------------------------------------
    {
        QVector<FefoBatch> sorted = {
            mk(1, 1, today.addDays(1)),
            mk(2, 1, today.addDays(2)),
            mk(3, 1, today.addDays(2)), // equal allowed
            mk(4, 1, today.addDays(9)),
        };
        bool threw = false;
        try {
            Fefo::assertSortedByExpiry(sorted);
        } catch (...) {
            threw = true;
        }
        s.check(!threw, QStringLiteral("assertSorted: sorted input ok"));

        QVector<FefoBatch> unsorted = {
            mk(1, 1, today.addDays(5)),
            mk(2, 1, today.addDays(2)),
        };
        threw = false;
        try {
            Fefo::assertSortedByExpiry(unsorted);
        } catch (...) {
            threw = true;
        }
        s.check(threw, QStringLiteral("assertSorted: unsorted throws"));

        // Empty + single are trivially sorted.
        threw = false;
        try {
            Fefo::assertSortedByExpiry({});
            Fefo::assertSortedByExpiry({mk(1, 1, today.addDays(3))});
        } catch (...) {
            threw = true;
        }
        s.check(!threw, QStringLiteral("assertSorted: empty/single ok"));
    }

    // --------------------------------------------------------------------
    // 17) Large many-batch set: random-ish expiries, verify global invariants
    //     across a sweep of demands.
    // --------------------------------------------------------------------
    {
        QVector<FefoBatch> big;
        int total = 0;
        // 20 batches, qty (i%4)+1, expiry offset jumbled by *7 mod 31.
        for (int i = 0; i < 20; ++i) {
            const int qty = (i % 4) + 1;
            const int off = ((i * 7) % 31) + 1; // 1..31, all future
            big.push_back(mk(1000 + i, qty, today.addDays(off)));
            total += qty;
        }
        for (int needed = 1; needed <= total; needed += 1) {
            const auto a = Fefo::allocate(big, needed, 1, today);
            s.check(sumDeductions(a) == needed, QStringLiteral("big sum==needed n=%1").arg(needed));
            s.check(isFefoOrdered(a), QStringLiteral("big ordered n=%1").arg(needed));
        }
        // Exactly draining the whole stock works.
        const auto full = Fefo::allocate(big, total, 1, today);
        s.check(sumDeductions(full) == total, QStringLiteral("big drains exactly"));
        // One past total throws with avail==total.
        bool threw = false;
        int avail = -1;
        try {
            Fefo::allocate(big, total + 1, 1, today);
        } catch (const InsufficientStockException &e) {
            threw = true;
            avail = e.available;
        }
        s.check(threw && avail == total, QStringLiteral("big over-demand avail==total"));
    }

    return s;
}

} // namespace pharmadesk_tests
