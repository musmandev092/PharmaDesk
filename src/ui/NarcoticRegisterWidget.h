#pragma once

#include <QSqlDatabase>
#include <QWidget>

class QDateEdit;
class QTableWidget;

// DRAP-style narcotic register tab. Mirrors ReportsPage's From/To + Filter +
// table layout, backed by AnalyticsRepository::narcoticRegister. Defaults to the
// last 180 days. Meant to be added as a tab inside ReportsPage.
class NarcoticRegisterWidget : public QWidget
{
    Q_OBJECT
public:
    explicit NarcoticRegisterWidget(QSqlDatabase db, QWidget *parent = nullptr);

private slots:
    void reload();

private:
    QSqlDatabase m_db;
    QDateEdit *m_from = nullptr;
    QDateEdit *m_to = nullptr;
    QTableWidget *m_table = nullptr;
};
