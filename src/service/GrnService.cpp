#include "service/GrnService.h"

#include "data/Audit.h"
#include "domain/CostBlender.h"
#include "domain/Money.h"

#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
struct GrnError
{
    QString message;
};
const QString kDateFmt = QStringLiteral("yyyy-MM-dd");
} // namespace

QString GrnService::nextGrnNumber()
{
    const QString today = QDate::currentDate().toString(QStringLiteral("yyyyMMdd"));
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT count(*) FROM grn_documents "
                          "WHERE date(created_at, 'localtime') = date('now', 'localtime')"));
    int count = 0;
    if (q.next()) count = q.value(0).toInt();

    for (int attempt = 0; attempt < 3; ++attempt) {
        const QString candidate = QStringLiteral("GRN-%1-%2")
                                      .arg(today)
                                      .arg(count + 1 + attempt, 4, 10, QLatin1Char('0'));
        QSqlQuery chk(m_db);
        chk.prepare(QStringLiteral("SELECT 1 FROM grn_documents WHERE grn_number = ? LIMIT 1"));
        chk.addBindValue(candidate);
        if (chk.exec() && !chk.next()) {
            return candidate;
        }
    }
    const quint32 r = QRandomGenerator::global()->generate();
    return QStringLiteral("GRN-%1-%2").arg(today).arg(r % 1000000u, 6, 10, QLatin1Char('0'));
}

