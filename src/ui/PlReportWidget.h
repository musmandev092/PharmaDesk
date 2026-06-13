#pragma once

#include <QSqlDatabase>
#include <QWidget>

class QDateEdit;
class QLabel;
class QTableWidget;

// Profit & Loss report tab. Mirrors ReportsPage's From/To + Filter + KPI cards +
// table layout, backed by AnalyticsRepository::profitAndLoss. Meant to be added
// as a tab inside ReportsPage.
class PlReportWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PlReportWidget(QSqlDatabase db, QWidget *parent = nullptr);

private slots:
    void reload();
    void exportCsv();

private:
    QSqlDatabase m_db;
    QDateEdit *m_from = nullptr;
    QDateEdit *m_to = nullptr;
    QLabel *m_kpiRevenue = nullptr;
    QLabel *m_kpiCogs = nullptr;
    QLabel *m_kpiMargin = nullptr;
    QTableWidget *m_table = nullptr;
};
