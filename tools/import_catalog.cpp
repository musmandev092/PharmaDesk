// Standalone CLI to load a medicine catalog CSV (the legacy Postgres export)
// into the PharmaDesk SQLite database. Idempotent — existing SKUs are skipped.
//
//   pharmadesk_import <path-to-medicines.csv>
//
// Operates on the real app database (AppPaths::dbFilePath()); run it once after
// first-run setup. Not shipped in the GUI.

#include "data/CatalogImporter.h"
#include "Branding.h"
#include "data/Database.h"
#include "data/UserRepository.h"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    // Must match the GUI app's identity so both resolve the SAME data dir
    // (QStandardPaths::AppDataLocation = ~/.local/share/<org>/<app>). A distinct
    // applicationName here would import into an isolated database the app can't see.
    app.setApplicationName(Branding::productName());
    app.setOrganizationName(Branding::organizationName());

    QTextStream out(stdout);
    if (argc < 2) {
        out << "usage: pharmadesk_import <catalog.csv>\n";
        return 2;
    }

    Database db;
    if (!db.open()) {
        out << "error: cannot open database: " << db.errorString() << "\n";
        return 2;
    }
    if (db.isFreshDb()) {
        out << "error: database has no schema yet — run the app once to set up first.\n";
        return 2;
    }
    if (!db.migrate()) {
        out << "error: schema upgrade failed: " << db.errorString() << "\n";
        return 2;
    }

    // Attribute the import to the first ADMIN (for the audit trail).
    qint64 adminId = 0;
    QSqlQuery q(db.handle());
    if (q.exec(QStringLiteral("SELECT id FROM users WHERE role='ADMIN' "
                              "AND is_active=1 AND deleted_at IS NULL ORDER BY id LIMIT 1"))
        && q.next()) {
        adminId = q.value(0).toLongLong();
    }
    if (adminId <= 0) {
        out << "error: no active admin user found — complete first-run setup first.\n";
        return 2;
    }

    const CatalogImporter::Result r
        = CatalogImporter::importFromCsv(db.handle(), QString::fromLocal8Bit(argv[1]), adminId);
    if (!r.ok) {
        out << "error: " << r.error << "\n";
        return 1;
    }
    out << QStringLiteral("imported %1, skipped %2, failed %3\n")
               .arg(r.imported)
               .arg(r.skipped)
               .arg(r.failed);
    return r.failed == 0 ? 0 : 1;
}
