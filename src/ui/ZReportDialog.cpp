#include "ui/ZReportDialog.h"

#include "data/SettingsRepository.h"
#include "domain/Money.h"
#include "domain/SettingsKeys.h"
#include "service/ZReportArchive.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSqlQuery>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QVariant>

namespace {
// One receipt row of the session, for the recap + listing.
struct Recap
{
    int cashCount = 0;
    int cardCount = 0;
    QString cashTotal = QStringLiteral("0.00");
    QString cardTotal = QStringLiteral("0.00");
    QString allTotal = QStringLiteral("0.00");
};

struct SaleLine
{
    QString receipt;
    QString soldAt;
    QString paymentMode;
    QString status;
    QString grandTotal;
};

QString trimTo19(const QString &ts)
{
    return ts.left(19);
}
} // namespace

ZReportDialog::ZReportDialog(QSqlDatabase db, qint64 sessionId, QWidget *parent)
    : QDialog(parent), m_db(std::move(db)), m_sessionId(sessionId)
{
    m_pharmacyName = SettingsRepository(m_db).get(SettingsKeys::PharmacyName,
                                                  QStringLiteral("CARE POINT PHARMACY"));
    m_summary = SessionRepository(m_db).sessionSummary(m_sessionId);

    setWindowTitle(QStringLiteral("Z-report — Session #%1").arg(m_sessionId));
    setModal(true);
    setMinimumWidth(460);
    resize(460, 620);

    computeSignature();
    buildText();

    auto *view = new QPlainTextEdit(this);
    view->setReadOnly(true);
    view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    view->setPlainText(m_plainText);

    auto *printBtn = new QPushButton(QStringLiteral("Print…"), this);
    auto *pdfBtn = new QPushButton(QStringLiteral("Save PDF…"), this);
    pdfBtn->setProperty("variant", "secondary");
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto *closeBtn = buttons->button(QDialogButtonBox::Close);
    if (closeBtn) {
        closeBtn->setProperty("variant", "secondary");
    }
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(printBtn, &QPushButton::clicked, this, &ZReportDialog::print);
    connect(pdfBtn, &QPushButton::clicked, this, &ZReportDialog::savePdf);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);
    btnRow->addStretch();
    btnRow->addWidget(buttons);
    btnRow->addWidget(pdfBtn);
    btnRow->addWidget(printBtn);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addWidget(view, 1);
    layout->addLayout(btnRow);
}

// Sign + archive (or read back) the Z-report for a closed shift. Only CLOSED or
// RECONCILED sessions are tamper-stamped; an OPEN session is a live preview and
// stays unsigned. Signing is idempotent (returns the stored signature on re-view).
void ZReportDialog::computeSignature()
{
    m_signatureHex.clear();
    if (!m_summary.valid) {
        return;
    }
    if (m_summary.status != QLatin1String("CLOSED")
        && m_summary.status != QLatin1String("RECONCILED")) {
        return; // OPEN / live preview — leave unsigned
    }
    // No current-user id is plumbed into this dialog; the signer accepts 0.
    const ZReportSignature sig = ZReportArchive(m_db, 0).signAndStore(m_sessionId);
    if (!sig.ok) {
        return; // signing failed — render as unsigned
    }
    m_signatureHex = sig.signatureHex;
}

// Read the session's sales and tally cash/card recap with Money (decimal-exact).
static Recap loadRecap(QSqlDatabase &db, qint64 sessionId, QVector<SaleLine> *outLines)
{
    Recap rec;
    Money cash, card, all;
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT receipt_number, sold_at, payment_mode, status, grand_total "
                             "  FROM sales WHERE cashier_session_id = ? ORDER BY sold_at"));
    q.addBindValue(sessionId);
    if (q.exec()) {
        while (q.next()) {
            SaleLine s;
            s.receipt = q.value(0).toString();
            s.soldAt = q.value(1).toString();
            s.paymentMode = q.value(2).toString();
            s.status = q.value(3).toString();
            s.grandTotal = q.value(4).toString();
            if (outLines) outLines->push_back(s);

            const Money g = Money::fromString(s.grandTotal);
            all = all + g;
            if (s.paymentMode == QLatin1String("CASH")) {
                ++rec.cashCount;
                cash = cash + g;
            } else if (s.paymentMode == QLatin1String("CARD")) {
                ++rec.cardCount;
                card = card + g;
            }
        }
    }
    rec.cashTotal = cash.toString(Money::ScaleMoney);
    rec.cardTotal = card.toString(Money::ScaleMoney);
    rec.allTotal = all.toString(Money::ScaleMoney);
    return rec;
}

