#pragma once

#include <QDialog>
#include <QSqlDatabase>

class QLabel;
class QTableWidget;

// Read-only detail view for a single goods-receipt note: header (GRN #, supplier,
// invoice, posted date, status, totals) plus its line items. Opened by
// double-clicking a GRN row in PurchasingPage. Loads its own data (a small
// GrnRepository-style query lives in the .cpp) so GrnRepository stays untouched.
class GrnDetailDialog : public QDialog
{
    Q_OBJECT
public:
    GrnDetailDialog(QSqlDatabase db, qint64 grnId, QWidget *parent = nullptr);

private:
    void load();

    QSqlDatabase m_db;
    qint64 m_grnId = 0;

    QLabel *m_header = nullptr;
    QLabel *m_meta = nullptr;
    QLabel *m_totals = nullptr;
    QTableWidget *m_lines = nullptr;
};
