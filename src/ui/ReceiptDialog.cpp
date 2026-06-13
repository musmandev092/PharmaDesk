#include "ui/ReceiptDialog.h"

#include "data/EscPosRenderer.h"
#include "data/SettingsRepository.h"
#include "domain/Money.h"
#include "domain/SettingsKeys.h"
#include "service/CupsRawPrinter.h"

#include <QDateTime>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSqlQuery>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

ReceiptDialog::ReceiptDialog(const SaleResult &sale, const QString &pharmacyName,
                             const QString &logoPath, QSqlDatabase db, qint64 cashierId,
                             QWidget *parent)
    : QDialog(parent), m_sale(sale), m_pharmacyName(pharmacyName), m_logoPath(logoPath),
      m_db(std::move(db)), m_cashierId(cashierId)
{
    setWindowTitle(QStringLiteral("Receipt — %1").arg(sale.receiptNumber));
    setModal(true);
    setMinimumWidth(420);
    resize(420, 560);

    QString r;
    auto line = [&](const QString &s = QString()) { r += s + QLatin1Char('\n'); };
    auto rule = [&]() { line(QStringLiteral("--------------------------------")); };

    line((m_pharmacyName.isEmpty() ? QStringLiteral("PHARMACY") : m_pharmacyName).toUpper());
    line(QStringLiteral("Receipt: %1").arg(sale.receiptNumber));
    rule();
    for (const SaleResultLine &l : sale.lines) {
        line(l.name);
        line(QStringLiteral("  %1 %2 x %3      %4")
                 .arg(l.qty)
                 .arg(l.unitLabel, l.unitMrp, l.lineTotal));
    }
    rule();
    line(QStringLiteral("Subtotal            %1").arg(sale.subtotal));
    if (sale.discountTotal != QLatin1String("0.00") && !sale.discountTotal.isEmpty()) {
        line(QStringLiteral("Discount           -%1").arg(sale.discountTotal));
    }
    line(QStringLiteral("TOTAL               %1").arg(sale.grandTotal));
    if (!sale.amountTendered.isEmpty()) {
        line(QStringLiteral("Tendered            %1").arg(sale.amountTendered));
        line(QStringLiteral("Change              %1").arg(sale.changeReturned));
    }
    line(QStringLiteral("Payment: %1").arg(sale.paymentMode));
    rule();
    line(QStringLiteral("Thank you."));

    Q_UNUSED(r); // plaintext kept for reference; preview now renders the print HTML
    // Preview renders the same HTML that prints, so the logo + layout match exactly.
    auto *view = new QTextEdit(this);
    view->setReadOnly(true);
    view->setHtml(buildHtml());

    auto *printBtn = new QPushButton(QStringLiteral("Print…"), this);
    auto *pdfBtn = new QPushButton(QStringLiteral("Save PDF…"), this);
    pdfBtn->setProperty("variant", "secondary");
    auto *closeBtn = new QPushButton(QStringLiteral("Close"), this);
    closeBtn->setProperty("variant", "secondary");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(printBtn, &QPushButton::clicked, this, &ReceiptDialog::print);
    connect(pdfBtn, &QPushButton::clicked, this, &ReceiptDialog::savePdf);

    // Layout: Close on the left, secondary actions next, primary (Print) right-most.
    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);
    btnRow->addWidget(closeBtn);
    btnRow->addStretch();
    btnRow->addWidget(pdfBtn);

    // Thermal print is offered only when a database is wired in AND the operator
    // has enabled thermal printing against a configured CUPS queue.
    if (m_db.isValid()) {
        SettingsRepository settings(m_db);
        const bool enabled = settings.get(SettingsKeys::ThermalEnabled) == QLatin1String("1");
        const bool hasQueue = !settings.get(SettingsKeys::ThermalQueue).trimmed().isEmpty();
        if (enabled && hasQueue) {
            auto *thermalBtn = new QPushButton(QStringLiteral("Print on thermal"), this);
            thermalBtn->setProperty("variant", "secondary");
            connect(thermalBtn, &QPushButton::clicked, this, &ReceiptDialog::printThermal);
            btnRow->addWidget(thermalBtn);
        }
    }

    btnRow->addWidget(printBtn);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addWidget(view, 1);
    layout->addLayout(btnRow);
}

