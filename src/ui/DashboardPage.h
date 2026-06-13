#pragma once

#include <QSqlDatabase>
#include <QWidget>

class QLabel;
class QTableWidget;

// Home dashboard: at-a-glance KPIs + quick-action tiles linking to every section
// + low-stock and expiring-soon shortlists. Emits openSection() so the shell can
// switch the active nav tab.
class DashboardPage : public QWidget
{
    Q_OBJECT
public:
    DashboardPage(QSqlDatabase db, QWidget *parent = nullptr);

    void reload();

signals:
    void openSection(const QString &title); // matches a MainWindow nav title

private:
    QSqlDatabase m_db;

    QLabel *m_kpiTodaySales = nullptr;
    QLabel *m_kpiTodayNet = nullptr;
    QLabel *m_kpiStockValue = nullptr;
    QLabel *m_kpiLowStock = nullptr;
    QLabel *m_kpiExpiring = nullptr;
    QLabel *m_kpiMedicines = nullptr;
    QTableWidget *m_lowStock = nullptr;
    QTableWidget *m_expiring = nullptr;
};
