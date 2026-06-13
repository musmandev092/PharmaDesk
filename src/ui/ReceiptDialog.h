#pragma once

#include "service/SaleService.h"

#include <QDialog>
#include <QSqlDatabase>

// Receipt preview with Print (QPrinter/QPrintDialog) and Save-PDF (QPrinter
// PdfFormat) — both render the receipt document via QTextDocument/QPainter.
// When a database + thermal printing are configured, also offers a "Print on
// thermal" action that streams an ESC/POS receipt to the configured CUPS queue.
class ReceiptDialog : public QDialog
{
    Q_OBJECT
public:
    // db + cashierId are optional: when valid and thermal printing is enabled in
    // settings, the dialog shows a thermal-print button. Existing callers that
    // pass neither keep the QPrinter/PDF-only behaviour.
    ReceiptDialog(const SaleResult &sale, const QString &pharmacyName,
                  const QString &logoPath = QString(), QSqlDatabase db = QSqlDatabase(),
                  qint64 cashierId = 0, QWidget *parent = nullptr);

private slots:
    void print();
    void savePdf();
    void printThermal();

private:
    QString buildHtml() const;

    SaleResult m_sale;
    QString m_pharmacyName;
    QString m_logoPath;
    QSqlDatabase m_db;
    qint64 m_cashierId = 0;
};
