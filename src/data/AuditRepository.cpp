#include "data/AuditRepository.h"

#include <QSqlQuery>

QVector<AuditRow> AuditRepository::list(int limit) const
{
    QVector<AuditRow> out;
    QSqlQuery q(m_db);
    q.prepare(
        QStringLiteral("SELECT a.timestamp, COALESCE(u.full_name, '(system)'), a.action_type, "
                       "       a.entity_type, COALESCE(a.entity_id, 0) "
                       "  FROM audit_log a LEFT JOIN users u ON u.id = a.user_id "
                       " ORDER BY a.id DESC LIMIT ?"));
    q.addBindValue(limit);
    if (q.exec()) {
        while (q.next()) {
            AuditRow r;
            r.timestamp = q.value(0).toString().left(19);
            r.userName = q.value(1).toString();
            r.actionType = q.value(2).toString();
            r.entityType = q.value(3).toString();
            r.entityId = q.value(4).toLongLong();
            out.push_back(r);
        }
    }
    return out;
}
