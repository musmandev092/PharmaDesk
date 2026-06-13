#pragma once

#include <QSqlDatabase>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

// Cashier sessions / day-close page. Tabbed:
//   • "My shift" — the logged-in user's current shift status with Open / Close
//     buttons and a table of recent sessions; double-clicking a closed/reconciled
//     row opens its Z-report. Port of pos/frontend/pages/pos/session.php.
//   • "Reconcile" (MANAGER/ADMIN) — closed shifts awaiting manager sign-off.
class SessionsPage : public QWidget
{
    Q_OBJECT
public:
    SessionsPage(QSqlDatabase db, qint64 userId, QWidget *parent = nullptr);

    void reload();

private slots:
    void openShift();
    void closeShift();
    void showZReport();
    void reconcileSelected();

private:
    qint64 selectedSessionId() const;
    void reloadSessions();
    qint64 selectedReconcileSessionId() const;

    // UI construction, split out of the constructor (one tab each). The
    // reconcile tab is only added for MANAGER/ADMIN (see constructor).
    QWidget *buildShiftTab();
    QWidget *buildReconcileTab();

    QSqlDatabase m_db;
    qint64 m_userId;

    QLabel *m_statusLabel = nullptr;
    QLabel *m_detailLabel = nullptr;
    QPushButton *m_openBtn = nullptr;
    QPushButton *m_closeBtn = nullptr;
    QPushButton *m_zBtn = nullptr;
    QTableWidget *m_table = nullptr;

    // Reconcile shifts (manager sign-off) — only present for MANAGER/ADMIN.
    QTableWidget *m_sessions = nullptr;
    QLineEdit *m_recountCash = nullptr;
    QLineEdit *m_reconcileNotes = nullptr;
    QPushButton *m_reconcileBtn = nullptr;
};
