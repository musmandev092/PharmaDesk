#include "ui/ReturnsPage.h"

#include "data/ReturnRepository.h"
#include "data/UserRepository.h"
#include "domain/ControlledSubstancePolicy.h"
#include "domain/Money.h"
#include "service/ReturnService.h"
#include "ui/ManagerOverrideDialog.h"
#include "ui/UiUtil.h"

#include <QCheckBox>
#include <QSqlQuery>
#include <QVariant>
#include <QComboBox>
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
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
const char *kReasonValues[] = {
    "CUSTOMER_CHANGED_MIND", "DAMAGED", "WRONG_ITEM", "ADVERSE_REACTION", "EXPIRED", "OTHER",
};

// Build a card panel (QFrame#card) with inner padding and an h2 header plus an
// optional muted one-line description. Returns the frame; *outBody is the
// QVBoxLayout to append the card's content to (below the header).
QFrame *makeCard(QWidget *parent, const QString &heading, const QString &desc,
                 QVBoxLayout **outBody)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("card"));
    auto *v = new QVBoxLayout(card);
    v->setContentsMargins(20, 16, 20, 16);
    v->setSpacing(12);

    auto *h = new QLabel(heading, card);
    h->setObjectName(QStringLiteral("h2"));
    v->addWidget(h);
    if (!desc.isEmpty()) {
        auto *d = new QLabel(desc, card);
        d->setObjectName(QStringLiteral("muted"));
        d->setWordWrap(true);
        v->addWidget(d);
    }
    *outBody = v;
    return card;
}
} // namespace

ReturnsPage::ReturnsPage(QSqlDatabase db, qint64 userId, QWidget *parent)
    : QWidget(parent), m_db(std::move(db)), m_userId(userId)
{
    auto *title = new QLabel(QStringLiteral("Returns & refunds"), this);
    title->setObjectName(QStringLiteral("h1"));

    // Query the role once to decide whether to expose the manager queues.
    QString role;
    {
        QSqlQuery rq(m_db);
        rq.prepare(QStringLiteral("SELECT role FROM users WHERE id = ?"));
        rq.addBindValue(m_userId);
        if (rq.exec() && rq.next()) {
            role = rq.value(0).toString();
        }
    }
    const bool canAdjudicate = role == QLatin1String("MANAGER") || role == QLatin1String("ADMIN");

    auto *subtitle = new QLabel(
        QStringLiteral("Look up a sale, refund individual lines, or void a whole receipt."), this);
    subtitle->setObjectName(QStringLiteral("muted"));

    auto *tabs = new QTabWidget(this);

    // ── "Process return" tab ────────────────────────────────────────────────
    tabs->addTab(buildProcessTab(), QStringLiteral("Process return"));

    // Review & supplier-settlement tabs are MANAGER/ADMIN only.
    if (canAdjudicate) {
        tabs->addTab(buildReviewQueueTab(), QStringLiteral("Review queue"));
        tabs->addTab(buildSupplierReturnsTab(), QStringLiteral("Supplier returns"));
    }

    // Wrap the entire page content in a content widget so a QScrollArea can host
    // it — keeps the page usable on small/short screens without clipping.
    auto *content = new QWidget;
    content->setObjectName(QStringLiteral("scrollContent"));
    content->setAutoFillBackground(false);
    content->setStyleSheet(
        QStringLiteral("#scrollContent { background: transparent; border: none; }"));

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
    scroll->setStyleSheet(
        QStringLiteral("#pageScroll, #pageScroll > QWidget > QWidget { background: transparent; "
                       "border: none; }"));

    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    pageLayout->addWidget(scroll);

    reload();
}

