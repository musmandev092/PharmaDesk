#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>

struct AuditRow
{
    QString timestamp;
    QString userName;
    QString actionType;
    QString entityType;
    qint64 entityId = 0;
};

// Read-only view of the append-only audit_log (most-recent first).
class AuditRepository
{
public:
    explicit AuditRepository(QSqlDatabase db) : m_db(std::move(db)) {}
    QVector<AuditRow> list(int limit = 500) const;

private:
    QSqlDatabase m_db;
};
