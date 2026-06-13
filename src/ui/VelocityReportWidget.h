#pragma once

#include <QSqlDatabase>
#include <QWidget>

class QTableWidget;

// Sales-velocity report tab. All-time 7/30/90-day windows, so there is no date
// filter — just a Refresh button + table, backed by
// AnalyticsRepository::velocityReport. Meant to be added as a tab inside
// ReportsPage.
class VelocityReportWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VelocityReportWidget(QSqlDatabase db, QWidget *parent = nullptr);

private slots:
    void reload();

private:
    QSqlDatabase m_db;
    QTableWidget *m_table = nullptr;
};
