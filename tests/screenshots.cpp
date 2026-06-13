// Screenshot harness: seeds a demo DB (in QStandardPaths test mode, so the real
// pharmacy DB is untouched), then renders each page/dialog to a PNG via
// QWidget::grab() under the offscreen platform. Lets us review the UI without a
// compositor. Usage: QT_QPA_PLATFORM=offscreen ./pharmadesk_shots [outDir]

#include "MainWindow.h"
#include "data/BatchRepository.h"
#include "data/Database.h"
#include "data/MedicineRepository.h"
#include "data/SaleRepository.h"
#include "data/SettingsRepository.h"
#include "data/SupplierRepository.h"
#include "data/UserRepository.h"
#include "domain/AppPaths.h"
#include "domain/Bcrypt.h"
#include "domain/SettingsKeys.h"
#include "service/GrnService.h"
#include "service/SaleService.h"
#include "ui/AdminPage.h"
#include "ui/ChangePinDialog.h"
#include "ui/DashboardPage.h"
#include "ui/GrnDialog.h"
#include "ui/GrnLineDialog.h"
#include "ui/InventoryPage.h"
#include "ui/LoginDialog.h"
#include "ui/MedicineDialog.h"
#include "ui/MedicinesPage.h"
#include "ui/PosTerminalPage.h"
#include "ui/PurchasingPage.h"
#include "ui/ReceiptDialog.h"
#include "ui/ReportsPage.h"
#include "ui/ReturnsPage.h"
#include "ui/SessionsPage.h"
#include "ui/SetupWizard.h"
#include "ui/StockInDialog.h"
#include "ui/Theme.h"
#include "ui/SupplierDialog.h"
#include "ui/UserDialog.h"

#include <QApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextStream>

static QString g_outDir;

static void shot(QWidget *w, const QString &name, int width = 1200, int height = 760)
{
    w->resize(width, height);
    w->show();
    QApplication::processEvents();
    QApplication::processEvents();
    const QString path = g_outDir + QLatin1Char('/') + name + QStringLiteral(".png");
    w->grab().save(path);
    QTextStream(stdout) << "  wrote " << path << "\n";
    w->hide();
}

