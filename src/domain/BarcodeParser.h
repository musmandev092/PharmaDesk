#pragma once

#include <QDate>
#include <QString>

// Scanned barcode / QR parser. A focused, dependency-light port of the common
// cases handled by pos/backend/services/QrParser.php — the formats a keyboard-
// wedge or USB scanner actually feeds a POS terminal:
//
//   • Plain EAN-8 / UPC-A / EAN-13 / GTIN-14 (or ITF-14) — bare numeric strings.
//   • GS1 AI element strings — DataMatrix / GS1-128 / DataBar, with or without
//     the FNC1 / Group-Separator (0x1D). AIs handled: (01)=GTIN, (17)=expiry
//     YYMMDD, (10)=lot, (21)=serial (plus (00),(02),(11),(15) for robustness).
//   • GS1 Digital Link URLs — https://host/01/<gtin>/10/<lot>/17/<exp>/21/<sn>.
//
// Pure function, no DB / Qt-Sql / network dependencies (QString /
// QRegularExpression / QDate only). Anything it cannot confidently decode comes
// back as PlainText with raw == the trimmed input, so the caller can fall back
// to a normal name/SKU search.
namespace Barcode {

struct ParsedBarcode
{
    enum Type
    {
        PlainText,
        GTIN,
        GS1,
        GS1DigitalLink
    };

    Type type = PlainText;
    QString raw;    // original (trimmed) string passed in
    QString gtin;   // canonical 14-digit GTIN (left-padded with zeros), or empty
    QString gtin13; // 13-digit form (leading zero stripped), or empty
    QString lot;    // AI (10) batch / lot number, or empty
    QString expiry; // AI (17) expiry as ISO yyyy-MM-dd if decodable, else empty
    QString serial; // AI (21) pack serial number, or empty

    // True when the scan was recognized as an actual barcode (GTIN / GS1 /
    // Digital Link), i.e. not a free-text fallback. When false, treat `raw` as
    // a plain search query.
    bool isBarcode = false;
};

// Parse a single scanned string. Never throws; always returns a value.
ParsedBarcode parse(const QString &scanned);

// GS1 YYMMDD → ISO yyyy-MM-dd. DD=00 means "last day of the month" (GS1 rule).
// Returns an empty string for malformed input. Exposed for testing.
QString gs1DateToIso(const QString &yymmdd);

// Left-pad a numeric string to the canonical 14-digit GTIN. Strips non-digits.
// Returns empty for empty input. Exposed for testing.
QString normalizeGtin(const QString &digits);

// 14-digit GTIN with leading zeros stripped (down to a minimum of 13 digits,
// matching the EAN-13 retail form). Exposed for testing.
QString gtin13From(const QString &gtin14);

} // namespace Barcode
