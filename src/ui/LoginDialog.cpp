#include "ui/LoginDialog.h"

#include "Branding.h"
#include "data/SettingsRepository.h"
#include "domain/Bcrypt.h"
#include "domain/SettingsKeys.h"
#include "ui/UiUtil.h"

#include <QDate>
#include <QFormLayout>
#include <QHBoxLayout>
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
    setMinimumSize(860, 560);

    // Brand-panel styling is scoped to this dialog so the teal surface + white
    // text are identical in light and dark themes (it is a brand surface).
    setStyleSheet(QStringLiteral(
        "#authBrand { background-color: #0a7166; }"
        "#authBrandName { color: rgba(255,255,255,0.96); font-size: 14px; font-weight: 600; }"
        "#authHeadline { color: #ffffff; font-size: 27px; font-weight: 800; }"
        "#authFeature { color: rgba(255,255,255,0.84); font-size: 13px; }"
        "#authFoot { color: rgba(255,255,255,0.62); font-size: 12px; }"));

    const QString pharmacyName
        = settings->get(SettingsKeys::PharmacyName, QStringLiteral("Pharmacy"));

    // ── Left: teal brand panel ───────────────────────────────────────────────
    auto *brand = new QFrame(this);
    brand->setObjectName(QStringLiteral("authBrand"));
    brand->setAttribute(Qt::WA_StyledBackground, true);
    auto *bl = new QVBoxLayout(brand);
    bl->setContentsMargins(40, 40, 40, 36);
    bl->setSpacing(0);

    auto *brandTop = new QHBoxLayout;
    brandTop->setSpacing(12);
    const QString logoPath = settings->get(SettingsKeys::LogoPath);
    QPixmap pm = logoPath.isEmpty() ? QPixmap(QStringLiteral(":/icon.png")) : QPixmap(logoPath);
    if (!pm.isNull()) {
        auto *logo = new QLabel(brand);
        logo->setPixmap(pm.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        brandTop->addWidget(logo, 0, Qt::AlignVCenter);
    }
    auto *bname = new QLabel(pharmacyName, brand);
    bname->setObjectName(QStringLiteral("authBrandName"));
    bname->setWordWrap(true);
    brandTop->addWidget(bname, 1, Qt::AlignVCenter);
    bl->addLayout(brandTop);
    bl->addStretch();

    auto *headline
        = new QLabel(QStringLiteral("Pharmacy operations,\nprofessionally managed."), brand);
    headline->setObjectName(QStringLiteral("authHeadline"));
    headline->setWordWrap(true);
    bl->addWidget(headline);
    auto *feature = new QLabel(
        QStringLiteral("Point-of-sale, FEFO batch dispensing, returns adjudication, and a "
                       "tamper-evident audit trail — fully offline, single deployment."),
        brand);
    feature->setObjectName(QStringLiteral("authFeature"));
    feature->setWordWrap(true);
    bl->addSpacing(14);
    bl->addWidget(feature);
    bl->addStretch();

    auto *foot = new QLabel(QStringLiteral("© %1 %2. All rights reserved.")
                                .arg(QDate::currentDate().year())
                                .arg(pharmacyName),
                            brand);
    foot->setObjectName(QStringLiteral("authFoot"));
    foot->setWordWrap(true);
    bl->addWidget(foot);

    // ── Right: sign-in form ──────────────────────────────────────────────────
    auto *formSide = new QWidget(this);
    auto *fsLayout = new QVBoxLayout(formSide);
    fsLayout->setContentsMargins(56, 40, 56, 28);
    fsLayout->setSpacing(0);
    fsLayout->addStretch();

    auto *col = new QWidget(formSide);
    col->setMaximumWidth(380);
    auto *cardLayout = new QVBoxLayout(col);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Sign in"), col);
    title->setObjectName(QStringLiteral("h1"));
    cardLayout->addWidget(title);
    auto *sub = new QLabel(QStringLiteral("Enter your username and PIN to continue."), col);
    sub->setObjectName(QStringLiteral("muted"));
    sub->setWordWrap(true);
    cardLayout->addWidget(sub);
    cardLayout->addSpacing(10);

    m_username = new QLineEdit(col);
    m_username->setMaxLength(60);
    m_username->setMinimumHeight(40);
    m_username->setPlaceholderText(QStringLiteral("Username"));
    m_username->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^[A-Za-z0-9._-]*$")), m_username));

    m_pin = new QLineEdit(col);
    m_pin->setEchoMode(QLineEdit::Password);
    m_pin->setMaxLength(8);
    m_pin->setMinimumHeight(40);
    m_pin->setPlaceholderText(QStringLiteral("PIN"));
    m_pin->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^\\d*$")), m_pin));

    cardLayout->addWidget(m_username);
    cardLayout->addWidget(m_pin);

    m_error = new QLabel(col);
    m_error->setWordWrap(true);
    m_error->setStyleSheet(
        QStringLiteral("color:%1; font-weight:600;").arg(UiUtil::Palette::Danger.name()));
    m_error->hide();
    cardLayout->addWidget(m_error);

    auto *signIn = new QPushButton(QStringLiteral("Sign in"), col);
    signIn->setDefault(true);
    signIn->setMinimumHeight(44);
    cardLayout->addSpacing(4);
    cardLayout->addWidget(signIn);

    fsLayout->addWidget(col, 0, Qt::AlignHCenter);
    fsLayout->addStretch();

    // Developer credit footer (fixed product credit; the pharmacy's own brand is
    // shown on the left panel).
    auto *credit = new QLabel(QStringLiteral("%1 v%2  ·  Developed by %3  ·  %4")
                                  .arg(Branding::productName(), Branding::appVersion(),
                                       Branding::developer(), Branding::developerGithub()),
                              formSide);
    credit->setObjectName(QStringLiteral("muted"));
    credit->setAlignment(Qt::AlignCenter);
    credit->setWordWrap(true);
    fsLayout->addWidget(credit);

    // ── Assemble: brand (left) | form (right) ────────────────────────────────
    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(brand, 5);
    outer->addWidget(formSide, 6);

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
