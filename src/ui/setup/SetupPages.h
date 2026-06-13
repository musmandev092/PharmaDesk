#pragma once

#include <QWizardPage>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QButtonGroup;
class UserRepository;

// ── Welcome ──────────────────────────────────────────────────────────────
class WelcomePage : public QWizardPage
{
    Q_OBJECT
public:
    explicit WelcomePage(QWidget *parent = nullptr);
};

// ── Pharmacy identity → settings ─────────────────────────────────────────
class PharmacyIdentityPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit PharmacyIdentityPage(QWidget *parent = nullptr);
    bool validatePage() override;

    QString name() const;
    QString address() const;
    QString phone() const;
    QString ntn() const;
    QString returnPolicy() const;
    QString receiptFooter() const;

private:
    QLineEdit *m_name = nullptr;
    QPlainTextEdit *m_address = nullptr;
    QLineEdit *m_phone = nullptr;
    QLineEdit *m_ntn = nullptr;
    QPlainTextEdit *m_policy = nullptr;
    QLineEdit *m_footer = nullptr;
    QLabel *m_error = nullptr;
};

// ── Logo (optional) ──────────────────────────────────────────────────────
class LogoPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit LogoPage(QWidget *parent = nullptr);

    // Source path of the picked image, or empty if skipped.
    QString sourcePath() const { return m_sourcePath; }

private slots:
    void chooseLogo();
    void clearLogo();

private:
    QString m_sourcePath;
    QLabel *m_preview = nullptr;
    QLabel *m_caption = nullptr;
};

// ── Admin account ─────────────────────────────────────────────────────────
class AdminAccountPage : public QWizardPage
{
    Q_OBJECT
public:
    // userRepo is borrowed (not owned) for the live username uniqueness check.
    explicit AdminAccountPage(UserRepository *userRepo, QWidget *parent = nullptr);
    bool validatePage() override;

    QString fullName() const;
    QString username() const;
    QString pin() const;

private:
    UserRepository *m_userRepo = nullptr;
    QLineEdit *m_fullName = nullptr;
    QLineEdit *m_username = nullptr;
    QLineEdit *m_pin = nullptr;
    QLineEdit *m_confirm = nullptr;
    QLabel *m_error = nullptr;
};

// ── Theme + finish/review ─────────────────────────────────────────────────
class ThemeFinishPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit ThemeFinishPage(QWidget *parent = nullptr);

    // Stored theme key: "" (default teal) or "1".."6".
    QString themeKey() const;

private:
    QButtonGroup *m_group = nullptr;
};
