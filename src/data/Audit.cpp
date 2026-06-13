#include "data/Audit.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace Audit {

bool write(QSqlDatabase &db, qint64 userId, const QString &actionType, const QString &entityType,
           qint64 entityId, const QString &beforeJson, const QString &afterJson)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO audit_log "
        "  (timestamp, user_id, action_type, entity_type, entity_id, before_value, after_value) "
        "VALUES (CURRENT_TIMESTAMP, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(userId > 0 ? QVariant(userId) : QVariant());
    q.addBindValue(actionType);
    q.addBindValue(entityType);
    q.addBindValue(entityId > 0 ? QVariant(entityId) : QVariant());
    q.addBindValue(beforeJson.isEmpty() ? QVariant() : QVariant(beforeJson));
    q.addBindValue(afterJson.isEmpty() ? QVariant() : QVariant(afterJson));
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

} // namespace Audit
