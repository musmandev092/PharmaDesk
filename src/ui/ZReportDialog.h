#pragma once

#include "data/SessionRepository.h"

#include <QDialog>
#include <QSqlDatabase>

// End-of-shift Z-report preview with Print (QPrinter/QPrintDialog) and Save-PDF
// (QPrinter PdfFormat) — both render via QTextDocument. Pattern copied from
// ReceiptDialog. Port of pos/frontend/pages/sessions/z-report.php (the printable
// HTML page; the HMAC-signature archive is deferred to the Phase-6 Audit port).
class ZReportDialog : public QDialog
{
    Q_OBJECT
public:
    ZReportDialog(QSqlDatabase db, qint64 sessionId, QWidget *parent = nullptr);

private slots:
    void print();
    void savePdf();

private:
    void buildText();
    QString buildHtml() const;

    // Sign (or read back) the session's archived Z-report and cache the hex.
    // Empty when the shift is not closed or signing fails (rendered as unsigned).
    void computeSignature();

    QSqlDatabase m_db;
    qint64 m_sessionId;
    QString m_pharmacyName;
    SessionSummary m_summary;
    QString m_plainText;
    QString m_signatureHex; // full HMAC-SHA256 hex, or empty if unsigned
};