QString ReceiptDialog::buildHtml() const
{
    // Each item gets two rows: the medicine name across the top, then an indented
    // "qty unit × price" on the left with the line total right-aligned beneath it.
    // This keeps everything legible at thermal-receipt width instead of cramming
    // four columns into ~58mm.
    QString rows;
    for (const SaleResultLine &l : m_sale.lines) {
        const QString mrp = Money::fromString(l.unitMrp).fmt();
        rows
            += QStringLiteral("<tr><td colspan=2 style='padding-top:4px;'>%1</td></tr>"
                              "<tr><td style='padding-left:10px; color:#444;'>%2 %3 &times; %4</td>"
                              "<td align=right>%5</td></tr>")
                   .arg(l.name)
                   .arg(l.qty)
                   .arg(l.unitLabel, mrp, l.lineTotal);
    }

    auto totalRow = [](const QString &label, const QString &value, bool bold = false) {
        const QString l = bold ? QStringLiteral("<b>%1</b>").arg(label) : label;
        const QString v = bold ? QStringLiteral("<b>%1</b>").arg(value) : value;
        return QStringLiteral("<tr><td align=right style='padding-right:8px;'>%1</td>"
                              "<td align=right>%2</td></tr>")
            .arg(l, v);
    };

    QString totals = totalRow(QStringLiteral("Subtotal"), m_sale.subtotal);
    if (m_sale.discountTotal != QLatin1String("0.00") && !m_sale.discountTotal.isEmpty()) {
        totals += totalRow(QStringLiteral("Discount"),
                           QStringLiteral("-%1").arg(m_sale.discountTotal));
    }
    totals += totalRow(QStringLiteral("TOTAL"), m_sale.grandTotal, true);
    if (!m_sale.amountTendered.isEmpty()) {
        totals += totalRow(QStringLiteral("Tendered"), m_sale.amountTendered);
        totals += totalRow(QStringLiteral("Change"), m_sale.changeReturned);
    }

    QString logoHtml;
    if (!m_logoPath.isEmpty()) {
        logoHtml = QStringLiteral("<div style='text-align:center;'>"
                                  "<img src='file://%1' height='56'></div>")
                       .arg(m_logoPath);
    }
    return logoHtml
           + QStringLiteral("<div style='font-family:monospace; font-size:10pt;'>"
                            "<h3 style='text-align:center; margin:0;'>%1</h3>"
                            "<p style='text-align:center; margin:2px 0;'>Receipt: %2</p><hr>"
                            "<table width='100%%' cellspacing='0' cellpadding='0'>%3</table><hr>"
                            "<table width='100%%' cellspacing='0' cellpadding='0'>%4</table><hr>"
                            "<p style='text-align:center;'>Payment: %5<br>Thank you.</p></div>")
                 .arg(m_pharmacyName.isEmpty() ? QStringLiteral("PHARMACY") : m_pharmacyName,
                      m_sale.receiptNumber, rows, totals, m_sale.paymentMode);
}

void ReceiptDialog::print()
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

void ReceiptDialog::savePdf()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save receipt PDF"),
        QStringLiteral("%1.pdf").arg(m_sale.receiptNumber), QStringLiteral("PDF (*.pdf)"));
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

void ReceiptDialog::printThermal()
{
    if (!m_db.isValid()) {
        return;
    }
    SettingsRepository settings(m_db);
    const QString queue = settings.get(SettingsKeys::ThermalQueue).trimmed();
    if (queue.isEmpty()) {
        return;
    }

    // Resolve the cashier's display name (best-effort; blank if not found).
    QString cashierName;
    if (m_cashierId > 0) {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral("SELECT full_name FROM users WHERE id = ?"));
        q.addBindValue(m_cashierId);
        if (q.exec() && q.next()) {
            cashierName = q.value(0).toString();
        }
    }

    EscPosHeader header;
    header.pharmacyName = settings.get(SettingsKeys::PharmacyName);
    if (header.pharmacyName.trimmed().isEmpty()) {
        header.pharmacyName = m_pharmacyName;
    }
    header.address = settings.get(SettingsKeys::PharmacyAddress);
    header.phone = settings.get(SettingsKeys::PharmacyPhone);
    header.ntn = settings.get(SettingsKeys::PharmacyNtn);
    header.cashierName = cashierName;
    header.soldAt = QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yy HH:mm"));
    header.returnPolicy = settings.get(SettingsKeys::ReturnPolicyText);
    header.footerMessage = settings.get(SettingsKeys::ReceiptFooter);

    const QByteArray bytes = EscPosRenderer::receipt(m_sale, header);
    const ThermalPrintResult res = CupsRawPrinter(queue).printRaw(bytes, m_sale.receiptNumber);
    if (!res.ok) {
        QMessageBox::warning(this, windowTitle(), res.error);
        return;
    }
    QMessageBox::information(
        this, windowTitle(),
        QStringLiteral("Receipt %1 sent to %2.").arg(m_sale.receiptNumber, queue));
}
