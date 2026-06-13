#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>

// One returnable line of a looked-up sale: a sale_items row plus how much of it
// has already been returned (summed across prior `returns` rows). Quantities are
// in BASE units (tablets/ampoules) so partial-pack returns work — a customer who
// bought 1 strip of 10 can bring back 3 loose tablets.
struct ReturnableLine
{
    qint64 saleItemId = 0;
    qint64 medicineId = 0;
    qint64 batchId = 0;
    QString name; // brand (+ strength)
    QString batchNumber;
    QString soldUnitLabel;  // e.g. "STRIP"
    int qtySoldDisplay = 0; // qty in the sold unit (display)
    int qtySold = 0;        // qty_in_base_units (the returnable quantity)
    int qtyAlreadyReturned = 0;
    QString unitMrp;   // decimal string, per BASE unit
    QString lineTotal; // decimal string
};

// A looked-up sale header for the returns screen.
struct ReturnSaleHeader
{
    bool found = false;
    qint64 saleId = 0;
    QString receiptNumber;
    QString soldAt;
    QString cashierName;
    QString paymentMode;
    QString status;     // COMPLETED / VOIDED / REFUNDED_PARTIAL / REFUNDED_FULL
    QString grandTotal; // decimal string
    QVector<ReturnableLine> lines;
};

// A row in the recent-returns history table.
struct ReturnRow
{
    qint64 id = 0;
    QString returnNumber;
    QString receiptNumber; // of the original sale
    QString medicineName;
    int qtyReturned = 0;
    QString refundAmount; // decimal string
    QString reason;
    QString status; // PENDING_REVIEW / APPROVED_RESTOCK / RETURN_TO_SUPPLIER / WRITE_OFF
    QString createdAt;
};

// A PENDING_REVIEW return awaiting a manager's adjudication.
struct PendingReturn
{
    qint64 id = 0;
    QString returnNumber;
    QString receiptNumber;
    QString medicineName;
    QString batchNumber;
    int qtyReturned = 0;
    QString refundAmount;
    QString reason;
    QString physicalCondition;
    QString initiatedBy; // full name
    QString createdAt;
};

// A RETURN_TO_SUPPLIER return in the supplier-settlement queue.
struct SupplierReturn
{
    qint64 id = 0;
    QString returnNumber;
    QString medicineName;
    int qtyReturned = 0;
    QString refundAmount;
    QString adjudicatedAt;
    bool settled = false;
    QString settledAt;
    QString supplierReference;
};

// Read-only access for the Returns module: look up a sale (+ its returnable
// lines) by receipt number, and list recent returns. Mirrors the queries in
// pos/frontend/pages/pos/returns/initiate.php and admin/returns.php.
class ReturnRepository
{
public:
    explicit ReturnRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    // Look up a sale by (case-insensitive) receipt number. Tolerates an
    // unpadded numeric suffix ("INV-20260518-1" → "...-0001"), matching the
    // PHP initiate page. header.found = false if no sale matches.
    ReturnSaleHeader lookupByReceipt(const QString &receiptNumber) const;

    // Most recent returns (newest first).
    QVector<ReturnRow> listRecent(int limit = 200) const;

    // The adjudication queue: returns still in PENDING_REVIEW (oldest first).
    QVector<PendingReturn> listPending() const;

    // The supplier-settlement queue: RETURN_TO_SUPPLIER returns (unsettled first).
    QVector<SupplierReturn> listSupplierReturns() const;

private:
    QSqlDatabase m_db;
};
