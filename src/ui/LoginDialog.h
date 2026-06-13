#pragma once

#include "data/UserRepository.h"

#include <QDialog>

class QLineEdit;
class QLabel;
class SettingsRepository;

// Sign-in gate: username + PIN, verified with bcrypt. On success, exposes the
// authenticated UserRecord and stamps last_login_at. Rate-limiting and audit
// logging are deferred to the Phase-6 Auth port.
class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    LoginDialog(UserRepository *users, SettingsRepository *settings, QWidget *parent = nullptr);

    UserRecord user() const { return m_user; }

private slots:
    void attempt();

private:
    UserRepository *m_users;
    QLineEdit *m_username = nullptr;
    QLineEdit *m_pin = nullptr;
    QLabel *m_error = nullptr;
    UserRecord m_user;
};
