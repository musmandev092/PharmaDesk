#include "ui/SessionsPage.h"

#include "data/SessionRepository.h"
#include "domain/Money.h"
#include "service/Reconciler.h"
#include "ui/UiUtil.h"
#include "ui/ZReportDialog.h"

#include <QDoubleValidator>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSqlQuery>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

SessionsPage::SessionsPage(QSqlDatabase db, qint64 userId, QWidget *parent)
    : QWidget(parent), m_db(std::move(db)), m_userId(userId)
{
    auto *title = new QLabel(QStringLiteral("Cashier sessions"), this);
    title->setObjectName(QStringLiteral("h1"));
    auto *subtitle = new QLabel(
        QStringLiteral("Open and close your drawer, then reconcile counted cash against expected."),
        this);
    subtitle->setObjectName(QStringLiteral("muted"));

    // Query the role once to decide whether to expose the reconcile queue.
    QString role;
    {
        QSqlQuery rq(m_db);
        rq.prepare(QStringLiteral("SELECT role FROM users WHERE id = ?"));
        rq.addBindValue(m_userId);
        if (rq.exec() && rq.next()) {
            role = rq.value(0).toString();
        }
    }
    const bool canReconcile = role == QLatin1String("MANAGER") || role == QLatin1String("ADMIN");

    auto *tabs = new QTabWidget(this);

    // ── "My shift" tab ──────────────────────────────────────────────────────
    tabs->addTab(buildShiftTab(), QStringLiteral("My shift"));

    // Reconcile (manager sign-off) tab is MANAGER/ADMIN only.
    if (canReconcile) {
        tabs->addTab(buildReconcileTab(), QStringLiteral("Reconcile"));
    }

    // ── Responsive wrapper ──────────────────────────────────────────────────
    // Move the page content into a dedicated content widget owning the root
    // layout, then host it inside a transparent, frameless QScrollArea so the
    // whole page scrolls on short/small screens without anything clipping.
    auto *content = new QWidget;
    content->setObjectName(QStringLiteral("scrollContent"));
    content->setAutoFillBackground(false);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addWidget(tabs, 1);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("pageScroll"));
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setAutoFillBackground(false);
    scroll->viewport()->setAutoFillBackground(false);
    // Keep the themed page background visible through the scroll area/content.
    scroll->setStyleSheet(QStringLiteral("QScrollArea#pageScroll, QWidget#scrollContent { "
                                         "background: transparent; border: none; }"));

    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    pageLayout->addWidget(scroll);

    reload();
}

