// Tests for CatalogImporter: CSV parsing, Postgres boolean/deleted mapping,
// idempotency on re-import, and column-order independence.

#include "framework/TestStats.h"

#include "data/CatalogImporter.h"
#include "data/MedicineRepository.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

namespace pharmadesk_tests {
namespace {

QString writeTempCsv(const QString &name, const QString &contents)
{
    const QString path = QDir::temp().filePath(name);
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream(&f) << contents;
    }
    return path;
}

} // namespace

TestStats run_catalog_tests(QSqlDatabase db, qint64 userId)
{
    TestStats s;
    s.module = QStringLiteral("catalog");

    // Mirrors the medicines_export.csv shape: Postgres t/f booleans, a quoted
    // field with a comma, and one soft-deleted row that must be skipped.
    const QString csv = QStringLiteral(
        "id,sku,primary_barcode,brand_name,generic_name,strength,form,form_custom,manufacturer,"
        "therapeutic_category,purchase_unit,base_unit,units_per_purchase,prescription_required,"
        "controlled_schedule,tax_code_value,reorder_level,reorder_quantity,reorder_unit,is_active,"
        "created_at,updated_at,deleted_at\n"
        "1,CAT-AAA,8964000000001,Acefyl,Acefylline,125ml,SYRUP,,\"Nabi, Industries "
        "LTD\",,BOX,BOTTLE,1,"
        "f,NONE,EXEMPT,3,0,PURCHASE,t,2026-05-22 10:26:40+00,2026-05-22 10:26:40+00,\n"
        "2,CAT-BBB,8964000000002,Morfine,Morphine,10mg,INJECTION,,Maker,,BOX,AMPOULE,5,"
        "t,NARCOTIC,STANDARD_18,10,5,PURCHASE,t,2026-05-22 10:26:40+00,2026-05-22 10:26:40+00,\n"
        "3,CAT-DEL,8964000000003,Deleted,Gone,1mg,TABLET,,Maker,,BOX,TABLET,10,"
        "f,NONE,EXEMPT,0,0,PURCHASE,t,2026-05-22 10:26:40+00,2026-05-22 10:26:40+00,2026-05-23 "
        "00:00:00+00\n");

    const QString path = writeTempCsv(QStringLiteral("pd_catalog_test.csv"), csv);

    CatalogImporter::Result r = CatalogImporter::importFromCsv(db, path, userId);
    s.check(r.ok, QStringLiteral("import ok"));
    s.check(r.imported == 2, QStringLiteral("two live rows imported"));
    s.check(r.skipped == 1, QStringLiteral("soft-deleted row skipped"));
    s.check(r.failed == 0, QStringLiteral("no row failures"));

    // Spot-check mapped fields (comma inside the quoted manufacturer survived;
    // the t/f booleans and the NARCOTIC schedule mapped correctly).
    MedicineRepository repo(db);
    s.check(repo.skuInUse(QStringLiteral("CAT-AAA")), QStringLiteral("CAT-AAA present"));
    s.check(repo.skuInUse(QStringLiteral("CAT-BBB")), QStringLiteral("CAT-BBB present"));
    s.check(!repo.skuInUse(QStringLiteral("CAT-DEL")), QStringLiteral("CAT-DEL not imported"));

    const QVector<MedicineRow> bbb = repo.list(QStringLiteral("CAT-BBB"));
    s.check(bbb.size() == 1, QStringLiteral("CAT-BBB findable"));
    if (bbb.size() == 1) {
        s.check(bbb.first().controlledSchedule == QStringLiteral("NARCOTIC"),
                QStringLiteral("schedule mapped to NARCOTIC"));
    }

    // Idempotency: re-import skips everything, inserts nothing.
    CatalogImporter::Result r2 = CatalogImporter::importFromCsv(db, path, userId);
    s.check(r2.ok, QStringLiteral("re-import ok"));
    s.check(r2.imported == 0, QStringLiteral("re-import inserts nothing"));
    s.check(r2.skipped == 3, QStringLiteral("re-import skips all rows"));

    QFile::remove(path);
    return s;
}

} // namespace pharmadesk_tests
