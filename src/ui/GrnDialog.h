#pragma once

#include "service/GrnService.h"

#include <QDialog>
#include <QSqlDatabase>
#include <QVector>

class QComboBox;
class QLineEdit;
class QDateEdit;
class QTableWidget;
class QLabel;

// Create + post a goods-receipt note: choose supplier, optional invoice details,
// add lines (each via GrnLineDialog), then post via GrnService.
class GrnDialog : public QDialog
{
    Q_OBJECT
public:
    GrnDialog(QSqlDatabase db, qint64 userId, QWidget *parent = nullptr);

private slots:
    void addLine();
    void removeLine();
    void postGrn();

private:
    void renderLines();

    QSqlDatabase m_db;
    qint64 m_userId;

    QComboBox *m_supplier = nullptr;
    QLineEdit *m_invoiceNumber = nullptr;
    QDateEdit *m_invoiceDate = nullptr;
    QTableWidget *m_lines = nullptr;
    QLabel *m_subtotal = nullptr;

    QVector<GrnLineInput> m_lineInputs;
    QStringList m_lineNames;
};