// ── "My shift" tab ──────────────────────────────────────────────────────
QWidget *SessionsPage::buildShiftTab()
{
    auto *shiftTab = new QWidget;

    // ── Current shift card ──────────────────────────────────────────────────
    auto *statusHeader = new QLabel(QStringLiteral("My shift"), shiftTab);
    statusHeader->setObjectName(QStringLiteral("h2"));

    m_statusLabel = new QLabel(shiftTab);
    m_statusLabel->setObjectName(QStringLiteral("h2"));
    m_detailLabel = new QLabel(shiftTab);
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setTextFormat(Qt::RichText);

    m_openBtn = new QPushButton(QStringLiteral("Open shift"), shiftTab);
    m_closeBtn = new QPushButton(QStringLiteral("Close shift && generate Z-report"), shiftTab);
    m_closeBtn->setProperty("variant", "secondary");

    // Footer action row — primary right-most.
    auto *cardBtns = new QHBoxLayout;
    cardBtns->setSpacing(8);
    cardBtns->addStretch();
    cardBtns->addWidget(m_closeBtn);
    cardBtns->addWidget(m_openBtn);

    auto *card = new QFrame(shiftTab);
    card->setObjectName(QStringLiteral("card"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 16, 20, 16);
    cardLayout->setSpacing(12);
    cardLayout->addWidget(statusHeader);
    cardLayout->addWidget(m_statusLabel);
    cardLayout->addWidget(m_detailLabel);
    cardLayout->addSpacing(4);
    cardLayout->addLayout(cardBtns);

    // ── Recent sessions card ────────────────────────────────────────────────
    auto *recentCard = new QFrame(shiftTab);
    recentCard->setObjectName(QStringLiteral("card"));
    auto *recentLayout = new QVBoxLayout(recentCard);
    recentLayout->setContentsMargins(20, 16, 20, 16);
    recentLayout->setSpacing(12);

    auto *recentLabel = new QLabel(QStringLiteral("Recent sessions"), shiftTab);
    recentLabel->setObjectName(QStringLiteral("h2"));
    auto *recentDesc = new QLabel(
        QStringLiteral("Your latest shifts. Double-click a closed shift to view its Z-report."),
        shiftTab);
    recentDesc->setObjectName(QStringLiteral("muted"));

    m_table = new QTableWidget(shiftTab);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Opened"), QStringLiteral("Cashier"),
                                        QStringLiteral("Sales"), QStringLiteral("Cash sales"),
                                        QStringLiteral("Counted"), QStringLiteral("Variance"),
                                        QStringLiteral("Status")});
    // Right-align headers over numeric/money columns (Sales, Cash sales, Counted, Variance)
    for (int col : {2, 3, 4, 5}) {
        if (QTableWidgetItem *h = m_table->horizontalHeaderItem(col))
            h->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setShowGrid(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    m_zBtn = new QPushButton(QStringLiteral("View Z-report"), shiftTab);
    m_zBtn->setProperty("variant", "secondary");
    auto *tableBtns = new QHBoxLayout;
    tableBtns->setSpacing(8);
    tableBtns->addStretch();
    tableBtns->addWidget(m_zBtn);

    recentLayout->addWidget(recentLabel);
    recentLayout->addWidget(recentDesc);
    recentLayout->addWidget(m_table, 1);
    recentLayout->addLayout(tableBtns);

    auto *shiftLayout = new QVBoxLayout(shiftTab);
    shiftLayout->setContentsMargins(0, 0, 0, 0);
    shiftLayout->setSpacing(16);
    shiftLayout->addWidget(card);
    shiftLayout->addWidget(recentCard, 1);

    connect(m_openBtn, &QPushButton::clicked, this, &SessionsPage::openShift);
    connect(m_closeBtn, &QPushButton::clicked, this, &SessionsPage::closeShift);
    connect(m_zBtn, &QPushButton::clicked, this, &SessionsPage::showZReport);
    connect(m_table, &QTableWidget::doubleClicked, this, &SessionsPage::showZReport);

    return shiftTab;
}

// ── "Reconcile" tab (manager sign-off) — MANAGER/ADMIN only ─────────────
QWidget *SessionsPage::buildReconcileTab()
{
    auto *sessionsTab = new QWidget;

    // ── Queue card: closed shifts awaiting sign-off ─────────────────────
    m_sessions = new QTableWidget(sessionsTab);
    m_sessions->setColumnCount(7);
    m_sessions->setHorizontalHeaderLabels({QStringLiteral("Session"), QStringLiteral("Cashier"),
                                           QStringLiteral("Opened"), QStringLiteral("Closed"),
                                           QStringLiteral("Cash sales"), QStringLiteral("Counted"),
                                           QStringLiteral("Variance")});
    m_sessions->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sessions->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sessions->setSelectionMode(QAbstractItemView::SingleSelection);
    m_sessions->setShowGrid(true);
    m_sessions->verticalHeader()->setVisible(false);
    m_sessions->horizontalHeader()->setHighlightSections(false);
    m_sessions->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    // Right-align money column headers (Cash sales, Counted, Variance) over their values.
    for (int col : {4, 5, 6}) {
        m_sessions->horizontalHeaderItem(col)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    auto *queueCard = new QFrame(sessionsTab);
    queueCard->setObjectName(QStringLiteral("card"));
    auto *queueLayout = new QVBoxLayout(queueCard);
    queueLayout->setContentsMargins(20, 16, 20, 16);
    queueLayout->setSpacing(12);
    auto *queueHeader = new QLabel(QStringLiteral("Reconcile queue"), sessionsTab);
    queueHeader->setObjectName(QStringLiteral("h2"));
    auto *queueDesc = new QLabel(
        QStringLiteral("Closed shifts awaiting manager sign-off (newest first)."), sessionsTab);
    queueDesc->setObjectName(QStringLiteral("muted"));
    queueLayout->addWidget(queueHeader);
    queueLayout->addWidget(queueDesc);
    queueLayout->addWidget(m_sessions, 1);

    // ── Sign-off card: recount + notes form, right-aligned action ───────
    m_recountCash = new QLineEdit(sessionsTab);
    m_recountCash->setPlaceholderText(QStringLiteral("Blank keeps the cashier's count"));
    m_recountCash->setValidator(new QDoubleValidator(0.0, 99999999.0, 2, m_recountCash));
    m_recountCash->setMinimumWidth(280);
    m_recountCash->setMaximumWidth(560);
    m_reconcileNotes = new QLineEdit(sessionsTab);
    m_reconcileNotes->setPlaceholderText(QStringLiteral("Optional reconciliation notes"));
    m_reconcileNotes->setMinimumWidth(280);
    m_reconcileNotes->setMaximumWidth(560);

    m_reconcileBtn = new QPushButton(QStringLiteral("Reconcile selected"), sessionsTab);
    connect(m_reconcileBtn, &QPushButton::clicked, this, &SessionsPage::reconcileSelected);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->addRow(QStringLiteral("Recount cash (PKR)"), m_recountCash);
    form->addRow(QStringLiteral("Notes"), m_reconcileNotes);

    auto *recBtns = new QHBoxLayout;
    recBtns->setSpacing(8);
    recBtns->addStretch();
    recBtns->addWidget(m_reconcileBtn);

    auto *signOffCard = new QFrame(sessionsTab);
    signOffCard->setObjectName(QStringLiteral("card"));
    auto *signOffLayout = new QVBoxLayout(signOffCard);
    signOffLayout->setContentsMargins(20, 16, 20, 16);
    signOffLayout->setSpacing(12);
    auto *signOffHeader = new QLabel(QStringLiteral("Manager sign-off"), sessionsTab);
    signOffHeader->setObjectName(QStringLiteral("h2"));
    auto *signOffDesc = new QLabel(
        QStringLiteral("Select a shift above, optionally recount the drawer, then reconcile."),
        sessionsTab);
    signOffDesc->setObjectName(QStringLiteral("muted"));
    signOffLayout->addWidget(signOffHeader);
    signOffLayout->addWidget(signOffDesc);
    signOffLayout->addLayout(form);
    signOffLayout->addSpacing(4);
    signOffLayout->addLayout(recBtns);

    auto *recLayout = new QVBoxLayout(sessionsTab);
    recLayout->setContentsMargins(0, 0, 0, 0);
    recLayout->setSpacing(16);
    recLayout->addWidget(queueCard, 1);
    recLayout->addWidget(signOffCard);

    return sessionsTab;
}

void SessionsPage::reload()
{
    SessionRepository repo(m_db);
    const qint64 openId = repo.openSessionId(m_userId);

    if (openId == -1) {
        m_statusLabel->setText(QStringLiteral("No open shift"));
        m_detailLabel->setText(
            QStringLiteral("Start your day by counting the cash currently in the drawer and "
                           "entering it as the opening float."));
        m_openBtn->setEnabled(true);
        m_closeBtn->setEnabled(false);
    } else {
        const SessionSummary s = repo.sessionSummary(openId);
        m_statusLabel->setText(QStringLiteral("Shift #%1 open").arg(openId));
        m_detailLabel->setText(
            QStringLiteral(
                "<table cellspacing='6'>"
                "<tr><td>Opened</td><td>&nbsp;&nbsp;</td><td>%1</td></tr>"
                "<tr><td>Opening float</td><td></td><td>PKR %2</td></tr>"
                "<tr><td>Cash sales so far</td><td></td><td>PKR %3</td></tr>"
                "<tr><td>Sales count</td><td></td><td>%4</td></tr>"
                "<tr><td><b>Expected in drawer</b></td><td></td><td><b>PKR %5</b></td></tr>"
                "</table>")
                .arg(s.openedAt.left(19), Money::fromString(s.openingFloat).fmt(),
                     Money::fromString(s.totalCashSales).fmt())
                .arg(s.totalSalesCount)
                .arg(Money::fromString(s.expectedCash).fmt()));
        m_openBtn->setEnabled(false);
        m_closeBtn->setEnabled(true);
    }

    const QVector<SessionRow> rows = repo.list(50);
    UiUtil::beginFill(m_table);
    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const SessionRow &r = rows.at(i);
        auto *opened = new QTableWidgetItem(r.openedAt.left(16));
        opened->setData(Qt::UserRole, r.id);
        opened->setData(Qt::UserRole + 1, r.status);
        m_table->setItem(i, 0, opened);
        m_table->setItem(i, 1, new QTableWidgetItem(r.cashierName));

        auto *count = new QTableWidgetItem(QString::number(r.totalSalesCount));
        UiUtil::rightAlign(count);
        m_table->setItem(i, 2, count);

        auto *cash = new QTableWidgetItem(Money::fromString(r.totalCashSales).display());
        UiUtil::rightAlign(cash);
        m_table->setItem(i, 3, cash);

        auto *counted = new QTableWidgetItem(r.countedCash.isEmpty()
                                                 ? QStringLiteral("—")
                                                 : Money::fromString(r.countedCash).display());
        UiUtil::rightAlign(counted);
        m_table->setItem(i, 4, counted);

        auto *variance = new QTableWidgetItem(r.cashVariance.isEmpty()
                                                  ? QStringLiteral("—")
                                                  : Money::fromString(r.cashVariance).display());
        UiUtil::rightAlign(variance);
        if (!r.cashVariance.isEmpty()) {
            const Money v = Money::fromString(r.cashVariance);
            if (v.isNegative())
                UiUtil::colorItem(variance, UiUtil::Palette::Danger);
            else if (!v.isZero())
                UiUtil::colorItem(variance, UiUtil::Palette::Warning);
            else
                UiUtil::colorItem(variance, UiUtil::Palette::Success);
        }
        m_table->setItem(i, 5, variance);

        const QColor stColor = r.status == QLatin1String("OPEN")         ? UiUtil::Palette::Warning
                               : r.status == QLatin1String("RECONCILED") ? UiUtil::Palette::Success
                                                                         : UiUtil::Palette::Muted;
        UiUtil::setBadge(m_table, i, 6, r.status, stColor);
    }
    UiUtil::emptyState(m_table, QStringLiteral("No sessions yet — click “Open shift”."));

    if (m_sessions) {
        reloadSessions();
    }
}

void SessionsPage::openShift()
{
    bool ok = false;
    const QString text = QInputDialog::getText(
        this, QStringLiteral("Open shift"),
        QStringLiteral("Opening float (PKR) — count every note & coin in the drawer:"),
        QLineEdit::Normal, QStringLiteral("0.00"), &ok);
    if (!ok) {
        return;
    }
    SessionRepository repo(m_db);
    const qint64 id = repo.openSession(m_userId, text);
    if (id == -1) {
        QMessageBox::warning(this, QStringLiteral("Open shift"),
                             QStringLiteral("Could not open shift: %1").arg(repo.errorString()));
        return;
    }
    QMessageBox::information(this, QStringLiteral("Open shift"),
                             QStringLiteral("Shift #%1 opened.").arg(id));
    reload();
}

void SessionsPage::closeShift()
{
    SessionRepository repo(m_db);
    const qint64 openId = repo.openSessionId(m_userId);
    if (openId == -1) {
        QMessageBox::information(this, QStringLiteral("Close shift"),
                                 QStringLiteral("You have no open shift."));
        return;
    }
    const SessionSummary s = repo.sessionSummary(openId);

    bool ok = false;
    const QString text = QInputDialog::getText(
        this, QStringLiteral("Close shift"),
        QStringLiteral("Expected in drawer: PKR %1\n\nCounted cash (PKR) — count the drawer now:")
            .arg(Money::fromString(s.expectedCash).fmt()),
        QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        return;
    }
    // Live variance feedback before committing.
    const Money counted = Money::fromString(text.trimmed());
    const Money variance = counted - Money::fromString(s.expectedCash);
    const QString varMsg
        = variance.isZero()
              ? QStringLiteral("balanced")
              : (variance.isNegative()
                     ? QStringLiteral("short by PKR %1").arg((Money() - variance).fmt())
                     : QStringLiteral("over by PKR %1").arg(variance.fmt()));
    if (QMessageBox::question(
            this, QStringLiteral("Close shift"),
            QStringLiteral("Counted PKR %1 vs expected PKR %2 — %3.\n\nClose the shift now?")
                .arg(counted.fmt(), Money::fromString(s.expectedCash).fmt(), varMsg))
        != QMessageBox::Yes) {
        return;
    }

    if (!repo.closeSession(openId, text, m_userId)) {
        QMessageBox::warning(this, QStringLiteral("Close shift"),
                             QStringLiteral("Could not close shift: %1").arg(repo.errorString()));
        return;
    }
    reload();
    ZReportDialog dlg(m_db, openId, this);
    dlg.exec();
}

qint64 SessionsPage::selectedSessionId() const
{
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 0)) {
        return -1;
    }
    return m_table->item(row, 0)->data(Qt::UserRole).toLongLong();
}

