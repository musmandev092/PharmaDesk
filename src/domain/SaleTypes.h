#pragma once

#include <QString>
#include <QVector>

// Plain data-transfer types for the sale-commit path, in the lowest (domain)
// layer so BOTH the service that produces them and the data/UI layers that read
// them can depend DOWNWARD on this header. Previously these lived in
// service/SaleService.h, which forced data/ headers (SaleRepository, EscPosRenderer)
// to include UP into service/ — a reverse layer edge. Keeping them here removes
// that edge without changing any type name or field.

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
