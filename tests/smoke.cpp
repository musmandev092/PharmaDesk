// Headless smoke test for the Phase-1 data + domain layers. Exercises schema
// bootstrap, bcrypt (incl. PHP $2y$ parity), PinPolicy, admin creation and login
// verification against a throwaway DB (QStandardPaths test mode → temp dir).
//
// Not shipped in the app; built as the `pharmadesk_smoke` target. Exit 0 = all pass.

#include "data/BatchRepository.h"
#include "data/Database.h"
#include "data/InventoryRepository.h"
#include "data/MedicineRepository.h"
#include "data/SettingsRepository.h"
#include "data/SupplierRepository.h"
#include "data/UserRepository.h"
#include "domain/AppPaths.h"
#include "domain/Bcrypt.h"
#include "domain/CostBlender.h"
#include "domain/Fefo.h"
#include "domain/Money.h"
#include "domain/PinPolicy.h"
#include "domain/SaleCalculator.h"
#include "domain/SettingsKeys.h"
#include "data/AuditRepository.h"
#include "data/SaleRepository.h"
#include "data/SalesReportRepository.h"
#include "service/BackupService.h"
#include "service/GrnService.h"
#include "service/SaleService.h"

#include <QDir>

#include <QCoreApplication>
#include <QDate>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTextStream>

static int g_failures = 0;
static QTextStream out(stdout);