void ZReportDialog::buildText()
{
    QVector<SaleLine> sales;
    const Recap rec = loadRecap(m_db, m_sessionId, &sales);

    QString r;
    auto line = [&](const QString &s = QString()) { r += s + QLatin1Char('\n'); };
    auto rule = [&]() { line(QStringLiteral("----------------------------------------")); };
    auto pkr = [](const QString &v) { return Money::fromString(v).display(); };

    line((m_pharmacyName.isEmpty() ? QStringLiteral("PHARMACY") : m_pharmacyName).toUpper());
    line(QStringLiteral("Z-REPORT  ·  Session #%1").arg(m_sessionId));
    rule();

    if (!m_summary.valid) {
        line(QStringLiteral("Session not found."));
        m_plainText = r;
        return;
    }

    line(QStringLiteral("Cashier   : %1").arg(m_summary.cashierName));
    line(QStringLiteral("Opened    : %1").arg(trimTo19(m_summary.openedAt)));
    line(QStringLiteral("Closed    : %1")
             .arg(m_summary.closedAt.isEmpty() ? QStringLiteral("—")
                                               : trimTo19(m_summary.closedAt)));
    line(QStringLiteral("Status    : %1").arg(m_summary.status));
    rule();

    line(QStringLiteral("CASH DRAWER"));
    line(QStringLiteral("Opening float    %1").arg(pkr(m_summary.openingFloat)));
    line(QStringLiteral("Expected cash    %1").arg(pkr(m_summary.expectedCash)));
    line(QStringLiteral("Counted cash     %1")
             .arg(m_summary.countedCash.isEmpty() ? QStringLiteral("—")
                                                  : pkr(m_summary.countedCash)));
    line(QStringLiteral("Variance         %1")
             .arg(m_summary.cashVariance.isEmpty() ? QStringLiteral("—")
                                                   : pkr(m_summary.cashVariance)));
    rule();

    line(QStringLiteral("SALES RECAP"));
    line(QStringLiteral("Cash sales  %1 x %2").arg(rec.cashCount).arg(pkr(rec.cashTotal)));
    line(QStringLiteral("Card sales  %1 x %2").arg(rec.cardCount).arg(pkr(rec.cardTotal)));
    line(QStringLiteral("Grand total      %1").arg(pkr(rec.allTotal)));
    rule();

    if (!sales.isEmpty()) {
        line(QStringLiteral("RECEIPTS"));
        for (const SaleLine &s : sales) {
            line(QStringLiteral("%1  %2  %3").arg(s.receipt, s.paymentMode, pkr(s.grandTotal)));
        }
        rule();
    }

    if (m_signatureHex.isEmpty()) {
        line(QStringLiteral("Signature: (unsigned — shift not closed)"));
    } else {
        line(QStringLiteral("Signature: %1…").arg(m_signatureHex.left(16)));
        line(QStringLiteral("(HMAC-SHA256 tamper-evident)"));
    }
    rule();

    line(QStringLiteral("End of report."));
    m_plainText = r;
}

