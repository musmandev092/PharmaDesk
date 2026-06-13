#pragma once

#include <QSqlDatabase>
#include <QString>

#include <stdexcept>

// Append-only audit writer. Port of the PHP Audit::write surface. The
// audit_log table is immutable (UPDATE/DELETE blocked by DB triggers); the
// HMAC chain (prev_hmac/row_hmac) is deferred to the Phase-6 Audit port.
namespace Audit {

// Inserts one audit row. before/after are JSON strings (or empty). Must be
// called inside the caller's transaction when part of a larger change.
bool write(QSqlDatabase &db, qint64 userId, const QString &actionType, const QString &entityType,
           qint64 entityId, const QString &beforeJson = QString(),
           const QString &afterJson = QString());

// Thrown by writeOrThrow() when the audit insert fails.
struct WriteError : std::runtime_error
{
    explicit WriteError(const QString &m) : std::runtime_error(m.toStdString()) {}
};

// Fail-CLOSED audit write: throws WriteError if the insert fails. Call this
// (not write()) from inside a money/stock mutation transaction so that a failed
// audit row ABORTS the enclosing change — the append-only audit guarantee is
// worthless if a transaction can commit without its audit record. The service's
// catch(const std::exception&) rolls back and surfaces the error.
void writeOrThrow(QSqlDatabase &db, qint64 userId, const QString &actionType,
                  const QString &entityType, qint64 entityId, const QString &beforeJson = QString(),
                  const QString &afterJson = QString());

} // namespace Audit
