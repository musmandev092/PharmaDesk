#pragma once

#include <QDialog>

class QLineEdit;
class QLabel;
class UserRepository;

// Change a user's own PIN: verify current PIN, validate the new one against the
// full PinPolicy (incl. last-5 history + differs-from-current), then persist.
class ChangePinDialog : public QDialog
{
    Q_OBJECT
public:
    ChangePinDialog(UserRepository *repo, qint64 userId, QWidget *parent = nullptr);

    // Forced-rotation mode: shows a notice explaining the PIN must be changed.
    void setMandatory(bool mandatory);

public slots:
    void accept() override;

private:
    UserRepository *m_repo;
    qint64 m_userId;
    QLabel *m_notice = nullptr;
    QLineEdit *m_current = nullptr;
    QLineEdit *m_new = nullptr;
    QLineEdit *m_confirm = nullptr;
};
