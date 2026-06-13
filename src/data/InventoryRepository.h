#pragma once

#include <QDate>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

// Inventory read models. Mirrors pos/frontend/pages/inventory/dashboard.php and
// stock.php: stock value (Money-exact), batch list with expiry status, low-stock
// and near-expiry queries.

struct InventoryTotals
{
    int medicinesCount = 0; // active, not deleted
    int activeBatches = 0;  // qty>0, not quarantined, not expired
    QString stockValueCost; // Σ(qty×cost) over non-quarantined in-stock, scale 2
    QString stockValueMrp;  // Σ(qty×mrp)  over non-quarantined in-stock, scale 2
};

struct StockBatchRow
{
    qint64 id = 0;
    QString brandName;
    QString genericName;
    QString baseUnit;
    QString batchNumber;
    QDate expiry;
    int currentQty = 0;
    QString costPerUnit;
    QString mrpPerUnit;
    bool quarantined = false;
    bool expired = false;
    int days = 0; // days from today to expiry (negative = past)
};

struct LowStockRow
{
    qint64 medicineId = 0;
    QString brandName;
    QString genericName;
    QString baseUnit;
    int reorderLevel = 0;
    int onHand = 0;
};

struct ExpiringRow
{
    qint64 batchId = 0;
    QString brandName;
    QString batchNumber;
    QString baseUnit;
    QDate expiry;
    int currentQty = 0;
    int days = 0;
};

class InventoryRepository
{
public:
    explicit InventoryRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    InventoryTotals totals() const;
    QVector<StockBatchRow> batchList(const QString &query = QString()) const;
    QVector<LowStockRow> lowStock(int limit = 0) const; // 0 = no limit
    QVector<ExpiringRow> expiringSoon(int withinDays, int limit = 0) const;

private:
    QSqlDatabase m_db;
};
