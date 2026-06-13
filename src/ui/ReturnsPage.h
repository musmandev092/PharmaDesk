#pragma once

#include "data/ReturnRepository.h"

#include <QSqlDatabase>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

// Returns / refunds / void screen. Tabbed:
//   • "Process return" — look up a sale by receipt number, pick a line + qty +
//     reason + restock, process the refund, void a whole sale, view recent returns.
//   • "Review queue" (MANAGER/ADMIN) — adjudicate pending returns.
//   • "Supplier returns" (MANAGER/ADMIN) — settle returns shipped back to suppliers.
// Drives ReturnService (one audited transaction each).
class ReturnsPage : public QWidget
{
    Q_OBJECT
public:
    ReturnsPage(QSqlDatabase db, qint64 userId, QWidget *parent = nullptr);

    void reload();

private slots:
    void lookup();
    void lineSelectionChanged();
    void processReturn();
    void voidSale();
    void reloadRecent();
    void adjudicateSelected(const QString &newStatus);
    void markSupplierSent();

private:
    void showSale(const ReturnSaleHeader &hdr);
    void clearSale();
    qint64 selectedSaleItemId(int *outRemaining = nullptr) const;
    void reloadPendingReturns();
    void reloadSupplierReturns();
    qint64 selectedPendingReturnId() const;
    qint64 selectedSupplierReturnId(bool *outSettled = nullptr) const;

    // UI construction, split out of the constructor (one tab each). The review
    // and supplier tabs are only added for MANAGER/ADMIN (see constructor).
    QWidget *buildProcessTab();
    QWidget *buildReviewQueueTab();
    QWidget *buildSupplierReturnsTab();

    QSqlDatabase m_db;
    qint64 m_userId;

    QLineEdit *m_receipt = nullptr;
    QLabel *m_saleInfo = nullptr;
    QPushButton *m_voidBtn = nullptr;
    QTableWidget *m_lines = nullptr;

    QSpinBox *m_qty = nullptr;
    QComboBox *m_reason = nullptr;
    QLineEdit *m_condition = nullptr;
    QCheckBox *m_restock = nullptr;
    QPushButton *m_processBtn = nullptr;

    QTableWidget *m_recent = nullptr;

    // Review queue (adjudication) — only present for MANAGER/ADMIN.
    QTableWidget *m_pendingReturns = nullptr;
    QLineEdit *m_returnNotes = nullptr;

    // Supplier returns (settlement) — only present for MANAGER/ADMIN.
    QTableWidget *m_supplierReturns = nullptr;
    QPushButton *m_markSentBtn = nullptr;

    qint64 m_currentSaleId = 0;
    QString m_currentStatus;
};