// ── "Process return" tab ───────────────────────────────────────────────
QWidget *ReturnsPage::buildProcessTab()
{
    auto *processTab = new QWidget;

    // ── Receipt lookup card ─────────────────────────────────────────────────
    QVBoxLayout *lookupBody = nullptr;
    auto *lookupCard
        = makeCard(processTab, QStringLiteral("Receipt lookup"),
                   QStringLiteral("Enter a receipt number to load the sale."), &lookupBody);

    m_receipt = new QLineEdit(processTab);
    m_receipt->setPlaceholderText(QStringLiteral("Receipt # (e.g. INV-20260518-0001)"));
    m_receipt->setClearButtonEnabled(true);
    m_receipt->setMinimumWidth(280);
    m_receipt->setMaximumWidth(560);
    auto *lookupBtn = new QPushButton(QStringLiteral("Look up"), processTab);
    lookupBtn->setProperty("variant", "primary");
    m_voidBtn = new QPushButton(QStringLiteral("Void sale"), processTab);
    m_voidBtn->setProperty("variant", "destructive");
    m_voidBtn->setEnabled(false);

    auto *lookupRow = new QHBoxLayout;
    lookupRow->setSpacing(8);
    lookupRow->addWidget(m_receipt, 1);
    lookupRow->addWidget(lookupBtn);
    lookupRow->addWidget(m_voidBtn);
    lookupBody->addLayout(lookupRow);

    m_saleInfo = new QLabel(QStringLiteral("Look up a receipt to begin."), processTab);
    m_saleInfo->setObjectName(QStringLiteral("muted"));
    m_saleInfo->setWordWrap(true);
    lookupBody->addWidget(m_saleInfo);

    // ── Sale lines card ─────────────────────────────────────────────────────
    QVBoxLayout *linesBody = nullptr;
    auto *linesCard = makeCard(processTab, QStringLiteral("Sale lines"),
                               QStringLiteral("Select a line to refund."), &linesBody);

    m_lines = new QTableWidget(processTab);
    m_lines->setColumnCount(6);
    m_lines->setHorizontalHeaderLabels({QStringLiteral("Item"), QStringLiteral("Batch"),
                                        QStringLiteral("Sold"), QStringLiteral("Returned"),
                                        QStringLiteral("Returnable"), QStringLiteral("Unit MRP")});
    // Right-align headers over the numeric columns (Sold, Returned, Returnable, Unit MRP).
    for (int col : {2, 3, 4, 5}) {
        m_lines->horizontalHeaderItem(col)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    m_lines->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_lines->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_lines->setSelectionMode(QAbstractItemView::SingleSelection);
    m_lines->verticalHeader()->setVisible(false);
    m_lines->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    UiUtil::emptyState(m_lines, QStringLiteral("No sale loaded."));
    linesBody->addWidget(m_lines);

    // ── Process-return card ─────────────────────────────────────────────────
    QVBoxLayout *retBody = nullptr;
    auto *retCard = makeCard(
        processTab, QStringLiteral("Process return"),
        QStringLiteral("Refund the selected line. Restocking returns goods to saleable stock."),
        &retBody);

    m_qty = new QSpinBox(processTab);
    m_qty->setMinimum(1);
    m_qty->setMaximum(1);
    m_qty->setMinimumWidth(80);
    m_reason = new QComboBox(processTab);
    for (const char *v : kReasonValues) {
        m_reason->addItem(QString::fromLatin1(v));
    }
    m_reason->setMinimumWidth(220);
    m_condition = new QLineEdit(processTab);
    m_condition->setPlaceholderText(QStringLiteral("Physical condition (optional)"));
    m_condition->setMinimumWidth(280);
    m_condition->setMaximumWidth(560);
    m_restock = new QCheckBox(QStringLiteral("Return to saleable stock"), processTab);
    m_restock->setChecked(true);
    m_processBtn = new QPushButton(QStringLiteral("Refund"), processTab);
    m_processBtn->setProperty("variant", "primary");
    m_processBtn->setEnabled(false);

    auto *retForm = new QFormLayout;
    retForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    retForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    retForm->setHorizontalSpacing(16);
    retForm->setVerticalSpacing(10);
    retForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    retForm->addRow(QStringLiteral("Quantity"), m_qty);
    retForm->addRow(QStringLiteral("Reason"), m_reason);
    retForm->addRow(QStringLiteral("Physical condition"), m_condition);
    retForm->addRow(QStringLiteral("Restock"), m_restock);
    retBody->addLayout(retForm);

    auto *retFooter = new QHBoxLayout;
    retFooter->setSpacing(8);
    retFooter->addStretch();
    retFooter->addWidget(m_processBtn);
    retBody->addLayout(retFooter);

    // ── Recent returns card ─────────────────────────────────────────────────
    QVBoxLayout *recentBody = nullptr;
    auto *recentCard
        = makeCard(processTab, QStringLiteral("Recent returns"), QString(), &recentBody);
    m_recent = new QTableWidget(processTab);
    m_recent->setColumnCount(7);
    m_recent->setHorizontalHeaderLabels({QStringLiteral("Return #"), QStringLiteral("Receipt"),
                                         QStringLiteral("Item"), QStringLiteral("Qty"),
                                         QStringLiteral("Refund"), QStringLiteral("Reason"),
                                         QStringLiteral("Status")});
    // Right-align headers over the numeric columns (Qty, Refund).
    for (int col : {3, 4}) {
        m_recent->horizontalHeaderItem(col)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    m_recent->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_recent->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_recent->verticalHeader()->setVisible(false);
    m_recent->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_recent->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    recentBody->addWidget(m_recent);

    auto *processLayout = new QVBoxLayout(processTab);
    processLayout->setContentsMargins(0, 0, 0, 0);
    processLayout->setSpacing(16);
    processLayout->addWidget(lookupCard);
    processLayout->addWidget(linesCard, 2);
    processLayout->addWidget(retCard);
    processLayout->addWidget(recentCard, 1);

    connect(lookupBtn, &QPushButton::clicked, this, &ReturnsPage::lookup);
    connect(m_receipt, &QLineEdit::returnPressed, this, &ReturnsPage::lookup);
    connect(m_voidBtn, &QPushButton::clicked, this, &ReturnsPage::voidSale);
    connect(m_lines, &QTableWidget::itemSelectionChanged, this, &ReturnsPage::lineSelectionChanged);
    connect(m_processBtn, &QPushButton::clicked, this, &ReturnsPage::processReturn);

    return processTab;
}

// ── "Review queue" tab (adjudication) — MANAGER/ADMIN only ──────────────
QWidget *ReturnsPage::buildReviewQueueTab()
{
    auto *returnsTab = new QWidget;
    m_pendingReturns = new QTableWidget(returnsTab);
    m_pendingReturns->setColumnCount(10);
    m_pendingReturns->setHorizontalHeaderLabels(
        {QStringLiteral("Return #"), QStringLiteral("Receipt"), QStringLiteral("Medicine"),
         QStringLiteral("Batch"), QStringLiteral("Qty"), QStringLiteral("Refund"),
         QStringLiteral("Reason"), QStringLiteral("Condition"), QStringLiteral("By"),
         QStringLiteral("When")});
    m_pendingReturns->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pendingReturns->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pendingReturns->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pendingReturns->verticalHeader()->setVisible(false);
    m_pendingReturns->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    // Right-align numeric/money column headers (Qty, Refund) over their values.
    for (int col : {4, 5}) {
        m_pendingReturns->horizontalHeaderItem(col)->setTextAlignment(Qt::AlignRight
                                                                      | Qt::AlignVCenter);
    }

    m_returnNotes = new QLineEdit(returnsTab);
    m_returnNotes->setPlaceholderText(QStringLiteral("Adjudication notes (optional)"));
    m_returnNotes->setMinimumWidth(280);
    m_returnNotes->setMaximumWidth(560);

    auto *approveBtn = new QPushButton(QStringLiteral("Approve & restock"), returnsTab);
    approveBtn->setProperty("variant", "primary");
    auto *toSupplierBtn = new QPushButton(QStringLiteral("Return to supplier"), returnsTab);
    toSupplierBtn->setProperty("variant", "secondary");
    auto *writeOffBtn = new QPushButton(QStringLiteral("Write off"), returnsTab);
    writeOffBtn->setProperty("variant", "destructive");
    connect(approveBtn, &QPushButton::clicked, this,
            [this] { adjudicateSelected(QStringLiteral("APPROVED_RESTOCK")); });
    connect(toSupplierBtn, &QPushButton::clicked, this,
            [this] { adjudicateSelected(QStringLiteral("RETURN_TO_SUPPLIER")); });
    connect(writeOffBtn, &QPushButton::clicked, this,
            [this] { adjudicateSelected(QStringLiteral("WRITE_OFF")); });

    QVBoxLayout *rBody = nullptr;
    auto *rCard = makeCard(
        returnsTab, QStringLiteral("Review queue"),
        QStringLiteral(
            "Returns awaiting review (oldest first). Pick a return, then choose a disposition."),
        &rBody);
    rBody->addWidget(m_pendingReturns, 1);

    auto *notesForm = new QFormLayout;
    notesForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    notesForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    notesForm->setHorizontalSpacing(16);
    notesForm->setVerticalSpacing(10);
    notesForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    notesForm->addRow(QStringLiteral("Notes"), m_returnNotes);
    rBody->addLayout(notesForm);

    auto *rBtns = new QHBoxLayout;
    rBtns->setSpacing(8);
    rBtns->addStretch();
    rBtns->addWidget(writeOffBtn);
    rBtns->addWidget(toSupplierBtn);
    rBtns->addWidget(approveBtn);
    rBody->addLayout(rBtns);

    auto *rLayout = new QVBoxLayout(returnsTab);
    rLayout->setContentsMargins(0, 0, 0, 0);
    rLayout->setSpacing(16);
    rLayout->addWidget(rCard, 1);

    return returnsTab;
}

// ── "Supplier returns" tab (settlement) — MANAGER/ADMIN only ───────────
QWidget *ReturnsPage::buildSupplierReturnsTab()
{
    auto *supplierTab = new QWidget;
    m_supplierReturns = new QTableWidget(supplierTab);
    m_supplierReturns->setColumnCount(7);
    m_supplierReturns->setHorizontalHeaderLabels(
        {QStringLiteral("Return #"), QStringLiteral("Medicine"), QStringLiteral("Qty"),
         QStringLiteral("Refund"), QStringLiteral("Adjudicated"), QStringLiteral("Settled?"),
         QStringLiteral("Reference")});
    m_supplierReturns->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_supplierReturns->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_supplierReturns->setSelectionMode(QAbstractItemView::SingleSelection);
    m_supplierReturns->verticalHeader()->setVisible(false);
    m_supplierReturns->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    // Right-align numeric/money column headers (Qty, Refund) over their values.
    for (int col : {2, 3}) {
        m_supplierReturns->horizontalHeaderItem(col)->setTextAlignment(Qt::AlignRight
                                                                       | Qt::AlignVCenter);
    }

    m_markSentBtn = new QPushButton(QStringLiteral("Mark as sent to supplier"), supplierTab);
    m_markSentBtn->setProperty("variant", "primary");
    m_markSentBtn->setEnabled(false);
    connect(m_markSentBtn, &QPushButton::clicked, this, &ReturnsPage::markSupplierSent);
    connect(m_supplierReturns, &QTableWidget::itemSelectionChanged, this, [this] {
        bool settled = true;
        const qint64 id = selectedSupplierReturnId(&settled);
        m_markSentBtn->setEnabled(id != 0 && !settled);
    });

    QVBoxLayout *sBody = nullptr;
    auto *sCard = makeCard(
        supplierTab, QStringLiteral("Supplier returns"),
        QStringLiteral("Returns shipped back to suppliers for credit (unsettled first)."), &sBody);
    sBody->addWidget(m_supplierReturns, 1);

    auto *sBtns = new QHBoxLayout;
    sBtns->setSpacing(8);
    sBtns->addStretch();
    sBtns->addWidget(m_markSentBtn);
    sBody->addLayout(sBtns);

    auto *sLayout = new QVBoxLayout(supplierTab);
    sLayout->setContentsMargins(0, 0, 0, 0);
    sLayout->setSpacing(16);
    sLayout->addWidget(sCard, 1);

    return supplierTab;
}

void ReturnsPage::reload()
{
    reloadRecent();
    if (m_pendingReturns) {
        reloadPendingReturns();
    }
    if (m_supplierReturns) {
        reloadSupplierReturns();
    }
}

void ReturnsPage::clearSale()
{
    m_currentSaleId = 0;
    m_currentStatus.clear();
    m_voidBtn->setEnabled(false);
    m_processBtn->setEnabled(false);
    UiUtil::beginFill(m_lines);
    m_lines->setRowCount(0);
    UiUtil::emptyState(m_lines, QStringLiteral("No sale loaded."));
    m_qty->setMaximum(1);
    m_qty->setValue(1);
}

void ReturnsPage::lookup()
{
    const QString receipt = m_receipt->text().trimmed();
    if (receipt.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Look up"),
                                 QStringLiteral("Type a receipt number first."));
        return;
    }
    ReturnRepository repo(m_db);
    const ReturnSaleHeader hdr = repo.lookupByReceipt(receipt);
    if (!hdr.found) {
        clearSale();
        m_saleInfo->setText(QStringLiteral("No sale found with receipt '%1'.").arg(receipt));
        return;
    }
    showSale(hdr);
}

void ReturnsPage::showSale(const ReturnSaleHeader &hdr)
{
    m_currentSaleId = hdr.saleId;
    m_currentStatus = hdr.status;

    m_saleInfo->setText(
        QStringLiteral("<b>%1</b> &nbsp; %2 &nbsp;·&nbsp; Cashier: %3 &nbsp;·&nbsp; %4 "
                       "&nbsp;·&nbsp; Total: PKR %5 &nbsp;·&nbsp; Status: <b>%6</b>")
            .arg(hdr.receiptNumber.toHtmlEscaped(), hdr.soldAt.left(19).toHtmlEscaped(),
                 hdr.cashierName.toHtmlEscaped(), hdr.paymentMode.toHtmlEscaped(),
                 Money::fromString(hdr.grandTotal).fmt(), hdr.status.toHtmlEscaped()));

    const bool refundable
        = hdr.status != QLatin1String("VOIDED") && hdr.status != QLatin1String("REFUNDED_FULL");
    // Void only allowed while still COMPLETED.
    m_voidBtn->setEnabled(hdr.status == QLatin1String("COMPLETED"));

    UiUtil::beginFill(m_lines);
    m_lines->setRowCount(hdr.lines.size());
    for (int i = 0; i < hdr.lines.size(); ++i) {
        const ReturnableLine &l = hdr.lines.at(i);
        const int remaining = qMax(0, l.qtySold - l.qtyAlreadyReturned);

        auto *name = new QTableWidgetItem(l.name);
        name->setData(Qt::UserRole, l.saleItemId);
        name->setData(Qt::UserRole + 1, remaining);
        name->setData(Qt::UserRole + 2, l.medicineId);
        m_lines->setItem(i, 0, name);
        m_lines->setItem(i, 1, new QTableWidgetItem(l.batchNumber));

        auto *sold = new QTableWidgetItem(QStringLiteral("%1 %2 (=%3)")
                                              .arg(l.qtySoldDisplay)
                                              .arg(l.soldUnitLabel)
                                              .arg(l.qtySold));
        UiUtil::rightAlign(sold);
        m_lines->setItem(i, 2, sold);

        auto *ret = new QTableWidgetItem(QString::number(l.qtyAlreadyReturned));
        UiUtil::rightAlign(ret);
        m_lines->setItem(i, 3, ret);

        auto *rem = new QTableWidgetItem(QString::number(remaining));
        UiUtil::rightAlign(rem);
        if (remaining == 0) UiUtil::colorItem(rem, UiUtil::Palette::Muted);
        m_lines->setItem(i, 4, rem);

        auto *mrp = new QTableWidgetItem(Money::fromString(l.unitMrp).display());
        UiUtil::rightAlign(mrp);
        m_lines->setItem(i, 5, mrp);
    }
    UiUtil::emptyState(m_lines, QStringLiteral("This sale has no items."));

    if (!refundable) {
        m_saleInfo->setText(
            m_saleInfo->text()
            + QStringLiteral(
                  "<br><span style='color:%1;'>No further refunds allowed on this sale.</span>")
                  .arg(UiUtil::Palette::Danger.name()));
    }
    m_processBtn->setEnabled(false);
}

qint64 ReturnsPage::selectedSaleItemId(int *outRemaining) const
{
    const int row = m_lines->currentRow();
    if (row < 0 || !m_lines->item(row, 0)) {
        return 0;
    }
    if (outRemaining) {
        *outRemaining = m_lines->item(row, 0)->data(Qt::UserRole + 1).toInt();
    }
    return m_lines->item(row, 0)->data(Qt::UserRole).toLongLong();
}

void ReturnsPage::lineSelectionChanged()
{
    int remaining = 0;
    const qint64 id = selectedSaleItemId(&remaining);
    const bool refundable = m_currentStatus != QLatin1String("VOIDED")
                            && m_currentStatus != QLatin1String("REFUNDED_FULL");
    const bool canRefund = id != 0 && remaining > 0 && refundable;
    m_processBtn->setEnabled(canRefund);
    if (canRefund) {
        m_qty->setMaximum(remaining);
        m_qty->setValue(qMin(m_qty->value(), remaining));
    } else {
        m_qty->setMaximum(1);
        m_qty->setValue(1);
    }
}

void ReturnsPage::processReturn()
{
    int remaining = 0;
    const qint64 saleItemId = selectedSaleItemId(&remaining);
    if (saleItemId == 0) {
        QMessageBox::information(this, QStringLiteral("Return"),
                                 QStringLiteral("Select a line first."));
        return;
    }
    const int qty = m_qty->value();
    if (qty < 1 || qty > remaining) {
        QMessageBox::warning(this, QStringLiteral("Return"),
                             QStringLiteral("Quantity must be between 1 and %1.").arg(remaining));
        return;
    }
    const QString reason = m_reason->currentText();
    const bool restock = m_restock->isChecked();

    if (QMessageBox::question(
            this, QStringLiteral("Confirm refund"),
            QStringLiteral("Refund %1 unit(s)?\nReason: %2\nDisposition: %3")
                .arg(qty)
                .arg(reason, restock ? QStringLiteral("RESTOCK") : QStringLiteral("WRITE-OFF")))
        != QMessageBox::Yes) {
        return;
    }

    // Refund authorization (segregation of duties — PHP pos/returns/initiate.php):
    // a CASHIER must enter a manager/admin PIN (a different person); a manager or
    // admin operator may self-authorize. verifyManagerOverride audits the grant
    // (OVERRIDE_GRANTED), recording who authorized the refund.
    UserRepository authUsers(m_db);
    qint64 authorizingManagerId = m_userId; // manager/admin operators self-authorize
    if (!authUsers.isActiveManagerOrAdmin(m_userId)) {
        ManagerOverrideDialog authDlg(
            QStringLiteral("Authorize refund"),
            QStringLiteral("A manager or admin PIN is required to authorize this refund."),
            /*needLicense=*/false, this);
        if (authDlg.exec() != QDialog::Accepted) {
            return;
        }
        const UserRepository::OverrideResult authOvr = authUsers.verifyManagerOverride(
            authDlg.pin(), m_userId, QStringLiteral("return_initiate"));
        if (!authOvr.ok) {
            QMessageBox::warning(this, QStringLiteral("Authorization required"), authOvr.error);
            return;
        }
        authorizingManagerId = authOvr.userId;
    }

    // Controlled medicines need a manager/admin witness (≠ the operator).
    qint64 witnessUserId = 0;
    const int row = m_lines->currentRow();
    const qint64 medicineId = (row >= 0 && m_lines->item(row, 0))
                                  ? m_lines->item(row, 0)->data(Qt::UserRole + 2).toLongLong()
                                  : 0;
    QString schedule;
    if (medicineId != 0) {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral("SELECT controlled_schedule FROM medicines WHERE id = ?"));
        q.addBindValue(medicineId);
        if (q.exec() && q.next()) {
            schedule = q.value(0).toString();
        }
    }
    if (ControlledSubstancePolicy::requiresWitness(schedule)) {
        ManagerOverrideDialog dlg(
            QStringLiteral("Controlled return"),
            QStringLiteral(
                "A manager/admin witness PIN is required to return a controlled medicine."),
            /*needLicense=*/false, this);
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
        // Witness must differ from BOTH the cashier and the authorizing manager
        // (PHP defence-in-depth). Forbidding the authorizing manager suffices: a
        // witness must be a manager/admin, so the cashier is already excluded.
        UserRepository users(m_db);
        const UserRepository::OverrideResult ovr = users.verifyManagerOverride(
            dlg.pin(), authorizingManagerId, QStringLiteral("return_witness"));
        if (!ovr.ok) {
            QMessageBox::warning(this, QStringLiteral("Witness required"), ovr.error);
            return;
        }
        witnessUserId = ovr.userId;
    }

    ReturnService svc(m_db, m_userId);
    const ReturnResult res
        = svc.commitReturn(saleItemId, qty, reason, restock, m_condition->text(), witnessUserId);
    if (!res.ok) {
        QMessageBox::warning(this, QStringLiteral("Return failed"), res.error);
        return;
    }
    QMessageBox::information(
        this, QStringLiteral("Refund processed"),
        QStringLiteral("Return %1 — refund PKR %2.\nSale is now %3.")
            .arg(res.returnNumber, Money::fromString(res.refundAmount).fmt(), res.saleStatus));

    m_condition->clear();
    reloadRecent();
    // Refresh the loaded sale so qtys/status update.
    lookup();
}

