#include "service/ReturnService.h"

#include "data/Audit.h"
#include "data/UserRepository.h"
#include "domain/ControlledSubstancePolicy.h"
#include "domain/Money.h"

#include <QDate>
#include <QRandomGenerator>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace {
struct ReturnError
{
    QString message;
};

const QSet<QString> kReasons = {
    QStringLiteral("CUSTOMER_CHANGED_MIND"),
    QStringLiteral("DAMAGED"),
    QStringLiteral("WRONG_ITEM"),
    QStringLiteral("ADVERSE_REACTION"),
    QStringLiteral("EXPIRED"),
    QStringLiteral("OTHER"),
};

const QSet<QString> kFinalStatuses = {
    QStringLiteral("APPROVED_RESTOCK"),
    QStringLiteral("RETURN_TO_SUPPLIER"),
    QStringLiteral("WRITE_OFF"),
};

// JSON string escaping for audit payloads.
QString jesc(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (const QChar c : s) {
        switch (c.unicode()) {
        case '"':
            out += QStringLiteral("\\\"");
            break;
        case '\\':
            out += QStringLiteral("\\\\");
            break;
        case '\n':
            out += QStringLiteral("\\n");
            break;
        case '\r':
            out += QStringLiteral("\\r");
            break;
        case '\t':
            out += QStringLiteral("\\t");
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

// Throws ReturnError when a controlled medicine is returned without a valid
// witness (an active MANAGER/ADMIN other than the operator) — mirrors the
// sale-time two-person rule.
void requireWitnessIfControlled(QSqlDatabase &db, const QString &schedule, qint64 witnessUserId,
                                qint64 operatorId)
{
    if (!ControlledSubstancePolicy::requiresWitness(schedule)) {
        return;
    }
    if (witnessUserId <= 0) {
        throw ReturnError{QStringLiteral(
            "This is a controlled medicine — a manager/admin witness PIN is required "
            "to process the return.")};
    }
    if (witnessUserId == operatorId) {
        throw ReturnError{
            QStringLiteral("The witness must be a different user than the operator.")};
    }
    if (!UserRepository(db).isActiveManagerOrAdmin(witnessUserId)) {
        throw ReturnError{QStringLiteral("The witness must be an active manager or admin.")};
    }
}

// Bump the operator's open shift refund counter for a CASH original sale, so the
// Z-report / reconciliation see the cash that left the drawer. No-op otherwise.
void bumpSessionRefund(QSqlDatabase &db, qint64 cashierId, const QString &paymentMode,
                       const QString &refund)
{
    if (paymentMode != QLatin1String("CASH")) {
        return;
    }
    QSqlQuery q(db);
    q.prepare(
        QStringLiteral("UPDATE cashier_sessions SET total_refunds_paid = total_refunds_paid + ?, "
                       "updated_at = CURRENT_TIMESTAMP WHERE cashier_id = ? AND status = 'OPEN'"));
    q.addBindValue(refund);
    q.addBindValue(cashierId);
    q.exec();
}
} // namespace

QString ReturnService::nextReturnNumber()
{
    const QString today = QDate::currentDate().toString(QStringLiteral("yyyyMMdd"));
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT count(*) FROM returns "
                          "WHERE date(created_at, 'localtime') = date('now', 'localtime')"));
    int count = 0;
    if (q.next()) count = q.value(0).toInt();

    for (int attempt = 0; attempt < 3; ++attempt) {
        const QString candidate = QStringLiteral("RET-%1-%2")
                                      .arg(today)
                                      .arg(count + 1 + attempt, 4, 10, QLatin1Char('0'));
        QSqlQuery chk(m_db);
        chk.prepare(QStringLiteral("SELECT 1 FROM returns WHERE return_number = ? LIMIT 1"));
        chk.addBindValue(candidate);
        if (chk.exec() && !chk.next()) {
            return candidate;
        }
    }
    const quint32 r = QRandomGenerator::global()->generate();
    return QStringLiteral("RET-%1-%2").arg(today).arg(r % 1000000u, 6, 10, QLatin1Char('0'));
}

