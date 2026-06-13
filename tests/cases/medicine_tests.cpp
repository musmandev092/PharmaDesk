// DB-backed tests for MedicineRepository: CRUD, uniqueness, soft delete, search,
// POS search (unit_mrp + in_stock).

#include "framework/TestStats.h"

#include "data/BatchRepository.h"
#include "data/MedicineRepository.h"

#include <QDate>

namespace pharmadesk_tests {
namespace {
int g_u = 0;
MedicineDraft draft(const QString &sku)
{
    MedicineDraft d;
    d.sku = sku;
    d.brandName = QStringLiteral("MedBrand");
    d.genericName = QStringLiteral("MedGeneric");
    d.strength = QStringLiteral("500mg");
    d.form = QStringLiteral("TABLET");
    d.baseUnit = QStringLiteral("TABLET");
    d.purchaseUnit = QStringLiteral("BOX");
    d.unitsPerPurchase = 10;
    return d;
}
} // namespace

TestStats run_medicine_tests(QSqlDatabase db, qint64 userId)
{
    TestStats s;
    s.module = QStringLiteral("medicine");

    MedicineRepository repo(db);

    // Create + find round-trip.
    const QString sku = QStringLiteral("TMED_%1").arg(++g_u);
    MedicineDraft d = draft(sku);
    d.primaryBarcode = QStringLiteral("8901234%1").arg(g_u, 5, 10, QLatin1Char('0'));
    d.manufacturer = QStringLiteral("Acme");
    d.form = QStringLiteral("OTHER");
    d.formCustom = QStringLiteral("Spansule");
    d.controlledSchedule = QStringLiteral("SCHEDULE_G");
    d.taxCode = QStringLiteral("STANDARD_18");
    d.reorderLevel = 25;
    d.prescriptionRequired = true;
    const qint64 id = repo.create(d, userId);
    s.check(id > 0, QStringLiteral("create returns id"));

    MedicineDraft got;
    s.check(repo.find(id, &got), QStringLiteral("find succeeds"));
    s.check(got.sku == sku, QStringLiteral("sku round-trips"));
    s.check(got.brandName == d.brandName, QStringLiteral("brand round-trips"));
    s.check(got.primaryBarcode == d.primaryBarcode, QStringLiteral("barcode round-trips"));
    s.check(got.form == QStringLiteral("OTHER"), QStringLiteral("form round-trips"));
    s.check(got.formCustom == QStringLiteral("Spansule"),
            QStringLiteral("form_custom round-trips"));
    s.check(got.controlledSchedule == QStringLiteral("SCHEDULE_G"),
            QStringLiteral("schedule round-trips"));
    s.check(got.taxCode == QStringLiteral("STANDARD_18"), QStringLiteral("tax code round-trips"));
    s.check(got.unitsPerPurchase == 10, QStringLiteral("units/pack round-trips"));
    s.check(got.reorderLevel == 25, QStringLiteral("reorder level round-trips"));
    s.check(got.prescriptionRequired, QStringLiteral("rx flag round-trips"));

    // Uniqueness (live rows).
    s.check(repo.skuInUse(sku), QStringLiteral("skuInUse true for existing"));
    s.check(repo.skuInUse(sku, id) == false, QStringLiteral("skuInUse excludes self via exceptId"));
    s.check(!repo.skuInUse(QStringLiteral("TMED_NOPE_%1").arg(g_u)),
            QStringLiteral("unused sku free"));
    s.check(repo.barcodeInUse(d.primaryBarcode), QStringLiteral("barcodeInUse true for existing"));
    s.check(!repo.barcodeInUse(QString()), QStringLiteral("empty barcode never in use"));

    // Duplicate live sku create fails (partial-unique index).
    const qint64 dup = repo.create(draft(sku), userId);
    s.check(dup < 0, QStringLiteral("duplicate live sku rejected"));

    // Update changes fields.
    got.brandName = QStringLiteral("Renamed");
    got.reorderLevel = 99;
    got.isActive = false;
    s.check(repo.update(id, got, userId), QStringLiteral("update ok"));
    MedicineDraft after;
    repo.find(id, &after);
    s.check(after.brandName == QStringLiteral("Renamed"), QStringLiteral("brand updated"));
    s.check(after.reorderLevel == 99, QStringLiteral("reorder updated"));
    s.check(!after.isActive, QStringLiteral("is_active updated"));

    // List search finds it (by sku substring).
    {
        const QVector<MedicineRow> rows = repo.list(sku);
        bool found = false;
        for (const MedicineRow &r : rows)
            if (r.id == id) found = true;
        s.check(found, QStringLiteral("list search finds by sku"));
    }

    // Soft delete: gone from list; sku becomes reusable (partial-unique).
    s.check(repo.softDelete(id, userId), QStringLiteral("soft delete ok"));
    {
        const QVector<MedicineRow> rows = repo.list(sku);
        bool found = false;
        for (const MedicineRow &r : rows)
            if (r.id == id) found = true;
        s.check(!found, QStringLiteral("deleted medicine not in list"));
    }
    s.check(!repo.skuInUse(sku), QStringLiteral("sku free after soft delete"));
    const qint64 reuse = repo.create(draft(sku), userId);
    s.check(reuse > 0, QStringLiteral("sku reusable after soft delete"));

    // POS search returns unit_mrp + in_stock after stocking.
    {
        const QString sku2 = QStringLiteral("TMED_POS_%1").arg(++g_u);
        MedicineDraft p = draft(sku2);
        p.brandName = QStringLiteral("PosSearchMed");
        const qint64 pid = repo.create(p, userId);
        // No stock yet → inStock 0, unitMrp empty.
        {
            const QVector<PosMedicine> hits = repo.searchForPos(QStringLiteral("PosSearchMed"));
            bool ok = false;
            for (const PosMedicine &h : hits)
                if (h.id == pid) ok = (h.inStock == 0 && h.unitMrp.isEmpty());
            s.check(ok, QStringLiteral("POS search: no stock → 0 / empty mrp"));
        }
        BatchRepository b(db);
        StockInDraft st;
        st.medicineId = pid;
        st.batchNumber = QStringLiteral("TMED_POSB_%1").arg(g_u);
        st.expiry = QDate::currentDate().addYears(1);
        st.quantity = 40;
        st.costPerUnit = QStringLiteral("2.0000");
        st.mrpPerUnit = QStringLiteral("5.00");
        b.addStock(st, userId);
        const QVector<PosMedicine> hits = repo.searchForPos(QStringLiteral("PosSearchMed"));
        bool ok = false;
        for (const PosMedicine &h : hits)
            if (h.id == pid)
                ok = (h.inStock == 40 && h.unitMrp == QStringLiteral("5.00")
                      && h.unitsPerPurchase == 10 && h.purchaseUnit == QStringLiteral("BOX"));
        s.check(ok, QStringLiteral("POS search: stock 40 + mrp 5.00 + pack info"));
    }

    // Barcode lookup + the NULL-sentinel behaviour of the `primary_barcode IN
    // (:g14,:g13)` clause (regression guard for the GTIN-sentinel fix).
    {
        // (a) A scanned barcode finds the medicine carrying that primary_barcode.
        MedicineDraft bc = draft(QStringLiteral("TMED_BC_%1").arg(++g_u));
        bc.brandName = QStringLiteral("BarcodeMed");
        bc.primaryBarcode = QStringLiteral("8412345000017"); // 13-digit GTIN
        const qint64 bcId = repo.create(bc, userId);
        const QVector<PosMedicine> byCode = repo.searchForPos(QStringLiteral("8412345000017"));
        bool foundByCode = false;
        for (const PosMedicine &h : byCode)
            if (h.id == bcId) foundByCode = true;
        s.check(foundByCode, QStringLiteral("POS search: scanned barcode matches primary_barcode"));

        // (b) A non-barcode text query that matches nothing must return nothing —
        // even though medicines exist with an EMPTY primary_barcode. Guards against
        // a '' sentinel making `primary_barcode IN (...)` match every blank row.
        MedicineDraft nob = draft(QStringLiteral("TMED_NOBC_%1").arg(++g_u));
        nob.brandName = QStringLiteral("NoBarcodeMed"); // primaryBarcode left empty
        const qint64 nobId = repo.create(nob, userId);
        const QVector<PosMedicine> none = repo.searchForPos(QStringLiteral("QZX_NOMATCH_QZX"));
        bool leaked = false;
        for (const PosMedicine &h : none)
            if (h.id == nobId) leaked = true;
        s.check(!leaked,
                QStringLiteral("POS search: empty-barcode row not matched by a text miss"));
        s.check(none.isEmpty(), QStringLiteral("POS search: no-match query returns nothing"));
    }

    return s;
}

} // namespace pharmadesk_tests