void SessionsPage::showZReport()
{
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 0)) {
        QMessageBox::information(this, QStringLiteral("Z-report"),
                                 QStringLiteral("Select a session first."));
        return;
    }
    const QString status = m_table->item(row, 0)->data(Qt::UserRole + 1).toString();
    if (status == QLatin1String("OPEN")) {
        QMessageBox::information(
            this, QStringLiteral("Z-report"),
            QStringLiteral("The shift is still open — close it to generate its Z-report."));
        return;
    }
    const qint64 id = selectedSessionId();
    if (id == -1) {
        return;
    }
    ZReportDialog dlg(m_db, id, this);
    dlg.exec();
}

// ── Reconcile shifts (manager sign-off) ─────────────────────────────────────

void SessionsPage::reloadSessions()
{
    static const QString kClosed = QStringLiteral("CLOSED");
    static const int kSessionLimit = 100;

    SessionRepository repo(m_db);
    const QVector<SessionRow> rows = repo.list(kSessionLimit);
    UiUtil::beginFill(m_sessions);

    int n = 0;
    m_sessions->setRowCount(rows.size());
    for (const SessionRow &s : rows) {
        if (s.status != kClosed) {
            continue;
        }
        auto *sess = new QTableWidgetItem(QStringLiteral("#%1").arg(s.id));
        sess->setData(Qt::UserRole, s.id);
        m_sessions->setItem(n, 0, sess);
        m_sessions->setItem(n, 1, new QTableWidgetItem(s.cashierName));
        m_sessions->setItem(n, 2, new QTableWidgetItem(s.openedAt.left(19)));
        m_sessions->setItem(n, 3, new QTableWidgetItem(s.closedAt.left(19)));

        auto *cashSales = new QTableWidgetItem(Money::fromString(s.totalCashSales).display());
        UiUtil::rightAlign(cashSales);
        m_sessions->setItem(n, 4, cashSales);

        auto *counted = new QTableWidgetItem(Money::fromString(s.countedCash).display());
        UiUtil::rightAlign(counted);
        m_sessions->setItem(n, 5, counted);

        auto *variance = new QTableWidgetItem(Money::fromString(s.cashVariance).display());
        UiUtil::rightAlign(variance);
        m_sessions->setItem(n, 6, variance);
        ++n;
    }
    m_sessions->setRowCount(n);
    UiUtil::emptyState(m_sessions, QStringLiteral("No closed shifts awaiting reconciliation."));
}

qint64 SessionsPage::selectedReconcileSessionId() const
{
    const int row = m_sessions->currentRow();
    if (row < 0 || !m_sessions->item(row, 0)) {
        return 0;
    }
    return m_sessions->item(row, 0)->data(Qt::UserRole).toLongLong();
}

void SessionsPage::reconcileSelected()
{
    const qint64 id = selectedReconcileSessionId();
    if (id == 0) {
        QMessageBox::information(this, QStringLiteral("Reconcile shifts"),
                                 QStringLiteral("Select a closed shift first."));
        return;
    }
    Reconciler rec(m_db, m_userId);
    const ReconcileResult res
        = rec.reconcile(id, m_recountCash->text().trimmed(), m_reconcileNotes->text().trimmed());
    if (!res.ok) {
        QMessageBox::warning(this, QStringLiteral("Reconciliation failed"), res.error);
        return;
    }
    QMessageBox::information(
        this, QStringLiteral("Shift reconciled"),
        QStringLiteral("Counted PKR %1, variance PKR %2.")
            .arg(Money::fromString(res.countedCash).fmt(), Money::fromString(res.variance).fmt()));
    m_recountCash->clear();
    m_reconcileNotes->clear();
    reload();
}