void ReturnsPage::voidSale()
{
    if (m_currentSaleId == 0) {
        QMessageBox::information(this, QStringLiteral("Void"),
                                 QStringLiteral("Look up a sale first."));
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("Void sale"),
            QStringLiteral("Void this entire sale? All items will be restocked and the sale "
                           "marked VOIDED. This cannot be undone."))
        != QMessageBox::Yes) {
        return;
    }
    ReturnService svc(m_db, m_userId);
    const VoidResult res = svc.voidSale(m_currentSaleId, /*sameDayOnly=*/false);
    if (!res.ok) {
        QMessageBox::warning(this, QStringLiteral("Void failed"), res.error);
        return;
    }
    QMessageBox::information(this, QStringLiteral("Sale voided"),
                             QStringLiteral("The sale has been voided and stock restored."));
    reloadRecent();
    lookup();
}

void ReturnsPage::reloadRecent()
{
    ReturnRepository repo(m_db);
    const QVector<ReturnRow> rows = repo.listRecent();
    UiUtil::beginFill(m_recent);
    m_recent->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const ReturnRow &r = rows.at(i);
        m_recent->setItem(i, 0, new QTableWidgetItem(r.returnNumber));
        m_recent->setItem(i, 1, new QTableWidgetItem(r.receiptNumber));
        m_recent->setItem(i, 2, new QTableWidgetItem(r.medicineName));
        auto *qty = new QTableWidgetItem(QString::number(r.qtyReturned));
        UiUtil::rightAlign(qty);
        m_recent->setItem(i, 3, qty);
        auto *refund = new QTableWidgetItem(Money::fromString(r.refundAmount).display());
        UiUtil::rightAlign(refund);
        m_recent->setItem(i, 4, refund);
        m_recent->setItem(i, 5, new QTableWidgetItem(r.reason));
        const QColor stColor
            = r.status == QLatin1String("APPROVED_RESTOCK") ? UiUtil::Palette::Success
              : r.status == QLatin1String("WRITE_OFF")      ? UiUtil::Palette::Danger
              : r.status == QLatin1String("PENDING_REVIEW") ? UiUtil::Palette::Warning
                                                            : UiUtil::Palette::Muted;
        UiUtil::setBadge(m_recent, i, 6, r.status, stColor);
    }
    UiUtil::emptyState(m_recent, QStringLiteral("No returns yet."));
}

