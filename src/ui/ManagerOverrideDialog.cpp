#include "ui/ManagerOverrideDialog.h"

#include "ui/UiUtil.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int PinMaxLength = 8;
const char *const VariantProperty = "variant";
const char *const VariantSecondary = "secondary";
} // namespace

ManagerOverrideDialog::ManagerOverrideDialog(const QString &title, const QString &message,
                                             bool needLicense, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setModal(true);
    setMinimumWidth(420);

    auto *prompt = new QLabel(message, this);
    prompt->setObjectName(QStringLiteral("muted"));
    prompt->setWordWrap(true);

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(8);
    if (needLicense) {
        m_license = new QLineEdit(this);
        m_license->setPlaceholderText(QStringLiteral("e.g. PRX-00000"));
        form->addRow(QStringLiteral("Prescriber license number"), m_license);
    }
    m_pin = new QLineEdit(this);
    m_pin->setEchoMode(QLineEdit::Password);
    m_pin->setMaxLength(PinMaxLength);
    form->addRow(QStringLiteral("Manager / admin PIN"), m_pin);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Cancel)->setProperty(VariantProperty, VariantSecondary);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addWidget(prompt);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (needLicense) {
        m_license->setFocus();
    } else {
        m_pin->setFocus();
    }
    UiUtil::makeScanSafe(this);
}

QString ManagerOverrideDialog::pin() const
{
    return m_pin->text();
}

QString ManagerOverrideDialog::license() const
{
    return m_license ? m_license->text() : QString();
}
