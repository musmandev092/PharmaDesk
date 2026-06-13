// DB-backed tests for cashier sessions (data/SessionRepository): open/close,
// per-sale bumps (via SaleService), expected-cash + variance maths.

#include "framework/TestStats.h"

#include "data/BatchRepository.h"
#include "data/MedicineRepository.h"
#include "data/SessionRepository.h"
#include "data/UserRepository.h"
#include "domain/Bcrypt.h"
#include "domain/Money.h"
#include "service/Reconciler.h"
#include "service/SaleService.h"

#include <QDate>
#include <QSqlQuery>
#include <QVariant>

namespace pharmadesk_tests {
namespace {
int g_u = 0;
qint64 makeCashier(QSqlDatabase db, qint64 actingUser)
{
    UserRepository u(db);
    const QString name = QStringLiteral("TSESS user %1").arg(++g_u);
    const QString username = QStringLiteral("tsess_%1").arg(g_u);
    return u.createUser(name, username, QStringLiteral("CASHIER"),
                        Bcrypt::hash(QStringLiteral("481920")), true, actingUser);
}
qint64 medWithStock(QSqlDatabase db, qint64 userId, int qty, const QString &mrp)
{
    MedicineRepository m(db);
    MedicineDraft d;
    d.sku = QStringLiteral("TSESS_SKU_%1").arg(++g_u, 5, 10, QLatin1Char('0'));
    d.brandName = QStringLiteral("SessMed");
    d.genericName = QStringLiteral("Gen");
    d.baseUnit = QStringLiteral("TABLET");
    d.purchaseUnit = QStringLiteral("BOX");
    d.unitsPerPurchase = 1;
    const qint64 id = m.create(d, userId);
    if (id <= 0) return -1;
    BatchRepository b(db);
    StockInDraft s;
    s.medicineId = id;
    s.batchNumber = QStringLiteral("TSESS_B_%1").arg(g_u);
    s.expiry = QDate::currentDate().addYears(2);
    s.quantity = qty;
    s.costPerUnit = QStringLiteral("1.0000");
    s.mrpPerUnit = mrp;
    b.addStock(s, userId);
    return id;
}
// Commit a sale as `cashier` for `qty` of `medId` at `mrp`; returns grand total.
QString sellCash(QSqlDatabase db, qint64 cashier, qint64 medId, int qty, const QString &mrp)
{
    SaleService svc(db, cashier);
    SaleInput in;
    in.paymentMode = QStringLiteral("CASH");
    in.amountTendered = QStringLiteral("100000");
    SaleLineInput li;
    li.medicineId = medId;
    li.qtySoldDisplay = qty;
    li.soldUnitLabel = QStringLiteral("TABLET");
    li.soldUnitFactor = 1;
    li.unitMrp = mrp;
    in.items << li;
    const SaleResult r = svc.commit(in);
    return r.ok ? r.grandTotal : QString();
}
// Read one TEXT column off a cashier_sessions row.
QString sessCol(QSqlDatabase db, qint64 sessionId, const QString &col)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT %1 FROM cashier_sessions WHERE id = ?").arg(col));
    q.addBindValue(sessionId);
    if (q.exec() && q.next()) return q.value(0).toString();
    return QString();
}
} // namespace

