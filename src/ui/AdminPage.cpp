#include "ui/AdminPage.h"

#include "Branding.h"
#include "domain/Licensing.h"

#include "data/AuditRepository.h"
#include "data/EscPosRenderer.h"
#include "data/SettingsRepository.h"
#include "domain/SettingsKeys.h"
#include "service/BackupService.h"
#include "service/CupsRawPrinter.h"
#include "service/CupsStatus.h"
#include "ui/ChangePinDialog.h"
#include "ui/Theme.h"
#include "ui/UiUtil.h"
#include "ui/UserDialog.h"

#include "domain/AppPaths.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <utility>

namespace {
// Card/header/form factories shared by every Admin tab — keep the look
// consistent. None capture state, so they live at file scope (previously
// constructor-local lambdas).
std::pair<QFrame *, QVBoxLayout *> makeCard(QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("card"));
    auto *box = new QVBoxLayout(card);
    box->setContentsMargins(20, 16, 20, 16);
    box->setSpacing(12);
    return std::make_pair(card, box);
}

QVBoxLayout *makeCardHeader(QWidget *parent, const QString &title, const QString &desc)
{
    auto *col = new QVBoxLayout;
    col->setSpacing(2);
    auto *h = new QLabel(title, parent);
    h->setObjectName(QStringLiteral("h2"));
    col->addWidget(h);
    if (!desc.isEmpty()) {
        auto *d = new QLabel(desc, parent);
        d->setObjectName(QStringLiteral("muted"));
        d->setWordWrap(true);
        col->addWidget(d);
    }
    return col;
}

QFormLayout *makeForm()
{
    auto *f = new QFormLayout;
    f->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    f->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    f->setHorizontalSpacing(16);
    f->setVerticalSpacing(10);
    f->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    f->setContentsMargins(0, 0, 0, 0);
    return f;
}
} // namespace

AdminPage::AdminPage(QSqlDatabase db, const UserRecord &user, QWidget *parent)
    : QWidget(parent), m_db(std::move(db)), m_user(user),
      m_isAdmin(user.role == QLatin1String("ADMIN"))
{
    // ── Page title row ─────────────────────────────────────────────────────
    auto *title = new QLabel(QStringLiteral("Admin & settings"), this);
    title->setObjectName(QStringLiteral("h1"));
    auto *subtitle = new QLabel(
        QStringLiteral("Pharmacy details, staff accounts, audit trail and backups."), this);
    subtitle->setObjectName(QStringLiteral("muted"));

    auto *changePinBtn = new QPushButton(QStringLiteral("Change my PIN"), this);
    changePinBtn->setProperty("variant", "secondary");
    connect(changePinBtn, &QPushButton::clicked, this, &AdminPage::changeMyPin);

    auto *titleCol = new QVBoxLayout;
    titleCol->setSpacing(2);
    titleCol->addWidget(title);
    titleCol->addWidget(subtitle);

    auto *topRow = new QHBoxLayout;
    topRow->addLayout(titleCol);
    topRow->addStretch();
    topRow->addWidget(changePinBtn, 0, Qt::AlignTop);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildPharmacyTab(), QStringLiteral("Pharmacy"));
    tabs->addTab(buildPrinterTab(), QStringLiteral("Printer"));
    tabs->addTab(buildUsersTab(), QStringLiteral("Users"));
    tabs->addTab(buildAuditTab(), QStringLiteral("Audit log"));
    tabs->addTab(buildBackupTab(), QStringLiteral("Backup"));
    tabs->addTab(buildAboutTab(), QStringLiteral("About"));

    // ── Full-width scroll wrapper ───────────────────────────────────────────
    // The whole page scrolls on short screens and uses the FULL viewport width
    // on wide/4K screens — cards and the Users/Audit tables span edge to edge.
    // Individual text inputs keep their own max-width (560px) so they stay
    // readable instead of stretching the whole way across a 4K panel.
    auto *content = new QWidget(this);
    content->setObjectName(QStringLiteral("scrollContent"));
    content->setAutoFillBackground(false);

    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addLayout(topRow);
    layout->addWidget(tabs, 1);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("scrollArea"));
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setStyleSheet(QStringLiteral("QScrollArea#scrollArea, QWidget#scrollContent { "
                                         "background: transparent; border: none; }"));
    scroll->viewport()->setAutoFillBackground(false);

    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    pageLayout->addWidget(scroll);

    loadBackupSettings();
    reload();
}