// ── Review queue (adjudication) ─────────────────────────────────────────────

void ReturnsPage::reloadPendingReturns()
{
    ReturnRepository repo(m_db);
    const QVector<PendingReturn> rows = repo.listPending();
    UiUtil::beginFill(m_pendingReturns);
    m_pendingReturns->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const PendingReturn &r = rows.at(i);
        auto *num = new QTableWidgetItem(r.returnNumber);
        num->setData(Qt::UserRole, r.id);
        m_pendingReturns->setItem(i, 0, num);
        m_pendingReturns->setItem(i, 1, new QTableWidgetItem(r.receiptNumber));
        m_pendingReturns->setItem(i, 2, new QTableWidgetItem(r.medicineName));
        m_pendingReturns->setItem(i, 3, new QTableWidgetItem(r.batchNumber));
        auto *qty = new QTableWidgetItem(QString::number(r.qtyReturned));
        UiUtil::rightAlign(qty);
        m_pendingReturns->setItem(i, 4, qty);
        auto *refund = new QTableWidgetItem(Money::fromString(r.refundAmount).display());
        UiUtil::rightAlign(refund);
        m_pendingReturns->setItem(i, 5, refund);
        m_pendingReturns->setItem(i, 6, new QTableWidgetItem(r.reason));
        m_pendingReturns->setItem(i, 7, new QTableWidgetItem(r.physicalCondition));
        m_pendingReturns->setItem(i, 8, new QTableWidgetItem(r.initiatedBy));
        m_pendingReturns->setItem(i, 9, new QTableWidgetItem(r.createdAt.left(19)));
    }
    UiUtil::emptyState(m_pendingReturns, QStringLiteral("No returns awaiting review."));
}

