// Headless unit test for the scanned-barcode / GS1 parser (domain/BarcodeParser).
// Style mirrors tests/smoke.cpp: print "  ok" / "  FAIL", exit 0 = all pass.
//
// Built as the `pharmadesk_barcode_test` target. Covers: plain EAN-13, GTIN-14, GS1
// AI strings (with FNC1 separators and the no-separator keyboard-wedge form),
// and a GS1 Digital Link URL.

#include "domain/BarcodeParser.h"

#include <QCoreApplication>
#include <QString>
#include <QTextStream>

using Barcode::ParsedBarcode;

static int g_failures = 0;
static QTextStream out(stdout);

static void check(bool cond, const QString &what)
{
    out << (cond ? "  ok   " : "  FAIL ") << what << "\n";
    if (!cond) ++g_failures;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QChar GS = QChar(0x1D);

    out << "== plain EAN-13 ==\n";
    {
        const ParsedBarcode p = Barcode::parse(QStringLiteral("8901173047861"));
        check(p.isBarcode, "EAN-13 recognized as barcode");
        check(p.type == ParsedBarcode::GTIN, "type == GTIN");
        check(p.gtin == QStringLiteral("08901173047861"), "GTIN left-padded to 14");
        check(p.gtin13 == QStringLiteral("8901173047861"), "gtin13 == 13-digit form");
        check(p.lot.isEmpty() && p.serial.isEmpty() && p.expiry.isEmpty(), "no lot/serial/expiry");
    }

    out << "== bare GTIN-14 (ITF-14) ==\n";
    {
        const ParsedBarcode p = Barcode::parse(QStringLiteral("03456789012345"));
        check(p.isBarcode && p.type == ParsedBarcode::GTIN, "GTIN-14 recognized");
        check(p.gtin == QStringLiteral("03456789012345"), "14-digit GTIN preserved");
        check(p.gtin13 == QStringLiteral("3456789012345"), "gtin13 strips one leading zero");
    }

    out << "== EAN-8 ==\n";
    {
        const ParsedBarcode p = Barcode::parse(QStringLiteral("96385074"));
        check(p.isBarcode && p.gtin == QStringLiteral("00000096385074"), "EAN-8 padded to 14");
    }

    out << "== GS1 AI string, no separators (keyboard wedge) ==\n";
    {
        // (01)03456789012345 (17)260101 (10)ABC123
        const ParsedBarcode p = Barcode::parse(
            QStringLiteral("0103456789012345172601011 0ABC123").remove(QLatin1Char(' ')));
        check(p.isBarcode && p.type == ParsedBarcode::GS1, "recognized as GS1");
        check(p.gtin == QStringLiteral("03456789012345"), "GTIN from AI 01");
        check(p.expiry == QStringLiteral("2026-01-01"), "expiry from AI 17 YYMMDD");
        check(p.lot == QStringLiteral("ABC123"), "lot from AI 10 (runs to end)");
    }

    out << "== GS1 AI string, with FNC1 separators ==\n";
    {
        // (01)03456789012345 (17)260101 (10)ABC123 <GS> (21)SN999
        QString s = QStringLiteral("0103456789012345") + QStringLiteral("17260101")
                    + QStringLiteral("10ABC123") + GS + QStringLiteral("21SN999");
        const ParsedBarcode p = Barcode::parse(s);
        check(p.isBarcode && p.type == ParsedBarcode::GS1, "recognized as GS1");
        check(p.gtin == QStringLiteral("03456789012345"), "GTIN from AI 01");
        check(p.expiry == QStringLiteral("2026-01-01"), "expiry decoded");
        check(p.lot == QStringLiteral("ABC123"), "lot terminated by GS, not bleeding into serial");
        check(p.serial == QStringLiteral("SN999"), "serial from AI 21");
    }

    out << "== GS1 AI: expiry day 00 → end of month ==\n";
    {
        // (01)03456789012345 (17)260200  → 2026-02-28
        QString s = QStringLiteral("0103456789012345") + QStringLiteral("17260200") + GS;
        const ParsedBarcode p = Barcode::parse(s);
        check(p.expiry == QStringLiteral("2026-02-28"), "DD=00 → last day of month (Feb 2026)");
    }

    out << "== GS1 Digital Link URL ==\n";
    {
        const ParsedBarcode p = Barcode::parse(
            QStringLiteral("https://id.gs1.org/01/03456789012345/10/ABC123/17/260101/21/SN999"));
        check(p.isBarcode && p.type == ParsedBarcode::GS1DigitalLink, "recognized as Digital Link");
        check(p.gtin == QStringLiteral("03456789012345"), "GTIN from /01/");
        check(p.gtin13 == QStringLiteral("3456789012345"), "gtin13 form available");
        check(p.lot == QStringLiteral("ABC123"), "lot from /10/");
        check(p.expiry == QStringLiteral("2026-01-01"), "expiry from /17/");
        check(p.serial == QStringLiteral("SN999"), "serial from /21/");
    }

    out << "== non-barcode fallbacks ==\n";
    {
        const ParsedBarcode t = Barcode::parse(QStringLiteral("Panadol 500"));
        check(!t.isBarcode && t.type == ParsedBarcode::PlainText, "free text → PlainText");
        check(t.raw == QStringLiteral("Panadol 500"), "raw preserved for search fallback");

        const ParsedBarcode u = Barcode::parse(QStringLiteral("https://example.com/page"));
        check(!u.isBarcode, "non-DigitalLink URL → not a barcode");

        const ParsedBarcode e = Barcode::parse(QStringLiteral("   "));
        check(!e.isBarcode && e.raw.isEmpty(), "blank input → empty PlainText");
    }

    out << "\n"
        << (g_failures == 0 ? QStringLiteral("ALL PASSED")
                            : QStringLiteral("%1 FAILURE(S)").arg(g_failures))
        << "\n";
    return g_failures == 0 ? 0 : 1;
}
