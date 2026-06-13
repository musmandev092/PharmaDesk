#include "MainWindow.h"

#include "Branding.h"
#include "data/SettingsRepository.h"
#include "domain/SettingsKeys.h"
#include "service/BackupService.h"
#include "ui/AdminPage.h"
#include "ui/DashboardPage.h"
#include "ui/InventoryPage.h"
#include "ui/MedicinesPage.h"
#include "ui/PosTerminalPage.h"
#include "ui/PurchasingPage.h"
#include "ui/ReportsPage.h"
#include "ui/ReturnsPage.h"
#include "ui/SessionsPage.h"

#include <QActionGroup>
#include <QDateTime>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPixmap>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

MainWindow::MainWindow(QSqlDatabase db, const UserRecord &user, QWidget *parent)
    : QMainWindow(parent), m_db(std::move(db)), m_user(user)
{
    // Usable floor for small laptop panels; restore size when un-maximized.
    setMinimumSize(1024, 640);
    resize(1360, 860);

    m_stack = new QStackedWidget(this);
    auto *dashboard = new DashboardPage(m_db, this);
    auto *pos = new PosTerminalPage(m_db, m_user.id, this);
    auto *meds = new MedicinesPage(m_db, m_user.id, this);
    auto *inventory = new InventoryPage(m_db, m_user.id, this);
    auto *purchasing = new PurchasingPage(m_db, m_user.id, this);
    auto *reports = new ReportsPage(m_db, this);
    auto *returns = new ReturnsPage(m_db, m_user.id, this);
    auto *sessions = new SessionsPage(m_db, m_user.id, this);
    auto *admin = new AdminPage(m_db, m_user, this);
    m_stack->addWidget(dashboard);
    m_stack->addWidget(pos);
    m_stack->addWidget(meds);
    m_stack->addWidget(inventory);
    m_stack->addWidget(purchasing);
    m_stack->addWidget(reports);
    m_stack->addWidget(returns);
    m_stack->addWidget(sessions);
    m_stack->addWidget(admin);
    setCentralWidget(m_stack);

    // ── Branded top navigation ─────────────────────────────────────────────
    auto *nav = addToolBar(QStringLiteral("Navigation"));
    nav->setMovable(false);
    nav->setToolButtonStyle(Qt::ToolButtonTextOnly);

    // Header brand: the operator's uploaded logo + their pharmacy name (set in
    // Setup / Admin → Pharmacy). Falls back to the bundled app icon + product name.
    SettingsRepository brandSettings(m_db);
    const QString pharmacyName
        = brandSettings.get(SettingsKeys::PharmacyName, Branding::productName());
    const QString logoPath = brandSettings.get(SettingsKeys::LogoPath);

    auto *brandWidget = new QWidget(this);
    auto *brandLayout = new QHBoxLayout(brandWidget);
    // Comfortable padding around the brand block, with extra room on the right to
    // separate it from the nav actions.
    brandLayout->setContentsMargins(12, 4, 24, 4);
    brandLayout->setSpacing(8);

    QPixmap logo = logoPath.isEmpty() ? QPixmap() : QPixmap(logoPath);
    if (logo.isNull()) {
        logo = QPixmap(QStringLiteral(":/icon.png"));
    }
    if (!logo.isNull()) {
        auto *logoLabel = new QLabel(brandWidget);
        // Scale to a consistent ~28px height with smooth transform so the logo
        // stays crisp regardless of the uploaded source size.
        logoLabel->setPixmap(logo.scaledToHeight(28, Qt::SmoothTransformation));
        brandLayout->addWidget(logoLabel);
    }
    auto *brand = new QLabel(pharmacyName, brandWidget);
    brand->setObjectName(QStringLiteral("brand"));
    brandLayout->addWidget(brand);
    nav->addWidget(brandWidget);

    // Role gating mirrors the PHP layout: cashiers see only the primary links;
    // the Inventory/Reports groups are manager+, and Admin is admin-only.
    const bool isMgr
        = (m_user.role == QLatin1String("MANAGER") || m_user.role == QLatin1String("ADMIN"));
    const bool isAdmin = (m_user.role == QLatin1String("ADMIN"));

    auto *group = new QActionGroup(this);
    group->setExclusive(true);
    QHash<QString, QAction *> actionByTitle;

    // Switches the central stack to `page`, refreshing it first, and retitles the
    // window. Shared by flat nav buttons and dropdown-group menu items.
    // title is used by callers for the actionByTitle map; showPage itself no
    // longer sets a per-section window title (the bar stays "PharmaDesk").
    auto showPage = [this](QWidget *page, const QString & /*title*/) {
        if (auto *p = qobject_cast<DashboardPage *>(page))
            p->reload();
        else if (auto *p = qobject_cast<MedicinesPage *>(page))
            p->reload();
        else if (auto *p = qobject_cast<InventoryPage *>(page))
            p->reload();
        else if (auto *p = qobject_cast<PurchasingPage *>(page))
            p->reload();
        else if (auto *p = qobject_cast<ReportsPage *>(page))
            p->reload();
        else if (auto *p = qobject_cast<ReturnsPage *>(page))
            p->reload();
        else if (auto *p = qobject_cast<SessionsPage *>(page))
            p->reload();
        else if (auto *p = qobject_cast<AdminPage *>(page))
            p->reload();
        m_stack->setCurrentWidget(page);
    };

    // A checkable flat nav button bound to a page.
    auto addFlat = [&](const QString &label, QWidget *page, const QString &title,
                       const QString &shortcut) -> QAction * {
        QAction *a = nav->addAction(label);
        a->setCheckable(true);
        if (!shortcut.isEmpty()) {
            a->setShortcut(QKeySequence(shortcut));
        }
        group->addAction(a);
        actionByTitle.insert(title, a);
        connect(a, &QAction::triggered, this, [showPage, page, title]() { showPage(page, title); });
        return a;
    };

    // Primary nav — everyone (cashier+). Mirrors the PHP $primary set.
    QAction *dashAction = addFlat(QStringLiteral("Dashboard"), dashboard,
                                  QStringLiteral("Dashboard"), QStringLiteral("Ctrl+1"));
    dashAction->setChecked(true);
    addFlat(QStringLiteral("POS"), pos, QStringLiteral("Point of sale"), QStringLiteral("Ctrl+2"));
    addFlat(QStringLiteral("Returns"), returns, QStringLiteral("Returns & refunds"),
            QStringLiteral("Ctrl+3"));
    addFlat(QStringLiteral("Sessions"), sessions, QStringLiteral("Cashier sessions"),
            QStringLiteral("Ctrl+4"));

    // Inventory dropdown group (manager+) — mirrors the PHP $groupInventory menu.
    if (isMgr) {
        auto *invBtn = new QToolButton(this);
        // Chevron is part of the label; the native menu-indicator is hidden in
        // the stylesheet (it rendered as a stray triangle under the text).
        invBtn->setText(QStringLiteral("Inventory  ▾"));
        invBtn->setPopupMode(QToolButton::InstantPopup);
        invBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        auto *invMenu = new QMenu(invBtn);
        auto addGrouped = [&](const QString &label, QWidget *page, const QString &title) {
            QAction *a = invMenu->addAction(label);
            a->setCheckable(true);
            group->addAction(a);
            actionByTitle.insert(title, a);
            connect(a, &QAction::triggered, this,
                    [showPage, page, title]() { showPage(page, title); });
        };
        addGrouped(QStringLiteral("Medicines"), meds, QStringLiteral("Medicines"));
        addGrouped(QStringLiteral("Inventory"), inventory, QStringLiteral("Inventory"));
        addGrouped(QStringLiteral("Purchasing"), purchasing, QStringLiteral("Purchasing"));
        invBtn->setMenu(invMenu);
        nav->addWidget(invBtn);
    }

    // Reports (manager+) and Admin (admin only) are single consolidated pages.
    if (isMgr) {
        addFlat(QStringLiteral("Reports"), reports, QStringLiteral("Reports"),
                QStringLiteral("Ctrl+5"));
    }
    if (isAdmin) {
        addFlat(QStringLiteral("Admin"), admin, QStringLiteral("Admin & settings"),
                QStringLiteral("Ctrl+6"));
    }

    // Dashboard quick-action tiles trigger the matching nav action (only those the
    // user can reach; gated tiles map to no action and no-op).
    connect(dashboard, &DashboardPage::openSection, this, [actionByTitle](const QString &title) {
        if (QAction *a = actionByTitle.value(title)) {
            a->trigger();
        }
    });

    // Right-aligned user box + Sign out.
    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    nav->addWidget(spacer);

    // Muted secondary text: the signed-in identity is context, not a control.
    // #userbox is purpose-themed (muted colour, small weight) in theme.qss.
    auto *userBox = new QLabel(QStringLiteral("%1  ·  %2").arg(m_user.fullName, m_user.role), this);
    userBox->setObjectName(QStringLiteral("userbox"));
    userBox->setContentsMargins(0, 0, 16, 0);
    nav->addWidget(userBox);

    // Sign out stays a low-emphasis toolbar control. Unlike the nav actions it is
    // never "checked", so the toolbar's default subtle (muted, transparent) style
    // keeps it visually secondary rather than primary-coloured.
    QAction *signOut = nav->addAction(QStringLiteral("Sign out"));
    connect(signOut, &QAction::triggered, this, [this]() { emit signedOut(); });

    // Steady product name in the title bar (never per-section).
    setWindowTitle(Branding::productName());

    // No persistent footer/status bar — it just showed a stray "Ready". Keep the
    // chrome clean; transient notices (e.g. a completed backup) flash briefly.
    statusBar()->setSizeGripEnabled(false);
    statusBar()->hide();

    // Run a due auto-backup on startup (off-machine copy of the DB).
    SettingsRepository settings(m_db);
    BackupService backup(m_db);
    const QDateTime now = QDateTime::currentDateTime();
    if (backup.autoBackupIfDue(settings, now.toString(Qt::ISODate),
                               now.toString(QStringLiteral("yyyyMMdd_HHmmss")))) {
        statusBar()->show();
        statusBar()->showMessage(QStringLiteral("Auto-backup completed."), 5000);
    }
}
