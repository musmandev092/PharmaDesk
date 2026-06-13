#pragma once

#include "service/SaleService.h" // for SaleResult / SaleResultLine

#include <QSqlDatabase>

// Loads a committed sale back into a SaleResult so the receipt can be reprinted
// from the sales report. Read-only.
class SaleRepository
{
public:
    explicit SaleRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    // Populates a SaleResult from sales + sale_items. ok=false if not found.
    SaleResult loadReceipt(qint64 saleId) const;

private:
    QSqlDatabase m_db;
};