// ── About tab (fixed product/developer credit + licensing status) ───────
QWidget *AdminPage::buildAboutTab()
{
    auto *tab = new QWidget;
    auto *outer = new QVBoxLayout(tab);
    outer->setContentsMargins(24, 24, 24, 24);
    outer->setSpacing(16);

    // Product + developer credit.
    auto [card, body] = makeCard(tab);
    body->addLayout(makeCardHeader(card, QStringLiteral("About %1").arg(Branding::productName()),
                                   Branding::productTagline()));
    auto *form = makeForm();
    form->addRow(
        QStringLiteral("Product"),
        new QLabel(Branding::productName() + QStringLiteral(" v") + Branding::appVersion(), card));
    form->addRow(QStringLiteral("Developer"), new QLabel(Branding::developer(), card));
    auto *gh = new QLabel(Branding::developerGithub(), card);
    gh->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(QStringLiteral("GitHub"), gh);
    auto *contact = new QLabel(Branding::developerEmails().join(QStringLiteral("  ·  ")), card);
    contact->setTextInteractionFlags(Qt::TextSelectableByMouse);
    contact->setWordWrap(true);
    form->addRow(QStringLiteral("Contact"), contact);
    body->addLayout(form);
    outer->addWidget(card);

    // Licensing / activation status.
    auto [lcard, lbody] = makeCard(tab);
    lbody->addLayout(makeCardHeader(lcard, QStringLiteral("Activation"),
                                    QStringLiteral("Offline, node-locked license for this PC.")));
    auto *lform = makeForm();
    auto *codeLbl = new QLabel(Licensing::currentCode(), lcard);
    codeLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lform->addRow(QStringLiteral("This computer's code"), codeLbl);
    if (Licensing::configured()) {
        const Licensing::CheckResult lc = Licensing::check();
        QString status = lc.state == QLatin1String("ok")
                             ? QStringLiteral("Licensed")
                             : QStringLiteral("Not activated (%1)").arg(lc.state);
        const QString licensee = lc.payload.value(QStringLiteral("licensee")).toString();
        const QString expiry = lc.payload.value(QStringLiteral("expiry")).toString();
        if (!licensee.isEmpty()) {
            status += QStringLiteral(" — %1").arg(licensee);
        }
        if (!expiry.isEmpty()) {
            status += QStringLiteral(" (expires %1)").arg(expiry);
        }
        lform->addRow(QStringLiteral("Status"), new QLabel(status, lcard));
    } else {
        lform->addRow(QStringLiteral("Status"),
                      new QLabel(QStringLiteral("Not enforced in this build."), lcard));
    }
    lbody->addLayout(lform);
    outer->addWidget(lcard);
    outer->addStretch();
    return tab;
}

