#pragma once

#include <QSqlDatabase>
#include <QString>

#include <stdexcept>

// Append-only audit writer. Port of the PHP Audit::write surface. The audit_log
// table is immutable (UPDATE/DELETE blocked by DB triggers) AND tamper-evident:
// every insert links into an HMAC chain (prev_hmac/row_hmac), mirroring the PHP
// app's Postgres trigger (db/triggers/audit_log_immutable.sql). The per-install
// secret lives off-DB in `audit.key` (owner-only), so editing a row, rewriting a
// row_hmac, or dropping+reinserting a row breaks the chain and cannot be healed
// without the key.
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

// Result of walking the HMAC chain.
struct ChainResult
{
    bool ok = true;         // false if a row's recomputed hmac / prev link doesn't match
    int checked = 0;        // number of chained rows verified
    qint64 brokenAtId = -1; // the first row id where the chain failed (or -1)
};

// Re-derive the HMAC chain over every chained audit row (row_hmac <> '') in id
// order and confirm each row's stored prev_hmac + row_hmac match what the secret
// produces. Detects edits, forged hmacs, and dropped/reinserted rows.
ChainResult verifyChain(QSqlDatabase &db);

} // namespace Audit
