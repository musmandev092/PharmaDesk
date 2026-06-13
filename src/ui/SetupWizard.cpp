#include "ui/SetupWizard.h"

#include "Branding.h"
#include "data/Database.h"
#include "data/SettingsRepository.h"
#include "data/UserRepository.h"
#include "domain/AppPaths.h"
#include "domain/Bcrypt.h"
#include "domain/SettingsKeys.h"
#include "ui/setup/SetupPages.h"

#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>

SetupWizard::SetupWizard(Database *db, SettingsRepository *settings, UserRepository *users,
                         QWidget *parent)
    : QWizard(parent), m_db(db), m_settings(settings), m_users(users)
{
    setWindowTitle(Branding::productName() + QStringLiteral(" — Setup"));
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoBackButtonOnStartPage, true);
    setMinimumSize(560, 520);

    addPage(new WelcomePage(this));
    m_identity = new PharmacyIdentityPage(this);
    m_logo = new LogoPage(this);
    m_admin = new AdminAccountPage(m_users, this);
    m_theme = new ThemeFinishPage(this);
    addPage(m_identity);
    addPage(m_logo);
    addPage(m_admin);
    addPage(m_theme);

    // Keep the nav buttons consistent with the rest of the app: the affirmative
    // Next/Finish stays primary (teal); Back/Cancel get the secondary look.
    auto secondary = [this](QWizard::WizardButton which) {
        if (auto *b = qobject_cast<QPushButton *>(button(which))) {
            b->setProperty("variant", "secondary");
            b->style()->unpolish(b);
            b->style()->polish(b);
        }
    };
    secondary(QWizard::BackButton);
    secondary(QWizard::CancelButton);

    // Order the nav buttons left-to-right: [stretch] Back, Cancel, then the
    // affirmative Next/Finish as the right-most (primary) button.
    setButtonLayout({QWizard::Stretch, QWizard::BackButton, QWizard::CancelButton,
                     QWizard::NextButton, QWizard::FinishButton});
}

QString SetupWizard::persistLogo() const
{
    const QString src = m_logo->sourcePath();
    if (src.isEmpty()) {
        return QString();
    }
    const QString ext = QFileInfo(src).suffix().toLower();
    const QString dest = AppPaths::logosDir() + QStringLiteral("/logo.")
                         + (ext.isEmpty() ? QStringLiteral("png") : ext);
    QFile::remove(dest); // overwrite any prior logo
    if (!QFile::copy(src, dest)) {
        return QString();
    }
    return dest;
}

void SetupWizard::accept()
{
    // 1) Logo file copy (filesystem isn't transactional; do it first and only
    //    persist its path if it succeeded).
    const QString logoPath = persistLogo();
    if (!m_logo->sourcePath().isEmpty() && logoPath.isEmpty()) {
        QMessageBox::warning(this, windowTitle(),
                             QStringLiteral("Could not save the logo image; "
                                            "continuing without it."));
    }

    // 2) Write settings first. They use a fallback in receipts anyway, and
    //    because the ADMIN user is the first-run "commit point", if anything
    //    below fails the wizard re-runs and these upserts simply overwrite.
    QMap<QString, QString> s;
    s.insert(SettingsKeys::PharmacyName, m_identity->name());
    s.insert(SettingsKeys::PharmacyAddress, m_identity->address());
    s.insert(SettingsKeys::PharmacyPhone, m_identity->phone());
    s.insert(SettingsKeys::PharmacyNtn, m_identity->ntn());
    s.insert(SettingsKeys::ReturnPolicyText, m_identity->returnPolicy());
    s.insert(SettingsKeys::ReceiptFooter, m_identity->receiptFooter());
    s.insert(SettingsKeys::LogoPath, logoPath);
    s.insert(SettingsKeys::Theme, m_theme->themeKey());
    if (!m_settings->setMany(s, -1)) {
        QMessageBox::critical(
            this, windowTitle(),
            QStringLiteral("Could not save settings:\n%1").arg(m_settings->errorString()));
        return; // keep the wizard open
    }

    // 3) Create the admin user (atomic: branch id=1 + user + pin_history).
    const QString pinHash = Bcrypt::hash(m_admin->pin());
    if (pinHash.isEmpty()) {
        QMessageBox::critical(this, windowTitle(),
                              QStringLiteral("Could not secure the PIN. Please try again."));
        return;
    }
    AdminDraft draft;
    draft.fullName = m_admin->fullName();
    draft.username = m_admin->username();
    draft.pinHash = pinHash;
    draft.pharmacyName = m_identity->name();

    const qint64 id = m_users->createAdmin(draft);
    if (id < 0) {
        QMessageBox::critical(
            this, windowTitle(),
            QStringLiteral("Could not create the administrator:\n%1").arg(m_users->errorString()));
        return; // keep the wizard open
    }

    QWizard::accept();
}