// ── Pharmacy tab (identity + theme + logo; editable any time) ──────────
QWidget *AdminPage::buildPharmacyTab()
{
    auto *phTab = new QWidget;
    m_phName = new QLineEdit(phTab);
    m_phName->setMaxLength(160);
    m_phName->setMinimumWidth(280);
    m_phName->setMaximumWidth(560);
    m_phAddress = new QPlainTextEdit(phTab);
    m_phAddress->setFixedHeight(56);
    m_phAddress->setMinimumWidth(280);
    m_phAddress->setMaximumWidth(560);
    m_phPhone = new QLineEdit(phTab);
    m_phPhone->setMaxLength(60);
    m_phPhone->setMinimumWidth(280);
    m_phPhone->setMaximumWidth(560);
    m_phNtn = new QLineEdit(phTab);
    m_phNtn->setMaxLength(40);
    m_phNtn->setMinimumWidth(280);
    m_phNtn->setMaximumWidth(560);
    m_phPolicy = new QPlainTextEdit(phTab);
    m_phPolicy->setFixedHeight(56);
    m_phPolicy->setMinimumWidth(280);
    m_phPolicy->setMaximumWidth(560);
    m_phFooter = new QLineEdit(phTab);
    m_phFooter->setMaxLength(200);
    m_phFooter->setMinimumWidth(280);
    m_phFooter->setMaximumWidth(560);
    m_theme = new QComboBox(phTab);
    m_theme->setMinimumWidth(200);
    m_theme->addItem(QStringLiteral("Light"), QString());
    m_theme->addItem(QStringLiteral("Dark"), QStringLiteral("dark"));
    m_logoPreview = new QLabel(phTab);
    m_logoPreview->setFixedSize(64, 64);
    m_logoPreview->setStyleSheet(QStringLiteral("border:1px solid %1; border-radius:8px;")
                                     .arg(UiUtil::Palette::BorderSubtle.name()));
    m_logoPreview->setAlignment(Qt::AlignCenter);
    auto *logoBtn = new QPushButton(QStringLiteral("Change logo…"), phTab);
    logoBtn->setProperty("variant", "secondary");
    auto *logoRow = new QHBoxLayout;
    logoRow->setSpacing(8);
    logoRow->addWidget(m_logoPreview);
    logoRow->addWidget(logoBtn);
    logoRow->addStretch();

    // Card: Pharmacy identity
    auto [identityCard, identityBox] = makeCard(phTab);
    identityBox->addLayout(
        makeCardHeader(identityCard, QStringLiteral("Pharmacy identity"),
                       QStringLiteral("Appears on receipts, reports and the sign-in screen.")));
    auto *identityForm = makeForm();
    identityForm->addRow(QStringLiteral("Pharmacy name *"), m_phName);
    identityForm->addRow(QStringLiteral("Address"), m_phAddress);
    identityForm->addRow(QStringLiteral("Phone"), m_phPhone);
    identityForm->addRow(QStringLiteral("Tax / registration line"), m_phNtn);
    identityBox->addLayout(identityForm);

    // Card: Receipt
    auto [receiptCard, receiptBox] = makeCard(phTab);
    receiptBox->addLayout(
        makeCardHeader(receiptCard, QStringLiteral("Receipt"),
                       QStringLiteral("Text printed at the bottom of every sale receipt.")));
    auto *receiptForm = makeForm();
    receiptForm->addRow(QStringLiteral("Return policy text"), m_phPolicy);
    receiptForm->addRow(QStringLiteral("Receipt footer"), m_phFooter);
    receiptBox->addLayout(receiptForm);

    // Card: Appearance
    auto [appearanceCard, appearanceBox] = makeCard(phTab);
    appearanceBox->addLayout(
        makeCardHeader(appearanceCard, QStringLiteral("Appearance"),
                       QStringLiteral("Theme changes apply immediately on Save.")));
    auto *appearanceForm = makeForm();
    appearanceForm->addRow(QStringLiteral("Theme"), m_theme);
    appearanceForm->addRow(QStringLiteral("Logo"), logoRow);
    appearanceBox->addLayout(appearanceForm);

    // Card: About — fixed product version + developer credit (the pharmacy's
    // own identity is set above; this is the credit for whoever built it).
    auto [aboutCard, aboutBox] = makeCard(phTab);
    aboutBox->addLayout(makeCardHeader(aboutCard, QStringLiteral("About"), QString()));
    auto *aboutProduct = new QLabel(
        QStringLiteral("<b>%1</b> v%2 — %3")
            .arg(Branding::productName(), Branding::appVersion(), Branding::productTagline()),
        aboutCard);
    aboutProduct->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QStringList emailLinks;
    for (const QString &e : Branding::developerEmails()) {
        emailLinks << QStringLiteral("<a href=\"mailto:%1\">%1</a>").arg(e);
    }
    auto *aboutDev = new QLabel(
        QStringLiteral("Developed by <b>%1</b><br>GitHub: <a href=\"https://%2\">%2</a>"
                       "<br>Email: %3")
            .arg(Branding::developer(), Branding::developerGithub(),
                 emailLinks.join(QStringLiteral("  ·  "))),
        aboutCard);
    aboutDev->setTextFormat(Qt::RichText);
    aboutDev->setOpenExternalLinks(true);
    aboutDev->setTextInteractionFlags(Qt::TextBrowserInteraction);
    aboutDev->setWordWrap(true);
    aboutBox->addWidget(aboutProduct);
    aboutBox->addWidget(aboutDev);

    auto *phSave = new QPushButton(QStringLiteral("Save pharmacy settings"), phTab);
    phSave->setEnabled(m_isAdmin);
    auto *phBtns = new QHBoxLayout;
    phBtns->setSpacing(8);
    phBtns->addStretch();
    phBtns->addWidget(phSave);

    auto *phLayout = new QVBoxLayout(phTab);
    phLayout->setContentsMargins(0, 0, 0, 0);
    phLayout->setSpacing(16);
    phLayout->addWidget(identityCard);
    phLayout->addWidget(receiptCard);
    phLayout->addWidget(appearanceCard);
    phLayout->addLayout(phBtns);
    phLayout->addWidget(aboutCard);
    phLayout->addStretch();

    connect(phSave, &QPushButton::clicked, this, &AdminPage::savePharmacy);
    connect(logoBtn, &QPushButton::clicked, this, &AdminPage::changeLogo);
    for (QWidget *w : {static_cast<QWidget *>(m_phName), static_cast<QWidget *>(m_phAddress),
                       static_cast<QWidget *>(m_phPhone), static_cast<QWidget *>(m_phNtn),
                       static_cast<QWidget *>(m_phPolicy), static_cast<QWidget *>(m_phFooter)}) {
        w->setEnabled(m_isAdmin);
    }
    m_theme->setEnabled(m_isAdmin);
    logoBtn->setEnabled(m_isAdmin);

    return phTab;
}

