#include "service/ZReportArchive.h"

#include "domain/AppPaths.h"

#include <QDir>
#include <QFile>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
const QString kSecretFile = QStringLiteral("zreport.key");
} // namespace

QByteArray ZReportArchive::secret()
{
    // The key lives beside the DB in the app-data dir, 0600. Generated once.
    const QString path = QDir(AppPaths::logosDir()).filePath(QStringLiteral("../") + kSecretFile);
    QFile f(path);
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        const QByteArray hex = f.readAll().trimmed();
        if (!hex.isEmpty()) {
            return QByteArray::fromHex(hex);
        }
    }
    // Generate 32 random bytes.
    QByteArray key(32, '\0');
    for (int i = 0; i < key.size(); ++i) {
        key[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    AppPaths::ensureDirs();
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(key.toHex());
        f.close();
        f.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    }
    return key;
}

QString ZReportArchive::canonicalPayload(QSqlDatabase &db, qint64 sessionId)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT cashier_id, opened_at, COALESCE(closed_at,''), opening_float, total_cash_sales, "
        "       total_refunds_paid, total_sales_count, COALESCE(counted_cash,''), "
        "       COALESCE(cash_variance,''), status "
        "  FROM cashier_sessions WHERE id = ?"));
    q.addBindValue(sessionId);
    if (!q.exec() || !q.next()) {
        return QString();
    }
    // Deterministic, pipe-joined key=value payload (stable field order).
    return QStringLiteral(
               "session_id=%1|cashier_id=%2|opened_at=%3|closed_at=%4|opening_float=%5|"
               "total_cash_sales=%6|total_refunds_paid=%7|total_sales_count=%8|counted_cash=%9|"
               "cash_variance=%10|status=%11")
        .arg(sessionId)
        .arg(q.value(0).toLongLong())
        .arg(q.value(1).toString(), q.value(2).toString(), q.value(3).toString(),
             q.value(4).toString(), q.value(5).toString())
        .arg(q.value(6).toInt())
        .arg(q.value(7).toString(), q.value(8).toString(), q.value(9).toString());
}

ZReportSignature ZReportArchive::signAndStore(qint64 sessionId)
{
    ZReportSignature res;

    // Return the existing archived signature if present (append-only).
    QSqlQuery ex(m_db);
    ex.prepare(QStringLiteral("SELECT signature_hex FROM z_report_archive WHERE session_id = ?"));
    ex.addBindValue(sessionId);
    if (ex.exec() && ex.next()) {
        res.ok = true;
        res.signatureHex = ex.value(0).toString();
        res.fromArchive = true;
        return res;
    }

    const QString payload = canonicalPayload(m_db, sessionId);
    if (payload.isEmpty()) {
        res.error = QStringLiteral("Session not found.");
        return res;
    }

    const QByteArray sig
        = QMessageAuthenticationCode::hash(payload.toUtf8(), secret(), QCryptographicHash::Sha256);
    res.signatureHex = QString::fromLatin1(sig.toHex());

    QSqlQuery ins(m_db);
    ins.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO z_report_archive (session_id, generated_by, payload, signature_hex) "
        "VALUES (?, ?, ?, ?)"));
    ins.addBindValue(sessionId);
    ins.addBindValue(m_userId > 0 ? QVariant(m_userId) : QVariant());
    ins.addBindValue(payload);
    ins.addBindValue(res.signatureHex);
    if (!ins.exec()) {
        res.error = ins.lastError().text();
        return res;
    }
    res.ok = true;
    return res;
}
