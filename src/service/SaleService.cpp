#include "service/SaleService.h"

#include "data/Audit.h"
#include "data/SessionRepository.h"
#include "data/UserRepository.h"
#include "domain/ControlledSubstancePolicy.h"
#include "domain/DiscountAuthorizationPolicy.h"
#include "domain/Fefo.h"
#include "domain/Money.h"
#include "domain/SaleCalculator.h"

#include <QDate>
#include <QDateTime>
#include <QRandomGenerator>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <cmath>

namespace {
// Thrown for input/business validation failures; carries a user-facing message.
struct SaleError
{
    QString message;
};
} // namespace

QString SaleService::nextReceiptNumber()
{
    const QString today = QDate::currentDate().toString(QStringLiteral("yyyyMMdd"));

    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT count(*) FROM sales "
                          "WHERE date(sold_at, 'localtime') = date('now', 'localtime')"));
    int count = 0;
    if (q.next()) {
        count = q.value(0).toInt();
    }
    const QString candidate
        = QStringLiteral("INV-%1-%2").arg(today).arg(count + 1, 4, 10, QLatin1Char('0'));

    QSqlQuery chk(m_db);
    chk.prepare(QStringLiteral("SELECT 1 FROM sales WHERE receipt_number = ? LIMIT 1"));
    chk.addBindValue(candidate);
    if (chk.exec() && !chk.next()) {
        return candidate;
    }
    // Defensive fallback — non-monotonic suffix rather than fail the sale.
    const quint32 r = QRandomGenerator::global()->generate();
    return QStringLiteral("INV-%1-%2").arg(today).arg(r % 1000000u, 6, 10, QLatin1Char('0'));
}

