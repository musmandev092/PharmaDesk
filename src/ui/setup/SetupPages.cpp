#include "ui/setup/SetupPages.h"

#include "data/UserRepository.h"
#include "domain/PinPolicy.h"
#include "domain/SettingsKeys.h"
#include "ui/UiUtil.h"

#include <QButtonGroup>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

namespace {
// Inline error label styled red; hidden until a message is set.
QLabel *makeErrorLabel(QWidget *parent)
{
    auto *l = new QLabel(parent);
    l->setWordWrap(true);
    l->setStyleSheet(
        QStringLiteral("color:%1; font-weight:600;").arg(UiUtil::Palette::Danger.name()));
    l->hide();
    return l;
}
void showError(QLabel *l, const QString &msg)
{
    l->setText(msg);
    l->setVisible(!msg.isEmpty());
}
} // namespace

// ── WelcomePage ────────────────────────────────────────────────────────────
WelcomePage::WelcomePage(QWidget *parent) : QWizardPage(parent)
{
    setTitle(QStringLiteral("Welcome"));
    setSubTitle(QStringLiteral("Let's set up your pharmacy. This runs once."));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    auto *intro = new QLabel(
        QStringLiteral("We'll create your administrator account and record your pharmacy's "
                       "details (name, address, phone, logo). These appear on receipts and "
                       "reports, and can be changed later in Settings.\n\n"
                       "Click Next to begin."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    layout->addStretch();
}

// ── PharmacyIdentityPage ────────────────────────────────────────────────────
PharmacyIdentityPage::PharmacyIdentityPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle(QStringLiteral("Pharmacy details"));
    setSubTitle(QStringLiteral("Printed on receipts and reports. Name is required."));

    m_name = new QLineEdit(this);
    m_name->setMaxLength(SettingsKeys::MaxPharmacyName);
    m_name->setPlaceholderText(QStringLiteral("e.g. City Care Pharmacy"));

    m_address = new QPlainTextEdit(this);
    m_address->setFixedHeight(64);

    m_phone = new QLineEdit(this);
    m_phone->setMaxLength(SettingsKeys::MaxPharmacyPhone);

    m_ntn = new QLineEdit(this);
    m_ntn->setMaxLength(SettingsKeys::MaxPharmacyNtn);
    m_ntn->setPlaceholderText(QStringLiteral("NTN / Tax ID / DRAP Lic. (free text)"));

    m_policy = new QPlainTextEdit(this);
    m_policy->setFixedHeight(64);
    m_policy->setPlainText(SettingsKeys::defaultReturnPolicy());

    m_footer = new QLineEdit(this);
    m_footer->setMaxLength(SettingsKeys::MaxReceiptFooter);
    m_footer->setText(SettingsKeys::defaultReceiptFooter());

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(8);
    form->addRow(QStringLiteral("Pharmacy name *"), m_name);
    form->addRow(QStringLiteral("Address"), m_address);
    form->addRow(QStringLiteral("Phone"), m_phone);
    form->addRow(QStringLiteral("Tax / registration line"), m_ntn);
    form->addRow(QStringLiteral("Return policy text"), m_policy);
    form->addRow(QStringLiteral("Receipt footer message"), m_footer);

    m_error = makeErrorLabel(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);
    layout->addLayout(form);
    layout->addWidget(m_error);
}

QString PharmacyIdentityPage::name() const
{
    return m_name->text().trimmed();
}
QString PharmacyIdentityPage::address() const
{
    return m_address->toPlainText().trimmed();
}
QString PharmacyIdentityPage::phone() const
{
    return m_phone->text().trimmed();
}
QString PharmacyIdentityPage::ntn() const
{
    return m_ntn->text().trimmed();
}
QString PharmacyIdentityPage::returnPolicy() const
{
    return m_policy->toPlainText().trimmed();
}
QString PharmacyIdentityPage::receiptFooter() const
{
    return m_footer->text().trimmed();
}

bool PharmacyIdentityPage::validatePage()
{
    if (name().isEmpty()) {
        showError(m_error, QStringLiteral("Pharmacy name is required."));
        return false;
    }
    if (address().length() > SettingsKeys::MaxPharmacyAddress) {
        showError(
            m_error,
            QStringLiteral("Address is too long (max %1).").arg(SettingsKeys::MaxPharmacyAddress));
        return false;
    }
    if (returnPolicy().length() > SettingsKeys::MaxReturnPolicy) {
        showError(m_error, QStringLiteral("Return policy text is too long (max %1).")
                               .arg(SettingsKeys::MaxReturnPolicy));
        return false;
    }
    showError(m_error, QString());
    return true;
}

// ── LogoPage ─────────────────────────────────────────────────────────────
LogoPage::LogoPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle(QStringLiteral("Logo (optional)"));
    setSubTitle(
        QStringLiteral("Shown on the sign-in screen and the app header. You can skip this."));

    m_preview = new QLabel(this);
    m_preview->setFixedSize(160, 160);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setStyleSheet(
        QStringLiteral("border:1px dashed %1; border-radius:10px; color:%2;")
            .arg(UiUtil::Palette::BorderSubtle.name(), UiUtil::Palette::Muted.name()));
    m_preview->setText(QStringLiteral("No logo"));

    m_caption = new QLabel(QStringLiteral("No file chosen."), this);
    m_caption->setObjectName(QStringLiteral("muted"));

    auto *choose = new QPushButton(QStringLiteral("Choose image…"), this);
    auto *remove = new QPushButton(QStringLiteral("Remove"), this);
    remove->setProperty("variant", "secondary");
    connect(choose, &QPushButton::clicked, this, &LogoPage::chooseLogo);
    connect(remove, &QPushButton::clicked, this, &LogoPage::clearLogo);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);
    btnRow->addWidget(choose);
    btnRow->addWidget(remove);
    btnRow->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addWidget(m_preview, 0, Qt::AlignHCenter);
    layout->addWidget(m_caption, 0, Qt::AlignHCenter);
    layout->addLayout(btnRow);
    layout->addStretch();
}