static qint64 seed(QSqlDatabase db)
{
    SettingsRepository settings(db);
    UserRepository users(db);
    AdminDraft a;
    a.fullName = QStringLiteral("Demo Admin");
    a.username = QStringLiteral("demo");
    a.pinHash = Bcrypt::hash(QStringLiteral("864209"));
    a.pharmacyName = QStringLiteral("City Care Pharmacy");
    const qint64 uid = users.createAdmin(a);
    settings.set(SettingsKeys::PharmacyName, QStringLiteral("City Care Pharmacy"), uid);
    settings.set(SettingsKeys::PharmacyPhone, QStringLiteral("042-111-222-333"), uid);
    // Demo logo so the header/receipt logo is visible in screenshots.
    AppPaths::ensureDirs();
    const QString demoLogo = AppPaths::logosDir() + QStringLiteral("/logo.png");
    QFile::remove(demoLogo);
    QFile::copy(QStringLiteral(":/icon.png"), demoLogo);
    settings.set(SettingsKeys::LogoPath, demoLogo, uid);

    SupplierRepository sup(db);
    SupplierRow s;
    s.name = QStringLiteral("Acme Pharma Distributors");
    s.contactPhone = QStringLiteral("0300-1234567");
    s.paymentTerms = QStringLiteral("CREDIT_30");
    const qint64 supId = sup.create(s, uid);

    MedicineRepository meds(db);
    struct M
    {
        QString sku, brand, gen, str, form, base, pur;
        int upp, reorder;
        QString sched;
    };
    const QVector<M> list = {
        {"PARA500", "Panadol", "Paracetamol", "500mg", "TABLET", "TABLET", "BOX", 100, 50, "NONE"},
        {"AUGM625", "Augmentin", "Amoxicillin+Clavulanate", "625mg", "TABLET", "TABLET", "BOX", 14,
         20, "NONE"},
        {"BRUF400", "Brufen", "Ibuprofen", "400mg", "TABLET", "TABLET", "BOX", 100, 200, "NONE"},
        {"VENT100", "Ventolin", "Salbutamol", "100mcg", "INHALER_MDI", "INHALER", "BOX", 1, 5,
         "NONE"},
        {"DISP120", "Disprol", "Paracetamol", "120mg/5ml", "SYRUP", "BOTTLE", "BOX", 1, 10, "NONE"},
        {"MORPH10", "Morphine Sulphate", "Morphine", "10mg/ml", "INJECTION_AMPOULE", "AMPOULE",
         "BOX", 10, 5, "NARCOTIC"},
    };
    QVector<qint64> ids;
    for (const M &m : list) {
        MedicineDraft d;
        d.sku = m.sku;
        d.brandName = m.brand;
        d.genericName = m.gen;
        d.strength = m.str;
        d.form = m.form;
        d.baseUnit = m.base;
        d.purchaseUnit = m.pur;
        d.unitsPerPurchase = m.upp;
        d.reorderLevel = m.reorder;
        d.controlledSchedule = m.sched;
        d.manufacturer = QStringLiteral("Demo Pharma Ltd");
        ids << meds.create(d, uid);
    }

    // GRN: stock most items well; Brufen stays below reorder; Augmentin near-expiry.
    GrnService grn(db, uid);
    auto line = [](qint64 mid, const QString &batch, const QDate &exp, int paid, int foc,
                   const QString &cost, const QString &mrp) {
        GrnLineInput l;
        l.medicineId = mid;
        l.batchNumber = batch;
        l.expiry = exp;
        l.paidQty = paid;
        l.focQty = foc;
        l.unitCost = cost;
        l.mrpPerPurchaseUnit = mrp;
        return l;
    };
    const QDate far = QDate::currentDate().addYears(2);
    const QDate soon = QDate::currentDate().addDays(20);
    grn.post(supId,
             {
                 line(ids[0], QStringLiteral("PA-2401"), far, 10, 1, QStringLiteral("180.00"),
                      QStringLiteral("300.00")),
                 line(ids[1], QStringLiteral("AU-2402"), soon, 8, 0, QStringLiteral("420.00"),
                      QStringLiteral("650.00")),
                 line(ids[2], QStringLiteral("BR-2403"), far, 1, 0, QStringLiteral("250.00"),
                      QStringLiteral("420.00")),
                 line(ids[3], QStringLiteral("VE-2404"), far, 12, 0, QStringLiteral("550.00"),
                      QStringLiteral("780.00")),
                 line(ids[4], QStringLiteral("DI-2405"), far, 20, 2, QStringLiteral("90.00"),
                      QStringLiteral("150.00")),
                 line(ids[5], QStringLiteral("MO-2406"), far, 4, 0, QStringLiteral("1200.00"),
                      QStringLiteral("1800.00")),
             },
             QStringLiteral("INV-2024-5567"), QDate::currentDate());

    // A few sales so the report + receipts have data.
    auto sell = [&](qint64 mid, int qty, const QString &mrp, const QString &mode,
                    const QString &tendered) {
        SaleService sale(db, uid);
        SaleInput in;
        in.paymentMode = mode;
        in.amountTendered = tendered;
        SaleLineInput li;
        li.medicineId = mid;
        li.qtySoldDisplay = qty;
        li.soldUnitLabel = QStringLiteral("unit");
        li.soldUnitFactor = 1;
        li.unitMrp = mrp;
        in.items << li;
        return sale.commit(in);
    };
    sell(ids[0], 20, QStringLiteral("3.00"), QStringLiteral("CASH"), QStringLiteral("100"));
    sell(ids[3], 1, QStringLiteral("780.00"), QStringLiteral("CARD"), QString());
    sell(ids[4], 2, QStringLiteral("150.00"), QStringLiteral("CASH"), QStringLiteral("500"));
    return uid;
}

static void clickButton(QWidget *w, const QString &textNeedle)
{
    for (QPushButton *b : w->findChildren<QPushButton *>()) {
        if (b->text().contains(textNeedle, Qt::CaseInsensitive)) {
            b->click();
            QApplication::processEvents();
            return;
        }
    }
}

