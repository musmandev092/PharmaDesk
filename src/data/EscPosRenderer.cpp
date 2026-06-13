#include "data/EscPosRenderer.h"

#include "domain/Money.h"

#include <QStringList>

namespace {

// ESC/POS command bytes (Black Copper BC-88AC, mirrors EscPos.php::CMD).
const QByteArray kInit = QByteArray::fromHex("1B40");
const QByteArray kCenter = QByteArray::fromHex("1B6101");
const QByteArray kLeft = QByteArray::fromHex("1B6100");
const QByteArray kBoldOn = QByteArray::fromHex("1B4501");
const QByteArray kBoldOff = QByteArray::fromHex("1B4500");
const QByteArray kDblHOn = QByteArray::fromHex("1B2110");
const QByteArray kDblHOff = QByteArray::fromHex("1B2100");
const QByteArray kCut = QByteArray::fromHex("1D564103");
const QByteArray kFeed3 = QByteArray("\n\n\n");

QString divider(QChar c)
{
    return QString(EscPosRenderer::Width, c);
}

QString clip(const QString &s, int n)
{
    return s.length() > n ? s.left(n) : s;
}

QString padLeft(const QString &s, int n)
{
    return s.length() >= n ? s : QString(n - s.length(), QLatin1Char(' ')) + s;
}
QString padRight(const QString &s, int n)
{
    return s.length() >= n ? s : s + QString(n - s.length(), QLatin1Char(' '));
}

// Label left-justified, value right-justified, to the full width.
QString row(const QString &label, const QString &value)
{
    int pad = EscPosRenderer::Width - label.length() - value.length();
    if (pad < 1) {
        pad = 1;
    }
    return clip(label + QString(pad, QLatin1Char(' ')) + value, EscPosRenderer::Width);
}

QStringList wordWrap(const QString &text, int width)
{
    QStringList out;
    QString line;
    for (const QString &word : text.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        if (line.isEmpty()) {
            line = word;
        } else if (line.length() + 1 + word.length() <= width) {
            line += QLatin1Char(' ') + word;
        } else {
            out << line;
            line = word;
        }
    }
    if (!line.isEmpty()) {
        out << line;
    }
    return out;
}

QString money2(const QString &decimal)
{
    return Money::fromString(decimal).toString(Money::ScaleMoney);
}

void appendLine(QByteArray &out, const QString &s)
{
    out += s.toUtf8();
    out += '\n';
}

} // namespace