// ── Printer tab (CUPS diagnostics + queue picker + self-test) ──────────
// Ports pos/frontend/pages/admin/printer.php: live CUPS reachability,
// driver and queue status, USB-printer detection, and a one-click
// self-test. The settings (enable + queue) live here too.
QWidget *AdminPage::buildPrinterTab()
{
    auto *printerTab = new QWidget;

    // Status card — three live indicators with a Refresh button.
    auto [statusCard, statusBox] = makeCard(printerTab);
    auto *statusHead = new QHBoxLayout;
    statusHead->addLayout(makeCardHeader(
        statusCard, QStringLiteral("Printer status"),
        QStringLiteral("Live view of CUPS, the ZJ-80 driver, and the receipt queue.")));
    statusHead->addStretch();
    auto *refreshBtn = new QPushButton(QStringLiteral("Refresh"), statusCard);
    refreshBtn->setProperty("variant", "secondary");
    statusHead->addWidget(refreshBtn, 0, Qt::AlignTop);
    statusBox->addLayout(statusHead);

    m_cupsStatus = new QLabel(statusCard);
    m_driverStatus = new QLabel(statusCard);
    m_queueStatus = new QLabel(statusCard);
    auto *statusForm = makeForm();
    statusForm->addRow(QStringLiteral("CUPS service"), m_cupsStatus);
    statusForm->addRow(QStringLiteral("Driver (zj80)"), m_driverStatus);
    statusForm->addRow(QStringLiteral("Receipt queue"), m_queueStatus);
    statusBox->addLayout(statusForm);

    // Settings card — enable + queue picker + self-test.
    auto [printerCard, printerBox] = makeCard(printerTab);
    printerBox->addLayout(makeCardHeader(
        printerCard, QStringLiteral("Receipt printer"),
        QStringLiteral("Thermal (ESC/POS) printing through an existing CUPS queue.")));

    m_thermalEnabled
        = new QCheckBox(QStringLiteral("Enable thermal (ESC/POS) printing"), printerCard);
    m_thermalQueue = new QComboBox(printerCard);
    m_thermalQueue->setEditable(true);
    m_thermalQueue->setInsertPolicy(QComboBox::NoInsert);
    m_thermalQueue->setMinimumWidth(240);
    m_thermalQueue->lineEdit()->setMaxLength(120);
    m_thermalQueue->lineEdit()->setPlaceholderText(CupsStatus::defaultQueue());
    auto *thermalTestBtn = new QPushButton(QStringLiteral("Print self-test"), printerCard);
    thermalTestBtn->setProperty("variant", "secondary");
    auto *thermalQueueRow = new QHBoxLayout;
    thermalQueueRow->setSpacing(8);
    thermalQueueRow->addWidget(m_thermalQueue, 1);
    thermalQueueRow->addWidget(thermalTestBtn);

    auto *printerForm = makeForm();
    printerForm->addRow(QStringLiteral("Enable thermal printing"), m_thermalEnabled);
    printerForm->addRow(QStringLiteral("CUPS queue"), thermalQueueRow);
    printerBox->addLayout(printerForm);

    auto *prSave = new QPushButton(QStringLiteral("Save printer settings"), printerCard);
    prSave->setEnabled(m_isAdmin);
    auto *prBtns = new QHBoxLayout;
    prBtns->setSpacing(8);
    prBtns->addStretch();
    prBtns->addWidget(prSave);
    printerBox->addLayout(prBtns);

    // USB-printers-detected card.
    auto [usbCard, usbBox] = makeCard(printerTab);
    usbBox->addLayout(
        makeCardHeader(usbCard, QStringLiteral("USB printers detected"),
                       QStringLiteral("Devices CUPS can see on the USB bus right now.")));
    m_usbPrinters = new QTableWidget(usbCard);
    m_usbPrinters->setColumnCount(2);
    m_usbPrinters->setHorizontalHeaderLabels(
        {QStringLiteral("Device URI"), QStringLiteral("Description")});
    m_usbPrinters->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_usbPrinters->setSelectionMode(QAbstractItemView::NoSelection);
    m_usbPrinters->verticalHeader()->setVisible(false);
    m_usbPrinters->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_usbPrinters->horizontalHeader()->setStretchLastSection(true);
    m_usbPrinters->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_usbPrinters->horizontalHeaderItem(1)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_usbPrinters->setMaximumHeight(160);
    m_usbEmpty = new QLabel(QStringLiteral("No USB printers detected."), usbCard);
    m_usbEmpty->setObjectName(QStringLiteral("muted"));
    usbBox->addWidget(m_usbPrinters);
    usbBox->addWidget(m_usbEmpty);

    auto *printerLayout = new QVBoxLayout(printerTab);
    printerLayout->setContentsMargins(0, 0, 0, 0);
    printerLayout->setSpacing(16);
    printerLayout->addWidget(statusCard);
    printerLayout->addWidget(printerCard);
    printerLayout->addWidget(usbCard);
    printerLayout->addStretch();

    connect(prSave, &QPushButton::clicked, this, &AdminPage::savePrinter);
    connect(refreshBtn, &QPushButton::clicked, this, &AdminPage::refreshPrinterStatus);
    connect(thermalTestBtn, &QPushButton::clicked, this, &AdminPage::testThermalPrint);
    // Re-probe the queue indicator as the operator changes the target queue.
    connect(m_thermalQueue, &QComboBox::currentTextChanged, this, &AdminPage::refreshPrinterStatus);
    m_thermalEnabled->setEnabled(m_isAdmin);
    m_thermalQueue->setEnabled(m_isAdmin);

    return printerTab;
}

