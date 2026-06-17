#include "Branding.h"
#include "MainWindow.h"
#include "data/Database.h"
#include "domain/AppPaths.h"
#include "domain/DesktopIntegration.h"
#include "domain/Log.h"
#include "data/SettingsRepository.h"
#include "data/UserRepository.h"
#include "domain/PinPolicy.h"
#include "domain/SettingsKeys.h"
#include "ui/ChangePinDialog.h"
#include "data/Audit.h"
#include "domain/Licensing.h"
#include "ui/ActivationDialog.h"
#include "ui/LoginDialog.h"
#include "ui/SetupWizard.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QIcon>
#include <QLockFile>
#include <QMessageBox>

#include <exception>

int main(int argc, char *argv[])
{
    // Cursor size fix: on a Wayland session this app runs through XWayland (the
    // bundled Qt ships only the xcb platform plugin). XWayland doesn't inherit the
    // compositor's cursor size, so without XCURSOR_SIZE the pointer renders
    // oversized over the app's windows while staying normal over the desktop.
    // Pin a sane default; only when unset, so an explicit user/HiDPI value wins.
    if (qEnvironmentVariableIsEmpty("XCURSOR_SIZE")) {
        qputenv("XCURSOR_SIZE", QByteArrayLiteral("24"));
    }

    QApplication app(argc, argv);
    app.setApplicationName(Branding::productName());
    app.setOrganizationName(Branding::organizationName());
    // NB: deliberately NOT setting applicationDisplayName — Qt would append it to
    // every window title ("Section — PharmaDesk"). The title bar stays a steady
    // "PharmaDesk" (set once in MainWindow).
    app.setWindowIcon(QIcon(QStringLiteral(":/icon.png")));
    // Associate windows with the installed .desktop entry so the dock/taskbar
    // shows the app icon (GNOME/Wayland) once integrated as an AppImage.
    app.setDesktopFileName(Branding::desktopId());

    AppPaths::ensureDirs();
    Log::init();

    // Crash safety: log the offending exception before the process dies, so a
    // field failure leaves a trace instead of vanishing silently.
    std::set_terminate([] {
        QString what = QStringLiteral("unknown");
        if (std::exception_ptr e = std::current_exception()) {
            try {
                std::rethrow_exception(e);
            } catch (const std::exception &ex) {
                what = QString::fromUtf8(ex.what());
            } catch (...) {
            }
        }
        Log::fatal(QStringLiteral("std::terminate — ") + what);
        std::abort();
    });

    // Single-instance guard: two copies sharing one SQLite file risks corruption.
    QLockFile lock(AppPaths::dbFilePath() + QStringLiteral(".lock"));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(200)) {
        QMessageBox::warning(nullptr, Branding::productName(),
                             QStringLiteral("%1 is already running.").arg(Branding::productName()));
        return 0;
    }

    Database db;
    if (!db.open()) {
        QMessageBox::critical(
            nullptr, Branding::productName(),
            QStringLiteral("Could not open the database:\n%1").arg(db.errorString()));
        return 1;
    }
    if (db.isFreshDb() && !db.bootstrap()) {
        QMessageBox::critical(
            nullptr, Branding::productName(),
            QStringLiteral("Could not initialise the database:\n%1").arg(db.errorString()));
        return 1;
    }
    // Bring an existing DB up to the latest schema (no-op on a fresh baseline).
    if (!db.migrate()) {
        QMessageBox::critical(
            nullptr, Branding::productName(),
            QStringLiteral("Could not upgrade the database:\n%1").arg(db.errorString()));
        return 1;
    }
    // Lock down the data dir + DB to the owner (local PIN hashes, audit trail).
    AppPaths::hardenPermissions();

    SettingsRepository settings(db.handle());
    UserRepository users(db.handle());
    Theme::apply(settings.get(SettingsKeys::Theme));

    // Offline, node-locked activation. Gated by Licensing::enforced(): only a
    // build with an embedded public key AND release/launcher opt-in enforces it, so
    // dev/CI/tests/screenshots are never blocked. Block here until a valid license
    // for THIS machine is installed (or the user quits).
    if (Licensing::enforced() && Licensing::check().state != QLatin1String("ok")) {
        ActivationDialog activation(Licensing::check().state);
        if (activation.exec() != QDialog::Accepted
            || Licensing::check().state != QLatin1String("ok")) {
            return 0;
        }
        Audit::write(db.handle(), 0, QStringLiteral("LICENSE_ACTIVATED"), QStringLiteral("system"),
                     0, QString(), Licensing::currentCode());
    }

    // Install the desktop menu entry + icon on AppImage launch and note whether
    // this is a first install or an upgrade (no-op outside an AppImage). The
    // one-line notice is shown once the app is otherwise ready, below.
    const QString installNotice = DesktopIntegration::installIfAppImage(settings);

    // First run: collect everything via the wizard.
    if (db.isFirstRun()) {
        SetupWizard wizard(&db, &settings, &users);
        if (wizard.exec() != QDialog::Accepted) {
            return 0;
        }
        Theme::apply(settings.get(SettingsKeys::Theme));
    }

    // Announce the install/upgrade once (after any first-run wizard).
    if (!installNotice.isEmpty()) {
        QMessageBox::information(nullptr, Branding::productName(), installNotice);
    }

    // Session loop: login → (forced PIN rotation if due) → main window. Signing
    // out returns to login without quitting; closing the window exits the app.
    try {
        forever
        {
            UserRecord current;
            {
                LoginDialog login(&users, &settings);
                if (login.exec() != QDialog::Accepted) {
                    return 0;
                }
                current = login.user();
            }

            // Enforce PIN rotation (admin-forced, or older than the 90-day policy).
            const bool due
                = current.mustRotatePin || users.pinAgeDays(current.id) >= PinPolicy::RotationDays;
            if (due) {
                ChangePinDialog cp(&users, current.id);
                cp.setMandatory(true);
                if (cp.exec() != QDialog::Accepted) {
                    continue; // refused → back to login, app stays up
                }
            }

            MainWindow w(db.handle(), current);
            bool signedOut = false;
            QObject::connect(&w, &MainWindow::signedOut, &app, [&]() {
                signedOut = true;
                w.close();
            });
            // Maximize so the app fills whatever display it lands on — a small laptop
            // panel or a 4K monitor — rather than a fixed window that overflows small
            // screens or sits tiny in the corner of large ones.
            w.showMaximized();
            app.exec();
            if (!signedOut) {
                break; // window closed by the user → exit
            }
        }
    } catch (const std::exception &e) {
        Log::fatal(QStringLiteral("unhandled exception — ") + QString::fromUtf8(e.what()));
        QMessageBox::critical(
            nullptr, Branding::productName(),
            QStringLiteral("An unexpected error occurred and %1 must close:\n\n%2\n\nDetails were "
                           "written to:\n%3")
                .arg(Branding::productName(), QString::fromUtf8(e.what()), Log::filePath()));
        return 1;
    }
    return 0;
}
