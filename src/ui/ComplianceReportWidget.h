#pragma once

#include <QSqlDatabase>
#include <QWidget>

class QDateEdit;
class QTableWidget;

// Controlled-drug compliance report tab. Mirrors ReportsPage's From/To + Filter
// + table layout, backed by AnalyticsRepository::complianceReport. Defaults to
// the last 90 days. Meant to be added as a tab inside ReportsPage.
class ComplianceReportWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ComplianceReportWidget(QSqlDatabase db, QWidget *parent = nullptr);

private slots:
    void reload();

private:
    QSqlDatabase m_db;
    QDateEdit *m_from = nullptr;
    QDateEdit *m_to = nullptr;
    QTableWidget *m_table = nullptr;
};
