#include "domain/BarcodeParser.h"

#include <QChar>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>

namespace Barcode {

namespace {

// ASCII Group Separator — FNC1 as emitted by scanners that don't drop it.
const QChar kGs = QChar(0x1D);

// Fixed-length GS1 AIs we care about (value width in characters). Everything
// else is treated as variable-length (FNC1 / next-AI terminated). Mirrors the
// subset of QrParser::FIXED_AIS the POS path needs.
const QHash<QString, int> &fixedAis()
{
    static const QHash<QString, int> m = {
        {QStringLiteral("00"), 18}, {QStringLiteral("01"), 14}, {QStringLiteral("02"), 14},
        {QStringLiteral("11"), 6},  {QStringLiteral("15"), 6},  {QStringLiteral("17"), 6},
    };
    return m;
}

// Variable-length AIs we extract by name.
bool isVarAi(const QString &ai)
{
    return ai == QStringLiteral("10") || ai == QStringLiteral("21");
}

// AIs that may act as a boundary terminator during a heuristic (no-FNC1) parse.
// Deliberately narrow — the same rationale as QrParser::HEURISTIC_BOUNDARY_AIS:
// digit pairs like "20"/"22" routinely occur *inside* real lot/serial values
// and must not mis-terminate a variable-length field.
bool isBoundaryAi(const QString &ai)
{
    return ai == QStringLiteral("01") || ai == QStringLiteral("10") || ai == QStringLiteral("11")
           || ai == QStringLiteral("15") || ai == QStringLiteral("17")
           || ai == QStringLiteral("21");
}

bool isDigits(const QString &s)
{
    if (s.isEmpty()) {
        return false;
    }
    for (const QChar c : s) {
        if (!c.isDigit()) {
            return false;
        }
    }
    return true;
}

// Try to match a known AI (2-digit prefix) at s[pos]. `seen` is the set of AIs
// already parsed; an AI is never matched twice, and AI 00 (SSCC) / 01 (GTIN)
// are mutually exclusive (a retail unit scan never carries both). When
// `boundaryOnly` is set, only AIs in the narrow boundary list are accepted.
QString matchAi(const QString &s, int pos, const QHash<QString, QString> &seen, bool boundaryOnly)
{
    if (pos + 2 > s.size()) {
        return QString();
    }
    const QString r2 = s.mid(pos, 2);
    if (!isDigits(r2)) {
        return QString();
    }
    const bool known = fixedAis().contains(r2) || isVarAi(r2);
    if (!known) {
        return QString();
    }
    if (seen.contains(r2)) {
        return QString();
    }
    if (r2 == QStringLiteral("00") && seen.contains(QStringLiteral("01"))) {
        return QString();
    }
    if (r2 == QStringLiteral("02") && !seen.contains(QStringLiteral("01"))) {
        return QString();
    }
    if (boundaryOnly && !isBoundaryAi(r2)) {
        return QString();
    }
    return r2;
}

// Strict parse — GS (0x1D) terminates every variable-length AI.
QHash<QString, QString> parseStrict(const QString &s)
{
    QHash<QString, QString> ais;
    int i = 0;
    const int len = s.size();
    while (i < len) {
        if (s.at(i) == kGs) {
            ++i;
            continue;
        }
        const QString ai = matchAi(s, i, ais, /*boundaryOnly*/ false);
        if (ai.isEmpty()) {
            break;
        }
        i += ai.size();
        const auto it = fixedAis().constFind(ai);
        if (it != fixedAis().constEnd()) {
            ais.insert(ai, s.mid(i, it.value()));
            i += it.value();
        } else {
            int end = s.indexOf(kGs, i);
            if (end < 0) {
                end = len;
            }
            ais.insert(ai, s.mid(i, end - i));
            i = end;
        }
    }
    return ais;
}

// Heuristic parse — no GS separators (keyboard-wedge scanners often drop them).
// Walks AI-by-AI; for a variable-length AI greedily consumes characters until
// the next 2 digits match a plausible boundary AI not yet seen.
QHash<QString, QString> parseHeuristic(const QString &s)
{
    QHash<QString, QString> ais;
    int i = 0;
    const int len = s.size();
    while (i < len) {
        const QString ai = matchAi(s, i, ais, /*boundaryOnly*/ false);
        if (ai.isEmpty()) {
            break;
        }
        i += ai.size();
        const auto it = fixedAis().constFind(ai);
        if (it != fixedAis().constEnd()) {
            ais.insert(ai, s.mid(i, it.value()));
            i += it.value();
            continue;
        }
        // Variable-length AI. Mark it seen up front so its own digit pair inside
        // the value isn't treated as a boundary, then consume to the next
        // boundary AI or end of string.
        ais.insert(ai, QString());
        int j = i;
        while (j < len) {
            if (j > i && !matchAi(s, j, ais, /*boundaryOnly*/ true).isEmpty()) {
                break;
            }
            ++j;
        }
        ais.insert(ai, s.mid(i, j - i));
        i = j;
    }
    return ais;
}

void fillFromAis(const QHash<QString, QString> &ais, ParsedBarcode *out)
{
    QString gtinRaw = ais.value(QStringLiteral("01"));
    if (gtinRaw.isEmpty()) {
        gtinRaw = ais.value(QStringLiteral("02"));
    }
    if (!gtinRaw.isEmpty()) {
        out->gtin = normalizeGtin(gtinRaw);
        out->gtin13 = gtin13From(out->gtin);
    }
    const QString exp = ais.value(QStringLiteral("17"));
    if (!exp.isEmpty()) {
        out->expiry = gs1DateToIso(exp);
    }
    out->lot = ais.value(QStringLiteral("10"));
    out->serial = ais.value(QStringLiteral("21"));
}

ParsedBarcode parseGs1AiString(const QString &s, const QString &raw, bool hadFnc1)
{
    ParsedBarcode out;
    out.type = ParsedBarcode::GS1;
    out.raw = raw;
    out.isBarcode = true;
    const QHash<QString, QString> ais = hadFnc1 ? parseStrict(s) : parseHeuristic(s);
    fillFromAis(ais, &out);
    return out;
}

ParsedBarcode parseDigitalLink(const QString &url, const QString &raw)
{
    ParsedBarcode out;
    out.type = ParsedBarcode::GS1DigitalLink;
    out.raw = raw;
    out.isBarcode = true;

    // Split off scheme://host, then walk the path as /AI/value pairs. A query
    // string (?17=...&10=...) carries the same AI=value pairs.
    QString rest = url;
    const int schemeEnd = rest.indexOf(QStringLiteral("://"));
    if (schemeEnd >= 0) {
        rest = rest.mid(schemeEnd + 3);
    }
    QString query;
    const int qpos = rest.indexOf(QLatin1Char('?'));
    if (qpos >= 0) {
        query = rest.mid(qpos + 1);
        rest = rest.left(qpos);
    }
    // Drop fragment.
    const int hpos = rest.indexOf(QLatin1Char('#'));
    if (hpos >= 0) {
        rest = rest.left(hpos);
    }
    // Drop host (everything up to the first '/').
    const int slash = rest.indexOf(QLatin1Char('/'));
    const QString path = slash >= 0 ? rest.mid(slash + 1) : QString();

    QHash<QString, QString> pairs;
    const QStringList segs = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (int i = 0; i + 1 < segs.size(); i += 2) {
        pairs.insert(segs.at(i), segs.at(i + 1));
    }
    if (!query.isEmpty()) {
        const QStringList kvs = query.split(QLatin1Char('&'), Qt::SkipEmptyParts);
        for (const QString &kv : kvs) {
            const int eq = kv.indexOf(QLatin1Char('='));
            if (eq > 0) {
                pairs.insert(kv.left(eq), kv.mid(eq + 1));
            }
        }
    }

    fillFromAis(pairs, &out);
    return out;
}

} // namespace

QString gs1DateToIso(const QString &yymmdd)
{
    static const QRegularExpression re(QStringLiteral("^\\d{6}$"));
    if (!re.match(yymmdd).hasMatch()) {
        return QString();
    }
    const int yy = yymmdd.mid(0, 2).toInt();
    const int mm = yymmdd.mid(2, 2).toInt();
    int dd = yymmdd.mid(4, 2).toInt();
    if (mm < 1 || mm > 12) {
        return QString();
    }
    const int year = yy < 50 ? 2000 + yy : 1900 + yy;
    if (dd == 0) {
        // Last day of the month (GS1: DD=00 ⇒ end of month).
        dd = QDate(year, mm, 1).daysInMonth();
    }
    const QDate d(year, mm, dd);
    if (!d.isValid()) {
        return QString();
    }
    return d.toString(QStringLiteral("yyyy-MM-dd"));
}

QString normalizeGtin(const QString &digits)
{
    QString clean;
    clean.reserve(digits.size());
    for (const QChar c : digits) {
        if (c.isDigit()) {
            clean.append(c);
        }
    }
    if (clean.isEmpty()) {
        return QString();
    }
    if (clean.size() >= 14) {
        return clean.right(14);
    }
    return clean.rightJustified(14, QLatin1Char('0'));
}

QString gtin13From(const QString &gtin14)
{
    if (gtin14.isEmpty()) {
        return QString();
    }
    QString s = gtin14;
    while (s.size() > 13 && s.startsWith(QLatin1Char('0'))) {
        s.remove(0, 1);
    }
    return s;
}

ParsedBarcode parse(const QString &scanned)
{
    ParsedBarcode out;
    const QString s = scanned.trimmed();
    out.raw = s;
    if (s.isEmpty()) {
        return out; // PlainText, empty
    }

    // 1) GS1 Digital Link URL: https://host/01/<gtin>...
    static const QRegularExpression dlRe(QStringLiteral("^https?://[^/]+/01/\\d{8,14}"),
                                         QRegularExpression::CaseInsensitiveOption);
    if (dlRe.match(s).hasMatch()) {
        return parseDigitalLink(s, s);
    }

    // 2) Any other http(s) URL is not a product barcode → plain text.
    static const QRegularExpression urlRe(QStringLiteral("^https?://"),
                                          QRegularExpression::CaseInsensitiveOption);
    if (urlRe.match(s).hasMatch()) {
        return out; // PlainText
    }

    // 3) GS1 AI element string — starts with a leading AI (01 / 00 / 02) then a
    //    digit. Most pharma DataMatrix / GS1-128 scans land here.
    static const QRegularExpression aiRe(QStringLiteral("^(01|00|02)\\d"));
    if (aiRe.match(s).hasMatch()) {
        const bool hadFnc1 = s.contains(kGs);
        return parseGs1AiString(s, s, hadFnc1);
    }

    // 4) Bare GTIN / EAN-8/12/13/14 (or ITF-14) — a pure numeric string.
    if (isDigits(s) && s.size() >= 8 && s.size() <= 18) {
        out.type = ParsedBarcode::GTIN;
        out.isBarcode = true;
        out.gtin = normalizeGtin(s);
        out.gtin13 = gtin13From(out.gtin);
        return out;
    }

    // 5) Fallback: not a barcode we recognize — treat as a search query.
    return out; // PlainText
}

} // namespace Barcode
