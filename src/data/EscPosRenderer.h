#pragma once

#include "domain/SaleTypes.h" // SaleResult / SaleResultLine (neutral DTO header)

#include <QByteArray>
#include <QString>

// Header/footer fields for a thermal receipt (pulled from settings + the sale's
// cashier/time by the caller).
struct EscPosHeader
{
    QString pharmacyName;
    QString address;
    QString phone;
    QString ntn;
    QString cashierName;
    QString soldAt; // display string, e.g. "09/06/26 14:30"
    QString returnPolicy;
    QString footerMessage;
};

// ESC/POS receipt byte-stream builder for an 80mm (48-column) thermal printer.
// Port of pos/backend/services/EscPos.php — same width, command bytes, and
// layout. Pure: no DB, no Qt widgets — unit-testable by asserting on the bytes.
class EscPosRenderer
{
public:
    static constexpr int Width = 48;

    // Build the full receipt byte stream (init … cut) for a committed sale.
    static QByteArray receipt(const SaleResult &sale, const EscPosHeader &header);

    // A printer self-test page (used by the "test print" action).
    static QByteArray selfTest(const QString &queueName, const QString &productName);
};
