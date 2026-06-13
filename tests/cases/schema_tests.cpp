// Schema-level constraint tests: CHECK enums, non-negative quantities, foreign
// keys, NOT NULL, partial-unique medicines, and audit-log immutability triggers.
// Exercised with raw SQL on the bootstrapped connection (foreign_keys = ON).

#include "framework/TestStats.h"

#include "data/MedicineRepository.h"

#include <QSqlQuery>
#include <QVariant>

namespace pharmadesk_tests {
namespace {
int g_u = 0;
// Run a statement; return true if it FAILED (constraint rejected it).
bool rejects(QSqlDatabase db, const QString &sql)
{
    QSqlQuery q(db);
    return !q.exec(sql);
}
} // namespace

TestStats run_schema_tests(QSqlDatabase db, qint64 userId)
{
    TestStats s;
    s.module = QStringLiteral("schema");

    // foreign_keys must be ON for the FK tests to mean anything.
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral("PRAGMA foreign_keys"));
        s.check(q.next() && q.value(0).toInt() == 1, QStringLiteral("PRAGMA foreign_keys = ON"));
    }

    // role CHECK constraint.
    s.check(rejects(db, QStringLiteral("INSERT INTO users(full_name,username,pin_hash,role) "
                                       "VALUES('x','tschema_baduser','h','BOGUS')")),
            QStringLiteral("users.role CHECK rejects invalid role"));

    // controlled_schedule + tax_code CHECK on medicines.
    s.check(
        rejects(
            db,
            QStringLiteral(
                "INSERT INTO medicines(sku,brand_name,generic_name,form,purchase_unit,base_unit,"
                "units_per_purchase,controlled_schedule) "
                "VALUES('TSCH_CS','b','g','TABLET','BOX','TAB',10,'WUT')")),
        QStringLiteral("medicines.controlled_schedule CHECK rejects bad value"));
    s.check(
        rejects(
            db,
            QStringLiteral(
                "INSERT INTO medicines(sku,brand_name,generic_name,form,purchase_unit,base_unit,"
                "units_per_purchase,tax_code_value) "
                "VALUES('TSCH_TX','b','g','TABLET','BOX','TAB',10,'NOPE')")),
        QStringLiteral("medicines.tax_code_value CHECK rejects bad value"));

    // NOT NULL.
    s.check(
        rejects(db,
                QStringLiteral(
                    "INSERT INTO "
                    "medicines(sku,generic_name,form,purchase_unit,base_unit,units_per_purchase) "
                    "VALUES('TSCH_NN','g','TABLET','BOX','TAB',10)")),
        QStringLiteral("medicines.brand_name NOT NULL enforced"));

    // Create a valid medicine to anchor batch tests.
    qint64 med = 0;
    {
        MedicineRepository m(db);
        MedicineDraft d;
        d.sku = QStringLiteral("TSCH_M_%1").arg(++g_u);
        d.brandName = QStringLiteral("SchemaMed");
        d.genericName = QStringLiteral("Gen");
        d.baseUnit = QStringLiteral("TABLET");
        d.purchaseUnit = QStringLiteral("BOX");
        d.unitsPerPurchase = 1;
        med = m.create(d, userId);
        s.check(med > 0, QStringLiteral("anchor medicine created"));
    }

    // batch CHECK: current_qty >= 0.
    s.check(
        rejects(
            db,
            QStringLiteral(
                "INSERT INTO batches(branch_id,medicine_id,batch_number,expiry_date,received_qty,"
                "current_qty,cost_per_unit,mrp_per_unit) "
                "VALUES(1,%1,'TSCH_NEG','2030-01-01',10,-1,'1.0000','2.00')")
                .arg(med)),
        QStringLiteral("batches.current_qty >= 0 CHECK enforced"));
    s.check(
        rejects(
            db,
            QStringLiteral(
                "INSERT INTO batches(branch_id,medicine_id,batch_number,expiry_date,received_qty,"
                "current_qty,cost_per_unit,mrp_per_unit) "
                "VALUES(1,%1,'TSCH_NEG2','2030-01-01',-5,10,'1.0000','2.00')")
                .arg(med)),
        QStringLiteral("batches.received_qty >= 0 CHECK enforced"));

    // Foreign key: batch referencing a non-existent medicine.
    s.check(
        rejects(
            db,
            QStringLiteral(
                "INSERT INTO batches(branch_id,medicine_id,batch_number,expiry_date,received_qty,"
                "current_qty,cost_per_unit,mrp_per_unit) "
                "VALUES(1,99999999,'TSCH_FK','2030-01-01',10,10,'1.0000','2.00')")),
        QStringLiteral("batches.medicine_id FK enforced"));

    // A valid batch insert SUCCEEDS (control).
    {
        QSqlQuery q(db);
        const bool ok = q.exec(
            QStringLiteral(
                "INSERT INTO batches(branch_id,medicine_id,batch_number,expiry_date,received_qty,"
                "current_qty,cost_per_unit,mrp_per_unit) "
                "VALUES(1,%1,'TSCH_OK','2030-01-01',10,10,'1.0000','2.00')")
                .arg(med));
        s.check(ok, QStringLiteral("valid batch insert succeeds (control)"));
    }

    // Partial-unique medicines.sku: live duplicate rejected; reusable after soft delete.
    {
        const QString sku = QStringLiteral("TSCH_UNIQ_%1").arg(++g_u);
        QSqlQuery q(db);
        const bool first = q.exec(QStringLiteral("INSERT INTO "
                                                 "medicines(sku,brand_name,generic_name,form,"
                                                 "purchase_unit,base_unit,units_per_purchase)"
                                                 " VALUES('%1','b','g','TABLET','BOX','TAB',1)")
                                      .arg(sku));
        s.check(first, QStringLiteral("first medicine with sku inserts"));
        s.check(rejects(db, QStringLiteral("INSERT INTO "
                                           "medicines(sku,brand_name,generic_name,form,purchase_"
                                           "unit,base_unit,units_per_purchase)"
                                           " VALUES('%1','b2','g2','TABLET','BOX','TAB',1)")
                                .arg(sku)),
                QStringLiteral("duplicate LIVE sku rejected (partial-unique)"));
        QSqlQuery upd(db);
        upd.exec(QStringLiteral("UPDATE medicines SET deleted_at=CURRENT_TIMESTAMP WHERE sku='%1'")
                     .arg(sku));
        QSqlQuery q2(db);
        const bool reuse = q2.exec(QStringLiteral("INSERT INTO "
                                                  "medicines(sku,brand_name,generic_name,form,"
                                                  "purchase_unit,base_unit,units_per_purchase)"
                                                  " VALUES('%1','b3','g3','TABLET','BOX','TAB',1)")
                                       .arg(sku));
        s.check(reuse, QStringLiteral("sku reusable after soft delete"));
    }

    // Audit-log immutability triggers.
    {
        QSqlQuery ins(db);
        const bool ok = ins.exec(QStringLiteral(
            "INSERT INTO audit_log(action_type,entity_type) VALUES('TSCH_ACTION','test')"));
        s.check(ok, QStringLiteral("audit row inserts"));
        const qint64 aid = ins.lastInsertId().toLongLong();
        s.check(rejects(db, QStringLiteral("UPDATE audit_log SET reason='x' WHERE id=%1").arg(aid)),
                QStringLiteral("audit_log UPDATE blocked by trigger"));
        s.check(rejects(db, QStringLiteral("DELETE FROM audit_log WHERE id=%1").arg(aid)),
                QStringLiteral("audit_log DELETE blocked by trigger"));
    }

    // z_report_archive immutability triggers (migration v5): a signed day-close
    // record must be append-only — no UPDATE, no DELETE — like audit_log.
    {
        QSqlQuery ins(db);
        const bool ok = ins.exec(
            QStringLiteral("INSERT INTO z_report_archive(session_id,payload,signature_hex) "
                           "VALUES(998001,'{}','deadbeef')"));
        s.check(ok, QStringLiteral("z_report_archive row inserts"));
        const qint64 zid = ins.lastInsertId().toLongLong();
        s.check(
            rejects(db, QStringLiteral("UPDATE z_report_archive SET signature_hex='x' WHERE id=%1")
                            .arg(zid)),
            QStringLiteral("z_report_archive UPDATE blocked by trigger"));
        s.check(rejects(db, QStringLiteral("DELETE FROM z_report_archive WHERE id=%1").arg(zid)),
                QStringLiteral("z_report_archive DELETE blocked by trigger"));
    }

    return s;
}

} // namespace pharmadesk_tests