QByteArray EscPosRenderer::receipt(const SaleResult &sale, const EscPosHeader &header)
{
    QByteArray out;
    out += kInit;

    // ── Header ──────────────────────────────────────────────────────────────
    out += kCenter;
    out += kBoldOn;
    appendLine(out, clip(header.pharmacyName.toUpper(), Width));
    out += kBoldOff;
    if (!header.address.trimmed().isEmpty()) {
        for (const QString &wr : wordWrap(header.address.simplified(), Width * 2 / 3)) {
            appendLine(out, wr);
        }
    }
    if (!header.phone.trimmed().isEmpty()) {
        appendLine(out, clip(QStringLiteral("Ph: ") + header.phone, Width));
    }
    if (!header.ntn.trimmed().isEmpty()) {
        appendLine(out, clip(header.ntn, Width));
    }

    out += kLeft;
    appendLine(out, divider(QLatin1Char('=')));
    appendLine(out, row(QStringLiteral("Invoice:"), sale.receiptNumber));
    appendLine(out, row(QStringLiteral("Date:"), header.soldAt));
    appendLine(out, row(QStringLiteral("Cashier:"), header.cashierName.toUpper()));
    appendLine(out, row(QStringLiteral("Mode:"), sale.paymentMode));
    appendLine(out, divider(QLatin1Char('-')));

    // ── Items ───────────────────────────────────────────────────────────────
    const int nameW = 27;
    appendLine(out, padRight(QStringLiteral("ITEM"), nameW) + padLeft(QStringLiteral("QTY"), 4)
                        + padLeft(QStringLiteral("PRICE"), 8)
                        + padLeft(QStringLiteral("TOTAL"), 9));
    appendLine(out, divider(QLatin1Char('-')));

    int totalQty = 0;
    for (const SaleResultLine &item : sale.lines) {
        const QString name = clip(item.name.toUpper(), nameW);
        appendLine(out, padRight(name, nameW) + padLeft(QString::number(item.qty), 4)
                            + padLeft(money2(item.unitMrp), 8)
                            + padLeft(money2(item.lineTotal), 9));
        if (!item.unitLabel.trimmed().isEmpty()) {
            appendLine(out,
                       QStringLiteral("  %1 x %2").arg(item.qty).arg(item.unitLabel.toUpper()));
        }
        totalQty += item.qty;
    }
    appendLine(out, divider(QLatin1Char('-')));

    // ── Totals ──────────────────────────────────────────────────────────────
    const bool hasDiscount = Money::fromString(sale.discountTotal).compare(Money()) > 0;
    appendLine(out, row(QStringLiteral("Total Qty:"), QString::number(totalQty)));
    appendLine(out,
               row(QStringLiteral("Subtotal:"), QStringLiteral("PKR ") + money2(sale.subtotal)));
    if (hasDiscount) {
        appendLine(out, row(QStringLiteral("Discount:"),
                            QStringLiteral("-PKR ") + money2(sale.discountTotal)));
    }
    appendLine(out, divider(QLatin1Char('=')));

    out += kDblHOn;
    out += kBoldOn;
    appendLine(out,
               row(QStringLiteral("PAYABLE:"), QStringLiteral("PKR ") + money2(sale.grandTotal)));
    out += kDblHOff;
    out += kBoldOff;
    appendLine(out, divider(QLatin1Char('=')));

    if (sale.paymentMode == QLatin1String("CASH") && !sale.amountTendered.isEmpty()) {
        appendLine(out, row(QStringLiteral("Cash Tendered:"),
                            QStringLiteral("PKR ") + money2(sale.amountTendered)));
        out += kBoldOn;
        appendLine(out, row(QStringLiteral("Change:"),
                            QStringLiteral("PKR ") + money2(sale.changeReturned)));
        out += kBoldOff;
    }

    // ── Footer ──────────────────────────────────────────────────────────────
    out += '\n';
    if (!header.returnPolicy.trimmed().isEmpty()) {
        for (const QString &line : wordWrap(header.returnPolicy.simplified(), Width)) {
            appendLine(out, line);
        }
    }
    appendLine(out, divider(QLatin1Char('-')));
    if (!header.footerMessage.trimmed().isEmpty()) {
        out += kCenter;
        for (const QString &line : wordWrap(header.footerMessage.simplified(), Width)) {
            appendLine(out, line);
        }
        out += kLeft;
    }

    out += kFeed3;
    out += kCut;
    return out;
}

QByteArray EscPosRenderer::selfTest(const QString &queueName, const QString &productName)
{
    QByteArray out;
    out += kInit;
    out += kCenter;
    out += kBoldOn;
    out += kDblHOn;
    appendLine(out, clip(productName, Width));
    out += kDblHOff;
    appendLine(out, QStringLiteral("Printer Self-Test"));
    out += kBoldOff;
    appendLine(out, divider(QLatin1Char('-')));
    out += kLeft;
    appendLine(out, row(QStringLiteral("Queue"), queueName));
    appendLine(out, row(QStringLiteral("Status"), QStringLiteral("OK")));
    appendLine(out, divider(QLatin1Char('-')));
    out += kCenter;
    appendLine(out, QStringLiteral("If you can read this,"));
    appendLine(out, QStringLiteral("the driver + queue work."));
    out += kFeed3;
    out += kCut;
    return out;
}
