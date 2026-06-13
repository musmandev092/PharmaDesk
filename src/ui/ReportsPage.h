#pragma once

#include <QSqlDatabase>
#include <QWidget>

class QDateEdit;
class QTableWidget;
class QLabel;

// Reports screen (Phase 5): sales report over a date range — KPIs (count, net,
// cash, card, refunds), per-sale table, CSV export, and reprint a selected sale.
class ReportsPage : public QWidget
{
    Q_OBJECT
public:
    ReportsPage(QSqlDatabase db, QWidget *parent = nullptr);

    void reload();

private slots:
    void exportCsv();
    void reprintSelected();

private:
    qint64 selectedSaleId() const;

    QSqlDatabase m_db;
    QDateEdit *m_from = nullptr;
    QDateEdit *m_to = nullptr;
    QLabel *m_kpiCount = nullptr;
    QLabel *m_kpiNet = nullptr;
    QLabel *m_kpiCash = nullptr;
    QLabel *m_kpiCard = nullptr;
    QLabel *m_kpiRefunds = nullptr;
    QTableWidget *m_table = nullptr;
};
