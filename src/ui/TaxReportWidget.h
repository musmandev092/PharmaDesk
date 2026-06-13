#pragma once

#include <QSqlDatabase>
#include <QWidget>

class QDateEdit;
class QLabel;
class QTableWidget;

// Tax summary report tab. Mirrors ReportsPage's From/To + Filter + KPI cards +
// table layout, backed by AnalyticsRepository::taxReport. Meant to be added as a
// tab inside ReportsPage.
class TaxReportWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TaxReportWidget(QSqlDatabase db, QWidget *parent = nullptr);

private slots:
    void reload();

private:
    QSqlDatabase m_db;
    QDateEdit *m_from = nullptr;
    QDateEdit *m_to = nullptr;
    QLabel *m_kpiBase = nullptr;
    QLabel *m_kpiTax = nullptr;
    QLabel *m_kpiTotal = nullptr;
    QTableWidget *m_table = nullptr;
};
