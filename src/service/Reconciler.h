#pragma once

#include <QSqlDatabase>
#include <QString>

struct ReconcileResult
{
    bool ok = false;
    QString error;
    QString countedCash; // decimal string, scale 2
    QString variance;    // counted − expected, scale 2
};

// Cashier-session reconciliation. Port of pos/backend/services/Reconciler.php.
//
// A manager reviews a CLOSED shift and signs it off (CLOSED → RECONCILED),
// optionally recounting the drawer (which recomputes the variance against the
// expected cash = opening_float + total_cash_sales − total_refunds_paid). Runs
// in one transaction and writes a SESSION_RECONCILED audit row.
class Reconciler
{
public:
    Reconciler(QSqlDatabase db, qint64 managerId) : m_db(std::move(db)), m_managerId(managerId) {}

    // recountedCash empty → keep the cashier's counted_cash; otherwise recount
    // and recompute the variance. notes are appended (preserving any prior note).
    ReconcileResult reconcile(qint64 sessionId, const QString &recountedCash = QString(),
                              const QString &notes = QString());

private:
    QSqlDatabase m_db;
    qint64 m_managerId;
};
