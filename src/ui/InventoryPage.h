#pragma once

#include <QSqlDatabase>
#include <QWidget>

class QLineEdit;
class QTableWidget;
class QLabel;
class QTabWidget;

// Inventory screen (Phase 3): Overview KPIs + alerts, full batch list with
// expiry status, low-stock and expiring-soon tables. Read-only views over the
// batch/medicine data; mirrors the PHP inventory dashboard + stock pages.
class InventoryPage : public QWidget
{
    Q_OBJECT
public:
    InventoryPage(QSqlDatabase db, qint64 userId, QWidget *parent = nullptr);

    void reload();

private slots:
    void reloadBatches();

private:
    void buildOverview(QWidget *tab);
    void buildBatches(QWidget *tab);
    void buildLowStock(QWidget *tab);
    void buildExpiring(QWidget *tab);

    QSqlDatabase m_db;
    qint64 m_userId = -1;
    QTabWidget *m_tabs = nullptr;

    // Overview
    QLabel *m_kpiMedicines = nullptr;
    QLabel *m_kpiBatches = nullptr;
    QLabel *m_kpiCost = nullptr;
    QLabel *m_kpiMrp = nullptr;
    QTableWidget *m_ovExpiring = nullptr;
    QTableWidget *m_ovLow = nullptr;

    // Tabs
    QLineEdit *m_batchSearch = nullptr;
    QTableWidget *m_batches = nullptr;
    QTableWidget *m_low = nullptr;
    QTableWidget *m_expiring = nullptr;
};