SaleResult SaleService::commit(const SaleInput &input)
{
    SaleResult res;
    res.paymentMode = input.paymentMode;
    bool inTxn = false;

    try {
        if (input.items.isEmpty()) {
            throw SaleError{QStringLiteral("Sale must contain at least one item.")};
        }

        // ── Compute totals (server-side; never trust the client). ──────────
        Money subtotalSum;
        struct Line
        {
            SaleLineInput in;
            qint64 qtyBase;
            QString lineSub;
        };
        QVector<Line> lines;
        for (const SaleLineInput &row : input.items) {
            const int qtyDisplay = row.qtySoldDisplay;
            if (qtyDisplay < 1) {
                throw SaleError{QStringLiteral("Every cart line must have qty ≥ 1.")};
            }
            if (qtyDisplay > 9999) {
                throw SaleError{
                    QStringLiteral("Quantity %1 is unrealistically large — confirm the scan.")
                        .arg(qtyDisplay)};
            }
            const Money unitMrp = Money::fromString(row.unitMrp);
            if (unitMrp.isNegative()) {
                throw SaleError{QStringLiteral("Unit MRP cannot be negative.")};
            }
            const qint64 qtyBase = static_cast<qint64>(qtyDisplay) * qMax(1, row.soldUnitFactor);
            const QString lineSub = SaleCalculator::lineSubtotal(qtyBase, row.unitMrp);
            subtotalSum = subtotalSum + Money::fromString(lineSub);
            lines.push_back({row, qtyBase, lineSub});
        }

        const Money subtotal = Money::fromString(subtotalSum.toString(Money::ScaleMoney));
        const Money discount
            = Money::fromString(Money::fromString(input.discountTotal).toString(Money::ScaleMoney));
        if (discount.isNegative()) {
            throw SaleError{QStringLiteral("Discount cannot be negative.")};
        }
        if (discount.compare(subtotal) > 0) {
            throw SaleError{QStringLiteral("Discount (PKR %1) cannot exceed subtotal (PKR %2).")
                                .arg(discount.fmt(), subtotal.fmt())};
        }
        const Money grand = Money::fromString((subtotal - discount).toString(Money::ScaleMoney));
        if (grand.isNegative()) {
            throw SaleError{QStringLiteral("Grand total cannot be negative.")};
        }

        if (input.paymentMode != QLatin1String("CASH") && input.paymentMode != QLatin1String("CARD")
            && input.paymentMode != QLatin1String("OTHER")) {
            throw SaleError{QStringLiteral("Invalid payment mode.")};
        }

        bool haveTendered = !input.amountTendered.trimmed().isEmpty();
        Money tendered;
        if (haveTendered) {
            tendered = Money::fromString(
                Money::fromString(input.amountTendered).toString(Money::ScaleMoney));
            if (tendered.isNegative()) {
                throw SaleError{QStringLiteral("Amount tendered cannot be negative.")};
            }
        }
        if (input.paymentMode == QLatin1String("CASH")) {
            if (!haveTendered || tendered.compare(grand) < 0) {
                throw SaleError{
                    QStringLiteral("Cash sale requires amount tendered ≥ grand total.")};
            }
        }
        const QString changeStr
            = haveTendered ? SaleCalculator::change(tendered.toString(), grand.toString())
                           : QString();

        // ── Controlled-substance gate + discount authorization (Phase 1). ──
        // Re-validated here server-side even though the UI prompts first.
        QStringList schedules;
        {
            QSet<qint64> ids;
            for (const Line &l : lines) {
                ids.insert(l.in.medicineId);
            }
            if (!ids.isEmpty()) {
                QStringList placeholders;
                for (int i = 0; i < ids.size(); ++i) {
                    placeholders << QStringLiteral("?");
                }
                QSqlQuery sq(m_db);
                sq.prepare(
                    QStringLiteral(
                        "SELECT DISTINCT controlled_schedule FROM medicines WHERE id IN (%1)")
                        .arg(placeholders.join(QLatin1Char(','))));
                for (qint64 id : ids) {
                    sq.addBindValue(id);
                }
                if (!sq.exec()) {
                    // Fail CLOSED: a DB error must never silently clear controlled
                    // status and let a narcotic through without a witness/license.
                    throw SaleError{QStringLiteral("Could not verify controlled-substance status: ")
                                    + sq.lastError().text()};
                }
                while (sq.next()) {
                    schedules << sq.value(0).toString();
                }
            }
        }
        const bool hasControlled = ControlledSubstancePolicy::anyControlled(schedules);
        UserRepository userRepo(m_db);

        if (ControlledSubstancePolicy::anyRequiresPrescriberLicense(schedules)
            && input.prescriberLicense.trimmed().isEmpty()) {
            throw SaleError{QStringLiteral("This sale includes a controlled medicine — enter the "
                                           "prescriber's license number.")};
        }
        qint64 witnessId = 0;
        QString witnessAtIso;
        if (ControlledSubstancePolicy::anyRequiresWitness(schedules)) {
            witnessId = input.controlledWitnessUserId;
            if (witnessId <= 0) {
                throw SaleError{QStringLiteral(
                    "This sale includes a controlled medicine — a manager/admin witness PIN is "
                    "required (two-person rule).")};
            }
            if (witnessId == m_cashierId) {
                throw SaleError{
                    QStringLiteral("The witness must be a different user than the cashier.")};
            }
            if (!userRepo.isActiveManagerOrAdmin(witnessId)) {
                throw SaleError{QStringLiteral("The witness must be an active manager or admin.")};
            }
            witnessAtIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        }

        const bool discountPositive = discount.compare(Money()) > 0;
        qint64 discountAuthBy = 0;
        if (discountPositive) {
            const QString cashierRole = userRepo.roleOf(m_cashierId);
            if (DiscountAuthorizationPolicy::requiresManagerOverride(cashierRole, true)) {
                discountAuthBy = input.discountAuthorizedBy;
                if (discountAuthBy <= 0) {
                    throw SaleError{QStringLiteral(
                        "A manager/admin override is required to apply a discount.")};
                }
                if (discountAuthBy == m_cashierId
                    || !userRepo.isActiveManagerOrAdmin(discountAuthBy)) {
                    throw SaleError{QStringLiteral(
                        "The discount must be authorized by a different active manager or admin.")};
                }
            } else {
                // Managerial cashier self-authorizes (or honors a supplied authorizer).
                discountAuthBy
                    = input.discountAuthorizedBy > 0 ? input.discountAuthorizedBy : m_cashierId;
            }
        }

        // ── Persist (one transaction). ─────────────────────────────────────
        if (!m_db.transaction()) {
            throw SaleError{m_db.lastError().text()};
        }
        inTxn = true;

        const QString receipt = nextReceiptNumber();
        const qint64 sessionId = SessionRepository(m_db).openSessionId(m_cashierId);

        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "INSERT INTO sales "
            "(branch_id, receipt_number, cashier_id, cashier_session_id, customer_name, "
            "customer_phone, "
            " has_controlled_drug, prescriber_license_number, narcotic_witness_user_id, "
            "narcotic_witness_at, "
            " discount_authorized_by, subtotal, tax_total, discount_total, pos_service_fee, "
            " grand_total, amount_tendered, change_returned, payment_mode, status, sold_at) "
            "VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, '0', ?, '0', ?, ?, ?, ?, 'COMPLETED', "
            "CURRENT_TIMESTAMP)"));
        q.addBindValue(receipt);
        q.addBindValue(m_cashierId);
        q.addBindValue(sessionId > 0 ? QVariant(sessionId) : QVariant());
        q.addBindValue(input.customerName.isEmpty() ? QVariant() : QVariant(input.customerName));
        q.addBindValue(input.customerPhone.isEmpty() ? QVariant() : QVariant(input.customerPhone));
        q.addBindValue(hasControlled ? 1 : 0);
        q.addBindValue(input.prescriberLicense.trimmed().isEmpty()
                           ? QVariant()
                           : QVariant(input.prescriberLicense.trimmed()));
        q.addBindValue(witnessId > 0 ? QVariant(witnessId) : QVariant());
        q.addBindValue(witnessAtIso.isEmpty() ? QVariant() : QVariant(witnessAtIso));
        q.addBindValue(discountAuthBy > 0 ? QVariant(discountAuthBy) : QVariant());
        q.addBindValue(subtotal.toString());
        q.addBindValue(discount.toString());
        q.addBindValue(grand.toString());
        q.addBindValue(haveTendered ? QVariant(tendered.toString()) : QVariant());
        q.addBindValue(haveTendered ? QVariant(changeStr) : QVariant());
        q.addBindValue(input.paymentMode);
        if (!q.exec()) {
            throw SaleError{q.lastError().text()};
        }
        const qint64 saleId = q.lastInsertId().toLongLong();

        // ── Per-item FEFO allocation + writes. ─────────────────────────────
        for (const Line &line : lines) {
            const qint64 medicineId = line.in.medicineId;
            const int qtyNeeded = static_cast<int>(line.qtyBase);
            const int factor = qMax(1, line.in.soldUnitFactor);

            QVector<FefoBatch> batches;
            {
                QSqlQuery bq(m_db);
                bq.prepare(QStringLiteral(
                    "SELECT id, current_qty, cost_per_unit, mrp_per_unit, expiry_date, "
                    "       is_quarantined, is_expired FROM batches "
                    " WHERE medicine_id = ? AND current_qty > 0 AND is_quarantined = 0 "
                    "   AND is_expired = 0 AND expiry_date > date('now', 'localtime') "
                    " ORDER BY expiry_date ASC, id ASC"));
                bq.addBindValue(medicineId);
                if (!bq.exec()) {
                    throw SaleError{bq.lastError().text()};
                }
                while (bq.next()) {
                    FefoBatch b;
                    b.id = bq.value(0).toLongLong();
                    b.currentQty = bq.value(1).toInt();
                    b.costPerUnit = bq.value(2).toString();
                    b.mrpPerUnit = bq.value(3).toString();
                    b.expiry
                        = QDate::fromString(bq.value(4).toString(), QStringLiteral("yyyy-MM-dd"));
                    b.quarantined = bq.value(5).toInt() != 0;
                    b.expired = bq.value(6).toInt() != 0;
                    batches.push_back(b);
                }
            }

            Fefo::assertSortedByExpiry(batches);
            const QVector<FefoAllocation> allocations
                = Fefo::allocate(batches, qtyNeeded, medicineId);

            for (const FefoAllocation &alloc : allocations) {
                const int deduction = alloc.deduction;
                const int qtyBefore = alloc.batch.currentQty;
                const int qtyAfter = qtyBefore - deduction;

                QSqlQuery uq(m_db);
                // Guarded, atomic decrement: subtract relative to the live row and
                // require enough stock, so a stale read or concurrent writer can
                // never drive current_qty negative (oversell). numRowsAffected()==1
                // confirms the row matched the >= guard.
                uq.prepare(QStringLiteral(
                    "UPDATE batches SET current_qty = current_qty - ?, "
                    "updated_at = CURRENT_TIMESTAMP WHERE id = ? AND current_qty >= ?"));
                uq.addBindValue(deduction);
                uq.addBindValue(alloc.batch.id);
                uq.addBindValue(deduction);
                if (!uq.exec()) {
                    throw SaleError{uq.lastError().text()};
                }
                if (uq.numRowsAffected() != 1) {
                    throw SaleError{
                        QStringLiteral("Stock changed during checkout — please retry.")};
                }

                const QString lineSub
                    = SaleCalculator::lineSubtotal(deduction, alloc.batch.mrpPerUnit);
                const int qtyDisplay
                    = static_cast<int>(std::ceil(static_cast<double>(deduction) / factor));

                QSqlQuery iq(m_db);
                iq.prepare(QStringLiteral(
                    "INSERT INTO sale_items "
                    "(sale_id, medicine_id, batch_id, qty_in_base_units, sold_unit_label, "
                    " sold_unit_factor, qty_sold_display, unit_cost, unit_mrp, line_subtotal, "
                    " line_tax, line_discount, line_total) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, '0', '0', ?)"));
                iq.addBindValue(saleId);
                iq.addBindValue(medicineId);
                iq.addBindValue(alloc.batch.id);
                iq.addBindValue(deduction);
                iq.addBindValue(line.in.soldUnitLabel);
                iq.addBindValue(factor);
                iq.addBindValue(qtyDisplay);
                iq.addBindValue(alloc.batch.costPerUnit);
                iq.addBindValue(alloc.batch.mrpPerUnit);
                iq.addBindValue(lineSub);
                iq.addBindValue(lineSub);
                if (!iq.exec()) {
                    throw SaleError{iq.lastError().text()};
                }

                QSqlQuery mq(m_db);
                mq.prepare(QStringLiteral(
                    "INSERT INTO inventory_movements "
                    "(branch_id, medicine_id, batch_id, movement_type, qty_delta, qty_before, "
                    " qty_after, ref_table, ref_id, performed_by) "
                    "VALUES (1, ?, ?, 'SALE', ?, ?, ?, 'sales', ?, ?)"));
                mq.addBindValue(medicineId);
                mq.addBindValue(alloc.batch.id);
                mq.addBindValue(-deduction);
                mq.addBindValue(qtyBefore);
                mq.addBindValue(qtyAfter);
                mq.addBindValue(saleId);
                mq.addBindValue(m_cashierId);
                if (!mq.exec()) {
                    throw SaleError{mq.lastError().text()};
                }
            }

            // Receipt line (as entered by the cashier).
            SaleResultLine rl;
            rl.name = QString::number(medicineId); // replaced below with the real name
            rl.qty = line.in.qtySoldDisplay;
            rl.unitLabel = line.in.soldUnitLabel;
            rl.unitMrp = line.in.unitMrp;
            rl.lineTotal = line.lineSub;
            res.lines.push_back(rl);
        }

        // Bump the open cashier shift's totals (no-op if no open session).
        SessionRepository(m_db).bumpForSale(sessionId, grand.toString(), input.paymentMode);

        Audit::writeOrThrow(
            m_db, m_cashierId, QStringLiteral("SALE_COMPLETED"), QStringLiteral("sales"), saleId,
            QString(),
            QStringLiteral("{\"receipt_number\":\"%1\",\"grand_total\":\"%2\",\"items\":%3}")
                .arg(receipt, grand.toString())
                .arg(input.items.size()));

        if (hasControlled) {
            // Audit verb matches the PHP spec (Sale.php: 'NARCOTIC_DISPENSED') so
            // DRAP/narcotic compliance reports keyed on this string keep working.
            Audit::writeOrThrow(m_db, m_cashierId, QStringLiteral("NARCOTIC_DISPENSED"),
                                QStringLiteral("sales"), saleId, QString(),
                                QStringLiteral("{\"receipt_number\":\"%1\",\"witness_id\":%2,"
                                               "\"prescriber_license\":\"%3\"}")
                                    .arg(receipt)
                                    .arg(witnessId)
                                    .arg(input.prescriberLicense.trimmed()));
        }
        if (discountPositive && discountAuthBy > 0) {
            Audit::writeOrThrow(
                m_db, discountAuthBy, QStringLiteral("DISCOUNT_AUTHORIZED"),
                QStringLiteral("sales"), saleId, QString(),
                QStringLiteral("{\"receipt_number\":\"%1\",\"discount_total\":\"%2\","
                               "\"authorized_by\":%3}")
                    .arg(receipt, discount.toString())
                    .arg(discountAuthBy));
        }

        if (!m_db.commit()) {
            m_db.rollback();
            throw SaleError{m_db.lastError().text()};
        }
        inTxn = false;

        // Fill in medicine display names for the receipt lines.
        for (int i = 0; i < res.lines.size(); ++i) {
            QSqlQuery nq(m_db);
            nq.prepare(QStringLiteral(
                "SELECT brand_name, COALESCE(strength,'') FROM medicines WHERE id = ?"));
            nq.addBindValue(input.items.at(i).medicineId);
            if (nq.exec() && nq.next()) {
                const QString brand = nq.value(0).toString();
                const QString strength = nq.value(1).toString();
                res.lines[i].name
                    = strength.isEmpty() ? brand : (brand + QLatin1Char(' ') + strength);
            }
        }

        res.ok = true;
        res.saleId = saleId;
        res.receiptNumber = receipt;
        res.subtotal = subtotal.toString();
        res.discountTotal = discount.toString();
        res.grandTotal = grand.toString();
        res.amountTendered = haveTendered ? tendered.toString() : QString();
        res.changeReturned = haveTendered ? changeStr : QString();
        return res;

    } catch (const SaleError &e) {
        if (inTxn) m_db.rollback();
        res.ok = false;
        res.error = e.message;
        return res;
    } catch (const InsufficientStockException &e) {
        if (inTxn) m_db.rollback();
        res.ok = false;
        res.error = QStringLiteral("Not enough stock: needed %1, only %2 available.")
                        .arg(e.needed)
                        .arg(e.available);
        return res;
    } catch (const std::exception &e) {
        if (inTxn) m_db.rollback();
        res.ok = false;
        res.error = QString::fromUtf8(e.what());
        return res;
    }
}
