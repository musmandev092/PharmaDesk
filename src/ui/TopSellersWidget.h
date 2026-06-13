#pragma once

#include <QSqlDatabase>
#include <QWidget>

class QDateEdit;
class QLabel;
class QTableWidget;

// Top-sellers report tab. From/To + Filter + KPI cards + ranked table, backed by
// AnalyticsRepository::topSellers. Meant to be added as a tab inside ReportsPage.
class TopSellersWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TopSellersWidget(QSqlDatabase db, QWidget *parent = nullptr);

private slots:
    void reload();
    void exportCsv();

private:
    QSqlDatabase m_db;
    QDateEdit *m_from = nullptr;
    QDateEdit *m_to = nullptr;
    QLabel *m_kpiRevenue = nullptr;
    QLabel *m_kpiUnits = nullptr;
    QLabel *m_kpiMargin = nullptr;
    QTableWidget *m_table = nullptr;
};