qint64 ReturnsPage::selectedPendingReturnId() const
{
    const int row = m_pendingReturns->currentRow();
    if (row < 0 || !m_pendingReturns->item(row, 0)) {
        return 0;
    }
    return m_pendingReturns->item(row, 0)->data(Qt::UserRole).toLongLong();
}

void ReturnsPage::adjudicateSelected(const QString &newStatus)
{
    const qint64 id = selectedPendingReturnId();
    if (id == 0) {
        QMessageBox::information(this, QStringLiteral("Returns review"),
                                 QStringLiteral("Select a return first."));
        return;
    }
    ReturnService svc(m_db, m_userId);
    const ReturnOpResult res = svc.adjudicate(id, newStatus, m_returnNotes->text().trimmed());
    if (!res.ok) {
        QMessageBox::warning(this, QStringLiteral("Adjudication failed"), res.error);
        return;
    }
    m_returnNotes->clear();
    reloadPendingReturns();
    reloadSupplierReturns();
    reloadRecent();
}

// ── Supplier returns (settlement) ───────────────────────────────────────────

void ReturnsPage::reloadSupplierReturns()
{
    ReturnRepository repo(m_db);
    const QVector<SupplierReturn> rows = repo.listSupplierReturns();
    UiUtil::beginFill(m_supplierReturns);
    m_supplierReturns->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const SupplierReturn &r = rows.at(i);
        auto *num = new QTableWidgetItem(r.returnNumber);
        num->setData(Qt::UserRole, r.id);
        num->setData(Qt::UserRole + 1, r.settled);
        m_supplierReturns->setItem(i, 0, num);
        m_supplierReturns->setItem(i, 1, new QTableWidgetItem(r.medicineName));
        auto *qty = new QTableWidgetItem(QString::number(r.qtyReturned));
        UiUtil::rightAlign(qty);
        m_supplierReturns->setItem(i, 2, qty);
        auto *refund = new QTableWidgetItem(Money::fromString(r.refundAmount).display());
        UiUtil::rightAlign(refund);
        m_supplierReturns->setItem(i, 3, refund);
        m_supplierReturns->setItem(i, 4, new QTableWidgetItem(r.adjudicatedAt.left(19)));
        auto *settled = new QTableWidgetItem(
            r.settled ? QStringLiteral("Settled %1").arg(r.settledAt.left(19))
                      : QStringLiteral("Pending"));
        if (r.settled)
            UiUtil::colorItem(settled, UiUtil::Palette::Success, true);
        else
            UiUtil::colorItem(settled, UiUtil::Palette::Warning);
        m_supplierReturns->setItem(i, 5, settled);
        m_supplierReturns->setItem(i, 6, new QTableWidgetItem(r.supplierReference));
    }
    UiUtil::emptyState(m_supplierReturns, QStringLiteral("No supplier returns."));
    m_markSentBtn->setEnabled(false);
}

