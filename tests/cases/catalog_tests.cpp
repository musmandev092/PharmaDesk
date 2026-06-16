// Tests for CatalogImporter: CSV parsing, Postgres boolean/deleted mapping,
// idempotency on re-import, and column-order independence.

#include "framework/TestStats.h"

#include "data/CatalogImporter.h"
#include "data/MedicineRepository.h"

#include <QDir>
#include <QFile>
#include <QSqlQuery>
#include <QTextStream>
#include <QVariant>

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

    // ── Friendly spreadsheet headers: "Brand Name" (space) normalizes to
    //    brand_name, and "Yes"/"No" map to prescription_required. ────────────
    const QString friendly = QStringLiteral(
        "SKU,Brand Name,Generic Name,Form,Purchase Unit,Base Unit,Units Per Purchase,"
        "Prescription Required\n"
        "FR-RX,Panadol,Paracetamol,TABLET,BOX,TABLET,10,Yes\n"
        "FR-OTC,Vitamin C,Ascorbic Acid,TABLET,BOX,TABLET,10,No\n");
    const QString fpath = writeTempCsv(QStringLiteral("pd_catalog_friendly.csv"), friendly);
    CatalogImporter::Result rf = CatalogImporter::importFromFile(db, fpath, userId);
    s.check(rf.ok && rf.imported == 2, QStringLiteral("friendly-header import: 2 rows"));
    s.check(repo.skuInUse(QStringLiteral("FR-RX")), QStringLiteral("friendly: FR-RX present"));
    {
        QSqlQuery pq(db);
        pq.prepare(QStringLiteral("SELECT prescription_required FROM medicines WHERE sku = ?"));
        pq.addBindValue(QStringLiteral("FR-RX"));
        s.check(pq.exec() && pq.next() && pq.value(0).toInt() == 1,
                QStringLiteral("friendly: 'Yes' -> prescription_required=1"));
        QSqlQuery pq2(db);
        pq2.prepare(QStringLiteral("SELECT prescription_required FROM medicines WHERE sku = ?"));
        pq2.addBindValue(QStringLiteral("FR-OTC"));
        s.check(pq2.exec() && pq2.next() && pq2.value(0).toInt() == 0,
                QStringLiteral("friendly: 'No' -> prescription_required=0"));
    }
    QFile::remove(fpath);

    // ── Excel .xlsx import (real ZIP + DEFLATE + shared strings). The fixture is
    //    a minimal workbook authored with shared strings, the form real Excel
    //    emits, so this exercises XlsxReader end-to-end. ──────────────────────
    const QByteArray xlsxB64
        = "UEsDBBQAAAAIACW70FzfaR5QDAEAALUCAAATAAAAW0NvbnRlbnRfVHlwZXNdLnhtbK1SyU7DMBD9FcvXKnbLASGU"
          "tAeWIyBR"
          "PmCwJ4lVb/K4Jf17nLQghAq99DSy36rR1KvBWbbDRCb4hi/EnDP0Kmjju4a/"
          "rR+rG84og9dgg8eG75H4almv9xGJFa2nhvc5"
          "x1spSfXogESI6AvShuQgl2fqZAS1gQ7l1Xx+LVXwGX2u8ujBl/"
          "U9trC1mT0M5fvQI6Elzu4OxDGr4RCjNQpyweXO618p1TFB"
          "FOXEod5EmhUClycTRuTvgKPuuSwmGY3sBVJ+AldYcrDyI6TNewgb8b/"
          "JiZahbY1CHdTWFYmgmBA09YjZWTFN4cD42fn8iUxy"
          "GosLF/"
          "n2P9ODekioX3Mqx0IXX8YP768ecjq75SdQSwMEFAAAAAgAJbvQXJja64uuAAAAJwEAAAsAAABfcmVscy8ucmVsc4"
          "3P"
          "wQ6CMAwG4FdZepeBB2MMg4sx4WrwAeZWBgHWZZsKb++OYjx4bPr3+"
          "9OyXuaJPdGHgayAIsuBoVWkB2sE3NrL7ggsRGm1nMii"
          "gBUD1FV5xUnGdBL6wQWWDBsE9DG6E+dB9TjLkJFDmzYd+VnGNHrDnVSjNMj3eX7g/tOArckaLcA3ugDWrg7/"
          "sanrBoVnUo8Z"
          "bfxR8ZVIsvQGo4Bl4i/y451ozBIKvCr55sHqDVBLAwQUAAAACAAlu9Bc3j+H/"
          "L4AAAAeAQAADwAAAHhsL3dvcmtib29rLnht"
          "bI2Pu27DMAxFf0Xg3sjOEBSGH0tRIEO29gNUiY6FSKRBKn38fYW43jvxhXt5bj9952Q+"
          "UTQyDdAeGjBInkOk6wDvb69Pz2C0"
          "OAouMeEAP6gwjf0Xy+2D+WaqnHSApZS1s1b9gtnpgVekeplZsit1lKvVVdAFXRBLTvbYNCebXSTYHDr5jwfPc/"
          "T4wv6ekcpm"
          "IphcqfC6xFVh7B8f9K8acrlCXzBEH6mim8f6HGpUMNLF2sg5tGDH3u5Ku4cbfwFQSwMEFAAAAAgAJbvQXNbffJbI"
          "AAAAtQEA"
          "ABoAAAB4bC9fcmVscy93b3JrYm9vay54bWwucmVsc62Qz2rDMAyHX8XovijpYZRRt5cy6HXrHkDYShya2Eby1vbt"
          "awb7E+hh"
          "h52EJPTp47fZXebJfLDomKKFrmnBcHTJj3Gw8HZ8fliD0ULR05QiW7iywm67eeGJSj3RMGY1lRHVQiglPyGqCzyT"
          "NilzrJs+"
          "yUyltjJgJneigXHVto8ovxmwZJqDtyAH34E5XjP/hZ36fnS8T+595ljuvMBzkpMG5lKhJAMXC98jxc/"
          "SNZUKeF9m9Z8yGkjY"
          "vxapSeuP0GL8JYOLuLc3UEsDBBQAAAAIACW70Fwq8aEN/"
          "gAAACUCAAAUAAAAeGwvc2hhcmVkU3RyaW5ncy54bWyNkcFKxEAM"
          "hl+lzH13Wg+LSNvFXdaT4MEK6qWM09gOdjLdJBXXp3eKqNAqeMz3J/"
          "OFTL59833yCsQuYKGydaoSQBsah22h7qqr1blKWAw2"
          "pg8IhToBq22ZM0sSJ5EL1YkMF1qz7cAbXocBMCbPgbyRWFKreSAwDXcA4nt9lqYb7Y1DldgwokTrRiUjuuMI+"
          "28QFa7MpeSX"
          "MddS5noqP9ETxXVqNB7mSQsI5Oyv2bTQnA0j2c4w1NEuC81fwcS4HoDqr/"
          "nFuwRsyQ0Sj1oTHEdH0MybHqvDbbXKFvhEAnZO"
          "9yCO3LvDhau63F0fqjnd3dz/rzFL5+QB+Afp+NHlB1BLAwQUAAAACAAlu9BcoElps/"
          "AAAACPAgAAGAAAAHhsL3dvcmtzaGVl"
          "dHMvc2hlZXQxLnhtbF3S0U7DIBQG4Fch597RVjenARa16/YA+gCkxbWxhQZIp28vLqY5hzvgg4T/"
          "B3H4nka2GB8GZyWUmwKY"
          "sa3rBnuR8PHe3O2Bhahtp0dnjYQfE+CgxNX5r9AbE1k6b4OEPsb5mfPQ9mbSYeNmY5N8Oj/"
          "pmKb+wsPsje5uh6aRV0Wx45Me"
          "LChxW6t11Ep4d2U+3SOttn+"
          "DlxJYlBDSfFGF4IsSvP23V2wltTdsFbUa2z21I7YHag22LbUTth21M7bH1XjKugau1sAV2rzP"
          "AmN7ygJjK7OmaoJZVUeCWVcNwaysE8GsrTPBbRabozfn62dSv1BLAQIUAxQAAAAIACW70FzfaR5QDAEAALUCAAAT"
          "AAAAAAAA"
          "AAAAAACAAQAAAABbQ29udGVudF9UeXBlc10ueG1sUEsBAhQDFAAAAAgAJbvQXJja64uuAAAAJwEAAAsAAAAAAAAA"
          "AAAAAIAB"
          "PQEAAF9yZWxzLy5yZWxzUEsBAhQDFAAAAAgAJbvQXN4/h/"
          "y+AAAAHgEAAA8AAAAAAAAAAAAAAIABFAIAAHhsL3dvcmtib29r"
          "LnhtbFBLAQIUAxQAAAAIACW70FzW33yWyAAAALUBAAAaAAAAAAAAAAAAAACAAf8CAAB4bC9fcmVscy93b3JrYm9v"
          "ay54bWwu"
          "cmVsc1BLAQIUAxQAAAAIACW70Fwq8aEN/"
          "gAAACUCAAAUAAAAAAAAAAAAAACAAf8DAAB4bC9zaGFyZWRTdHJpbmdzLnhtbFBL"
          "AQIUAxQAAAAIACW70FygSWmz8AAAAI8CAAAYAAAAAAAAAAAAAACAAS8FAAB4bC93b3Jrc2hlZXRzL3NoZWV0MS54"
          "bWxQSwUG"
          "AAAAAAYABgCHAQAAVQYAAAAA";
    const QString xpath = QDir::temp().filePath(QStringLiteral("pd_catalog_test.xlsx"));
    {
        QFile xf(xpath);
        if (xf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            xf.write(QByteArray::fromBase64(xlsxB64));
        }
    }
    CatalogImporter::Result rx = CatalogImporter::importFromFile(db, xpath, userId);
    s.check(rx.ok, QStringLiteral("xlsx import ok"));
    s.check(rx.imported == 1, QStringLiteral("xlsx: one medicine imported"));
    s.check(repo.skuInUse(QStringLiteral("ZTEST-1")),
            QStringLiteral("xlsx: shared-string SKU 'ZTEST-1' resolved + imported"));
    {
        const QVector<MedicineRow> z = repo.list(QStringLiteral("ZTEST-1"));
        s.check(z.size() == 1 && z.first().brandName == QStringLiteral("Zyrtec"),
                QStringLiteral("xlsx: brand 'Zyrtec' read from shared strings"));
    }
    QFile::remove(xpath);
    return s;
}

} // namespace pharmadesk_tests
