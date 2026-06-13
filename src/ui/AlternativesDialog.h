#pragma once

#include <QDialog>
#include <QSqlDatabase>
#include <QString>

class QTableWidget;

// Modal lookup of in-stock alternatives (other brands of the same generic) for a
// medicine, driven by AlternativesFinder. Read-only: it never changes stock or
// the cart — the cashier reads off a substitute and adds it via the normal POS
// search. Columns: Brand, Strength, Form, Manufacturer, On hand, MRP.
class AlternativesDialog : public QDialog
{
    Q_OBJECT
public:
    AlternativesDialog(QSqlDatabase db, qint64 medicineId, const QString &medicineName,
                       QWidget *parent = nullptr);

private:
    QSqlDatabase m_db;
    qint64 m_medicineId;

    QTableWidget *m_table = nullptr;
};
