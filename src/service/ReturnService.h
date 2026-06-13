#pragma once

#include <QSqlDatabase>
#include <QString>

struct ReturnResult
{
    bool ok = false;
    QString error;
    qint64 returnId = -1;
    QString returnNumber;
    QString refundAmount; // decimal string, scale 2
    QString saleStatus;   // resulting status of the original sale
};

struct VoidResult
{
    bool ok = false;
    QString error;
    qint64 saleId = -1;
};

// Result of an adjudication / supplier-settlement step.
struct ReturnOpResult
{
    bool ok = false;
    QString error;
    QString saleStatus; // resulting status of the original sale (after adjudication)
};

// Returns / refunds / void. Port of pos/backend/services/Returns.php.
//
// Two-step workflow (the PHP model):
//   initiate()  → status PENDING_REVIEW + a RETURN_QUARANTINE ledger row; stock
//                 is NOT changed (goods are physically out, awaiting review).
//   adjudicate()→ a manager sets the final disposition: APPROVED_RESTOCK (stock
//                 += qty, RETURN_RESTOCK row), or RETURN_TO_SUPPLIER / WRITE_OFF
//                 (stock stays out, a zero-delta WRITE_OFF row records where it
//                 went), then the original sale's status is recomputed.
//   markSupplierReturnSent() closes the supplier-credit loop for RETURN_TO_SUPPLIER.
//
// commitReturn() is retained as an express one-step path (initiate + immediate
// final disposition) for simple counter refunds. All paths run in one
// transaction, are audited, and never leave stock inconsistent. Controlled
// medicines require a manager/admin witness (≠ the operator) at initiate time.
class ReturnService
{
public:
    ReturnService(QSqlDatabase db, qint64 userId) : m_db(std::move(db)), m_userId(userId) {}

    // Step 1 — file a return for review. `qtyReturned` is in BASE units.
    // reason ∈ {CUSTOMER_CHANGED_MIND, DAMAGED, WRONG_ITEM, ADVERSE_REACTION,
    // EXPIRED, OTHER}. For a controlled medicine, controlledWitnessUserId must be
    // an active MANAGER/ADMIN other than the operator. Inserts a PENDING_REVIEW
    // row + RETURN_QUARANTINE movement; stock is unchanged.
    ReturnResult initiate(qint64 saleItemId, int qtyReturned, const QString &reason,
                          const QString &physicalCondition = QString(),
                          qint64 controlledWitnessUserId = 0);

    // Step 2 — a manager sets the final disposition on a PENDING_REVIEW return.
    // newStatus ∈ {APPROVED_RESTOCK, RETURN_TO_SUPPLIER, WRITE_OFF}. RESTOCK is
    // refused if the batch is now quarantined/expired.
    ReturnOpResult adjudicate(qint64 returnId, const QString &newStatus,
                              const QString &notes = QString());

    // Record that a RETURN_TO_SUPPLIER return has been physically shipped back,
    // with an optional supplier credit-note / waybill reference.
    ReturnOpResult markSupplierReturnSent(qint64 returnId,
                                          const QString &supplierReference = QString());

    // Express one-step refund: initiate + immediate final disposition.
    //   restock — true: APPROVED_RESTOCK (stock += qty); false: WRITE_OFF.
    // Controlled medicines still require controlledWitnessUserId.
    ReturnResult commitReturn(qint64 saleItemId, int qtyReturned, const QString &reason,
                              bool restock, const QString &physicalCondition = QString(),
                              qint64 controlledWitnessUserId = 0);

    // Void an entire sale (only if it is still COMPLETED). Every line is restocked
    // (current_qty += qty_in_base_units, RETURN_RESTOCK movement) and the sale is
    // set to VOIDED. If sameDayOnly, refuses sales not made today.
    VoidResult voidSale(qint64 saleId, bool sameDayOnly = false);

private:
    QString nextReturnNumber();
    // Recompute REFUNDED_PARTIAL / REFUNDED_FULL from final-status returns.
    QString recomputeSaleStatus(qint64 saleId);

    QSqlDatabase m_db;
    qint64 m_userId;
};