// Mirrors Returns::recomputeSaleStatus. Sums final-status returns vs sold base
// units → REFUNDED_PARTIAL / REFUNDED_FULL. Never touches VOIDED sales. Must be
// called inside the caller's transaction. Returns the resulting status.
QString ReturnService::recomputeSaleStatus(qint64 saleId)
{
    QSqlQuery sq(m_db);
    sq.prepare(QStringLiteral("SELECT status FROM sales WHERE id = ?"));
    sq.addBindValue(saleId);
    if (!sq.exec() || !sq.next()) {
        throw ReturnError{QStringLiteral("Original sale not found.")};
    }
    const QString current = sq.value(0).toString();
    if (current == QLatin1String("VOIDED")) {
        return current;
    }

    QSqlQuery soldQ(m_db);
    soldQ.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(qty_in_base_units), 0) FROM sale_items WHERE sale_id = ?"));
    soldQ.addBindValue(saleId);
    int soldBase = 0;
    if (soldQ.exec() && soldQ.next()) soldBase = soldQ.value(0).toInt();

    QSqlQuery retQ(m_db);
    retQ.prepare(
        QStringLiteral("SELECT COALESCE(SUM(qty_returned_units), 0) FROM returns "
                       " WHERE original_sale_id = ? "
                       "   AND status IN ('APPROVED_RESTOCK','RETURN_TO_SUPPLIER','WRITE_OFF')"));
    retQ.addBindValue(saleId);
    int returnedBase = 0;
    if (retQ.exec() && retQ.next()) returnedBase = retQ.value(0).toInt();

    if (soldBase == 0 || returnedBase == 0) {
        return current;
    }

    const QString next = returnedBase >= soldBase ? QStringLiteral("REFUNDED_FULL")
                                                  : QStringLiteral("REFUNDED_PARTIAL");
    if (next != current) {
        QSqlQuery uq(m_db);
        uq.prepare(QStringLiteral("UPDATE sales SET status = ? WHERE id = ?"));
        uq.addBindValue(next);
        uq.addBindValue(saleId);
        if (!uq.exec()) {
            throw ReturnError{uq.lastError().text()};
        }
        Audit::writeOrThrow(m_db, m_userId, QStringLiteral("SALE_STATUS_RECOMPUTED"),
                            QStringLiteral("sales"), saleId,
                            QStringLiteral("{\"status\":\"%1\"}").arg(jesc(current)),
                            QStringLiteral("{\"status\":\"%1\"}").arg(jesc(next)));
    }
    return next;
}