static void check(bool cond, const QString &what)
{
    out << (cond ? "  ok   " : "  FAIL ") << what << "\n";
    if (!cond) ++g_failures;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("PharmaDesk"));
    app.setApplicationName(QStringLiteral("PharmaDesk Test"));
    QStandardPaths::setTestModeEnabled(true); // redirect AppDataLocation to temp

    // Start from a clean DB file.
    QFile::remove(AppPaths::dbFilePath());

    out << "== bcrypt ==\n";
    const QString h = Bcrypt::hash(QStringLiteral("481920"));
    check(h.startsWith(QStringLiteral("$2b$")), "hash() produces a $2b$ hash");
    check(Bcrypt::verify(QStringLiteral("481920"), h), "verify() accepts correct PIN");
    check(!Bcrypt::verify(QStringLiteral("999999"), h), "verify() rejects wrong PIN");
    // PHP password_hash('472913', PASSWORD_BCRYPT, ['cost'=>12]) from the seed:
    const QString phpHash
        = QStringLiteral("$2y$12$7FVQGjyhVZfhjAeQ9bLHNulGgnaSwGNEcxwim0Lo9S1bzY8dW0V5q");
    check(Bcrypt::verify(QStringLiteral("472913"), phpHash), "verify() accepts a PHP $2y$ hash");
    check(!Bcrypt::verify(QStringLiteral("000000"), phpHash), "verify() rejects wrong PIN on $2y$");

    out << "== PinPolicy ==\n";
    check(PinPolicy::validate(QStringLiteral("12345")).has_value(), "rejects 5-digit (too short)");
    check(PinPolicy::validate(QStringLiteral("123456789")).has_value(),
          "rejects 9-digit (too long)");
    check(PinPolicy::validate(QStringLiteral("123456")).has_value(), "rejects blocklisted 123456");
    check(PinPolicy::validate(QStringLiteral("000000")).has_value(), "rejects blocklisted 000000");
    check(PinPolicy::validate(QStringLiteral("345678")).has_value(),
          "rejects ascending run 345678");
    check(PinPolicy::validate(QStringLiteral("9876543")).has_value(),
          "rejects descending run 9876543");
    check(!PinPolicy::validate(QStringLiteral("481920")).has_value(), "accepts good PIN 481920");
    {
        const QStringList hist = {Bcrypt::hash(QStringLiteral("481920"))};
        check(PinPolicy::validate(QStringLiteral("481920"), hist).has_value(),
              "rejects reuse of a PIN in history");
    }

    out << "== Database bootstrap ==\n";
    Database db;
    check(db.open(), "open() succeeds");
    check(db.isFreshDb(), "isFreshDb() true on empty file");
    check(db.bootstrap(), "bootstrap() runs schema");
    check(db.migrate(), "migrate() brings schema to latest");
    check(!db.isFreshDb(), "isFreshDb() false after bootstrap");
    check(db.isFirstRun(), "isFirstRun() true before any admin");

    out << "== createAdmin + login ==\n";
    SettingsRepository settings(db.handle());
    UserRepository users(db.handle());

    check(!users.usernameExistsCI(QStringLiteral("owner")), "username free before create");
    AdminDraft draft;
    draft.fullName = QStringLiteral("Alice Khan");
    draft.username = QStringLiteral("owner");
    draft.pinHash = Bcrypt::hash(QStringLiteral("481920"));
    draft.pharmacyName = QStringLiteral("City Care Pharmacy");
    const qint64 id = users.createAdmin(draft);
    check(id > 0, "createAdmin returns a new id");
    check(!db.isFirstRun(), "isFirstRun() false after admin created");
    check(users.usernameExistsCI(QStringLiteral("OWNER")), "usernameExistsCI is case-insensitive");

    QString storedHash;
    UserRecord rec = users.findForLogin(QStringLiteral("Owner"), &storedHash);
    check(rec.valid, "findForLogin finds user case-insensitively");
    check(rec.role == QStringLiteral("ADMIN") && rec.branchId == 1, "role ADMIN, branch 1");
    check(Bcrypt::verify(QStringLiteral("481920"), storedHash), "login verifies correct PIN");
    check(!Bcrypt::verify(QStringLiteral("000000"), storedHash), "login rejects wrong PIN");

    out << "== settings ==\n";
    settings.set(SettingsKeys::PharmacyName, draft.pharmacyName, id);
    check(settings.get(SettingsKeys::PharmacyName) == draft.pharmacyName, "settings upsert + read");
    settings.set(SettingsKeys::PharmacyName, QStringLiteral("Renamed"), id);
    check(settings.get(SettingsKeys::PharmacyName) == QStringLiteral("Renamed"),
          "settings upsert overwrites");

    out << "== Money (vs PHP bcmath HALF_UP) ==\n";
    check(SaleCalculator::lineSubtotal(3, QStringLiteral("10.50")) == QStringLiteral("31.50"),
          "3 x 10.50 = 31.50");
    check(Money::fromString(QStringLiteral("2.345")).toString(2) == QStringLiteral("2.35"),
          "round 2.345 -> 2.35 (HALF_UP)");
    check(Money::fromString(QStringLiteral("2.344")).toString(2) == QStringLiteral("2.34"),
          "round 2.344 -> 2.34");
    check(Money::fromString(QStringLiteral("-2.345")).toString(2) == QStringLiteral("-2.35"),
          "round -2.345 -> -2.35");
    check(SaleCalculator::change(QStringLiteral("100"), QStringLiteral("31.50"))
              == QStringLiteral("68.50"),
          "change 100 - 31.50 = 68.50");
    check(SaleCalculator::change(QStringLiteral("20"), QStringLiteral("31.50"))
              == QStringLiteral("0.00"),
          "change clamps at 0");
    check(Money::fromString(QStringLiteral("1234567.5")).fmt(2) == QStringLiteral("1,234,567.50"),
          "fmt thousands");
    check(Money::fromString(QStringLiteral("12.3456")).toString(4) == QStringLiteral("12.3456"),
          "cost scale-4 preserved");

    out << "== Fefo ==\n";
    {
        QVector<FefoBatch> bs;
        FefoBatch b1;
        b1.id = 1;
        b1.currentQty = 10;
        b1.expiry = QDate(2030, 6, 1);
        b1.mrpPerUnit = "5.00";
        FefoBatch b2;
        b2.id = 2;
        b2.currentQty = 10;
        b2.expiry = QDate(2029, 1, 1);
        b2.mrpPerUnit = "5.00";
        bs << b1 << b2; // intentionally not expiry-sorted
        auto alloc = Fefo::allocate(bs, 15, 42);
        check(alloc.size() == 2 && alloc[0].batch.id == 2 && alloc[0].deduction == 10
                  && alloc[1].batch.id == 1 && alloc[1].deduction == 5,
              "allocates soonest-expiry first, spills over");
        bool threw = false;
        try {
            Fefo::allocate(bs, 999, 42);
        } catch (const InsufficientStockException &) {
            threw = true;
        }
        check(threw, "throws when short of stock");
    }

    out << "== SaleService (commit + stock decrement) ==\n";
    {
        MedicineRepository meds(db.handle());
        BatchRepository batches(db.handle());
        MedicineDraft m;
        m.sku = QStringLiteral("PARA500");
        m.brandName = QStringLiteral("Panadol");
        m.genericName = QStringLiteral("Paracetamol");
        m.strength = QStringLiteral("500mg");
        m.baseUnit = QStringLiteral("TABLET");
        m.purchaseUnit = QStringLiteral("BOX");
        m.unitsPerPurchase = 100;
        const qint64 medId = meds.create(m, id);
        check(medId > 0, "medicine created");

        StockInDraft s1;
        s1.medicineId = medId;
        s1.batchNumber = "B1";
        s1.expiry = QDate::currentDate().addYears(2);
        s1.quantity = 6;
        s1.costPerUnit = "1.2500";
        s1.mrpPerUnit = "2.50";
        StockInDraft s2 = s1;
        s2.batchNumber = "B2";
        s2.expiry = QDate::currentDate().addYears(1);
        s2.quantity = 10;
        s2.mrpPerUnit = "2.50";
        batches.addStock(s1, id);
        batches.addStock(s2, id);
        check(batches.onHand(medId) == 16, "on-hand = 16 after two stock-ins");

        SaleService sale(db.handle(), id);
        SaleInput in;
        SaleLineInput li;
        li.medicineId = medId;
        li.qtySoldDisplay = 12;
        li.soldUnitLabel = "TABLET";
        li.soldUnitFactor = 1;
        li.unitMrp = "2.50";
        in.items << li;
        in.paymentMode = "CASH";
        in.amountTendered = "50";
        SaleResult r = sale.commit(in);
        check(r.ok, "sale commits");
        check(r.subtotal == QStringLiteral("30.00"), "subtotal 12 x 2.50 = 30.00");
        check(r.grandTotal == QStringLiteral("30.00"), "grand total = 30.00");
        check(r.changeReturned == QStringLiteral("20.00"), "change 50 - 30 = 20.00");
        check(batches.onHand(medId) == 4, "on-hand 16 - 12 = 4 (FEFO across batches)");

        // FEFO order: B2 (expires sooner) drained first to 0, then B1 to 4.
        QVector<FefoBatch> after = batches.candidateBatchesForSale(medId);
        check(after.size() == 1 && after[0].currentQty == 4,
              "only the later-expiry batch remains, qty 4");

        // Oversell is refused and leaves stock unchanged.
        SaleInput big;
        big.items << li;
        big.items[0].qtySoldDisplay = 99;
        big.paymentMode = "CASH";
        big.amountTendered = "9999";
        SaleResult rr = sale.commit(big);
        check(!rr.ok, "oversell refused");
        check(batches.onHand(medId) == 4, "stock unchanged after refused oversell");
    }

    out << "== InventoryRepository (Phase 3) ==\n";
    {
        InventoryRepository inv(db.handle());
        const InventoryTotals t = inv.totals();
        check(t.activeBatches >= 1, "active batches counted");
        check(!Money::fromString(t.stockValueCost).isZero(), "stock value at cost > 0");
        check(inv.batchList().size() >= 1, "batch list non-empty");

        MedicineRepository meds(db.handle());
        BatchRepository batches(db.handle());
        MedicineDraft lm;
        lm.sku = QStringLiteral("LOWSTK");
        lm.brandName = QStringLiteral("Brufen");
        lm.genericName = QStringLiteral("Ibuprofen");
        lm.baseUnit = QStringLiteral("TABLET");
        lm.purchaseUnit = QStringLiteral("BOX");
        lm.unitsPerPurchase = 10;
        lm.reorderLevel = 100;
        const qint64 lid = meds.create(lm, id);

        StockInDraft ls;
        ls.medicineId = lid;
        ls.batchNumber = QStringLiteral("LB1");
        ls.expiry = QDate::currentDate().addYears(1);
        ls.quantity = 10;
        ls.costPerUnit = QStringLiteral("1.0000");
        ls.mrpPerUnit = QStringLiteral("2.00");
        batches.addStock(ls, id);

        const QVector<LowStockRow> low = inv.lowStock(0);
        bool flagged = false;
        for (const LowStockRow &r : low)
            if (r.medicineId == lid) flagged = true;
        check(flagged, "low-stock medicine flagged (10 < 100)");

        StockInDraft es;
        es.medicineId = lid;
        es.batchNumber = QStringLiteral("LB2");
        es.expiry = QDate::currentDate().addDays(20);
        es.quantity = 5;
        es.costPerUnit = QStringLiteral("1.0000");
        es.mrpPerUnit = QStringLiteral("2.00");
        batches.addStock(es, id);

        const QVector<ExpiringRow> exp = inv.expiringSoon(90, 0);
        bool soon = false;
        for (const ExpiringRow &r : exp)
            if (r.days >= 0 && r.days <= 25) soon = true;
        check(soon, "near-expiry batch (20 days) listed within 90-day window");
    }

    out << "== CostBlender (vs PHP) ==\n";
    check(CostBlender::blendedCostPerBaseUnit(10, 2, QStringLiteral("100.00"), 10)
              == QStringLiteral("8.3333"),
          "blended cost (paid 10 + free 2) x100/pack of 10 = 8.3333/unit");
    check(CostBlender::blendedCostPerBaseUnit(5, 0, QStringLiteral("50.00"), 10)
              == QStringLiteral("5.0000"),
          "blended cost 5x50 / (5x10) = 5.0000/unit");
    check(CostBlender::mrpPerBaseUnit(QStringLiteral("80.00"), 10) == QStringLiteral("8.0000"),
          "MRP/base = 80 / 10 = 8.0000");
    check(CostBlender::lineTotal(5, QStringLiteral("50.00")) == QStringLiteral("250.00"),
          "line total 5 x 50 = 250.00");

    out << "== GrnService (post → costed batch) ==\n";
    {
        SupplierRepository sup(db.handle());
        SupplierRow s;
        s.name = QStringLiteral("Acme Distributors");
        const qint64 supId = sup.create(s, id);
        check(supId > 0, "supplier created");

        MedicineRepository meds(db.handle());
        MedicineDraft m;
        m.sku = QStringLiteral("AMOX250");
        m.brandName = QStringLiteral("Amoxil");
        m.genericName = QStringLiteral("Amoxicillin");
        m.baseUnit = QStringLiteral("CAPSULE");
        m.purchaseUnit = QStringLiteral("BOX");
        m.unitsPerPurchase = 10;
        const qint64 medId = meds.create(m, id);

        GrnService grn(db.handle(), id);
        GrnLineInput gl;
        gl.medicineId = medId;
        gl.batchNumber = QStringLiteral("G1");
        gl.expiry = QDate::currentDate().addYears(2);
        gl.paidQty = 5;
        gl.focQty = 0;
        gl.unitCost = QStringLiteral("50.00");
        gl.mrpPerPurchaseUnit = QStringLiteral("80.00");
        GrnResult gr = grn.post(supId, {gl}, QStringLiteral("INV-777"), QDate::currentDate());
        check(gr.ok, "GRN posts");
        check(gr.grnNumber.startsWith(QStringLiteral("GRN-")), "GRN number assigned");
        check(gr.subtotal == QStringLiteral("250.00"), "GRN subtotal = 250.00");

        BatchRepository batches(db.handle());
        check(batches.onHand(medId) == 50, "stock on hand = 50 (5 packs x 10)");
        QVector<FefoBatch> cb = batches.candidateBatchesForSale(medId);
        check(cb.size() == 1 && cb[0].currentQty == 50
                  && cb[0].costPerUnit == QStringLiteral("5.0000")
                  && cb[0].mrpPerUnit == QStringLiteral("8.0000"),
              "batch created with blended cost 5.0000 and MRP 8.0000");

        // Empty GRN refused.
        GrnResult empty = grn.post(supId, {});
        check(!empty.ok, "empty GRN refused");
    }

    out << "== SalesReport + reprint (Phase 5) ==\n";
    {
        SalesReportRepository rep(db.handle());
        const QDate today = QDate::currentDate();
        const SalesReport r = rep.report(today.addDays(-1), today.addDays(1));
        check(r.summary.count >= 1, "report finds the committed sale");
        // The only committed sale was the 30.00 CASH sale from the SaleService block.
        check(Money::fromString(r.summary.gross).compare(Money::fromString(QStringLiteral("30.00")))
                  >= 0,
              "gross ≥ 30.00");
        check(r.summary.net == r.summary.gross, "net == gross (no refunds yet)");
        check(!r.rows.isEmpty(), "report has rows");

        SaleRepository sr(db.handle());
        const SaleResult loaded = sr.loadReceipt(r.rows.first().saleId);
        check(loaded.ok, "loadReceipt returns the sale");
        check(!loaded.lines.isEmpty(), "loaded receipt has line items");
        check(loaded.receiptNumber == r.rows.first().receiptNumber, "receipt number matches");

        const QDate past = today.addDays(-400);
        check(rep.report(past, past.addDays(1)).summary.count == 0, "empty range → no sales");
    }

    out << "== User admin + PIN ops (Phase 6) ==\n";
    {
        UserRepository repo(db.handle());
        const int before = repo.listUsers().size();
        const qint64 uid = repo.createUser(QStringLiteral("Bilal Cashier"), QStringLiteral("bilal"),
                                           QStringLiteral("CASHIER"),
                                           Bcrypt::hash(QStringLiteral("314159")), true, id);
        check(uid > 0, "createUser returns id");
        check(repo.listUsers().size() == before + 1, "user appears in list");

        // Reset PIN, then reusing it must be rejected by PinPolicy (history).
        check(repo.setPin(uid, Bcrypt::hash(QStringLiteral("271828")), id), "setPin succeeds");
        const QStringList hist = repo.recentPinHashes(uid, PinPolicy::HistoryKeep);
        check(PinPolicy::validate(QStringLiteral("271828"), hist).has_value(),
              "reusing a recent PIN is rejected");
        check(!PinPolicy::validate(QStringLiteral("862071"), hist).has_value(),
              "a fresh PIN is accepted");
        check(Bcrypt::verify(QStringLiteral("271828"), repo.pinHash(uid)),
              "current hash is the new PIN");

        AuditRepository audit(db.handle());
        bool sawUserCreated = false;
        for (const AuditRow &a : audit.list())
            if (a.actionType == QLatin1String("USER_CREATED")) sawUserCreated = true;
        check(sawUserCreated, "audit log records USER_CREATED");
    }

    out << "== BackupService (Phase 6) ==\n";
    {
        const QString destDir = QDir::tempPath() + QStringLiteral("/pms_backup_test");
        QDir(destDir).removeRecursively();
        BackupService svc(db.handle());
        const BackupService::Result r = svc.backupNow(destDir, QStringLiteral("20260101_000000"));
        check(r.ok, "backupNow copies the DB");
        check(QFile::exists(r.path), "backup file exists on disk");
        // The copy is a valid SQLite DB with our data.
        {
            QSqlDatabase chk
                = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("bkchk"));
            chk.setDatabaseName(r.path);
            bool opened = chk.open();
            int users = 0;
            if (opened) {
                QSqlQuery cq(chk);
                if (cq.exec(QStringLiteral("SELECT count(*) FROM users")) && cq.next())
                    users = cq.value(0).toInt();
            }
            check(opened && users >= 1, "backup is a valid SQLite DB containing users");
            chk.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("bkchk"));
        QDir(destDir).removeRecursively();
    }

    out << "\n"
        << (g_failures == 0 ? "ALL PASSED" : QStringLiteral("%1 FAILURE(S)").arg(g_failures))
        << "\n";
    return g_failures == 0 ? 0 : 1;
}
