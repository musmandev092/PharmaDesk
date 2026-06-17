#include "data/Audit.h"

#include "domain/AppPaths.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QList>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace Audit {
namespace {

const QString kKeyFile = QStringLiteral("audit.key");
const QString kArchiveFile = QStringLiteral("audit.key.archive");

QString keyPath()
{
    return QDir(AppPaths::logosDir()).filePath(QStringLiteral("../") + kKeyFile);
}
QString archivePath()
{
    return QDir(AppPaths::logosDir()).filePath(QStringLiteral("../") + kArchiveFile);
}

// The CURRENT per-install HMAC secret: 32 random bytes beside the DB in the
// app-data dir, owner-only (0600), generated once. All NEW audit rows are signed
// with this key.
QByteArray secret()
{
    QFile f(keyPath());
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

// Retired keys (one hex per line in audit.key.archive). Rows signed before a
// rotation still verify under the key that signed them.
QList<QByteArray> archivedKeys()
{
    QList<QByteArray> out;
    QFile f(archivePath());
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        for (const QByteArray &line : f.readAll().split('\n')) {
            const QByteArray hex = line.trimmed();
            if (!hex.isEmpty()) {
                out << QByteArray::fromHex(hex);
            }
        }
    }
    return out;
}

// All keys a row may legitimately have been signed with: the current key first,
// then any retired keys (rotation policy — see docs/security-model.md).
QList<QByteArray> verificationKeys()
{
    QList<QByteArray> keys;
    keys << secret();
    keys << archivedKeys();
    return keys;
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
    const QList<QByteArray> keys = verificationKeys(); // current + any retired keys
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
        bool hmacOk = false;
        for (const QByteArray &k : keys) {
            if (storedHmac == hmacHex(prev, payload, k)) {
                hmacOk = true;
                break;
            }
        }
        if (storedPrev != prev || !hmacOk) {
            res.ok = false;
            res.brokenAtId = id;
            return res;
        }
        prev = storedHmac;
        ++res.checked;
    }
    return res;
}

bool rotateKey()
{
    // Planned rotation: archive the current key (so the rows it signed still
    // verify) and generate a fresh current key that all NEW rows will use.
    const QByteArray current = secret();
    AppPaths::ensureDirs();
    QFile arc(archivePath());
    if (!arc.open(QIODevice::Append | QIODevice::Text)) {
        return false;
    }
    arc.write(current.toHex() + '\n');
    arc.close();
    arc.setPermissions(QFile::ReadOwner | QFile::WriteOwner);

    QByteArray fresh(32, '\0');
    for (int i = 0; i < fresh.size(); ++i) {
        fresh[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    QFile f(keyPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    f.write(fresh.toHex());
    f.close();
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    return true;
}

} // namespace Audit
