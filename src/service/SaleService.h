#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>

// One requested cart line (client-supplied; the server recomputes money).
struct SaleLineInput
{
    qint64 medicineId = 0;
    int qtySoldDisplay = 0; // quantity in the sold unit
    QString soldUnitLabel;  // e.g. "TABLET"
    int soldUnitFactor = 1; // base units per sold unit
    QString unitMrp;        // decimal string (scale 2)
};

struct SaleInput
{
    QVector<SaleLineInput> items;
    QString paymentMode = QStringLiteral("CASH"); // CASH / CARD / OTHER
    QString customerName;
    QString customerPhone;
    QString discountTotal = QStringLiteral("0"); // decimal string (scale 2)
    QString amountTendered;                      // empty → none

    // Compliance & authorization (Phase 1). The UI collects these (PIN-verified)
    // and the service re-validates them server-side as defense in depth.
    QString prescriberLicense; // required when a license-gated controlled item is in the cart
    qint64 controlledWitnessUserId
        = 0; // active MANAGER/ADMIN ≠ cashier; required when a witness-gated item is present
    qint64 discountAuthorizedBy
        = 0; // manager/admin who approved the discount (self for managerial cashiers)
};

// Result of a committed sale (for the receipt preview).
struct SaleResultLine
{
    QString name; // brand + strength
    int qty = 0;  // base units
    QString unitLabel;
    QString unitMrp;
    QString lineTotal;
};
struct SaleResult
{
    bool ok = false;
    QString error; // set when ok == false
    qint64 saleId = -1;
    QString receiptNumber;
    QString subtotal;
    QString discountTotal;
    QString grandTotal;
    QString amountTendered;
    QString changeReturned;
    QString paymentMode;
    QVector<SaleResultLine> lines;
};

// Atomic sale commit. Port of pos/backend/services/Sale.php (minus the
// Postgres-specific advisory lock / SERIALIZABLE retry, which a single-PC
// SQLite connection doesn't need). Enforces the money/stock/FEFO guardrails, the
// controlled-substance gate (prescriber license + manager/admin witness) and
// manager-authorized discounts.
class SaleService
{
public:
    SaleService(QSqlDatabase db, qint64 cashierId) : m_db(std::move(db)), m_cashierId(cashierId) {}

    SaleResult commit(const SaleInput &input);

private:
    QString nextReceiptNumber();

    QSqlDatabase m_db;
    qint64 m_cashierId;
};