// ── Users tab ──────────────────────────────────────────────────────────
QWidget *AdminPage::buildUsersTab()
{
    auto *usersTab = new QWidget;
    auto [usersCard, usersBox] = makeCard(usersTab);
    usersBox->addLayout(
        makeCardHeader(usersCard, QStringLiteral("Staff accounts"),
                       QStringLiteral("Cashier and admin logins for this pharmacy.")));

    m_users = new QTableWidget(usersCard);
    m_users->setColumnCount(5);
    m_users->setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Username"),
                                        QStringLiteral("Role"), QStringLiteral("Active"),
                                        QStringLiteral("Last login")});
    m_users->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_users->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_users->setSelectionMode(QAbstractItemView::SingleSelection);
    m_users->verticalHeader()->setVisible(false);
    m_users->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_users->horizontalHeader()->setStretchLastSection(true);
    // Left-align text-column headers (Name/Username/Role/Active) to match their data.
    m_users->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_users->horizontalHeaderItem(1)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_users->horizontalHeaderItem(2)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_users->horizontalHeaderItem(3)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    // Right-align the Last login header to match its right-aligned data.
    m_users->horizontalHeaderItem(4)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    usersBox->addWidget(m_users, 1);

    auto *addUserBtn = new QPushButton(QStringLiteral("Add user"), usersCard);
    auto *editUserBtn = new QPushButton(QStringLiteral("Edit / reset PIN"), usersCard);
    editUserBtn->setProperty("variant", "secondary");
    addUserBtn->setEnabled(m_isAdmin);
    editUserBtn->setEnabled(m_isAdmin);
    connect(addUserBtn, &QPushButton::clicked, this, &AdminPage::addUser);
    connect(editUserBtn, &QPushButton::clicked, this, &AdminPage::editUser);

    auto *uBtns = new QHBoxLayout;
    uBtns->setSpacing(8);
    if (!m_isAdmin) {
        auto *note = new QLabel(QStringLiteral("Only admins can manage users."), usersCard);
        note->setObjectName(QStringLiteral("muted"));
        uBtns->addWidget(note);
    }
    uBtns->addStretch();
    uBtns->addWidget(editUserBtn);
    uBtns->addWidget(addUserBtn);
    usersBox->addLayout(uBtns);

    auto *uLayout = new QVBoxLayout(usersTab);
    uLayout->setContentsMargins(0, 0, 0, 0);
    uLayout->setSpacing(16);
    uLayout->addWidget(usersCard, 1);

    return usersTab;
}

// ── Audit tab ──────────────────────────────────────────────────────────
QWidget *AdminPage::buildAuditTab()
{
    auto *auditTab = new QWidget;
    auto [auditCard, auditBox] = makeCard(auditTab);
    auditBox->addLayout(
        makeCardHeader(auditCard, QStringLiteral("Audit log"),
                       QStringLiteral("Append-only record of changes (most recent first).")));

    m_audit = new QTableWidget(auditCard);
    m_audit->setColumnCount(5);
    m_audit->setHorizontalHeaderLabels({QStringLiteral("When"), QStringLiteral("User"),
                                        QStringLiteral("Action"), QStringLiteral("Entity"),
                                        QStringLiteral("ID")});
    m_audit->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_audit->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_audit->verticalHeader()->setVisible(false);
    m_audit->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    // Left-align text-column headers (When/User/Action/Entity) to match their data.
    m_audit->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_audit->horizontalHeaderItem(1)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_audit->horizontalHeaderItem(2)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_audit->horizontalHeaderItem(3)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    // Right-align the ID header to match its right-aligned data.
    m_audit->horizontalHeaderItem(4)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auditBox->addWidget(m_audit, 1);

    auto *auditTabLayout = new QVBoxLayout(auditTab);
    auditTabLayout->setContentsMargins(0, 0, 0, 0);
    auditTabLayout->setSpacing(16);
    auditTabLayout->addWidget(auditCard, 1);

    return auditTab;
}

