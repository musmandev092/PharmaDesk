#pragma once

#include <QWizard>

class Database;
class SettingsRepository;
class UserRepository;
class PharmacyIdentityPage;
class LogoPage;
class AdminAccountPage;
class ThemeFinishPage;

// First-run setup wizard. Collects pharmacy identity, logo, admin account, and
// theme, then commits everything in accept(). Borrows the repositories (not
// owned); they outlive the wizard.
class SetupWizard : public QWizard
{
    Q_OBJECT
public:
    SetupWizard(Database *db, SettingsRepository *settings, UserRepository *users,
                QWidget *parent = nullptr);

    // Override: validate already happened per-page; this is the single commit
    // point (logo copy + settings + admin user). Only calls QWizard::accept()
    // after a successful commit.
    void accept() override;

private:
    Database *m_db;
    SettingsRepository *m_settings;
    UserRepository *m_users;

    PharmacyIdentityPage *m_identity = nullptr;
    LogoPage *m_logo = nullptr;
    AdminAccountPage *m_admin = nullptr;
    ThemeFinishPage *m_theme = nullptr;

    // Copies the chosen logo into the app-data logos/ dir. Returns the stored
    // path (empty if none/failed).
    QString persistLogo() const;
};
