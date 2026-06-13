#include "ui/LoginDialog.h"

#include "Branding.h"
#include "data/SettingsRepository.h"
#include "domain/Bcrypt.h"
#include "domain/SettingsKeys.h"
#include "ui/UiUtil.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

LoginDialog::LoginDialog(UserRepository *users, SettingsRepository *settings, QWidget *parent)
    : QDialog(parent), m_users(users)
{
    setWindowTitle(Branding::productName() + QStringLiteral(" — Sign in"));
    setModal(true);
    setMinimumSize(440, 520);

    // Centered sign-in card.
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("card"));
    card->setFixedWidth(360);
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(24, 24, 24, 24);
    cardLayout->setSpacing(12);

    // Brand: logo from settings if present, else the app icon.
    const QString logoPath = settings->get(SettingsKeys::LogoPath);
    QPixmap pm = logoPath.isEmpty() ? QPixmap(QStringLiteral(":/icon.png")) : QPixmap(logoPath);
    if (!pm.isNull()) {
        auto *logo = new QLabel(card);
        logo->setPixmap(pm.scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        logo->setAlignment(Qt::AlignCenter);
        cardLayout->addWidget(logo, 0, Qt::AlignHCenter);
    }
    auto *title = new QLabel(
        settings->get(SettingsKeys::PharmacyName, QStringLiteral("Pharmacy Management System")),
        card);
    title->setObjectName(QStringLiteral("h2"));
    title->setAlignment(Qt::AlignCenter);
    title->setWordWrap(true); // long pharmacy names wrap instead of being clipped
    cardLayout->addWidget(title);
    auto *sub = new QLabel(QStringLiteral("Sign in to continue"), card);
    sub->setObjectName(QStringLiteral("muted"));
    sub->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(sub);
    cardLayout->addSpacing(8);

    m_username = new QLineEdit(card);
    m_username->setMaxLength(60);
    m_username->setPlaceholderText(QStringLiteral("Username"));
    m_username->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^[A-Za-z0-9._-]*$")), m_username));

    m_pin = new QLineEdit(card);
    m_pin->setEchoMode(QLineEdit::Password);
    m_pin->setMaxLength(8);
    m_pin->setPlaceholderText(QStringLiteral("PIN"));
    m_pin->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^\\d*$")), m_pin));

    cardLayout->addWidget(m_username);
    cardLayout->addWidget(m_pin);

    m_error = new QLabel(card);
    m_error->setWordWrap(true);
    m_error->setStyleSheet(
        QStringLiteral("color:%1; font-weight:600;").arg(UiUtil::Palette::Danger.name()));
    m_error->hide();
    cardLayout->addWidget(m_error);

    auto *signIn = new QPushButton(QStringLiteral("Sign in"), card);
    signIn->setDefault(true);
    signIn->setMinimumHeight(40);
    cardLayout->addWidget(signIn);

    // Center the card in the dialog.
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, 24, 24, 24);
    outer->setSpacing(16);
    outer->addStretch();
    outer->addWidget(card, 0, Qt::AlignHCenter);
    outer->addStretch();

    // Developer credit footer (fixed product credit; the pharmacy's own brand
    // is shown on the card above).
    auto *credit = new QLabel(QStringLiteral("%1 v%2  ·  Developed by %3  ·  %4")
                                  .arg(Branding::productName(), Branding::appVersion(),
                                       Branding::developer(), Branding::developerGithub()),
                              this);
    credit->setObjectName(QStringLiteral("muted"));
    credit->setAlignment(Qt::AlignCenter);
    credit->setWordWrap(true);
    outer->addWidget(credit);

    connect(signIn, &QPushButton::clicked, this, &LoginDialog::attempt);
    connect(m_pin, &QLineEdit::returnPressed, this, &LoginDialog::attempt);
    connect(m_username, &QLineEdit::returnPressed, this, &LoginDialog::attempt);
}

void LoginDialog::attempt()
{
    const QString username = m_username->text().trimmed();

    // Brute-force throttle: refuse to even verify while the account is locked.
    const int wait = m_users->secondsLockedOut(username);
    if (wait > 0) {
        m_error->setText(QStringLiteral("Too many attempts. Try again in %1s.").arg(wait));
        m_error->show();
        m_pin->clear();
        m_pin->setFocus();
        return;
    }

    QString storedHash;
    const UserRecord rec = m_users->findForLogin(username, &storedHash);

    // Generic message on any failure — never disclose which field was wrong.
    if (!rec.valid || !Bcrypt::verify(m_pin->text(), storedHash)) {
        m_users->recordLogin(rec.valid ? rec.id : 0, false, username);
        m_users->registerFailedLogin(username);
        m_error->setText(QStringLiteral("Invalid username or PIN."));
        m_error->show();
        m_pin->clear();
        m_pin->setFocus();
        return;
    }

    m_users->clearFailedLogins(rec.id);
    m_users->touchLastLogin(rec.id);
    m_users->recordLogin(rec.id, true, rec.username);
    m_user = rec;
    accept();
}