static void setSearch(QWidget *page, const QString &needle, const QString &text)
{
    for (QLineEdit *e : page->findChildren<QLineEdit *>()) {
        if (e->placeholderText().contains(needle, Qt::CaseInsensitive)) {
            e->setText(text);
            QApplication::processEvents();
            return;
        }
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("PharmaDesk"));
    app.setApplicationName(QStringLiteral("PharmaDesk Shots"));
    QStandardPaths::setTestModeEnabled(true);

    QFile qss(QStringLiteral(":/theme.qss"));
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
    }

    g_outDir = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("screenshots");
    QDir().mkpath(g_outDir);

    // Start every run from a truly fresh DB (test-mode location) so seeding is
    // deterministic and createAdmin returns a valid id (a stale DB made it fail
    // and hand back -1, which broke role-gated screenshots).
    AppPaths::ensureDirs();
    for (const QString &suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm")}) {
        QFile::remove(AppPaths::dbFilePath() + suffix);
    }

    Database db;
    if (!db.open()) {
        return 1;
    }
    if (db.isFreshDb()) {
        db.bootstrap();
    }
    db.migrate();
    const qint64 uid = seed(db.handle());

    UserRecord admin;
    admin.id = uid;
    admin.fullName = QStringLiteral("Demo Admin");
    admin.username = QStringLiteral("demo");
    admin.role = QStringLiteral("ADMIN");
    admin.branchId = 1;
    admin.valid = true;

    QSqlDatabase h = db.handle();

    // Everything below is rendered once per theme (light + dark) — see the loop
    // at the end of main().
    auto renderAll = [&]() {
        {
            MainWindow w(h, admin);
            shot(&w, QStringLiteral("01_main_pos"), 1280, 800);
        }
        {
            DashboardPage p(h);
            shot(&p, QStringLiteral("00_dashboard"), 1280, 800);
        }
        {
            LoginDialog d(new UserRepository(h), new SettingsRepository(h));
            shot(&d, QStringLiteral("02_login"), 420, 460);
        }
        {
            PosTerminalPage p(h, uid);
            setSearch(&p, QStringLiteral("medicine to sell"), QStringLiteral("a"));
            shot(&p, QStringLiteral("03_pos"));
        }
        {
            MedicinesPage p(h, uid);
            shot(&p, QStringLiteral("04_medicines"));
        }
        {
            InventoryPage p(h, uid);
            shot(&p, QStringLiteral("05_inventory_overview"));
            if (auto *t = p.findChild<QTabWidget *>()) {
                t->setCurrentIndex(1);
                QApplication::processEvents();
                shot(&p, QStringLiteral("06_inventory_batches"));
            }
        }
        {
            PurchasingPage p(h, uid);
            shot(&p, QStringLiteral("07_purchasing"));
        }
        {
            ReportsPage p(h);
            shot(&p, QStringLiteral("08_reports"));
        }
        {
            AdminPage p(h, admin);
            shot(&p, QStringLiteral("09_admin_users"));
            if (auto *t = p.findChild<QTabWidget *>()) {
                t->setCurrentIndex(1);
                QApplication::processEvents();
                shot(&p, QStringLiteral("09b_admin_printer"));
                t->setCurrentIndex(3);
                QApplication::processEvents();
                shot(&p, QStringLiteral("10_admin_audit"));
            }
        }
        {
            MedicineDialog d(new MedicineRepository(h), uid, 0);
            shot(&d, QStringLiteral("11_medicine_dialog"), 540, 660);
        }
        {
            GrnDialog d(h, uid);
            shot(&d, QStringLiteral("12_grn_dialog"), 640, 580);
        }

        // POS with a populated cart (add two products, enter tendered).
        {
            PosTerminalPage p(h, uid);
            setSearch(&p, QStringLiteral("medicine to sell"), QStringLiteral("a"));
            auto tables = p.findChildren<QTableWidget *>();
            if (!tables.isEmpty()) {
                tables.first()->selectRow(0);
                clickButton(&p, QStringLiteral("Add unit")); // a base-unit line
                setSearch(&p, QStringLiteral("medicine to sell"), QStringLiteral("a"));
                tables.first()->selectRow(0);
                clickButton(&p, QStringLiteral("Add pack")); // a pack line
            }
            shot(&p, QStringLiteral("13_pos_cart"));
        }

        // Receipt for the first committed sale.
        {
            SaleRepository sr(h);
            SaleResult sale = sr.loadReceipt(1);
            ReceiptDialog d(sale, QStringLiteral("City Care Pharmacy"),
                            SettingsRepository(h).get(SettingsKeys::LogoPath));
            shot(&d, QStringLiteral("14_receipt"), 380, 520);
        }

        qint64 medId = 0;
        {
            QSqlQuery q(h);
            if (q.exec(QStringLiteral("SELECT id FROM medicines ORDER BY id LIMIT 1")) && q.next())
                medId = q.value(0).toLongLong();
        }

        {
            SupplierDialog d(new SupplierRepository(h), uid, 0);
            shot(&d, QStringLiteral("15_supplier_dialog"), 460, 500);
        }
        {
            UserDialog d(new UserRepository(h), uid, 0, QString(), QString(),
                         QStringLiteral("CASHIER"), true);
            shot(&d, QStringLiteral("16_user_dialog"), 420, 320);
        }
        {
            ChangePinDialog d(new UserRepository(h), uid);
            shot(&d, QStringLiteral("17_changepin_dialog"), 380, 240);
        }
        {
            StockInDialog d(new BatchRepository(h), uid, medId, QStringLiteral("Panadol 500mg"));
            shot(&d, QStringLiteral("18_stockin_dialog"), 400, 360);
        }

        // GRN line entry, populated.
        {
            GrnLineDialog d(h);
            setSearch(&d, QStringLiteral("medicine to receive"), QStringLiteral("pan"));
            if (auto *t = d.findChild<QTableWidget *>()) t->selectRow(0);
            QApplication::processEvents();
            shot(&d, QStringLiteral("19_grn_line_dialog"), 480, 640);
        }

        // Setup wizard: welcome, then the pharmacy-identity page.
        {
            Database *wdb = &db;
            SetupWizard wiz(wdb, new SettingsRepository(h), new UserRepository(h));
            wiz.show();
            QApplication::processEvents();
            shot(&wiz, QStringLiteral("20_wizard_welcome"), 600, 540);
            wiz.next();
            QApplication::processEvents();
            shot(&wiz, QStringLiteral("21_wizard_pharmacy"), 600, 540);
            wiz.next();
            wiz.next();
            QApplication::processEvents();
            shot(&wiz, QStringLiteral("22_wizard_admin"), 600, 540);
        }

        {
            SessionsPage p(h, uid);
            shot(&p, QStringLiteral("24_sessions"));
        }
        {
            ReturnsPage p(h, uid);
            shot(&p, QStringLiteral("25_returns"));
        }
    }; // renderAll

    // Render every screen in BOTH the light and the dark (theme 6) palette, into
    // <outDir>/light and <outDir>/dark.
    struct Pass
    {
        QString sub;
        QString key;
    };
    const Pass passes[] = {
        {QStringLiteral("light"), QString()},
        {QStringLiteral("dark"), QStringLiteral("dark")},
    };
    const QString baseDir = g_outDir;
    for (const Pass &pass : passes) {
        g_outDir = baseDir + QLatin1Char('/') + pass.sub;
        QDir().mkpath(g_outDir);
        Theme::apply(pass.key);
        QApplication::processEvents();
        QTextStream(stdout) << "== " << pass.sub << " theme ==\n";
        renderAll();
    }

    // ── Responsive diagnostic: render heavy screens at min-size + 4K ────────
    {
        g_outDir = baseDir + QStringLiteral("/sizes");
        QDir().mkpath(g_outDir);
        Theme::apply(QString());
        {
            MainWindow w(h, admin);
            shot(&w, QStringLiteral("mw_min"), 1024, 640);
            shot(&w, QStringLiteral("mw_4k"), 3840, 2160);
        }
        {
            AdminPage p(h, admin);
            shot(&p, QStringLiteral("admin_min"), 1024, 640);
            shot(&p, QStringLiteral("admin_1920"), 1920, 1080);
            shot(&p, QStringLiteral("admin_4k"), 3840, 2160);
        }
        {
            ReportsPage p(h);
            shot(&p, QStringLiteral("reports_4k"), 3840, 2160);
        }
    }

    QTextStream(stdout) << "done\n";
    return 0;
}
