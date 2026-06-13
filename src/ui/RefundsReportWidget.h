#pragma once

#include <QSqlDatabase>
#include <QWidget>

class QDateEdit;
class QLabel;
class QTableWidget;

// Refunds report tab. Mirrors ReportsPage's From/To + Filter + KPI cards + table
// layout, backed by AnalyticsRepository::refundsReport. Meant to be added as a
// tab inside ReportsPage.
class RefundsReportWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RefundsReportWidget(QSqlDatabase db, QWidget *parent = nullptr);

private slots:
    void reload();

private:
    QSqlDatabase m_db;
    QDateEdit *m_from = nullptr;
    QDateEdit *m_to = nullptr;
    QLabel *m_kpiCount = nullptr;
    QLabel *m_kpiAmount = nullptr;
    QLabel *m_kpiRate = nullptr;
    QTableWidget *m_table = nullptr;
};