TestStats run_sessions_tests(QSqlDatabase db, qint64 userId)
{
    TestStats s;
    s.module = QStringLiteral("sessions");

    SessionRepository sr(db);
    const qint64 cid = makeCashier(db, userId);
    s.check(cid > 0, QStringLiteral("cashier user created"));

    s.check(sr.openSessionId(cid) == -1, QStringLiteral("no open session initially"));

    const qint64 sid = sr.openSession(cid, QStringLiteral("1000.00"));
    s.check(sid > 0, QStringLiteral("openSession returns id"));
    s.check(sr.openSessionId(cid) == sid, QStringLiteral("openSessionId finds the open shift"));
    s.check(sr.openSession(cid, QStringLiteral("500.00")) < 0,
            QStringLiteral("cannot open a second shift while one is open"));

    SessionSummary sum = sr.sessionSummary(sid);
    s.check(sum.valid, QStringLiteral("summary valid"));
    s.check(sum.status == QLatin1String("OPEN"), QStringLiteral("status OPEN"));
    s.check(Money::fromString(sum.openingFloat).toString() == QStringLiteral("1000.00"),
            QStringLiteral("opening float 1000.00"));
    s.check(Money::fromString(sum.totalCashSales).isZero(),
            QStringLiteral("cash sales 0 initially"));
    s.check(sum.totalSalesCount == 0, QStringLiteral("sales count 0 initially"));
    s.check(Money::fromString(sum.expectedCash).toString() == QStringLiteral("1000.00"),
            QStringLiteral("expected cash = float initially"));

    // A CASH sale via SaleService should bump the shift (it links the open session).
    const qint64 med = medWithStock(db, userId, 500, QStringLiteral("10.00"));
    s.check(med > 0, QStringLiteral("test medicine + stock created"));
    const QString g1 = sellCash(db, cid, med, 5, QStringLiteral("10.00")); // 50.00
    s.check(g1 == QStringLiteral("50.00"), QStringLiteral("cash sale grand 50.00"));
    sum = sr.sessionSummary(sid);
    s.check(sum.totalSalesCount == 1, QStringLiteral("sales count 1 after sale"));
    s.check(Money::fromString(sum.totalCashSales).toString() == QStringLiteral("50.00"),
            QStringLiteral("cash sales 50.00 after sale"));
    s.check(Money::fromString(sum.expectedCash).toString() == QStringLiteral("1050.00"),
            QStringLiteral("expected = float + cash sales"));

    const QString g2 = sellCash(db, cid, med, 3, QStringLiteral("10.00")); // 30.00
    s.check(g2 == QStringLiteral("30.00"), QStringLiteral("second cash sale 30.00"));
    sum = sr.sessionSummary(sid);
    s.check(sum.totalSalesCount == 2, QStringLiteral("sales count 2"));
    s.check(Money::fromString(sum.totalCashSales).toString() == QStringLiteral("80.00"),
            QStringLiteral("cash sales 80.00"));

    // Direct bumpForSale: CARD does not add to cash, but bumps count.
    s.check(sr.bumpForSale(sid, QStringLiteral("99.00"), QStringLiteral("CARD")),
            QStringLiteral("bumpForSale CARD ok"));
    sum = sr.sessionSummary(sid);
    s.check(sum.totalSalesCount == 3, QStringLiteral("count 3 after card bump"));
    s.check(Money::fromString(sum.totalCashSales).toString() == QStringLiteral("80.00"),
            QStringLiteral("card does not change cash bucket"));

    // bumpForSale with sessionId<=0 is a no-op (returns true), per the header.
    s.check(sr.bumpForSale(-1, QStringLiteral("10.00"), QStringLiteral("CASH")),
            QStringLiteral("bump no-op on sessionId<=0"));

    // Close: counted = expected → zero variance.
    const QString expected = sr.sessionSummary(sid).expectedCash; // 1080.00
    s.check(Money::fromString(expected).toString() == QStringLiteral("1080.00"),
            QStringLiteral("expected cash 1080.00 before close"));
    s.check(sr.closeSession(sid, expected, cid), QStringLiteral("closeSession ok"));
    sum = sr.sessionSummary(sid);
    s.check(sum.status == QLatin1String("CLOSED"), QStringLiteral("status CLOSED after close"));
    s.check(Money::fromString(sum.countedCash).toString() == QStringLiteral("1080.00"),
            QStringLiteral("counted cash stored"));
    s.check(Money::fromString(sum.cashVariance).isZero(),
            QStringLiteral("zero variance when counted=expected"));
    s.check(sr.openSessionId(cid) == -1, QStringLiteral("no open session after close"));

    // A fresh shift, close with a surplus and a shortage.
    const qint64 sid2 = sr.openSession(cid, QStringLiteral("0.00"));
    s.check(sid2 > 0, QStringLiteral("second shift opens after close"));
    sr.bumpForSale(sid2, QStringLiteral("200.00"), QStringLiteral("CASH"));
    s.check(sr.closeSession(sid2, QStringLiteral("250.00"), cid),
            QStringLiteral("close with surplus"));
    sum = sr.sessionSummary(sid2);
    s.check(Money::fromString(sum.cashVariance).toString() == QStringLiteral("50.00"),
            QStringLiteral("surplus variance +50.00"));

    const qint64 sid3 = sr.openSession(cid, QStringLiteral("0.00"));
    sr.bumpForSale(sid3, QStringLiteral("200.00"), QStringLiteral("CASH"));
    sr.closeSession(sid3, QStringLiteral("180.00"), cid);
    sum = sr.sessionSummary(sid3);
    s.check(Money::fromString(sum.cashVariance).toString() == QStringLiteral("-20.00"),
            QStringLiteral("shortage variance -20.00"));

    s.check(sr.list(50).size() >= 3, QStringLiteral("list returns recent sessions"));

    // ====================================================================
    // RECONCILE — a manager signs off a CLOSED shift (CLOSED → RECONCILED).
    // ====================================================================
    Reconciler rec(db, userId); // userId is the seeded manager/admin

    // R1) Reconcile a closed shift WITHOUT a recount → keep counted, RECONCILED.
    {
        const qint64 rsid = sr.openSession(cid, QStringLiteral("1000.00"));
        s.check(rsid > 0, QStringLiteral("recon-1: shift opened"));
        sr.bumpForSale(rsid, QStringLiteral("75.00"), QStringLiteral("CASH"));
        const QString exp = sr.sessionSummary(rsid).expectedCash; // 1075.00
        s.check(sr.closeSession(rsid, exp, cid), QStringLiteral("recon-1: shift closed"));
        s.check(sessCol(db, rsid, QStringLiteral("status")) == QStringLiteral("CLOSED"),
                QStringLiteral("recon-1: status CLOSED before reconcile"));

        const ReconcileResult r = rec.reconcile(rsid);
        s.check(r.ok, QStringLiteral("recon-1: reconcile (no recount) ok"));
        s.check(r.error.isEmpty(), QStringLiteral("recon-1: no error"));
        s.check(sessCol(db, rsid, QStringLiteral("status")) == QStringLiteral("RECONCILED"),
                QStringLiteral("recon-1: DB status RECONCILED"));
        // counted_cash kept from the close; variance unchanged (counted==expected).
        s.check(Money::fromString(r.countedCash).toString() == QStringLiteral("1075.00"),
                QStringLiteral("recon-1: counted kept from close (1075.00)"));
        s.check(Money::fromString(r.variance).isZero(),
                QStringLiteral("recon-1: variance 0 (counted==expected)"));
    }

    // R2) Reconcile WITH a recount → counted_cash updated, variance recomputed.
    //     expected = opening_float + total_cash_sales − total_refunds_paid.
    {
        const qint64 rsid = sr.openSession(cid, QStringLiteral("500.00"));
        s.check(rsid > 0, QStringLiteral("recon-2: shift opened"));
        sr.bumpForSale(rsid, QStringLiteral("120.00"), QStringLiteral("CASH"));
        // Close at expected (so the close variance is 0); the manager then recounts.
        const QString exp = sr.sessionSummary(rsid).expectedCash; // 620.00
        s.check(sr.closeSession(rsid, exp, cid), QStringLiteral("recon-2: shift closed"));

        const QString recount = QStringLiteral("600.00"); // 20 short of expected
        const ReconcileResult r = rec.reconcile(rsid, recount, QStringLiteral("manager recount"));
        s.check(r.ok, QStringLiteral("recon-2: reconcile with recount ok"));
        s.check(sessCol(db, rsid, QStringLiteral("status")) == QStringLiteral("RECONCILED"),
                QStringLiteral("recon-2: status RECONCILED"));
        // counted_cash equals the recount value.
        s.check(Money::fromString(sessCol(db, rsid, QStringLiteral("counted_cash"))).toString()
                    == QStringLiteral("600.00"),
                QStringLiteral("recon-2: counted_cash == recount (600.00)"));
        s.check(Money::fromString(r.countedCash).toString() == QStringLiteral("600.00"),
                QStringLiteral("recon-2: result counted_cash == recount"));

        // variance == counted − expected, where
        //   expected = opening_float + total_cash_sales − total_refunds_paid.
        const Money openF = Money::fromString(sessCol(db, rsid, QStringLiteral("opening_float")));
        const Money cashS
            = Money::fromString(sessCol(db, rsid, QStringLiteral("total_cash_sales")));
        const Money refP
            = Money::fromString(sessCol(db, rsid, QStringLiteral("total_refunds_paid")));
        const QString expectedCash = (openF + cashS - refP).toString(Money::ScaleMoney);
        const QString wantVar = (Money::fromString(recount) - Money::fromString(expectedCash))
                                    .toString(Money::ScaleMoney);
        s.check(Money::fromString(r.variance).toString() == wantVar,
                QStringLiteral("recon-2: variance == counted − expected"));
        s.check(wantVar == QStringLiteral("-20.00"),
                QStringLiteral("recon-2: variance is -20.00 (600 − 620)"));
        s.check(Money::fromString(sessCol(db, rsid, QStringLiteral("cash_variance"))).toString()
                    == wantVar,
                QStringLiteral("recon-2: DB cash_variance matches"));
    }

    // R3) Reconciling a shift that is still OPEN is refused (error mentions CLOSED).
    {
        const qint64 rsid = sr.openSession(cid, QStringLiteral("0.00"));
        s.check(rsid > 0, QStringLiteral("recon-3: shift opened (left OPEN)"));
        const ReconcileResult r = rec.reconcile(rsid);
        s.check(!r.ok, QStringLiteral("recon-3: reconcile on OPEN shift refused"));
        s.check(r.error.contains(QStringLiteral("CLOSED"), Qt::CaseInsensitive),
                QStringLiteral("recon-3: error mentions CLOSED"));
        s.check(sessCol(db, rsid, QStringLiteral("status")) == QStringLiteral("OPEN"),
                QStringLiteral("recon-3: status still OPEN"));
        // Tidy up so this cashier can open further shifts if needed.
        sr.closeSession(rsid, QStringLiteral("0.00"), cid);
    }

    // R4) Reconciling an already-RECONCILED shift is refused (only CLOSED allowed).
    {
        const qint64 rsid = sr.openSession(cid, QStringLiteral("10.00"));
        s.check(rsid > 0, QStringLiteral("recon-4: shift opened"));
        s.check(sr.closeSession(rsid, QStringLiteral("10.00"), cid),
                QStringLiteral("recon-4: shift closed"));
        const ReconcileResult first = rec.reconcile(rsid);
        s.check(first.ok, QStringLiteral("recon-4: first reconcile ok"));
        const ReconcileResult second = rec.reconcile(rsid);
        s.check(!second.ok, QStringLiteral("recon-4: re-reconcile refused"));
        s.check(sessCol(db, rsid, QStringLiteral("status")) == QStringLiteral("RECONCILED"),
                QStringLiteral("recon-4: status remains RECONCILED"));
    }

    return s;
}

} // namespace pharmadesk_tests
