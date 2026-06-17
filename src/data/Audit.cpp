#include "data/Audit.h"

#include "domain/AppPaths.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace Audit {
namespace {

const QString kKeyFile = QStringLiteral("audit.key");

// The per-install HMAC secret: 32 random bytes beside the DB in the app-data
// dir, owner-only (0600), generated once. Same scheme as the Z-report key.
// NOTE (owner): a single per-install key with no rotation. Rotating the key
// invalidates verification of rows written under the old key, so a rotation
// procedure must archive the old key alongside the rows it signed. Documented in
// docs/security-model.md.
QByteArray secret()
{
    const QString path = QDir(AppPaths::logosDir()).filePath(QStringLiteral("../") + kKeyFile);
    QFile f(path);
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        const QByteArray hex = f.readAll().trimmed();
        if (!hex.isEmpty()) {
            return QByteArray::fromHex(hex);
        }
    }
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

// Canonical payload: every field pipe-separated, NULL→'' — identical field order
// to the Postgres trigger's audit_log_compute_hmac so the semantics match the PHP
// app (the HMAC values differ only because the per-install secret differs).
QString canonical(const QString &ts, const QString &userId, const QString &action,
                  const QString &etype, const QString &entityId, const QString &before,
                  const QString &after, const QString &reason, const QString &ip, const QString &ua)
{
    return ts + QLatin1Char('|') + userId + QLatin1Char('|') + action + QLatin1Char('|') + etype
           + QLatin1Char('|') + entityId + QLatin1Char('|') + before + QLatin1Char('|') + after
           + QLatin1Char('|') + reason + QLatin1Char('|') + ip + QLatin1Char('|') + ua;
}

QString hmacHex(const QString &prev, const QString &payload, const QByteArray &key)
{
    return QString::fromLatin1(
        QMessageAuthenticationCode::hash((prev + payload).toUtf8(), key, QCryptographicHash::Sha256)
            .toHex());
}

// The current chain tip: row_hmac of the highest id whose row_hmac is set. Rows
// written before this column was populated carry '' and are outside the chain.
QString chainTip(QSqlDatabase &db)
{
    QSqlQuery q(db);
    if (q.exec(QStringLiteral(
            "SELECT row_hmac FROM audit_log WHERE row_hmac <> '' ORDER BY id DESC LIMIT 1"))
        && q.next()) {
        return q.value(0).toString();
    }
    return QString();
}

} // namespace

bool write(QSqlDatabase &db, qint64 userId, const QString &actionType, const QString &entityType,
           qint64 entityId, const QString &beforeJson, const QString &afterJson)
{
    // Compute the chain BEFORE insert (audit_log blocks UPDATE, so we can't patch
    // the hmac afterwards) — which means generating the timestamp here so the value
    // we hash is exactly the value we store. SQLite CURRENT_TIMESTAMP is UTC
    // "yyyy-MM-dd hh:mm:ss"; match it.
    const QByteArray key = secret();
    const QString prev = chainTip(db);
    const QString ts
        = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    const QString uid = userId > 0 ? QString::number(userId) : QString();
    const QString eid = entityId > 0 ? QString::number(entityId) : QString();

    const QString payload = canonical(ts, uid, actionType, entityType, eid, beforeJson, afterJson,
                                      QString(), QString(), QString());
    const QString rowHmac = hmacHex(prev, payload, key);

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO audit_log "
        "  (timestamp, user_id, action_type, entity_type, entity_id, before_value, after_value, "
        "   prev_hmac, row_hmac) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(ts);
    q.addBindValue(userId > 0 ? QVariant(userId) : QVariant());
    q.addBindValue(actionType);
    q.addBindValue(entityType);
    q.addBindValue(entityId > 0 ? QVariant(entityId) : QVariant());
    q.addBindValue(beforeJson.isEmpty() ? QVariant() : QVariant(beforeJson));
    q.addBindValue(afterJson.isEmpty() ? QVariant() : QVariant(afterJson));
    // prev_hmac is NOT NULL. The genesis row's prev is empty — bind a guaranteed
    // non-null empty string (a null QString would bind as SQL NULL and violate the
    // constraint). row_hmac is always a 64-char hex, so it's never null.
    q.addBindValue(prev.isNull() ? QString::fromLatin1("") : prev);
    q.addBindValue(rowHmac);
    return q.exec();
}

void writeOrThrow(QSqlDatabase &db, qint64 userId, const QString &actionType,
                  const QString &entityType, qint64 entityId, const QString &beforeJson,
                  const QString &afterJson)
{
    if (!write(db, userId, actionType, entityType, entityId, beforeJson, afterJson)) {
        throw WriteError(QStringLiteral("Failed to write audit record (%1).").arg(actionType));
    }
}

ChainResult verifyChain(QSqlDatabase &db)
{
    ChainResult res;
    const QByteArray key = secret();
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "SELECT id, timestamp, user_id, action_type, entity_type, entity_id, before_value, "
            "       after_value, reason, ip_address, user_agent, prev_hmac, row_hmac "
            "FROM audit_log WHERE row_hmac <> '' ORDER BY id ASC"))) {
        res.ok = false;
        return res;
    }
    QString prev;
    while (q.next()) {
        const qint64 id = q.value(0).toLongLong();
        const QString payload
            = canonical(q.value(1).toString(), q.value(2).toString(), q.value(3).toString(),
                        q.value(4).toString(), q.value(5).toString(), q.value(6).toString(),
                        q.value(7).toString(), q.value(8).toString(), q.value(9).toString(),
                        q.value(10).toString());
        const QString storedPrev = q.value(11).toString();
        const QString storedHmac = q.value(12).toString();
        if (storedPrev != prev || storedHmac != hmacHex(prev, payload, key)) {
            res.ok = false;
            res.brokenAtId = id;
            return res;
        }
        prev = storedHmac;
        ++res.checked;
    }
    return res;
}

} // namespace Audit