QString ZReportDialog::buildHtml() const
{
    QVector<SaleLine> sales;
    Recap rec;
    {
        QSqlDatabase db = m_db; // loadRecap takes a non-const ref
        rec = loadRecap(db, m_sessionId, &sales);
    }
    auto pkr = [](const QString &v) { return Money::fromString(v).display(); };

    if (!m_summary.valid) {
        return QStringLiteral("<p>Session not found.</p>");
    }

    QString rows;
    for (const SaleLine &s : sales) {
        rows += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td align=right>%4</td></tr>")
                    .arg(s.receipt, trimTo19(s.soldAt), s.paymentMode, pkr(s.grandTotal));
    }
    const QString closed
        = m_summary.closedAt.isEmpty() ? QStringLiteral("—") : trimTo19(m_summary.closedAt);
    const QString counted
        = m_summary.countedCash.isEmpty() ? QStringLiteral("—") : pkr(m_summary.countedCash);
    const QString variance
        = m_summary.cashVariance.isEmpty() ? QStringLiteral("—") : pkr(m_summary.cashVariance);

    const QString signatureBlock
        = m_signatureHex.isEmpty()
              ? QStringLiteral("<p style='text-align:center; font-size:9pt; margin:4px 0;'>"
                               "Signature: (unsigned — shift not closed)</p>")
              : QStringLiteral("<p style='text-align:center; font-size:9pt; margin:4px 0;'>"
                               "Signature: %1&hellip;<br>"
                               "<span style='color:#666;'>HMAC-SHA256 tamper-evident</span></p>")
                    .arg(m_signatureHex.left(16));

    return QStringLiteral(
               "<div style='font-family:sans-serif; font-size:11pt;'>"
               "<h2 style='text-align:center; margin:0;'>%1</h2>"
               "<p style='text-align:center; margin:2px 0;'>Z-Report &middot; Session #%2</p><hr>"
               "<table cellspacing='0' cellpadding='2'>"
               "<tr><td><b>Cashier</b></td><td>%3</td></tr>"
               "<tr><td><b>Opened</b></td><td>%4</td></tr>"
               "<tr><td><b>Closed</b></td><td>%5</td></tr>"
               "<tr><td><b>Status</b></td><td>%6</td></tr>"
               "</table>"
               "<h3>Cash drawer</h3>"
               "<table cellspacing='0' cellpadding='2'>"
               "<tr><td>Opening float</td><td align=right>%7</td></tr>"
               "<tr><td>Expected cash</td><td align=right>%8</td></tr>"
               "<tr><td>Counted cash</td><td align=right>%9</td></tr>"
               "<tr><td>Variance</td><td align=right>%10</td></tr>"
               "</table>"
               "<h3>Sales recap</h3>"
               "<table cellspacing='0' cellpadding='2'>"
               "<tr><td>Cash sales</td><td align=right>%11 &times; %12</td></tr>"
               "<tr><td>Card sales</td><td align=right>%13 &times; %14</td></tr>"
               "<tr><td><b>Grand total</b></td><td align=right><b>%15</b></td></tr>"
               "</table>"
               "<h3>Receipts</h3>"
               "<table width='100%%' cellspacing='0' cellpadding='2'>"
               "<tr><th align=left>Receipt #</th><th align=left>Time</th>"
               "<th align=left>Mode</th><th align=right>Total</th></tr>%16"
               "</table><hr>%17"
               "<p style='text-align:center; font-size:9pt;'>End of report.</p></div>")
        .arg(m_pharmacyName.isEmpty() ? QStringLiteral("PHARMACY") : m_pharmacyName)
        .arg(m_sessionId)
        .arg(m_summary.cashierName, trimTo19(m_summary.openedAt), closed, m_summary.status,
             pkr(m_summary.openingFloat), pkr(m_summary.expectedCash), counted, variance)
        .arg(rec.cashCount)
        .arg(pkr(rec.cashTotal))
        .arg(rec.cardCount)
        .arg(pkr(rec.cardTotal), pkr(rec.allTotal), rows)
        .arg(signatureBlock);
}

void ZReportDialog::print()
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    QTextDocument doc;
    doc.setHtml(buildHtml());
    doc.print(&printer);
}

void ZReportDialog::savePdf()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save Z-report PDF"),
        QStringLiteral("z-report-session-%1.pdf").arg(m_sessionId), QStringLiteral("PDF (*.pdf)"));
    if (path.isEmpty()) {
        return;
    }
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    QTextDocument doc;
    doc.setHtml(buildHtml());
    doc.print(&printer);
    QMessageBox::information(this, windowTitle(), QStringLiteral("Saved %1").arg(path));
}