// ── Backup tab ─────────────────────────────────────────────────────────
QWidget *AdminPage::buildBackupTab()
{
    auto *backupTab = new QWidget;
    auto [backupCard, backupBox] = makeCard(backupTab);
    backupBox->addLayout(makeCardHeader(
        backupCard, QStringLiteral("Backup"),
        QStringLiteral("Pharmacy records can't be recovered if lost. Point this at a USB "
                       "drive or network share so a copy lives off this machine.")));

    m_backupDir = new QLineEdit(backupCard);
    m_backupDir->setReadOnly(true);
    m_backupDir->setMinimumWidth(280);
    m_backupDir->setMaximumWidth(560);
    m_backupDir->setPlaceholderText(QStringLiteral("No folder chosen"));
    auto *chooseBtn = new QPushButton(QStringLiteral("Choose folder…"), backupCard);
    chooseBtn->setProperty("variant", "secondary");
    m_autoBackup = new QCheckBox(QStringLiteral("Back up automatically (daily)"), backupCard);
    m_lastBackup = new QLabel(backupCard);
    m_lastBackup->setObjectName(QStringLiteral("muted"));
    auto *backupNowBtn = new QPushButton(QStringLiteral("Back up now"), backupCard);
    auto *saveBtn = new QPushButton(QStringLiteral("Save backup settings"), backupCard);
    saveBtn->setProperty("variant", "secondary");

    connect(chooseBtn, &QPushButton::clicked, this, &AdminPage::chooseBackupDir);
    connect(backupNowBtn, &QPushButton::clicked, this, &AdminPage::backupNow);
    connect(saveBtn, &QPushButton::clicked, this, &AdminPage::saveBackupSettings);
    connect(m_autoBackup, &QCheckBox::toggled, this, &AdminPage::saveBackupSettings);

    auto *dirRow = new QHBoxLayout;
    dirRow->setSpacing(8);
    dirRow->addWidget(m_backupDir, 1);
    dirRow->addWidget(chooseBtn);

    auto *backupForm = makeForm();
    backupForm->addRow(QStringLiteral("Backup folder"), dirRow);
    backupForm->addRow(QString(), m_autoBackup);
    backupForm->addRow(QString(), m_lastBackup);
    backupBox->addLayout(backupForm);

    auto *bBtns = new QHBoxLayout;
    bBtns->setSpacing(8);
    bBtns->addStretch();
    bBtns->addWidget(saveBtn);
    bBtns->addWidget(backupNowBtn);
    backupBox->addLayout(bBtns);

    auto *bLayout = new QVBoxLayout(backupTab);
    bLayout->setContentsMargins(0, 0, 0, 0);
    bLayout->setSpacing(16);
    bLayout->addWidget(backupCard);
    bLayout->addStretch();

    return backupTab;
}

void AdminPage::reload()
{
    loadPharmacy();
    loadPrinter();
    reloadUsers();
    reloadAudit();
    loadBackupSettings();
}

