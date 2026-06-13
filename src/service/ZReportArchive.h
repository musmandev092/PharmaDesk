#pragma once

#include <QSqlDatabase>
#include <QString>

struct ZReportSignature
{
    bool ok = false;
    QString error;
    QString signatureHex;     // HMAC-SHA256 of the canonical payload (hex)
    bool fromArchive = false; // true if returned from a previously stored row
};

// Signs and archives end-of-shift Z-reports. Port of the PHP ZReport HMAC chain,
// simplified for a single PC: the signing secret lives in a 0600 key file under
// the app-data dir (auto-generated once), and signed reports are stored
// append-only in z_report_archive (one per session, idempotent). This gives a
// tamper-evident day-close record without the Postgres GUC machinery.
class ZReportArchive
{
public:
    ZReportArchive(QSqlDatabase db, qint64 userId) : m_db(std::move(db)), m_userId(userId) {}

    // Build the canonical payload for a session, sign it, and store it if not
    // already archived (returns the existing signature otherwise). Safe to call
    // every time the Z-report is viewed for a CLOSED/RECONCILED session.
    ZReportSignature signAndStore(qint64 sessionId);

private:
    static QByteArray secret(); // read or create the 0600 key file
    static QString canonicalPayload(QSqlDatabase &db, qint64 sessionId);

    QSqlDatabase m_db;
    qint64 m_userId;
};