void LogoPage::chooseLogo()
{
    const QString file
        = QFileDialog::getOpenFileName(this, QStringLiteral("Choose a logo image"), QString(),
                                       QStringLiteral("Images (*.png *.jpg *.jpeg *.svg *.bmp)"));
    if (file.isEmpty()) {
        return;
    }
    QPixmap pm(file);
    if (pm.isNull()) {
        m_caption->setText(QStringLiteral("That file isn't a readable image."));
        return;
    }
    m_sourcePath = file;
    m_preview->setPixmap(
        pm.scaled(m_preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_caption->setText(QFileInfo(file).fileName());
}

void LogoPage::clearLogo()
{
    m_sourcePath.clear();
    m_preview->setPixmap(QPixmap());
    m_preview->setText(QStringLiteral("No logo"));
    m_caption->setText(QStringLiteral("No file chosen."));
}

// ── AdminAccountPage ────────────────────────────────────────────────────────
AdminAccountPage::AdminAccountPage(UserRepository *userRepo, QWidget *parent)
    : QWizardPage(parent), m_userRepo(userRepo)
{
    setTitle(QStringLiteral("Administrator account"));
    setSubTitle(QStringLiteral("You'll sign in with this. The PIN is 6–8 digits."));

    m_fullName = new QLineEdit(this);
    m_fullName->setMaxLength(120);

    m_username = new QLineEdit(this);
    m_username->setMaxLength(60);
    m_username->setPlaceholderText(QStringLiteral("letters, digits, . _ -"));
    m_username->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^[A-Za-z0-9._-]*$")), m_username));

    m_pin = new QLineEdit(this);
    m_pin->setEchoMode(QLineEdit::Password);
    m_pin->setMaxLength(8);
    m_pin->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^\\d*$")), m_pin));

    m_confirm = new QLineEdit(this);
    m_confirm->setEchoMode(QLineEdit::Password);
    m_confirm->setMaxLength(8);
    m_confirm->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^\\d*$")), m_confirm));

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(8);
    form->addRow(QStringLiteral("Full name *"), m_fullName);
    form->addRow(QStringLiteral("Username *"), m_username);
    form->addRow(QStringLiteral("PIN (6–8 digits) *"), m_pin);
    form->addRow(QStringLiteral("Confirm PIN *"), m_confirm);

    m_error = makeErrorLabel(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);
    layout->addLayout(form);
    layout->addWidget(m_error);
}

QString AdminAccountPage::fullName() const
{
    return m_fullName->text().trimmed();
}
QString AdminAccountPage::username() const
{
    return m_username->text().trimmed();
}
QString AdminAccountPage::pin() const
{
    return m_pin->text();
}

bool AdminAccountPage::validatePage()
{
    if (fullName().isEmpty()) {
        showError(m_error, QStringLiteral("Full name is required."));
        return false;
    }
    const QString user = username();
    static const QRegularExpression userRe(QStringLiteral("^[A-Za-z0-9._-]+$"));
    if (!userRe.match(user).hasMatch()) {
        showError(m_error, QStringLiteral("Username may use letters, digits, dot, "
                                          "underscore and hyphen only."));
        return false;
    }
    if (m_userRepo && m_userRepo->usernameExistsCI(user)) {
        showError(m_error, QStringLiteral("That username is already taken."));
        return false;
    }
    if (auto err = PinPolicy::validate(pin())) {
        showError(m_error, *err);
        return false;
    }
    if (pin() != m_confirm->text()) {
        showError(m_error, QStringLiteral("The two PINs don't match."));
        return false;
    }
    showError(m_error, QString());
    return true;
}

// ── ThemeFinishPage ─────────────────────────────────────────────────────────
ThemeFinishPage::ThemeFinishPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle(QStringLiteral("Appearance"));
    setSubTitle(QStringLiteral("Pick a colour theme. (Only the default renders for now; "
                               "the others arrive in a later update.)"));

    m_group = new QButtonGroup(this);
    struct Opt
    {
        const char *label;
        const char *key;
    };
    const Opt opts[] = {
        {"Light", ""},
        {"Dark", "dark"},
    };

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(8);
    int id = 0;
    for (const Opt &o : opts) {
        auto *rb = new QRadioButton(QString::fromLatin1(o.label), this);
        rb->setProperty("themeKey", QString::fromLatin1(o.key));
        if (id == 0) {
            rb->setChecked(true);
        }
        m_group->addButton(rb, id++);
        layout->addWidget(rb);
    }
    layout->addStretch();
}

QString ThemeFinishPage::themeKey() const
{
    if (auto *btn = m_group->checkedButton()) {
        return btn->property("themeKey").toString();
    }
    return QString();
}
