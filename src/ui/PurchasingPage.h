#pragma once

#include <QSqlDatabase>
#include <QWidget>

class QLineEdit;
class QTableWidget;
class QTabWidget;

// Purchasing screen (Phase 4): Suppliers (list + add/edit/delete) and Goods
// Receipts (list of posted GRNs + New GRN).
class PurchasingPage : public QWidget
{
    Q_OBJECT
public:
    PurchasingPage(QSqlDatabase db, qint64 userId, QWidget *parent = nullptr);

    void reload();

private slots:
    void reloadSuppliers();
    void reloadGrns();
    void addSupplier();
    void editSupplier();
    void deleteSupplier();
    void newGrn();
    void openGrn();

private:
    qint64 selectedSupplierId() const;

    QSqlDatabase m_db;
    qint64 m_userId;
    QTabWidget *m_tabs = nullptr;

    QLineEdit *m_supplierSearch = nullptr;
    QTableWidget *m_suppliers = nullptr;
    QTableWidget *m_grns = nullptr;
};
