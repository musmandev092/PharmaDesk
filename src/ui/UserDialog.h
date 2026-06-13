#pragma once

#include <QDialog>

class QLineEdit;
class QComboBox;
class QCheckBox;
class UserRepository;

// Add or edit a staff user (admin only). On create, a PIN is required and is
// validated against PinPolicy. On edit, name/role/active change; the PIN is
// only reset when a new one is typed.
class UserDialog : public QDialog
{
    Q_OBJECT
public:
    UserDialog(UserRepository *repo, qint64 actingUserId, qint64 editId, const QString &fullName,
               const QString &username, const QString &role, bool isActive,
               QWidget *parent = nullptr);

public slots:
    void accept() override;

private:
    UserRepository *m_repo;
    qint64 m_actingUserId;
    qint64 m_editId;

    QLineEdit *m_fullName = nullptr;
    QLineEdit *m_username = nullptr;
    QComboBox *m_role = nullptr;
    QLineEdit *m_pin = nullptr;
    QCheckBox *m_active = nullptr;
};