GrnResult GrnService::post(qint64 supplierId, const QVector<GrnLineInput> &lines,
                           const QString &invoiceNumber, const QDate &invoiceDate,
                           const QString &notes)
{
    GrnResult res;
    bool inTxn = false;
    const QDate today = QDate::currentDate();

    try {
        if (lines.isEmpty()) {
            throw GrnError{QStringLiteral("A goods-receipt note must have at least one line.")};
        }

        // Validate supplier.
        {
            QSqlQuery sq(m_db);
            sq.prepare(
                QStringLiteral("SELECT 1 FROM suppliers WHERE id = ? AND deleted_at IS NULL"));
            sq.addBindValue(supplierId);
            if (!sq.exec() || !sq.next()) {
                throw GrnError{QStringLiteral("Choose a supplier.")};
            }
        }

        if (!m_db.transaction()) {
            throw GrnError{m_db.lastError().text()};
        }
        inTxn = true;

        const QString grnNumber = nextGrnNumber();

        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "INSERT INTO grn_documents "
            "(branch_id, grn_number, supplier_id, invoice_number, invoice_date, received_by, "
            " posted_by, posted_at, subtotal, tax_total, discount_total, grand_total, status, "
            "notes) "
            "VALUES (1, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP, '0', '0', '0', '0', 'POSTED', ?)"));
        q.addBindValue(grnNumber);
        q.addBindValue(supplierId);
        q.addBindValue(invoiceNumber.isEmpty() ? QVariant() : QVariant(invoiceNumber));
        q.addBindValue(invoiceDate.isValid() ? QVariant(invoiceDate.toString(kDateFmt))
                                             : QVariant());
        q.addBindValue(m_userId);
        q.addBindValue(m_userId);
        q.addBindValue(notes.isEmpty() ? QVariant() : QVariant(notes));
        if (!q.exec()) {
            throw GrnError{q.lastError().text()};
        }
        const qint64 grnId = q.lastInsertId().toLongLong();

        Money subtotal;

        for (const GrnLineInput &line : lines) {
            // Medicine lookup + active check.
            QSqlQuery mq(m_db);
            mq.prepare(
                QStringLiteral("SELECT brand_name, units_per_purchase, is_active, deleted_at "
                               "FROM medicines WHERE id = ?"));
            mq.addBindValue(line.medicineId);
            if (!mq.exec() || !mq.next()) {
                throw GrnError{QStringLiteral("Medicine %1 not found.").arg(line.medicineId)};
            }
            const QString brand = mq.value(0).toString();
            const int upp = mq.value(1).toInt();
            const bool active = mq.value(2).toInt() != 0;
            const bool deleted = !mq.value(3).isNull();
            if (!active || deleted) {
                throw GrnError{
                    QStringLiteral("'%1' is no longer active — cannot receive stock.").arg(brand)};
            }

            if (line.paidQty < 1) {
                throw GrnError{QStringLiteral("Paid qty must be ≥ 1 (%1).").arg(brand)};
            }
            if (line.focQty < 0) {
                throw GrnError{QStringLiteral("Free qty cannot be negative (%1).").arg(brand)};
            }
            const QString batchNum = line.batchNumber.trimmed();
            if (batchNum.isEmpty()) {
                throw GrnError{QStringLiteral("Batch number is required (%1).").arg(brand)};
            }
            if (line.unitCost.trimmed().isEmpty()
                || Money::fromString(line.unitCost).compare(Money()) <= 0) {
                throw GrnError{QStringLiteral("Unit cost must be > 0 (%1).").arg(brand)};
            }
            if (line.mrpPerPurchaseUnit.trimmed().isEmpty()
                || Money::fromString(line.mrpPerPurchaseUnit).compare(Money()) <= 0) {
                throw GrnError{
                    QStringLiteral("MRP per purchase unit must be > 0 (%1).").arg(brand)};
            }
            if (!line.expiry.isValid() || line.expiry < today) {
                throw GrnError{
                    QStringLiteral("Batch %1 (%2): expiry is in the past.").arg(batchNum, brand)};
            }

            const int totalBase = (line.paidQty + line.focQty) * upp;
            const QString blendedCost = CostBlender::blendedCostPerBaseUnit(
                line.paidQty, line.focQty, line.unitCost, upp);
            const QString mrpPerBase = CostBlender::mrpPerBaseUnit(line.mrpPerPurchaseUnit, upp);
            const QString lineTotal = CostBlender::lineTotal(line.paidQty, line.unitCost);
            subtotal = subtotal + Money::fromString(lineTotal);

            const QString expStr = line.expiry.toString(kDateFmt);

            // Find-or-create batch keyed (medicine, batch#, expiry) FIRST, so the
            // grn_lines row can reference batch_id (traceability).
            QSqlQuery fb(m_db);
            fb.prepare(
                QStringLiteral("SELECT id, current_qty FROM batches "
                               "WHERE medicine_id = ? AND batch_number = ? AND expiry_date = ?"));
            fb.addBindValue(line.medicineId);
            fb.addBindValue(batchNum);
            fb.addBindValue(expStr);
            qint64 batchId = -1;
            int qtyBefore = 0;
            if (fb.exec() && fb.next()) {
                batchId = fb.value(0).toLongLong();
                qtyBefore = fb.value(1).toInt();
                QSqlQuery ub(m_db);
                // NOTE: PHP set received_qty = current_qty + delta (a quirk that
                // mis-tracks cumulative receipts once stock is sold). We use the
                // correct received_qty + delta; current_qty is identical.
                ub.prepare(QStringLiteral(
                    "UPDATE batches SET received_qty = received_qty + ?, foc_qty = foc_qty + ?, "
                    " current_qty = current_qty + ?, cost_per_unit = ?, mrp_per_unit = ?, "
                    " updated_at = CURRENT_TIMESTAMP WHERE id = ?"));
                ub.addBindValue(totalBase);
                ub.addBindValue(line.focQty * upp);
                ub.addBindValue(totalBase);
                ub.addBindValue(blendedCost);
                ub.addBindValue(mrpPerBase);
                ub.addBindValue(batchId);
                if (!ub.exec()) {
                    throw GrnError{ub.lastError().text()};
                }
            } else {
                QSqlQuery ib(m_db);
                ib.prepare(QStringLiteral(
                    "INSERT INTO batches "
                    "(branch_id, medicine_id, supplier_id, grn_id, batch_number, expiry_date, "
                    " received_qty, foc_qty, current_qty, cost_per_unit, mrp_per_unit) "
                    "VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
                ib.addBindValue(line.medicineId);
                ib.addBindValue(supplierId);
                ib.addBindValue(grnId);
                ib.addBindValue(batchNum);
                ib.addBindValue(expStr);
                ib.addBindValue(totalBase);
                ib.addBindValue(line.focQty * upp);
                ib.addBindValue(totalBase);
                ib.addBindValue(blendedCost);
                ib.addBindValue(mrpPerBase);
                if (!ib.exec()) {
                    throw GrnError{ib.lastError().text()};
                }
                batchId = ib.lastInsertId().toLongLong();
            }

            // grn_lines row (now that we have the batch id for traceability).
            QSqlQuery lq(m_db);
            lq.prepare(QStringLiteral(
                "INSERT INTO grn_lines "
                "(grn_id, medicine_id, batch_id, batch_number, expiry_date, paid_qty, foc_qty, "
                " qty_in_base_units, unit_cost, mrp_per_base_unit, line_total) "
                "VALUES (?,?,?,?,?,?,?,?,?,?,?)"));
            lq.addBindValue(grnId);
            lq.addBindValue(line.medicineId);
            lq.addBindValue(batchId);
            lq.addBindValue(batchNum);
            lq.addBindValue(expStr);
            lq.addBindValue(line.paidQty);
            lq.addBindValue(line.focQty);
            lq.addBindValue(totalBase);
            lq.addBindValue(line.unitCost);
            lq.addBindValue(mrpPerBase);
            lq.addBindValue(lineTotal);
            if (!lq.exec()) {
                throw GrnError{lq.lastError().text()};
            }

            // GRN_RECEIPT movement.
            QSqlQuery mv(m_db);
            mv.prepare(QStringLiteral(
                "INSERT INTO inventory_movements "
                "(branch_id, medicine_id, batch_id, movement_type, qty_delta, qty_before, "
                " qty_after, ref_table, ref_id, performed_by) "
                "VALUES (1, ?, ?, 'GRN_RECEIPT', ?, ?, ?, 'grn_documents', ?, ?)"));
            mv.addBindValue(line.medicineId);
            mv.addBindValue(batchId);
            mv.addBindValue(totalBase);
            mv.addBindValue(qtyBefore);
            mv.addBindValue(qtyBefore + totalBase);
            mv.addBindValue(grnId);
            mv.addBindValue(m_userId);
            if (!mv.exec()) {
                throw GrnError{mv.lastError().text()};
            }
        }

        const QString subtotalStr = subtotal.toString(Money::ScaleMoney);
        QSqlQuery uq(m_db);
        uq.prepare(QStringLiteral("UPDATE grn_documents SET subtotal = ?, grand_total = ?, "
                                  "updated_at = CURRENT_TIMESTAMP "
                                  "WHERE id = ?"));
        uq.addBindValue(subtotalStr);
        uq.addBindValue(subtotalStr);
        uq.addBindValue(grnId);
        if (!uq.exec()) {
            throw GrnError{uq.lastError().text()};
        }

        Audit::writeOrThrow(
            m_db, m_userId, QStringLiteral("GRN_POSTED"), QStringLiteral("grn_documents"), grnId,
            QString(),
            QStringLiteral("{\"grn_number\":\"%1\",\"subtotal\":\"%2\",\"lines\":%3}")
                .arg(grnNumber, subtotalStr)
                .arg(lines.size()));

        if (!m_db.commit()) {
            m_db.rollback();
            throw GrnError{m_db.lastError().text()};
        }
        inTxn = false;

        res.ok = true;
        res.grnId = grnId;
        res.grnNumber = grnNumber;
        res.subtotal = subtotalStr;
        return res;

    } catch (const GrnError &e) {
        if (inTxn) m_db.rollback();
        res.ok = false;
        res.error = e.message;
        return res;
    } catch (const std::exception &e) {
        if (inTxn) m_db.rollback();
        res.ok = false;
        res.error = QString::fromUtf8(e.what());
        return res;
    }
}
