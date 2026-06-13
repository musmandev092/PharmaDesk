#pragma once

#include "data/ParkedCartRepository.h"

#include <QDialog>
#include <QSqlDatabase>

class QTableWidget;
class QPushButton;
class QLabel;

// Picker for parked (suspended) bills belonging to one cashier. Lists each
// parked cart (label, item count, total, when) with Resume / Discard actions.
// On Resume the chosen cart is exposed via resumedCart() and the dialog
// accept()s; the caller loads those items back into the POS and then calls
// ParkedCartRepository::discard() to consume the row. Discard deletes a row in
// place and refreshes the list.
class ParkedBillsDialog : public QDialog
{
    Q_OBJECT
public:
    ParkedBillsDialog(QSqlDatabase db, qint64 cashierId, QWidget *parent = nullptr);

    // Valid only after the dialog was accepted via Resume. id == 0 otherwise.
    const ParkedCart &resumedCart() const { return m_resumed; }

private slots:
    void resumeSelected();
    void discardSelected();

private:
    void reload();
    qint64 selectedId() const;

    QSqlDatabase m_db;
    qint64 m_cashierId;
    ParkedCartRepository m_repo;

    QTableWidget *m_table = nullptr;
    QPushButton *m_resume = nullptr;
    QPushButton *m_discard = nullptr;
    QLabel *m_empty = nullptr;

    ParkedCart m_resumed;
};
