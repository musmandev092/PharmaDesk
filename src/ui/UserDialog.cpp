#include "ui/UserDialog.h"

#include "data/UserRepository.h"
#include "domain/Bcrypt.h"
#include "domain/PinPolicy.h"
#include "ui/UiUtil.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

UserDialog::UserDialog(UserRepository *repo, qint64 actingUserId, qint64 editId,
                       const QString &fullName, const QString &username, const QString &role,
                       bool isActive, QWidget *parent)
    : QDialog(parent), m_repo(repo), m_actingUserId(actingUserId), m_editId(editId)
{
    setWindowTitle(editId == 0 ? QStringLiteral("Add user") : QStringLiteral("Edit user"));
    setModal(true);
    setMinimumWidth(420);

    m_fullName = new QLineEdit(this);
    m_fullName->setMaxLength(120);
    m_fullName->setText(fullName);

    m_username = new QLineEdit(this);
    m_username->setMaxLength(60);
    m_username->setText(username);
    m_username->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^[A-Za-z0-9._-]*$")), m_username));
    if (editId != 0) {
        m_username->setReadOnly(true); // username is the login id; keep stable
    }

    m_role = new QComboBox(this);
    m_role->addItems(
        {QStringLiteral("CASHIER"), QStringLiteral("MANAGER"), QStringLiteral("ADMIN")});
    if (!role.isEmpty()) {
        m_role->setCurrentText(role);
    }

    m_pin = new QLineEdit(this);
    m_pin->setEchoMode(QLineEdit::Password);
    m_pin->setMaxLength(8);
    m_pin->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^\\d*$")), m_pin));
    m_pin->setPlaceholderText(editId == 0 ? QStringLiteral("6–8 digits")
                                          : QStringLiteral("leave blank to keep current"));

    m_active = new QCheckBox(QStringLiteral("Active"), this);
    m_active->setChecked(isActive);

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(8);
    form->addRow(QStringLiteral("Full name *"), m_fullName);
    form->addRow(QStringLiteral("Username *"), m_username);
    form->addRow(QStringLiteral("Role"), m_role);
    form->addRow(editId == 0 ? QStringLiteral("PIN *") : QStringLiteral("Reset PIN"), m_pin);
    form->addRow(QString(), m_active);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Cancel)->setProperty("variant", "secondary");
    connect(buttons, &QDialogButtonBox::accepted, this, &UserDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Footer order: Cancel (secondary) on the LEFT, Save (primary) RIGHT-most.
    auto *footer = new QHBoxLayout();
    footer->setContentsMargins(0, 0, 0, 0);
    footer->addWidget(buttons->button(QDialogButtonBox::Cancel));
    footer->addStretch(1);
    footer->addWidget(buttons->button(QDialogButtonBox::Save));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addLayout(form);
    layout->addLayout(footer);
    UiUtil::makeScanSafe(this);
}

void UserDialog::accept()
{
    auto fail = [this](const QString &m) { QMessageBox::warning(this, windowTitle(), m); };

    const QString fullName = m_fullName->text().trimmed();
    const QString username = m_username->text().trimmed();
    const QString role = m_role->currentText();
    const QString pin = m_pin->text();

    if (fullName.isEmpty()) {
        return fail(QStringLiteral("Full name is required."));
    }
    static const QRegularExpression userRe(QStringLiteral("^[A-Za-z0-9._-]+$"));
    if (!userRe.match(username).hasMatch()) {
        return fail(
            QStringLiteral("Username may use letters, digits, dot, underscore and hyphen only."));
    }

    if (m_editId == 0) {
        // Create: PIN required + policy.
        if (auto err = PinPolicy::validate(pin)) {
            return fail(*err);
        }
        if (m_repo->usernameExistsCI(username)) {
            return fail(QStringLiteral("That username is already taken."));
        }
        const QString hash = Bcrypt::hash(pin);
        if (hash.isEmpty()
            || m_repo->createUser(fullName, username, role, hash, m_active->isChecked(),
                                  m_actingUserId)
                   < 0) {
            return fail(QStringLiteral("Could not create the user: %1").arg(m_repo->errorString()));
        }
    } else {
        if (!m_repo->updateUser(m_editId, fullName, role, m_active->isChecked(), m_actingUserId)) {
            return fail(QStringLiteral("Could not update the user: %1").arg(m_repo->errorString()));
        }
        // Optional PIN reset.
        if (!pin.isEmpty()) {
            if (auto err = PinPolicy::validate(
                    pin, m_repo->recentPinHashes(m_editId, PinPolicy::HistoryKeep),
                    m_repo->pinHash(m_editId))) {
                return fail(*err);
            }
            const QString hash = Bcrypt::hash(pin);
            // An admin resetting someone else's PIN forces a change at next login.
            const bool forceRotate = (m_editId != m_actingUserId);
            if (hash.isEmpty() || !m_repo->setPin(m_editId, hash, m_actingUserId, forceRotate)) {
                return fail(QStringLiteral("User saved, but PIN reset failed: %1")
                                .arg(m_repo->errorString()));
            }
        }
    }
    QDialog::accept();
}
