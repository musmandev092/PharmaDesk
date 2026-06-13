#pragma once

#include <QString>

class QSqlDatabase;

// Infrastructure: one-time import of a medicine catalog from a CSV export of the
// legacy Postgres `medicines` table into the SQLite catalog. Idempotent — a row
// whose SKU already exists among live medicines is skipped, so re-running is
// safe. Soft-deleted source rows (deleted_at set) are not imported. Postgres
// booleans ("t"/"f") are mapped; the created/updated timestamps are ignored and
// left to the DB defaults.
namespace CatalogImporter {

struct Result
{
    int imported = 0; // rows inserted
    int skipped = 0;  // SKU already present, or source row soft-deleted
    int failed = 0;   // row failed validation / insert (import continues)
    bool ok = false;  // false only on a fatal error (file/transaction)
    QString error;    // populated when ok == false
};

// Imports medicines from `csvPath`. All inserts run inside one transaction and
// are attributed to `userId` in the audit log.
Result importFromCsv(QSqlDatabase &db, const QString &csvPath, qint64 userId);

} // namespace CatalogImporter
