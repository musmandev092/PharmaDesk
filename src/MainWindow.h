#pragma once

#include "data/UserRepository.h"

#include <QMainWindow>
#include <QSqlDatabase>

class QStackedWidget;

// Main shell: branded top nav (POS / Medicines / Inventory / Purchasing /
// Reports / Admin) with the signed-in user and a Sign out control, over a
// QStackedWidget. Emits signedOut() so main() can return to the login screen
// without quitting the process.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QSqlDatabase db, const UserRecord &user, QWidget *parent = nullptr);

signals:
    void signedOut();

private:
    QSqlDatabase m_db;
    UserRecord m_user;
    QStackedWidget *m_stack = nullptr;
};
