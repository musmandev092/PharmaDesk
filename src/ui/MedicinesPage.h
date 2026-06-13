#pragma once

#include <QSqlDatabase>
#include <QWidget>

class QLineEdit;
class QTableWidget;
class QPushButton;

// Medicine catalog screen: search, list (with on-hand stock), add/edit, soft
// delete, and a quick "Add stock" so items become sellable in the POS.
class MedicinesPage : public QWidget
{
    Q_OBJECT
public:
    MedicinesPage(QSqlDatabase db, qint64 userId, QWidget *parent = nullptr);

    void reload();

private slots:
    void addMedicine();
    void editSelected();
    void deleteSelected();
    void addStockSelected();

private:
    qint64 selectedId() const;
    QString selectedName() const;

    QSqlDatabase m_db;
    qint64 m_userId;
    QLineEdit *m_search = nullptr;
    QTableWidget *m_table = nullptr;
};
