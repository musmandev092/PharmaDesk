#include "ui/ChangePinDialog.h"

#include "data/UserRepository.h"
#include "domain/Bcrypt.h"
#include "domain/PinPolicy.h"
#include "ui/UiUtil.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

ChangePinDialog::ChangePinDialog(UserRepository *repo, qint64 userId, QWidget *parent)
    : QDialog(parent), m_repo(repo), m_userId(userId)
{
    setWindowTitle(QStringLiteral("Change my PIN"));
    setModal(true);
    setMinimumWidth(420);

    m_notice = new QLabel(this);
    m_notice->setObjectName(QStringLiteral("muted"));
    m_notice->setWordWrap(true);
    m_notice->hide();

    auto numeric = [this]() {
        auto *e = new QLineEdit(this);
        e->setEchoMode(QLineEdit::Password);
        e->setMaxLength(8);
        e->setValidator(
            new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^\\d*$")), e));
        return e;
    };
    m_current = numeric();
    m_new = numeric();
    m_confirm = numeric();

    auto *form = new QFormLayout;
    form->setSpacing(8);
    form->addRow(QStringLiteral("Current PIN"), m_current);
    form->addRow(QStringLiteral("New PIN (6–8 digits)"), m_new);
    form->addRow(QStringLiteral("Confirm new PIN"), m_confirm);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Cancel)->setProperty("variant", "secondary");
    connect(buttons, &QDialogButtonBox::accepted, this, &ChangePinDialog::accept);
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
    layout->addWidget(m_notice);
    layout->addLayout(form);
    layout->addLayout(footer);
    UiUtil::makeScanSafe(this);
}

void ChangePinDialog::setMandatory(bool mandatory)
{
    if (mandatory) {
        setWindowTitle(QStringLiteral("PIN change required"));
        m_notice->setText(
            QStringLiteral("Your PIN must be changed before you can continue (security policy)."));
        m_notice->show();
    }
}

void ChangePinDialog::accept()
{
    auto fail = [this](const QString &m) { QMessageBox::warning(this, windowTitle(), m); };

    const QString currentHash = m_repo->pinHash(m_userId);
    if (!Bcrypt::verify(m_current->text(), currentHash)) {
        return fail(QStringLiteral("Current PIN is incorrect."));
    }
    if (m_new->text() != m_confirm->text()) {
        return fail(QStringLiteral("The new PINs don't match."));
    }
    const QStringList history = m_repo->recentPinHashes(m_userId, PinPolicy::HistoryKeep);
    if (auto err = PinPolicy::validate(m_new->text(), history, currentHash)) {
        return fail(*err);
    }
    const QString newHash = Bcrypt::hash(m_new->text());
    if (newHash.isEmpty() || !m_repo->setPin(m_userId, newHash, m_userId)) {
        return fail(QStringLiteral("Could not change the PIN: %1").arg(m_repo->errorString()));
    }
    QMessageBox::information(this, windowTitle(), QStringLiteral("Your PIN has been changed."));
    QDialog::accept();
}
