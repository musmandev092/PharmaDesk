#pragma once

#include "domain/SaleTypes.h" // SaleInput / SaleLineInput / SaleResult / SaleResultLine

#include <QSqlDatabase>
#include <QString>

// Atomic sale commit. Port of pos/backend/services/Sale.php (minus the
// Postgres-specific advisory lock / SERIALIZABLE retry, which a single-PC
// SQLite connection doesn't need). Enforces the money/stock/FEFO guardrails, the
// controlled-substance gate (prescriber license + manager/admin witness) and
// manager-authorized discounts.
class SaleService
{
public:
    SaleService(QSqlDatabase db, qint64 cashierId) : m_db(std::move(db)), m_cashierId(cashierId) {}

    SaleResult commit(const SaleInput &input);

private:
    QString nextReceiptNumber();

    QSqlDatabase m_db;
    qint64 m_cashierId;
};
