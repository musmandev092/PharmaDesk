// Aggregate test runner. Bootstraps an isolated test-mode SQLite DB, seeds one
// ADMIN, then runs every test module and prints per-module + total pass/fail.
// Exit code 0 only if every assertion passed.

#include "data/Database.h"
#include "data/UserRepository.h"
#include "domain/AppPaths.h"
#include "domain/Bcrypt.h"
#include "framework/TestStats.h"

#include <QCoreApplication>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

namespace pharmadesk_tests {
// Each module is authored in tests/cases/<module>_tests.cpp.
TestStats run_money_tests(QSqlDatabase, qint64);
TestStats run_salecalc_tests(QSqlDatabase, qint64);
TestStats run_costblender_tests(QSqlDatabase, qint64);
TestStats run_fefo_tests(QSqlDatabase, qint64);
TestStats run_barcode_tests(QSqlDatabase, qint64);
TestStats run_pinpolicy_tests(QSqlDatabase, qint64);
TestStats run_policy_tests(QSqlDatabase, qint64);
TestStats run_sale_tests(QSqlDatabase, qint64);
TestStats run_returns_tests(QSqlDatabase, qint64);
TestStats run_adjustment_tests(QSqlDatabase, qint64);
TestStats run_sessions_tests(QSqlDatabase, qint64);
TestStats run_grn_tests(QSqlDatabase, qint64);
TestStats run_medicine_tests(QSqlDatabase, qint64);
TestStats run_inventory_tests(QSqlDatabase, qint64);
TestStats run_analytics_tests(QSqlDatabase, qint64);
TestStats run_parked_tests(QSqlDatabase, qint64);
TestStats run_schema_tests(QSqlDatabase, qint64);
TestStats run_catalog_tests(QSqlDatabase, qint64);
TestStats run_printing_tests(QSqlDatabase, qint64);
TestStats run_auth_tests(QSqlDatabase, qint64);
TestStats run_invariant_tests(QSqlDatabase, qint64);
} // namespace pharmadesk_tests

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("PharmaDesk"));
    app.setApplicationName(QStringLiteral("PharmaDesk Tests"));
    QStandardPaths::setTestModeEnabled(true);

    QTextStream out(stdout);

    // Start every run from a clean slate so modules can use fixed identifiers.
    AppPaths::ensureDirs();
    for (const QString &suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm")}) {
        QFile::remove(AppPaths::dbFilePath() + suffix);
    }

    Database db;
    if (!db.open()) {
        out << "FATAL: cannot open test DB: " << db.errorString() << "\n";
        return 2;
    }
    if (db.isFreshDb() && !db.bootstrap()) {
        out << "FATAL: bootstrap failed: " << db.errorString() << "\n";
        return 2;
    }
    if (!db.migrate()) {
        out << "FATAL: migrate failed: " << db.errorString() << "\n";
        return 2;
    }

    UserRepository users(db.handle());
    qint64 adminId = 1;
    if (db.isFirstRun()) {
        pharmadesk_tests::TestStats ignore;
        AdminDraft a;
        a.fullName = QStringLiteral("Test Admin");
        a.username = QStringLiteral("tester");
        a.pinHash = Bcrypt::hash(QStringLiteral("481920"));
        a.pharmacyName = QStringLiteral("Test Pharmacy");
        adminId = users.createAdmin(a);
    }

    using Fn = pharmadesk_tests::TestStats (*)(QSqlDatabase, qint64);
    struct Mod
    {
        const char *name;
        Fn fn;
    };
    const Mod mods[] = {
        {"money", pharmadesk_tests::run_money_tests},
        {"salecalc", pharmadesk_tests::run_salecalc_tests},
        {"costblender", pharmadesk_tests::run_costblender_tests},
        {"fefo", pharmadesk_tests::run_fefo_tests},
        {"barcode", pharmadesk_tests::run_barcode_tests},
        {"pinpolicy", pharmadesk_tests::run_pinpolicy_tests},
        {"policy", pharmadesk_tests::run_policy_tests},
        {"sale", pharmadesk_tests::run_sale_tests},
        {"returns", pharmadesk_tests::run_returns_tests},
        {"adjustment", pharmadesk_tests::run_adjustment_tests},
        {"sessions", pharmadesk_tests::run_sessions_tests},
        {"grn", pharmadesk_tests::run_grn_tests},
        {"medicine", pharmadesk_tests::run_medicine_tests},
        {"inventory", pharmadesk_tests::run_inventory_tests},
        {"analytics", pharmadesk_tests::run_analytics_tests},
        {"parked", pharmadesk_tests::run_parked_tests},
        {"schema", pharmadesk_tests::run_schema_tests},
        {"catalog", pharmadesk_tests::run_catalog_tests},
        {"printing", pharmadesk_tests::run_printing_tests},
        {"auth", pharmadesk_tests::run_auth_tests},
        {"invariant", pharmadesk_tests::run_invariant_tests}, // runs last: scans the whole DB
    };

    int totalPass = 0, totalFail = 0;
    QStringList allFailures;
    out << "==== PMS automated test suite ====\n";
    for (const Mod &m : mods) {
        const pharmadesk_tests::TestStats s = m.fn(db.handle(), adminId);
        out << QStringLiteral("  %1%2  %3 passed, %4 failed\n")
                   .arg(QString::fromLatin1(m.name), -14)
                   .arg(QString(), 0)
                   .arg(s.passed, 5)
                   .arg(s.failed);
        totalPass += s.passed;
        totalFail += s.failed;
        allFailures += s.failures;
    }

    out << "----------------------------------\n";
    out << QStringLiteral("TOTAL: %1 assertions, %2 passed, %3 failed\n")
               .arg(totalPass + totalFail)
               .arg(totalPass)
               .arg(totalFail);
    if (!allFailures.isEmpty()) {
        out << "\nFAILURES (first 40):\n";
        for (int i = 0; i < allFailures.size() && i < 40; ++i) {
            out << "  ✗ " << allFailures.at(i) << "\n";
        }
    }
    out << (totalFail == 0 ? "\nALL PASSED\n" : QStringLiteral("\n%1 FAILED\n").arg(totalFail));
    return totalFail == 0 ? 0 : 1;
}