ReturnResult ReturnService::initiate(qint64 saleItemId, int qtyReturned, const QString &reason,
                                     const QString &physicalCondition,
                                     qint64 controlledWitnessUserId)
{
    ReturnResult res;
    bool inTxn = false;
    try {
        if (qtyReturned < 1) {
            throw ReturnError{QStringLiteral("Quantity to return must be at least 1.")};
        }
        if (!kReasons.contains(reason)) {
            throw ReturnError{QStringLiteral("Choose a valid return reason.")};
        }

        if (!m_db.transaction()) {
            throw ReturnError{m_db.lastError().text()};
        }
        inTxn = true;

        QSqlQuery si(m_db);
        si.prepare(QStringLiteral(
            "SELECT si.sale_id, si.medicine_id, si.batch_id, si.qty_in_base_units, si.unit_mrp, "
            "       s.branch_id, s.status, s.receipt_number, s.payment_mode, m.controlled_schedule "
            "  FROM sale_items si JOIN sales s ON s.id = si.sale_id "
            "  JOIN medicines m ON m.id = si.medicine_id "
            " WHERE si.id = ?"));
        si.addBindValue(saleItemId);
        if (!si.exec() || !si.next()) {
            throw ReturnError{QStringLiteral("Sale item not found.")};
        }
        const qint64 saleId = si.value(0).toLongLong();
        const qint64 medicineId = si.value(1).toLongLong();
        const qint64 batchId = si.value(2).toLongLong();
        const int qtySold = si.value(3).toInt();
        const QString unitMrp = si.value(4).toString();
        const qint64 branchId = si.value(5).toLongLong();
        const QString saleStatus = si.value(6).toString();
        const QString receiptNumber = si.value(7).toString();
        const QString paymentMode = si.value(8).toString();
        const QString schedule = si.value(9).toString();

        if (saleStatus == QLatin1String("VOIDED") || saleStatus == QLatin1String("REFUNDED_FULL")) {
            throw ReturnError{QStringLiteral("Receipt %1 is %2 — no further refunds allowed.")
                                  .arg(receiptNumber, saleStatus)};
        }
        requireWitnessIfControlled(m_db, schedule, controlledWitnessUserId, m_userId);

        QSqlQuery already(m_db);
        already.prepare(QStringLiteral(
            "SELECT COALESCE(SUM(qty_returned_units), 0) FROM returns WHERE sale_item_id = ?"));
        already.addBindValue(saleItemId);
        int alreadyReturned = 0;
        if (already.exec() && already.next()) alreadyReturned = already.value(0).toInt();

        const int remaining = qtySold - alreadyReturned;
        if (qtyReturned > remaining) {
            throw ReturnError{
                remaining <= 0
                    ? QStringLiteral("This item has already been fully returned.")
                    : QStringLiteral("Only %1 unit(s) are still returnable for this item.")
                          .arg(remaining)};
        }

        const QString refund
            = Money::fromString(unitMrp).mul(qtyReturned).toString(Money::ScaleMoney);
        const QString returnNumber = nextReturnNumber();
        const qint64 authorizedBy
            = controlledWitnessUserId > 0 ? controlledWitnessUserId : m_userId;

        QSqlQuery ins(m_db);
        ins.prepare(QStringLiteral(
            "INSERT INTO returns "
            "(branch_id, return_number, original_sale_id, sale_item_id, medicine_id, batch_id, "
            " qty_returned_units, refund_amount, reason, physical_condition, initiated_by, "
            " authorized_by, status) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'PENDING_REVIEW')"));
        ins.addBindValue(branchId);
        ins.addBindValue(returnNumber);
        ins.addBindValue(saleId);
        ins.addBindValue(saleItemId);
        ins.addBindValue(medicineId);
        ins.addBindValue(batchId);
        ins.addBindValue(qtyReturned);
        ins.addBindValue(refund);
        ins.addBindValue(reason);
        ins.addBindValue(physicalCondition.trimmed().isEmpty()
                             ? QVariant()
                             : QVariant(physicalCondition.trimmed()));
        ins.addBindValue(m_userId);
        ins.addBindValue(authorizedBy);
        if (!ins.exec()) {
            throw ReturnError{ins.lastError().text()};
        }
        const qint64 returnId = ins.lastInsertId().toLongLong();

        // Stock is physically out but current_qty is unchanged until adjudication
        // — record a zero-delta QUARANTINE movement so the ledger shows the event.
        QSqlQuery bq(m_db);
        bq.prepare(QStringLiteral("SELECT current_qty FROM batches WHERE id = ?"));
        bq.addBindValue(batchId);
        int qtyNow = 0;
        if (bq.exec() && bq.next()) qtyNow = bq.value(0).toInt();

        QSqlQuery mv(m_db);
        mv.prepare(QStringLiteral(
            "INSERT INTO inventory_movements "
            "(branch_id, medicine_id, batch_id, movement_type, qty_delta, qty_before, "
            " qty_after, ref_table, ref_id, performed_by) "
            "VALUES (?, ?, ?, 'RETURN_QUARANTINE', 0, ?, ?, 'returns', ?, ?)"));
        mv.addBindValue(branchId);
        mv.addBindValue(medicineId);
        mv.addBindValue(batchId);
        mv.addBindValue(qtyNow);
        mv.addBindValue(qtyNow);
        mv.addBindValue(returnId);
        mv.addBindValue(m_userId);
        if (!mv.exec()) {
            throw ReturnError{mv.lastError().text()};
        }

        bumpSessionRefund(m_db, m_userId, paymentMode, refund);

        Audit::writeOrThrow(m_db, m_userId, QStringLiteral("RETURN_INITIATED"),
                            QStringLiteral("returns"), returnId, QString(),
                            QStringLiteral("{\"return_number\":\"%1\",\"original_sale_id\":%2,"
                                           "\"qty_returned_units\":%3,\"refund_amount\":\"%4\","
                                           "\"reason\":\"%5\"}")
                                .arg(jesc(returnNumber))
                                .arg(saleId)
                                .arg(qtyReturned)
                                .arg(jesc(refund), jesc(reason)));

        if (!m_db.commit()) {
            m_db.rollback();
            throw ReturnError{m_db.lastError().text()};
        }
        inTxn = false;

        res.ok = true;
        res.returnId = returnId;
        res.returnNumber = returnNumber;
        res.refundAmount = refund;
        res.saleStatus = QStringLiteral("PENDING_REVIEW");
        return res;

    } catch (const ReturnError &e) {
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

ReturnOpResult ReturnService::adjudicate(qint64 returnId, const QString &newStatus,
                                         const QString &notes)
{
    ReturnOpResult res;
    bool inTxn = false;
    try {
        if (!kFinalStatuses.contains(newStatus)) {
            throw ReturnError{
                QStringLiteral("Pick APPROVED_RESTOCK, RETURN_TO_SUPPLIER, or WRITE_OFF.")};
        }

        if (!m_db.transaction()) {
            throw ReturnError{m_db.lastError().text()};
        }
        inTxn = true;

        QSqlQuery rq(m_db);
        rq.prepare(QStringLiteral(
            "SELECT branch_id, return_number, original_sale_id, medicine_id, batch_id, "
            "       qty_returned_units, status FROM returns WHERE id = ?"));
        rq.addBindValue(returnId);
        if (!rq.exec() || !rq.next()) {
            throw ReturnError{QStringLiteral("Return not found.")};
        }
        const qint64 branchId = rq.value(0).toLongLong();
        const QString returnNumber = rq.value(1).toString();
        const qint64 saleId = rq.value(2).toLongLong();
        const qint64 medicineId = rq.value(3).toLongLong();
        const qint64 batchId = rq.value(4).toLongLong();
        const int qty = rq.value(5).toInt();
        const QString status = rq.value(6).toString();

        if (status != QLatin1String("PENDING_REVIEW")) {
            throw ReturnError{
                QStringLiteral("Return %1 is already adjudicated.").arg(returnNumber)};
        }

        QSqlQuery bq(m_db);
        bq.prepare(QStringLiteral("SELECT current_qty, is_quarantined, is_expired, cost_per_unit "
                                  "FROM batches WHERE id = ?"));
        bq.addBindValue(batchId);
        int qtyBefore = 0;
        bool quarantined = false, expired = false;
        QString costPerUnit = QStringLiteral("0");
        if (bq.exec() && bq.next()) {
            qtyBefore = bq.value(0).toInt();
            quarantined = bq.value(1).toInt() != 0;
            expired = bq.value(2).toInt() != 0;
            costPerUnit = bq.value(3).toString();
        }

        if (newStatus == QLatin1String("APPROVED_RESTOCK") && (quarantined || expired)) {
            throw ReturnError{
                QStringLiteral(
                    "Cannot restock return %1: the batch is now %2. Use RETURN_TO_SUPPLIER or "
                    "WRITE_OFF instead.")
                    .arg(returnNumber,
                         quarantined ? QStringLiteral("QUARANTINED") : QStringLiteral("EXPIRED"))};
        }

        QSqlQuery uq(m_db);
        uq.prepare(
            QStringLiteral("UPDATE returns SET status = ?, adjudicated_by = ?, adjudicated_at = "
                           "CURRENT_TIMESTAMP, "
                           "adjudication_notes = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?"));
        uq.addBindValue(newStatus);
        uq.addBindValue(m_userId);
        uq.addBindValue(notes.trimmed().isEmpty() ? QVariant() : QVariant(notes.trimmed()));
        uq.addBindValue(returnId);
        if (!uq.exec()) {
            throw ReturnError{uq.lastError().text()};
        }

        if (newStatus == QLatin1String("APPROVED_RESTOCK")) {
            const int qtyAfter = qtyBefore + qty;
            QSqlQuery ub(m_db);
            ub.prepare(QStringLiteral(
                "UPDATE batches SET current_qty = current_qty + ?, updated_at = CURRENT_TIMESTAMP "
                "WHERE id = ?"));
            ub.addBindValue(qty);
            ub.addBindValue(batchId);
            if (!ub.exec()) {
                throw ReturnError{ub.lastError().text()};
            }
            QSqlQuery mv(m_db);
            mv.prepare(QStringLiteral(
                "INSERT INTO inventory_movements "
                "(branch_id, medicine_id, batch_id, movement_type, qty_delta, qty_before, "
                " qty_after, ref_table, ref_id, performed_by) "
                "VALUES (?, ?, ?, 'RETURN_RESTOCK', ?, ?, ?, 'returns', ?, ?)"));
            mv.addBindValue(branchId);
            mv.addBindValue(medicineId);
            mv.addBindValue(batchId);
            mv.addBindValue(qty);
            mv.addBindValue(qtyBefore);
            mv.addBindValue(qtyAfter);
            mv.addBindValue(returnId);
            mv.addBindValue(m_userId);
            if (!mv.exec()) {
                throw ReturnError{mv.lastError().text()};
            }
        } else {
            // RETURN_TO_SUPPLIER / WRITE_OFF: stock already left at sale time; a
            // zero-delta WRITE_OFF row records the disposition in the ledger.
            QSqlQuery mv(m_db);
            mv.prepare(QStringLiteral(
                "INSERT INTO inventory_movements "
                "(branch_id, medicine_id, batch_id, movement_type, qty_delta, qty_before, "
                " qty_after, ref_table, ref_id, performed_by) "
                "VALUES (?, ?, ?, 'WRITE_OFF', 0, ?, ?, 'returns', ?, ?)"));
            mv.addBindValue(branchId);
            mv.addBindValue(medicineId);
            mv.addBindValue(batchId);
            mv.addBindValue(qtyBefore);
            mv.addBindValue(qtyBefore);
            mv.addBindValue(returnId);
            mv.addBindValue(m_userId);
            if (!mv.exec()) {
                throw ReturnError{mv.lastError().text()};
            }
        }

        const QString resultStatus = recomputeSaleStatus(saleId);
        const QString costImpact
            = Money::fromString(costPerUnit).mul(qty).toString(Money::ScaleMoney);

        Audit::writeOrThrow(m_db, m_userId, QStringLiteral("RETURN_ADJUDICATED"),
                            QStringLiteral("returns"), returnId,
                            QStringLiteral("{\"status\":\"PENDING_REVIEW\"}"),
                            QStringLiteral("{\"return_number\":\"%1\",\"new_status\":\"%2\","
                                           "\"qty_returned_units\":%3,\"cost_impact\":\"%4\"}")
                                .arg(jesc(returnNumber), jesc(newStatus))
                                .arg(qty)
                                .arg(jesc(costImpact)));

        if (!m_db.commit()) {
            m_db.rollback();
            throw ReturnError{m_db.lastError().text()};
        }
        inTxn = false;

        res.ok = true;
        res.saleStatus = resultStatus;
        return res;

    } catch (const ReturnError &e) {
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

ReturnOpResult ReturnService::markSupplierReturnSent(qint64 returnId,
                                                     const QString &supplierReference)
{
    ReturnOpResult res;
    bool inTxn = false;
    try {
        if (!m_db.transaction()) {
            throw ReturnError{m_db.lastError().text()};
        }
        inTxn = true;

        QSqlQuery rq(m_db);
        rq.prepare(QStringLiteral(
            "SELECT return_number, status, supplier_settled_at FROM returns WHERE id = ?"));
        rq.addBindValue(returnId);
        if (!rq.exec() || !rq.next()) {
            throw ReturnError{QStringLiteral("Return not found.")};
        }
        const QString returnNumber = rq.value(0).toString();
        const QString status = rq.value(1).toString();
        const bool alreadySettled = !rq.value(2).isNull();

        if (status != QLatin1String("RETURN_TO_SUPPLIER")) {
            throw ReturnError{QStringLiteral("Return %1 is not flagged for supplier — current "
                                             "status: %2.")
                                  .arg(returnNumber, status)};
        }
        if (alreadySettled) {
            throw ReturnError{
                QStringLiteral("Return %1 was already marked sent.").arg(returnNumber)};
        }

        QSqlQuery uq(m_db);
        uq.prepare(QStringLiteral(
            "UPDATE returns SET supplier_settled_at = CURRENT_TIMESTAMP, supplier_settled_by = ?, "
            "supplier_reference = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?"));
        uq.addBindValue(m_userId);
        uq.addBindValue(supplierReference.trimmed().isEmpty()
                            ? QVariant()
                            : QVariant(supplierReference.trimmed()));
        uq.addBindValue(returnId);
        if (!uq.exec()) {
            throw ReturnError{uq.lastError().text()};
        }

        Audit::writeOrThrow(
            m_db, m_userId, QStringLiteral("SUPPLIER_RETURN_SENT"), QStringLiteral("returns"),
            returnId, QString(),
            QStringLiteral("{\"return_number\":\"%1\",\"supplier_reference\":\"%2\"}")
                .arg(jesc(returnNumber), jesc(supplierReference.trimmed())));

        if (!m_db.commit()) {
            m_db.rollback();
            throw ReturnError{m_db.lastError().text()};
        }
        inTxn = false;

        res.ok = true;
        return res;

    } catch (const ReturnError &e) {
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

ReturnResult ReturnService::commitReturn(qint64 saleItemId, int qtyReturned, const QString &reason,
                                         bool restock, const QString &physicalCondition,
                                         qint64 controlledWitnessUserId)
{
    ReturnResult res;
    bool inTxn = false;
    try {
        if (qtyReturned < 1) {
            throw ReturnError{QStringLiteral("Quantity to return must be at least 1.")};
        }
        if (!kReasons.contains(reason)) {
            throw ReturnError{QStringLiteral("Choose a valid return reason.")};
        }

        if (!m_db.transaction()) {
            throw ReturnError{m_db.lastError().text()};
        }
        inTxn = true;

        // Load the sale_item + its sale meta + controlled schedule.
        QSqlQuery si(m_db);
        si.prepare(QStringLiteral(
            "SELECT si.sale_id, si.medicine_id, si.batch_id, si.qty_in_base_units, si.unit_mrp, "
            "       s.branch_id, s.status, s.receipt_number, s.payment_mode, m.controlled_schedule "
            "  FROM sale_items si JOIN sales s ON s.id = si.sale_id "
            "  JOIN medicines m ON m.id = si.medicine_id "
            " WHERE si.id = ?"));
        si.addBindValue(saleItemId);
        if (!si.exec() || !si.next()) {
            throw ReturnError{QStringLiteral("Sale item not found.")};
        }
        const qint64 saleId = si.value(0).toLongLong();
        const qint64 medicineId = si.value(1).toLongLong();
        const qint64 batchId = si.value(2).toLongLong();
        const int qtySold = si.value(3).toInt();
        const QString unitMrp = si.value(4).toString();
        const qint64 branchId = si.value(5).toLongLong();
        const QString saleStatus = si.value(6).toString();
        const QString receiptNumber = si.value(7).toString();
        const QString paymentMode = si.value(8).toString();
        const QString schedule = si.value(9).toString();

        if (saleStatus == QLatin1String("VOIDED") || saleStatus == QLatin1String("REFUNDED_FULL")) {
            throw ReturnError{QStringLiteral("Receipt %1 is %2 — no further refunds allowed.")
                                  .arg(receiptNumber, saleStatus)};
        }
        requireWitnessIfControlled(m_db, schedule, controlledWitnessUserId, m_userId);

        QSqlQuery already(m_db);
        already.prepare(QStringLiteral(
            "SELECT COALESCE(SUM(qty_returned_units), 0) FROM returns WHERE sale_item_id = ?"));
        already.addBindValue(saleItemId);
        int alreadyReturned = 0;
        if (already.exec() && already.next()) alreadyReturned = already.value(0).toInt();

        const int remaining = qtySold - alreadyReturned;
        if (qtyReturned > remaining) {
            throw ReturnError{
                remaining <= 0
                    ? QStringLiteral("This item has already been fully returned.")
                    : QStringLiteral("Only %1 unit(s) are still returnable for this item.")
                          .arg(remaining)};
        }

        const QString refund
            = Money::fromString(unitMrp).mul(qtyReturned).toString(Money::ScaleMoney);
        const QString returnNumber = nextReturnNumber();
        const QString finalStatus
            = restock ? QStringLiteral("APPROVED_RESTOCK") : QStringLiteral("WRITE_OFF");
        const qint64 authorizedBy
            = controlledWitnessUserId > 0 ? controlledWitnessUserId : m_userId;

        QSqlQuery ins(m_db);
        ins.prepare(QStringLiteral(
            "INSERT INTO returns "
            "(branch_id, return_number, original_sale_id, sale_item_id, medicine_id, batch_id, "
            " qty_returned_units, refund_amount, reason, physical_condition, initiated_by, "
            " authorized_by, status, adjudicated_by, adjudicated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP)"));
        ins.addBindValue(branchId);
        ins.addBindValue(returnNumber);
        ins.addBindValue(saleId);
        ins.addBindValue(saleItemId);
        ins.addBindValue(medicineId);
        ins.addBindValue(batchId);
        ins.addBindValue(qtyReturned);
        ins.addBindValue(refund);
        ins.addBindValue(reason);
        ins.addBindValue(physicalCondition.trimmed().isEmpty()
                             ? QVariant()
                             : QVariant(physicalCondition.trimmed()));
        ins.addBindValue(m_userId);
        ins.addBindValue(authorizedBy);
        ins.addBindValue(finalStatus);
        ins.addBindValue(m_userId);
        if (!ins.exec()) {
            throw ReturnError{ins.lastError().text()};
        }
        const qint64 returnId = ins.lastInsertId().toLongLong();

        QSqlQuery bq(m_db);
        bq.prepare(QStringLiteral("SELECT current_qty FROM batches WHERE id = ?"));
        bq.addBindValue(batchId);
        int qtyBefore = 0;
        if (bq.exec() && bq.next()) qtyBefore = bq.value(0).toInt();

        if (restock) {
            const int qtyAfter = qtyBefore + qtyReturned;
            QSqlQuery ub(m_db);
            ub.prepare(QStringLiteral(
                "UPDATE batches SET current_qty = current_qty + ?, updated_at = CURRENT_TIMESTAMP "
                "WHERE id = ?"));
            ub.addBindValue(qtyReturned);
            ub.addBindValue(batchId);
            if (!ub.exec()) {
                throw ReturnError{ub.lastError().text()};
            }
            QSqlQuery mv(m_db);
            mv.prepare(QStringLiteral(
                "INSERT INTO inventory_movements "
                "(branch_id, medicine_id, batch_id, movement_type, qty_delta, qty_before, "
                " qty_after, ref_table, ref_id, performed_by) "
                "VALUES (?, ?, ?, 'RETURN_RESTOCK', ?, ?, ?, 'returns', ?, ?)"));
            mv.addBindValue(branchId);
            mv.addBindValue(medicineId);
            mv.addBindValue(batchId);
            mv.addBindValue(qtyReturned);
            mv.addBindValue(qtyBefore);
            mv.addBindValue(qtyAfter);
            mv.addBindValue(returnId);
            mv.addBindValue(m_userId);
            if (!mv.exec()) {
                throw ReturnError{mv.lastError().text()};
            }
        } else {
            QSqlQuery mv(m_db);
            mv.prepare(QStringLiteral(
                "INSERT INTO inventory_movements "
                "(branch_id, medicine_id, batch_id, movement_type, qty_delta, qty_before, "
                " qty_after, ref_table, ref_id, performed_by) "
                "VALUES (?, ?, ?, 'WRITE_OFF', 0, ?, ?, 'returns', ?, ?)"));
            mv.addBindValue(branchId);
            mv.addBindValue(medicineId);
            mv.addBindValue(batchId);
            mv.addBindValue(qtyBefore);
            mv.addBindValue(qtyBefore);
            mv.addBindValue(returnId);
            mv.addBindValue(m_userId);
            if (!mv.exec()) {
                throw ReturnError{mv.lastError().text()};
            }
        }

        bumpSessionRefund(m_db, m_userId, paymentMode, refund);
        const QString resultStatus = recomputeSaleStatus(saleId);

        Audit::writeOrThrow(m_db, m_userId, QStringLiteral("RETURN_PROCESSED"),
                            QStringLiteral("returns"), returnId, QString(),
                            QStringLiteral("{\"return_number\":\"%1\",\"original_sale_id\":%2,"
                                           "\"qty_returned_units\":%3,\"refund_amount\":\"%4\","
                                           "\"reason\":\"%5\",\"disposition\":\"%6\"}")
                                .arg(jesc(returnNumber))
                                .arg(saleId)
                                .arg(qtyReturned)
                                .arg(jesc(refund), jesc(reason), jesc(finalStatus)));

        if (!m_db.commit()) {
            m_db.rollback();
            throw ReturnError{m_db.lastError().text()};
        }
        inTxn = false;

        res.ok = true;
        res.returnId = returnId;
        res.returnNumber = returnNumber;
        res.refundAmount = refund;
        res.saleStatus = resultStatus;
        return res;

    } catch (const ReturnError &e) {
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

VoidResult ReturnService::voidSale(qint64 saleId, bool sameDayOnly)
{
    VoidResult res;
    bool inTxn = false;
    try {
        if (!m_db.transaction()) {
            throw ReturnError{m_db.lastError().text()};
        }
        inTxn = true;

        QSqlQuery sq(m_db);
        sq.prepare(QStringLiteral("SELECT branch_id, receipt_number, status, "
                                  "       date(sold_at, 'localtime') AS sold_day "
                                  "  FROM sales WHERE id = ?"));
        sq.addBindValue(saleId);
        if (!sq.exec() || !sq.next()) {
            throw ReturnError{QStringLiteral("Sale not found.")};
        }
        const qint64 branchId = sq.value(0).toLongLong();
        const QString receiptNumber = sq.value(1).toString();
        const QString status = sq.value(2).toString();
        const QString soldDay = sq.value(3).toString();

        if (status != QLatin1String("COMPLETED")) {
            throw ReturnError{
                QStringLiteral("Receipt %1 is %2 — only COMPLETED sales can be voided.")
                    .arg(receiptNumber, status)};
        }
        if (sameDayOnly && soldDay != QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))) {
            throw ReturnError{QStringLiteral("Receipt %1 was not sold today — same-day void only. "
                                             "Process a return instead.")
                                  .arg(receiptNumber)};
        }

        // Restock every line.
        QSqlQuery items(m_db);
        items.prepare(QStringLiteral(
            "SELECT medicine_id, batch_id, qty_in_base_units FROM sale_items WHERE sale_id = ?"));
        items.addBindValue(saleId);
        if (!items.exec()) {
            throw ReturnError{items.lastError().text()};
        }
        struct Line
        {
            qint64 medicineId;
            qint64 batchId;
            int qty;
        };
        QVector<Line> lines;
        while (items.next()) {
            lines.push_back(
                {items.value(0).toLongLong(), items.value(1).toLongLong(), items.value(2).toInt()});
        }

        for (const Line &l : lines) {
            QSqlQuery bq(m_db);
            bq.prepare(QStringLiteral("SELECT current_qty FROM batches WHERE id = ?"));
            bq.addBindValue(l.batchId);
            int qtyBefore = 0;
            if (bq.exec() && bq.next()) qtyBefore = bq.value(0).toInt();
            const int qtyAfter = qtyBefore + l.qty;

            QSqlQuery ub(m_db);
            ub.prepare(QStringLiteral(
                "UPDATE batches SET current_qty = current_qty + ?, updated_at = CURRENT_TIMESTAMP "
                "WHERE id = ?"));
            ub.addBindValue(l.qty);
            ub.addBindValue(l.batchId);
            if (!ub.exec()) {
                throw ReturnError{ub.lastError().text()};
            }

            QSqlQuery mv(m_db);
            mv.prepare(QStringLiteral(
                "INSERT INTO inventory_movements "
                "(branch_id, medicine_id, batch_id, movement_type, qty_delta, qty_before, "
                " qty_after, ref_table, ref_id, performed_by) "
                "VALUES (?, ?, ?, 'RETURN_RESTOCK', ?, ?, ?, 'sales', ?, ?)"));
            mv.addBindValue(branchId);
            mv.addBindValue(l.medicineId);
            mv.addBindValue(l.batchId);
            mv.addBindValue(l.qty);
            mv.addBindValue(qtyBefore);
            mv.addBindValue(qtyAfter);
            mv.addBindValue(saleId);
            mv.addBindValue(m_userId);
            if (!mv.exec()) {
                throw ReturnError{mv.lastError().text()};
            }
        }

        QSqlQuery uq(m_db);
        uq.prepare(QStringLiteral("UPDATE sales SET status = 'VOIDED' WHERE id = ?"));
        uq.addBindValue(saleId);
        if (!uq.exec()) {
            throw ReturnError{uq.lastError().text()};
        }

        Audit::writeOrThrow(
            m_db, m_userId, QStringLiteral("SALE_VOIDED"), QStringLiteral("sales"), saleId,
            QStringLiteral("{\"status\":\"%1\"}").arg(jesc(status)),
            QStringLiteral("{\"status\":\"VOIDED\",\"receipt_number\":\"%1\",\"lines\":%2}")
                .arg(jesc(receiptNumber))
                .arg(lines.size()));

        if (!m_db.commit()) {
            m_db.rollback();
            throw ReturnError{m_db.lastError().text()};
        }
        inTxn = false;

        res.ok = true;
        res.saleId = saleId;
        return res;

    } catch (const ReturnError &e) {
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
