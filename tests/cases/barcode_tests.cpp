// Exhaustive unit tests for the scanned-barcode / GS1 parser
// (domain/BarcodeParser). Pure: no DB / userId needed. Feeds many strings —
// bare GTINs, GS1 AI element strings (with and without FNC1), GS1 Digital Link
// URLs, non-GTIN symbologies and free text — and asserts the isBarcode flag,
// extracted gtin / gtin13 / lot / expiry / serial, and raw preservation.
//
// All expectations match the actual header/impl behavior, in particular:
//   • normalizeGtin: >=14 digits → rightmost 14; else left-pad to 14.
//   • gtin13From:    strip leading zeros down to a minimum of 13 digits.
//   • bare numeric is a GTIN only for length 8..18 (inclusive).
//   • GS1 AI string must start with (01|00|02) followed by a digit.
//   • Digital Link requires ^https?://host/01/<8..14 digits>.

#include "domain/BarcodeParser.h"
#include "framework/TestStats.h"

#include <QChar>
#include <QString>

using Barcode::ParsedBarcode;

namespace pharmadesk_tests {

namespace {

const QChar kGs = QChar(0x1D);

// Expected canonical 14-digit GTIN for a bare numeric string of len 8..18.
QString expectGtin14(const QString &digits)
{
    if (digits.size() >= 14) {
        return digits.right(14);
    }
    return digits.rightJustified(14, QLatin1Char('0'));
}

QString expectGtin13(const QString &gtin14)
{
    QString s = gtin14;
    while (s.size() > 13 && s.startsWith(QLatin1Char('0'))) {
        s.remove(0, 1);
    }
    return s;
}

} // namespace

TestStats run_barcode_tests(QSqlDatabase db, qint64 userId)
{
    (void)db;
    (void)userId;
    TestStats s;
    s.module = QStringLiteral("barcode");

    // --------------------------------------------------------------------
    // 1) gs1DateToIso direct: many YYMMDD inputs.
    // --------------------------------------------------------------------
    s.check(Barcode::gs1DateToIso(QStringLiteral("260101")) == QStringLiteral("2026-01-01"),
            QStringLiteral("date 260101"));
    s.check(Barcode::gs1DateToIso(QStringLiteral("991231")) == QStringLiteral("1999-12-31"),
            QStringLiteral("date 991231 → 1999"));
    s.check(Barcode::gs1DateToIso(QStringLiteral("000101")) == QStringLiteral("2000-01-01"),
            QStringLiteral("date 000101 → 2000"));
    s.check(Barcode::gs1DateToIso(QStringLiteral("490101")) == QStringLiteral("2049-01-01"),
            QStringLiteral("date 49 → 2049 (pivot)"));
    s.check(Barcode::gs1DateToIso(QStringLiteral("500101")) == QStringLiteral("1950-01-01"),
            QStringLiteral("date 50 → 1950 (pivot)"));
    s.check(Barcode::gs1DateToIso(QStringLiteral("260200")) == QStringLiteral("2026-02-28"),
            QStringLiteral("date DD=00 → end of Feb 2026"));
    s.check(Barcode::gs1DateToIso(QStringLiteral("240200")) == QStringLiteral("2024-02-29"),
            QStringLiteral("date DD=00 → end of Feb 2024 (leap)"));
    s.check(Barcode::gs1DateToIso(QStringLiteral("260400")) == QStringLiteral("2026-04-30"),
            QStringLiteral("date DD=00 → end of Apr (30)"));
    s.check(Barcode::gs1DateToIso(QStringLiteral("261200")) == QStringLiteral("2026-12-31"),
            QStringLiteral("date DD=00 → end of Dec (31)"));
    // Malformed dates → empty.
    s.check(Barcode::gs1DateToIso(QStringLiteral("261301")).isEmpty(),
            QStringLiteral("date month 13 invalid"));
    s.check(Barcode::gs1DateToIso(QStringLiteral("260001")).isEmpty(),
            QStringLiteral("date month 00 invalid"));
    s.check(Barcode::gs1DateToIso(QStringLiteral("260230")).isEmpty(),
            QStringLiteral("date Feb 30 invalid"));
    s.check(Barcode::gs1DateToIso(QStringLiteral("26010")).isEmpty(),
            QStringLiteral("date 5 chars invalid"));
    s.check(Barcode::gs1DateToIso(QStringLiteral("2601011")).isEmpty(),
            QStringLiteral("date 7 chars invalid"));
    s.check(Barcode::gs1DateToIso(QStringLiteral("26AB01")).isEmpty(),
            QStringLiteral("date non-digit invalid"));
    s.check(Barcode::gs1DateToIso(QString()).isEmpty(), QStringLiteral("date empty invalid"));
    s.check(Barcode::gs1DateToIso(QStringLiteral("260131")) == QStringLiteral("2026-01-31"),
            QStringLiteral("date 260131 valid Jan 31"));
    s.check(Barcode::gs1DateToIso(QStringLiteral("260615")) == QStringLiteral("2026-06-15"),
            QStringLiteral("date 260615 mid-month"));

    // --------------------------------------------------------------------
    // 2) normalizeGtin direct.
    // --------------------------------------------------------------------
    s.check(Barcode::normalizeGtin(QStringLiteral("12345678")) == QStringLiteral("00000012345678"),
            QStringLiteral("normalize 8→pad14"));
    s.check(Barcode::normalizeGtin(QStringLiteral("8901173047861"))
                == QStringLiteral("08901173047861"),
            QStringLiteral("normalize 13→pad14"));
    s.check(Barcode::normalizeGtin(QStringLiteral("03456789012345"))
                == QStringLiteral("03456789012345"),
            QStringLiteral("normalize 14 unchanged"));
    s.check(Barcode::normalizeGtin(QStringLiteral("123456789012345"))
                == QStringLiteral("23456789012345"),
            QStringLiteral("normalize 15→rightmost14"));
    s.check(Barcode::normalizeGtin(QStringLiteral("12-34 56")) == QStringLiteral("00000000123456"),
            QStringLiteral("normalize strips non-digits"));
    s.check(Barcode::normalizeGtin(QStringLiteral("abc")).isEmpty(),
            QStringLiteral("normalize all non-digit → empty"));
    s.check(Barcode::normalizeGtin(QString()).isEmpty(), QStringLiteral("normalize empty → empty"));

    // --------------------------------------------------------------------
    // 3) gtin13From direct.
    // --------------------------------------------------------------------
    s.check(Barcode::gtin13From(QStringLiteral("08901173047861"))
                == QStringLiteral("8901173047861"),
            QStringLiteral("gtin13 strips one leading zero"));
    s.check(Barcode::gtin13From(QStringLiteral("00000012345678"))
                == QStringLiteral("0000012345678"),
            QStringLiteral("gtin13 strips only down to 13 digits"));
    s.check(Barcode::gtin13From(QStringLiteral("03456789012345"))
                == QStringLiteral("3456789012345"),
            QStringLiteral("gtin13 one zero from 14"));
    s.check(Barcode::gtin13From(QStringLiteral("13456789012345"))
                == QStringLiteral("13456789012345"),
            QStringLiteral("gtin13 no leading zero unchanged"));
    s.check(Barcode::gtin13From(QString()).isEmpty(), QStringLiteral("gtin13 empty → empty"));

    // --------------------------------------------------------------------
    // 4) Bare numeric strings of every length 8..18 → GTIN, with computed
    //    gtin / gtin13. Lengths 1..7 and 19..22 → PlainText.
    // --------------------------------------------------------------------
    {
        // Build an all-digit string of a given length from a repeating pattern.
        const QString base = QStringLiteral("1234567890123456789012");
        for (int len = 1; len <= 22; ++len) {
            const QString digits = base.left(len);
            const ParsedBarcode p = Barcode::parse(digits);
            const bool shouldBe = (len >= 8 && len <= 18);
            s.check(p.isBarcode == shouldBe,
                    QStringLiteral("bare numeric len=%1 isBarcode=%2").arg(len).arg(shouldBe));
            s.check(p.raw == digits, QStringLiteral("bare numeric len=%1 raw preserved").arg(len));
            if (shouldBe) {
                s.check(p.type == ParsedBarcode::GTIN,
                        QStringLiteral("bare numeric len=%1 type GTIN").arg(len));
                s.check(p.gtin == expectGtin14(digits),
                        QStringLiteral("bare numeric len=%1 gtin").arg(len));
                s.check(p.gtin13 == expectGtin13(expectGtin14(digits)),
                        QStringLiteral("bare numeric len=%1 gtin13").arg(len));
                s.check(p.lot.isEmpty() && p.serial.isEmpty() && p.expiry.isEmpty(),
                        QStringLiteral("bare numeric len=%1 no lot/serial/expiry").arg(len));
            } else {
                s.check(p.type == ParsedBarcode::PlainText,
                        QStringLiteral("bare numeric len=%1 type PlainText").arg(len));
                s.check(p.gtin.isEmpty() && p.gtin13.isEmpty(),
                        QStringLiteral("bare numeric len=%1 no gtin").arg(len));
            }
        }
    }

    // --------------------------------------------------------------------
    // 5) Specific real EAN-13 / UPC-A / EAN-8 / GTIN-14 samples.
    // --------------------------------------------------------------------
    {
        struct Sample
        {
            const char *code;
            const char *g14;
            const char *g13;
        };
        // gtin13 strips leading zeros only down to 13 digits (never fewer), so a
        // 14-digit GTIN drops at most one leading zero. UPC-A strings beginning
        // "01"+digit are intentionally excluded here (they are parsed as GS1 AI
        // strings, covered elsewhere).
        const Sample samples[] = {
            {"8901173047861", "08901173047861", "8901173047861"},   // EAN-13
            {"4006381333931", "04006381333931", "4006381333931"},   // EAN-13
            {"036000291452", "00036000291452", "0036000291452"},    // UPC-A (12)
            {"96385074", "00000096385074", "0000096385074"},        // EAN-8
            {"03456789012345", "03456789012345", "3456789012345"},  // GTIN-14
            {"10012345678902", "10012345678902", "10012345678902"}, // GTIN-14 no leading zero
        };
        for (const Sample &sm : samples) {
            const ParsedBarcode p = Barcode::parse(QString::fromLatin1(sm.code));
            s.check(p.isBarcode && p.type == ParsedBarcode::GTIN,
                    QStringLiteral("sample %1 is GTIN").arg(QString::fromLatin1(sm.code)));
            s.check(p.gtin == QString::fromLatin1(sm.g14),
                    QStringLiteral("sample %1 gtin14").arg(QString::fromLatin1(sm.code)));
            s.check(p.gtin13 == QString::fromLatin1(sm.g13),
                    QStringLiteral("sample %1 gtin13").arg(QString::fromLatin1(sm.code)));
            s.check(p.raw == QString::fromLatin1(sm.code),
                    QStringLiteral("sample %1 raw").arg(QString::fromLatin1(sm.code)));
        }
    }

    // --------------------------------------------------------------------
    // 6) GS1 AI strings WITH FNC1 (GS) separators (strict parse).
    // --------------------------------------------------------------------
    {
        // (01)(17)(10)<GS>(21)
        const QString full = QStringLiteral("0103456789012345") + QStringLiteral("17260101")
                             + QStringLiteral("10ABC123") + kGs + QStringLiteral("21SN999");
        const ParsedBarcode p = Barcode::parse(full);
        s.check(p.isBarcode && p.type == ParsedBarcode::GS1, QStringLiteral("gs1/fnc1 type GS1"));
        s.check(p.gtin == QStringLiteral("03456789012345"), QStringLiteral("gs1/fnc1 gtin"));
        s.check(p.gtin13 == QStringLiteral("3456789012345"), QStringLiteral("gs1/fnc1 gtin13"));
        s.check(p.expiry == QStringLiteral("2026-01-01"), QStringLiteral("gs1/fnc1 expiry"));
        s.check(p.lot == QStringLiteral("ABC123"), QStringLiteral("gs1/fnc1 lot terminated by GS"));
        s.check(p.serial == QStringLiteral("SN999"), QStringLiteral("gs1/fnc1 serial"));
        s.check(p.raw == full, QStringLiteral("gs1/fnc1 raw preserved"));
    }
    {
        // Order with serial before lot, both variable, both GS-terminated.
        const QString str = QStringLiteral("0103456789012345") + QStringLiteral("21SN-007") + kGs
                            + QStringLiteral("10LOT/42") + kGs + QStringLiteral("17251130");
        const ParsedBarcode p = Barcode::parse(str);
        s.check(p.gtin == QStringLiteral("03456789012345"),
                QStringLiteral("gs1/fnc1 reorder gtin"));
        s.check(p.serial == QStringLiteral("SN-007"), QStringLiteral("gs1/fnc1 reorder serial"));
        s.check(p.lot == QStringLiteral("LOT/42"), QStringLiteral("gs1/fnc1 reorder lot"));
        s.check(p.expiry == QStringLiteral("2025-11-30"),
                QStringLiteral("gs1/fnc1 reorder expiry"));
    }
    {
        // Lot containing digit-pairs that look like AIs — must not split (GS terminates).
        const QString str = QStringLiteral("0109501101020917") + QStringLiteral("10172110")
                            + kGs; // lot literally "172110"
        const ParsedBarcode p = Barcode::parse(str);
        s.check(p.lot == QStringLiteral("172110"),
                QStringLiteral("gs1/fnc1 lot with AI-like digits intact"));
        s.check(p.expiry.isEmpty(), QStringLiteral("gs1/fnc1 no false expiry from lot digits"));
    }
    {
        // GTIN + expiry only, GS at end.
        const QString str = QStringLiteral("0103456789012345") + QStringLiteral("17260200") + kGs;
        const ParsedBarcode p = Barcode::parse(str);
        s.check(p.expiry == QStringLiteral("2026-02-28"),
                QStringLiteral("gs1/fnc1 expiry DD=00 end of month"));
        s.check(p.lot.isEmpty() && p.serial.isEmpty(),
                QStringLiteral("gs1/fnc1 only expiry present"));
    }
    {
        // (00) SSCC fixed 18 — recognized as GS1, no GTIN.
        const QString str = QStringLiteral("00") + QStringLiteral("123456789012345678");
        const ParsedBarcode p = Barcode::parse(str);
        s.check(p.isBarcode && p.type == ParsedBarcode::GS1,
                QStringLiteral("gs1 AI 00 SSCC recognized"));
        s.check(p.gtin.isEmpty(), QStringLiteral("gs1 AI 00 has no GTIN"));
    }

    // --------------------------------------------------------------------
    // 7) GS1 AI strings WITHOUT separators (heuristic / keyboard-wedge).
    // --------------------------------------------------------------------
    {
        // (01)(17)(10) — lot runs to end.
        const QString str = QStringLiteral("0103456789012345") + QStringLiteral("17260101")
                            + QStringLiteral("10ABC123");
        const ParsedBarcode p = Barcode::parse(str);
        s.check(p.isBarcode && p.type == ParsedBarcode::GS1, QStringLiteral("gs1/heur type GS1"));
        s.check(p.gtin == QStringLiteral("03456789012345"), QStringLiteral("gs1/heur gtin"));
        s.check(p.expiry == QStringLiteral("2026-01-01"), QStringLiteral("gs1/heur expiry"));
        s.check(p.lot == QStringLiteral("ABC123"), QStringLiteral("gs1/heur lot runs to end"));
        s.check(p.serial.isEmpty(), QStringLiteral("gs1/heur no serial"));
    }
    {
        // (01)(10)(17) — heuristic: lot stops at boundary AI 17.
        const QString str = QStringLiteral("0103456789012345") + QStringLiteral("10LOTX")
                            + QStringLiteral("17251231");
        const ParsedBarcode p = Barcode::parse(str);
        s.check(p.gtin == QStringLiteral("03456789012345"),
                QStringLiteral("gs1/heur lot-first gtin"));
        s.check(p.lot == QStringLiteral("LOTX"),
                QStringLiteral("gs1/heur lot stops at AI17 boundary"));
        s.check(p.expiry == QStringLiteral("2025-12-31"),
                QStringLiteral("gs1/heur expiry after lot"));
    }
    {
        // (01)(21)serial(10)lot — serial stops at boundary AI 10.
        const QString str = QStringLiteral("0103456789012345") + QStringLiteral("21SER5")
                            + QStringLiteral("10LOT9");
        const ParsedBarcode p = Barcode::parse(str);
        s.check(p.serial == QStringLiteral("SER5"),
                QStringLiteral("gs1/heur serial stops at AI10"));
        s.check(p.lot == QStringLiteral("LOT9"),
                QStringLiteral("gs1/heur lot after serial to end"));
    }
    {
        // GTIN only via AI 01 (14 fixed digits, nothing trailing).
        const QString str = QStringLiteral("0108901173047861");
        const ParsedBarcode p = Barcode::parse(str);
        s.check(p.isBarcode && p.type == ParsedBarcode::GS1,
                QStringLiteral("gs1/heur gtin-only GS1"));
        s.check(p.gtin == QStringLiteral("08901173047861"),
                QStringLiteral("gs1/heur gtin-only value"));
        s.check(p.gtin13 == QStringLiteral("8901173047861"),
                QStringLiteral("gs1/heur gtin-only gtin13"));
        s.check(p.lot.isEmpty() && p.serial.isEmpty() && p.expiry.isEmpty(),
                QStringLiteral("gs1/heur gtin-only no extras"));
    }

    // --------------------------------------------------------------------
    // 8) Parametric sweep over GS1 AI strings with many expiry/lot/serial
    //    combos, FNC1 vs heuristic.
    // --------------------------------------------------------------------
    {
        const char *gtins[] = {"03456789012345", "08901173047861", "10012345678902"};
        const char *exps[] = {"260101", "251231", "240229", "270630"};
        const char *isos[] = {"2026-01-01", "2025-12-31", "2024-02-29", "2027-06-30"};
        const char *lots[] = {"L1", "BATCH99", "X"};
        const char *sers[] = {"S1", "SN12345", "Z"};
        for (int gi = 0; gi < 3; ++gi) {
            for (int ei = 0; ei < 4; ++ei) {
                for (int li = 0; li < 3; ++li) {
                    const QString lot = QString::fromLatin1(lots[li]);
                    const QString ser = QString::fromLatin1(sers[li]);
                    // FNC1 form: (01)(17)(10)<GS>(21)<GS>
                    const QString withGs = QStringLiteral("01") + QString::fromLatin1(gtins[gi])
                                           + QStringLiteral("17") + QString::fromLatin1(exps[ei])
                                           + QStringLiteral("10") + lot + kGs + QStringLiteral("21")
                                           + ser + kGs;
                    const ParsedBarcode p = Barcode::parse(withGs);
                    s.check(p.gtin == QString::fromLatin1(gtins[gi]),
                            QStringLiteral("sweep g=%1 e=%2 l=%3 gtin").arg(gi).arg(ei).arg(li));
                    s.check(p.expiry == QString::fromLatin1(isos[ei]),
                            QStringLiteral("sweep g=%1 e=%2 l=%3 expiry").arg(gi).arg(ei).arg(li));
                    s.check(p.lot == lot,
                            QStringLiteral("sweep g=%1 e=%2 l=%3 lot").arg(gi).arg(ei).arg(li));
                    s.check(p.serial == ser,
                            QStringLiteral("sweep g=%1 e=%2 l=%3 serial").arg(gi).arg(ei).arg(li));
                    s.check(p.type == ParsedBarcode::GS1,
                            QStringLiteral("sweep g=%1 e=%2 l=%3 GS1").arg(gi).arg(ei).arg(li));
                }
            }
        }
    }

    // --------------------------------------------------------------------
    // 9) GS1 Digital Link URLs.
    // --------------------------------------------------------------------
    {
        const ParsedBarcode p = Barcode::parse(
            QStringLiteral("https://id.gs1.org/01/03456789012345/10/ABC123/17/260101/21/SN999"));
        s.check(p.isBarcode && p.type == ParsedBarcode::GS1DigitalLink,
                QStringLiteral("dl full type"));
        s.check(p.gtin == QStringLiteral("03456789012345"), QStringLiteral("dl full gtin"));
        s.check(p.gtin13 == QStringLiteral("3456789012345"), QStringLiteral("dl full gtin13"));
        s.check(p.lot == QStringLiteral("ABC123"), QStringLiteral("dl full lot"));
        s.check(p.expiry == QStringLiteral("2026-01-01"), QStringLiteral("dl full expiry"));
        s.check(p.serial == QStringLiteral("SN999"), QStringLiteral("dl full serial"));
    }
    {
        // http (not https), only GTIN in path.
        const ParsedBarcode p
            = Barcode::parse(QStringLiteral("http://example.com/01/08901173047861"));
        s.check(p.isBarcode && p.type == ParsedBarcode::GS1DigitalLink,
                QStringLiteral("dl http gtin-only type"));
        s.check(p.gtin == QStringLiteral("08901173047861"),
                QStringLiteral("dl http gtin-only value"));
        s.check(p.lot.isEmpty() && p.serial.isEmpty() && p.expiry.isEmpty(),
                QStringLiteral("dl http gtin-only no extras"));
    }
    {
        // Digital Link with a query-string carrying the AIs.
        const ParsedBarcode p = Barcode::parse(
            QStringLiteral("https://id.gs1.org/01/03456789012345?17=251130&10=QLOT&21=QSN"));
        s.check(p.gtin == QStringLiteral("03456789012345"), QStringLiteral("dl query gtin"));
        s.check(p.expiry == QStringLiteral("2025-11-30"), QStringLiteral("dl query expiry"));
        s.check(p.lot == QStringLiteral("QLOT"), QStringLiteral("dl query lot"));
        s.check(p.serial == QStringLiteral("QSN"), QStringLiteral("dl query serial"));
    }
    {
        // Case-insensitive scheme + uppercase host.
        const ParsedBarcode p
            = Barcode::parse(QStringLiteral("HTTPS://ID.GS1.ORG/01/03456789012345/17/260101"));
        s.check(p.isBarcode && p.type == ParsedBarcode::GS1DigitalLink,
                QStringLiteral("dl uppercase scheme recognized"));
        s.check(p.gtin == QStringLiteral("03456789012345"), QStringLiteral("dl uppercase gtin"));
        s.check(p.expiry == QStringLiteral("2026-01-01"), QStringLiteral("dl uppercase expiry"));
    }
    {
        // Digital Link with a fragment that should be dropped.
        const ParsedBarcode p = Barcode::parse(
            QStringLiteral("https://id.gs1.org/01/03456789012345/10/FRAGLOT#section"));
        s.check(p.gtin == QStringLiteral("03456789012345"), QStringLiteral("dl fragment gtin"));
        s.check(p.lot == QStringLiteral("FRAGLOT"), QStringLiteral("dl fragment lot, # dropped"));
    }
    {
        // Parametric: many Digital Link URLs.
        const char *hosts[] = {"id.gs1.org", "example.com", "trace.pharma.io"};
        const char *gtins[] = {"03456789012345", "08901173047861"};
        for (int hi = 0; hi < 3; ++hi) {
            for (int gi = 0; gi < 2; ++gi) {
                const QString url = QStringLiteral("https://") + QString::fromLatin1(hosts[hi])
                                    + QStringLiteral("/01/") + QString::fromLatin1(gtins[gi])
                                    + QStringLiteral("/17/261231/10/DL") + QString::number(hi);
                const ParsedBarcode p = Barcode::parse(url);
                s.check(p.isBarcode && p.type == ParsedBarcode::GS1DigitalLink,
                        QStringLiteral("dl sweep h=%1 g=%2 type").arg(hi).arg(gi));
                s.check(p.gtin == QString::fromLatin1(gtins[gi]),
                        QStringLiteral("dl sweep h=%1 g=%2 gtin").arg(hi).arg(gi));
                s.check(p.expiry == QStringLiteral("2026-12-31"),
                        QStringLiteral("dl sweep h=%1 g=%2 expiry").arg(hi).arg(gi));
                s.check(p.lot == (QStringLiteral("DL") + QString::number(hi)),
                        QStringLiteral("dl sweep h=%1 g=%2 lot").arg(hi).arg(gi));
                s.check(p.raw == url, QStringLiteral("dl sweep h=%1 g=%2 raw").arg(hi).arg(gi));
            }
        }
    }

    // --------------------------------------------------------------------
    // 10) Non-DigitalLink URLs → PlainText (not a barcode), raw preserved.
    // --------------------------------------------------------------------
    {
        const char *urls[] = {
            "https://example.com/page",
            "http://google.com",
            "https://id.gs1.org/about",
            "https://shop.example.com/products/123",
            "https://example.com/02/03456789012345", // AI must be 01 in path
            "ftp://example.com/01/03456789012345",   // non-http scheme
        };
        for (const char *u : urls) {
            const QString url = QString::fromLatin1(u);
            const ParsedBarcode p = Barcode::parse(url);
            s.check(!p.isBarcode, QStringLiteral("url not barcode: %1").arg(url));
            s.check(p.type == ParsedBarcode::PlainText,
                    QStringLiteral("url PlainText: %1").arg(url));
            s.check(p.raw == url, QStringLiteral("url raw preserved: %1").arg(url));
            s.check(p.gtin.isEmpty(), QStringLiteral("url no gtin: %1").arg(url));
        }
    }

    // --------------------------------------------------------------------
    // 11) Free text and other non-GTIN symbologies → PlainText, raw preserved.
    // --------------------------------------------------------------------
    {
        const char *texts[] = {
            "Panadol 500", "Aspirin", "ABC-123", "Vitamin C 1000mg", "SKU0099",
            "12AB34",  // alphanumeric, not pure digits
            "123 456", // space → not pure digits
            "0",       // single digit, too short
            "1234567", // 7 digits, below GTIN min
            "code:42",     "med#7",   "QWERTY",  "tablet x10",       "x",
            "паравол", // non-latin
        };
        for (const char *t : texts) {
            const QString txt = QString::fromUtf8(t);
            const ParsedBarcode p = Barcode::parse(txt);
            s.check(!p.isBarcode, QStringLiteral("free text not barcode: %1").arg(txt));
            s.check(p.type == ParsedBarcode::PlainText,
                    QStringLiteral("free text PlainText: %1").arg(txt));
            s.check(p.raw == txt, QStringLiteral("free text raw preserved: %1").arg(txt));
            s.check(p.gtin.isEmpty() && p.gtin13.isEmpty() && p.lot.isEmpty() && p.serial.isEmpty()
                        && p.expiry.isEmpty(),
                    QStringLiteral("free text no fields: %1").arg(txt));
        }
    }

    // --------------------------------------------------------------------
    // 12) Whitespace handling: input is trimmed; raw == trimmed.
    // --------------------------------------------------------------------
    {
        const ParsedBarcode p = Barcode::parse(QStringLiteral("  8901173047861  "));
        s.check(p.isBarcode && p.type == ParsedBarcode::GTIN,
                QStringLiteral("trim: padded numeric still GTIN"));
        s.check(p.raw == QStringLiteral("8901173047861"), QStringLiteral("trim: raw is trimmed"));
        s.check(p.gtin == QStringLiteral("08901173047861"), QStringLiteral("trim: gtin correct"));
    }
    {
        const ParsedBarcode p = Barcode::parse(QStringLiteral("  Panadol  "));
        s.check(!p.isBarcode && p.raw == QStringLiteral("Panadol"),
                QStringLiteral("trim: free text trimmed"));
    }

    // --------------------------------------------------------------------
    // 13) Blank / whitespace-only / empty inputs → empty PlainText.
    // --------------------------------------------------------------------
    {
        const char *blanks[] = {"", " ", "   ", "\t", "\n", " \t \n "};
        for (const char *b : blanks) {
            const ParsedBarcode p = Barcode::parse(QString::fromLatin1(b));
            s.check(
                !p.isBarcode,
                QStringLiteral("blank not barcode [%1]").arg(int(QString::fromLatin1(b).size())));
            s.check(p.type == ParsedBarcode::PlainText,
                    QStringLiteral("blank PlainText [%1]").arg(int(QString::fromLatin1(b).size())));
            s.check(p.raw.isEmpty(),
                    QStringLiteral("blank raw empty [%1]").arg(int(QString::fromLatin1(b).size())));
        }
    }

    // --------------------------------------------------------------------
    // 14) Too-short numeric boundary: 7 digits PlainText, 8 digits GTIN;
    //     18 digits GTIN, 19 digits PlainText.
    // --------------------------------------------------------------------
    {
        s.check(!Barcode::parse(QStringLiteral("1234567")).isBarcode,
                QStringLiteral("boundary 7 digits PlainText"));
        s.check(Barcode::parse(QStringLiteral("12345678")).isBarcode,
                QStringLiteral("boundary 8 digits GTIN"));
        s.check(Barcode::parse(QStringLiteral("123456789012345678")).isBarcode,
                QStringLiteral("boundary 18 digits GTIN"));
        s.check(!Barcode::parse(QStringLiteral("1234567890123456789")).isBarcode,
                QStringLiteral("boundary 19 digits PlainText"));
    }

    // --------------------------------------------------------------------
    // 15) Robustness AIs (11 prod date, 15 best-before) parse without crashing
    //     and don't fabricate gtin/lot/serial wrongly.
    // --------------------------------------------------------------------
    {
        // (01)(11)prod(17)exp  — 11 is fixed-6, decoded but not surfaced as a field.
        const QString str = QStringLiteral("0103456789012345") + QStringLiteral("11260101")
                            + QStringLiteral("17270101");
        const ParsedBarcode p = Barcode::parse(str);
        s.check(p.gtin == QStringLiteral("03456789012345"),
                QStringLiteral("ai11 gtin still parsed"));
        s.check(p.expiry == QStringLiteral("2027-01-01"), QStringLiteral("ai11 then ai17 expiry"));
    }

    // ── Printed-label free text (port of QrParser.php::parseFreeText) ─────────
    {
        const ParsedBarcode p
            = Barcode::parse(QStringLiteral("Batch No: AB123  Exp Date: 03/2027"));
        s.check(p.type == ParsedBarcode::FreeText, QStringLiteral("label: type FreeText"));
        s.check(!p.isBarcode, QStringLiteral("label: not a product barcode"));
        s.check(p.lot == QStringLiteral("AB123"), QStringLiteral("label: batch extracted"));
        // MM/YYYY → last day of the month.
        s.check(p.expiry == QStringLiteral("2027-03-31"),
                QStringLiteral("label: MM/YYYY expiry -> end of month"));
    }
    {
        const ParsedBarcode p
            = Barcode::parse(QStringLiteral("BATCH NO. XY-99/2   Expiry: 15/06/2026"));
        s.check(p.type == ParsedBarcode::FreeText, QStringLiteral("label2: FreeText"));
        s.check(p.lot == QStringLiteral("XY-99/2"), QStringLiteral("label2: batch with -/ chars"));
        s.check(p.expiry == QStringLiteral("2026-06-15"),
                QStringLiteral("label2: DD/MM/YYYY expiry"));
    }
    {
        // An MRP label with no batch/expiry still classifies as FreeText.
        const ParsedBarcode p = Barcode::parse(QStringLiteral("M.R.P. Rs. 250.00"));
        s.check(p.type == ParsedBarcode::FreeText, QStringLiteral("label3: MRP label -> FreeText"));
        s.check(p.lot.isEmpty() && p.expiry.isEmpty(),
                QStringLiteral("label3: no batch/expiry captured"));
    }
    {
        // A plain product name (no label keywords) stays PlainText for search.
        const ParsedBarcode p = Barcode::parse(QStringLiteral("Panadol 500mg"));
        s.check(p.type == ParsedBarcode::PlainText, QStringLiteral("plain name stays PlainText"));
        s.check(!p.isBarcode, QStringLiteral("plain name not a barcode"));
    }

    return s;
}

} // namespace pharmadesk_tests