void AdminPage::loadPharmacy()
{
    SettingsRepository s(m_db);
    m_phName->setText(s.get(SettingsKeys::PharmacyName));
    m_phAddress->setPlainText(s.get(SettingsKeys::PharmacyAddress));
    m_phPhone->setText(s.get(SettingsKeys::PharmacyPhone));
    m_phNtn->setText(s.get(SettingsKeys::PharmacyNtn));
    m_phPolicy->setPlainText(s.get(SettingsKeys::ReturnPolicyText));
    m_phFooter->setText(s.get(SettingsKeys::ReceiptFooter));
    const int ti = m_theme->findData(s.get(SettingsKeys::Theme));
    m_theme->setCurrentIndex(ti >= 0 ? ti : 0);
    m_logoPath = s.get(SettingsKeys::LogoPath);
    if (!m_logoPath.isEmpty()) {
        QPixmap pm(m_logoPath);
        if (!pm.isNull()) {
            m_logoPreview->setPixmap(
                pm.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    } else {
        m_logoPreview->setText(QStringLiteral("—"));
    }
}

void AdminPage::changeLogo()
{
    const QString file
        = QFileDialog::getOpenFileName(this, QStringLiteral("Choose logo image"), QString(),
                                       QStringLiteral("Images (*.png *.jpg *.jpeg *.svg *.bmp)"));
    if (file.isEmpty()) {
        return;
    }
    QPixmap pm(file);
    if (pm.isNull()) {
        QMessageBox::warning(this, QStringLiteral("Logo"),
                             QStringLiteral("That isn't a readable image."));
        return;
    }
    QDir().mkpath(AppPaths::logosDir());
    const QString ext = QFileInfo(file).suffix().toLower();
    const QString dest = AppPaths::logosDir() + QStringLiteral("/logo.")
                         + (ext.isEmpty() ? QStringLiteral("png") : ext);
    QFile::remove(dest);
    if (!QFile::copy(file, dest)) {
        QMessageBox::warning(this, QStringLiteral("Logo"),
                             QStringLiteral("Could not save the logo."));
        return;
    }
    m_logoPath = dest;
    m_logoPreview->setPixmap(pm.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void AdminPage::savePharmacy()
{
    if (m_phName->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Pharmacy"),
                             QStringLiteral("Pharmacy name is required."));
        return;
    }
    SettingsRepository s(m_db);
    s.set(SettingsKeys::PharmacyName, m_phName->text().trimmed(), m_user.id);
    s.set(SettingsKeys::PharmacyAddress, m_phAddress->toPlainText().trimmed(), m_user.id);
    s.set(SettingsKeys::PharmacyPhone, m_phPhone->text().trimmed(), m_user.id);
    s.set(SettingsKeys::PharmacyNtn, m_phNtn->text().trimmed(), m_user.id);
    s.set(SettingsKeys::ReturnPolicyText, m_phPolicy->toPlainText().trimmed(), m_user.id);
    s.set(SettingsKeys::ReceiptFooter, m_phFooter->text().trimmed(), m_user.id);
    s.set(SettingsKeys::Theme, m_theme->currentData().toString(), m_user.id);
    s.set(SettingsKeys::LogoPath, m_logoPath, m_user.id);
    Theme::apply(m_theme->currentData().toString()); // live, no restart needed
    QMessageBox::information(this, QStringLiteral("Pharmacy"), QStringLiteral("Settings saved."));
}

void AdminPage::loadPrinter()
{
    SettingsRepository s(m_db);
    m_thermalEnabled->setChecked(s.get(SettingsKeys::ThermalEnabled) == QLatin1String("1"));

    // Populate the picker with detected CUPS destinations, then select the
    // saved queue (adding it if CUPS isn't reachable / hasn't been set up yet).
    const QString saved = s.get(SettingsKeys::ThermalQueue);
    m_thermalQueue->blockSignals(true);
    m_thermalQueue->clear();
    const QStringList detected = CupsStatus::listQueues();
    m_thermalQueue->addItems(detected);
    if (!saved.isEmpty() && !detected.contains(saved)) {
        m_thermalQueue->addItem(saved);
    }
    m_thermalQueue->setCurrentText(saved);
    m_thermalQueue->blockSignals(false);

    refreshPrinterStatus();
}

void AdminPage::savePrinter()
{
    SettingsRepository s(m_db);
    s.set(SettingsKeys::ThermalEnabled,
          m_thermalEnabled->isChecked() ? QStringLiteral("1") : QStringLiteral("0"), m_user.id);
    s.set(SettingsKeys::ThermalQueue, m_thermalQueue->currentText().trimmed(), m_user.id);
    QMessageBox::information(this, QStringLiteral("Printer"),
                             QStringLiteral("Printer settings saved."));
    refreshPrinterStatus();
}

void AdminPage::refreshPrinterStatus()
{
    // Colour helper matching the theme tokens (success / warning / danger).
    auto badge = [](QLabel *lbl, bool ok, const QString &okText, const QString &badText,
                    const QString &badColor) {
        lbl->setText(ok ? okText : badText);
        lbl->setStyleSheet(QStringLiteral("color:%1; font-weight:600;")
                               .arg(ok ? UiUtil::Palette::Success.name() : badColor));
    };

    const QString queue = m_thermalQueue->currentText().trimmed();
    const CupsSnapshot snap = CupsStatus::snapshot(queue);

    badge(m_cupsStatus, snap.cupsActive, QStringLiteral("Active"), QStringLiteral("Not reachable"),
          UiUtil::Palette::Danger.name());
    badge(m_driverStatus, snap.driverInstalled, QStringLiteral("Installed"),
          QStringLiteral("Missing"), UiUtil::Palette::Danger.name());
    badge(m_queueStatus, snap.queueExists,
          queue.isEmpty() ? QStringLiteral("—") : QStringLiteral("Ready — %1").arg(queue),
          queue.isEmpty() ? QStringLiteral("No queue selected")
                          : QStringLiteral("Missing — %1").arg(queue),
          UiUtil::Palette::Warning.name());

    // USB devices CUPS can currently see.
    const QVector<UsbPrinter> usb = CupsStatus::usbPrinters();
    m_usbPrinters->setRowCount(usb.size());
    for (int i = 0; i < usb.size(); ++i) {
        m_usbPrinters->setItem(i, 0, new QTableWidgetItem(usb.at(i).uri));
        m_usbPrinters->setItem(i, 1, new QTableWidgetItem(usb.at(i).description));
    }
    m_usbPrinters->setVisible(!usb.isEmpty());
    m_usbEmpty->setVisible(usb.isEmpty());
}

void AdminPage::testThermalPrint()
{
    static const QString kTitle = QStringLiteral("Self-test");

    const QString queue = m_thermalQueue->currentText().trimmed();
    if (queue.isEmpty()) {
        QMessageBox::warning(this, kTitle, QStringLiteral("Choose a CUPS queue first."));
        return;
    }

    const QByteArray bytes = EscPosRenderer::selfTest(queue, Branding::productName());
    const ThermalPrintResult res
        = CupsRawPrinter(queue).printRaw(bytes, QStringLiteral("self-test"));
    if (!res.ok) {
        QMessageBox::warning(this, kTitle, res.error);
        return;
    }
    QMessageBox::information(this, kTitle,
                             QStringLiteral("Self-test ticket sent to %1.").arg(queue));
}

void AdminPage::reloadUsers()
{
    UserRepository repo(m_db);
    const QVector<UserAdminRow> rows = repo.listUsers();
    UiUtil::beginFill(m_users);
    m_users->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const UserAdminRow &u = rows.at(i);
        auto *name = new QTableWidgetItem(u.fullName);
        name->setData(Qt::UserRole, u.id);
        name->setData(Qt::UserRole + 1, u.fullName);
        name->setData(Qt::UserRole + 2, u.username);
        name->setData(Qt::UserRole + 3, u.role);
        name->setData(Qt::UserRole + 4, u.isActive);
        m_users->setItem(i, 0, name);
        m_users->setItem(i, 1, new QTableWidgetItem(u.username));
        m_users->setItem(i, 2, new QTableWidgetItem(u.role));
        m_users->setItem(
            i, 3, new QTableWidgetItem(u.isActive ? QStringLiteral("Yes") : QStringLiteral("No")));
        m_users->setItem(
            i, 4,
            new QTableWidgetItem(u.lastLoginAt.isEmpty() ? QStringLiteral("—") : u.lastLoginAt));
    }
    UiUtil::emptyState(m_users, QStringLiteral("No users yet."));
}

void AdminPage::reloadAudit()
{
    AuditRepository repo(m_db);
    const QVector<AuditRow> rows = repo.list();
    UiUtil::beginFill(m_audit);
    m_audit->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const AuditRow &a = rows.at(i);
        m_audit->setItem(i, 0, new QTableWidgetItem(a.timestamp));
        m_audit->setItem(i, 1, new QTableWidgetItem(a.userName));
        m_audit->setItem(i, 2, new QTableWidgetItem(a.actionType));
        m_audit->setItem(i, 3, new QTableWidgetItem(a.entityType));
        auto *id = new QTableWidgetItem(a.entityId ? QString::number(a.entityId) : QString());
        UiUtil::rightAlign(id);
        m_audit->setItem(i, 4, id);
    }
    UiUtil::emptyState(m_audit, QStringLiteral("No audit entries yet."));
}

void AdminPage::loadBackupSettings()
{
    SettingsRepository settings(m_db);
    m_backupDir->setText(settings.get(SettingsKeys::BackupDir));
    m_autoBackup->blockSignals(true);
    m_autoBackup->setChecked(settings.get(SettingsKeys::AutoBackup) == QLatin1String("1"));
    m_autoBackup->blockSignals(false);
    const QString last = settings.get(SettingsKeys::LastBackupAt);
    m_lastBackup->setText(last.isEmpty() ? QStringLiteral("Last backup: never")
                                         : QStringLiteral("Last backup: %1").arg(last.left(19)));
}

qint64 AdminPage::selectedUserId() const
{
    const int row = m_users->currentRow();
    if (row < 0 || !m_users->item(row, 0)) {
        return 0;
    }
    return m_users->item(row, 0)->data(Qt::UserRole).toLongLong();
}

void AdminPage::addUser()
{
    UserRepository repo(m_db);
    UserDialog dlg(&repo, m_user.id, 0, QString(), QString(), QStringLiteral("CASHIER"), true,
                   this);
    if (dlg.exec() == QDialog::Accepted) {
        reloadUsers();
        reloadAudit();
    }
}

void AdminPage::editUser()
{
    const int row = m_users->currentRow();
    if (row < 0 || !m_users->item(row, 0)) {
        QMessageBox::information(this, QStringLiteral("Edit user"),
                                 QStringLiteral("Select a user first."));
        return;
    }
    QTableWidgetItem *it = m_users->item(row, 0);
    UserRepository repo(m_db);
    UserDialog dlg(&repo, m_user.id, it->data(Qt::UserRole).toLongLong(),
                   it->data(Qt::UserRole + 1).toString(), it->data(Qt::UserRole + 2).toString(),
                   it->data(Qt::UserRole + 3).toString(), it->data(Qt::UserRole + 4).toBool(),
                   this);
    if (dlg.exec() == QDialog::Accepted) {
        reloadUsers();
        reloadAudit();
    }
}

void AdminPage::changeMyPin()
{
    UserRepository repo(m_db);
    ChangePinDialog dlg(&repo, m_user.id, this);
    dlg.exec();
    reloadAudit();
}

void AdminPage::chooseBackupDir()
{
    const QString dir
        = QFileDialog::getExistingDirectory(this, QStringLiteral("Choose backup folder"));
    if (!dir.isEmpty()) {
        m_backupDir->setText(dir);
        saveBackupSettings();
    }
}

void AdminPage::saveBackupSettings()
{
    SettingsRepository settings(m_db);
    settings.set(SettingsKeys::BackupDir, m_backupDir->text(), m_user.id);
    settings.set(SettingsKeys::AutoBackup,
                 m_autoBackup->isChecked() ? QStringLiteral("1") : QStringLiteral("0"), m_user.id);
}

void AdminPage::backupNow()
{
    if (m_backupDir->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Backup"),
                             QStringLiteral("Choose a backup folder first."));
        return;
    }
    const QDateTime now = QDateTime::currentDateTime();
    BackupService svc(m_db);
    const BackupService::Result r
        = svc.backupNow(m_backupDir->text(), now.toString(QStringLiteral("yyyyMMdd_HHmmss")));
    if (!r.ok) {
        QMessageBox::warning(this, QStringLiteral("Backup"), r.error);
        return;
    }
    SettingsRepository settings(m_db);
    settings.set(SettingsKeys::LastBackupAt, now.toString(Qt::ISODate), m_user.id);
    loadBackupSettings();
    QMessageBox::information(this, QStringLiteral("Backup"),
                             QStringLiteral("Backed up to:\n%1").arg(r.path));
}