qint64 ReturnsPage::selectedSupplierReturnId(bool *outSettled) const
{
    const int row = m_supplierReturns->currentRow();
    if (row < 0 || !m_supplierReturns->item(row, 0)) {
        return 0;
    }
    if (outSettled) {
        *outSettled = m_supplierReturns->item(row, 0)->data(Qt::UserRole + 1).toBool();
    }
    return m_supplierReturns->item(row, 0)->data(Qt::UserRole).toLongLong();
}

void ReturnsPage::markSupplierSent()
{
    bool settled = true;
    const qint64 id = selectedSupplierReturnId(&settled);
    if (id == 0) {
        QMessageBox::information(this, QStringLiteral("Supplier returns"),
                                 QStringLiteral("Select an unsettled return first."));
        return;
    }
    if (settled) {
        QMessageBox::information(this, QStringLiteral("Supplier returns"),
                                 QStringLiteral("This return is already settled."));
        return;
    }
    bool okPressed = false;
    const QString ref = QInputDialog::getText(
        this, QStringLiteral("Mark as sent to supplier"),
        QStringLiteral("Supplier credit-note / waybill reference (optional):"), QLineEdit::Normal,
        QString(), &okPressed);
    if (!okPressed) {
        return;
    }
    ReturnService svc(m_db, m_userId);
    const ReturnOpResult res = svc.markSupplierReturnSent(id, ref.trimmed());
    if (!res.ok) {
        QMessageBox::warning(this, QStringLiteral("Mark sent failed"), res.error);
        return;
    }
    reloadSupplierReturns();
}
