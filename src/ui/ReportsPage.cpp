#include "ui/ReportsPage.h"

#include "data/SaleRepository.h"
#include "data/SalesReportRepository.h"
#include "data/SettingsRepository.h"
#include "domain/Money.h"
#include "domain/SettingsKeys.h"
#include "ui/ComplianceReportWidget.h"
#include "ui/NarcoticRegisterWidget.h"
#include "ui/PlReportWidget.h"
#include "ui/ReceiptDialog.h"
#include "ui/RefundsReportWidget.h"
#include "ui/TaxReportWidget.h"
#include "ui/TopSellersWidget.h"
#include "ui/UiUtil.h"
#include "ui/VelocityReportWidget.h"

#include <QDateEdit>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

ReportsPage::ReportsPage(QSqlDatabase db, QWidget *parent) : QWidget(parent), m_db(std::move(db))
{
    // Page header.
    auto *title = new QLabel(QStringLiteral("Sales report"), this);
    title->setObjectName(QStringLiteral("h1"));
    auto *subtitle = new QLabel(
        QStringLiteral("Review sales over a date range, export them, or reprint a receipt."), this);
    subtitle->setObjectName(QStringLiteral("muted"));

    // --- Filter card -------------------------------------------------------
    m_from = new QDateEdit(this);
    m_from->setCalendarPopup(true);
    m_from->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_from->setDate(QDate::currentDate().addDays(-30));
    m_from->setMinimumWidth(150);
    m_from->setMaximumWidth(220);
    m_to = new QDateEdit(this);
    m_to->setCalendarPopup(true);
    m_to->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_to->setDate(QDate::currentDate());
    m_to->setMinimumWidth(150);
    m_to->setMaximumWidth(220);

    auto *filterBtn = new QPushButton(QStringLiteral("Filter"), this);

    auto *filterCard = new QFrame(this);
    filterCard->setObjectName(QStringLiteral("card"));
    auto *filterCardLayout = new QVBoxLayout(filterCard);
    filterCardLayout->setContentsMargins(20, 16, 20, 16);
    filterCardLayout->setSpacing(12);
    auto *filterHeader = new QLabel(QStringLiteral("Date range"), filterCard);
    filterHeader->setObjectName(QStringLiteral("h2"));
    filterCardLayout->addWidget(filterHeader);

    // Compact single-row filter bar: From / To / Filter.
    auto *filters = new QHBoxLayout;
    filters->setSpacing(16);
    auto *fromLabel = new QLabel(QStringLiteral("From"), filterCard);
    auto *toLabel = new QLabel(QStringLiteral("To"), filterCard);
    filters->addWidget(fromLabel);
    filters->addWidget(m_from);
    filters->addSpacing(8);
    filters->addWidget(toLabel);
    filters->addWidget(m_to);
    filters->addSpacing(8);
    filters->addWidget(filterBtn);
    filters->addStretch();
    filterCardLayout->addLayout(filters);

    // --- KPI card row ------------------------------------------------------
    auto makeKpi = [this](const QString &caption, QLabel **out) {
        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("card"));
        card->setMinimumWidth(150);
        auto *l = new QVBoxLayout(card);
        l->setContentsMargins(20, 16, 20, 16);
        l->setSpacing(4);
        auto *cap = new QLabel(caption, card);
        cap->setObjectName(QStringLiteral("muted"));
        auto *val = new QLabel(QStringLiteral("—"), card);
        val->setObjectName(QStringLiteral("h2"));
        l->addWidget(cap);
        l->addWidget(val);
        *out = val;
        return card;
    };
    auto *kpis = new QHBoxLayout;
    kpis->setSpacing(12);
    kpis->addWidget(makeKpi(QStringLiteral("Sales"), &m_kpiCount));
    kpis->addWidget(makeKpi(QStringLiteral("Net total"), &m_kpiNet));
    kpis->addWidget(makeKpi(QStringLiteral("Cash (net)"), &m_kpiCash));
    kpis->addWidget(makeKpi(QStringLiteral("Card (net)"), &m_kpiCard));
    kpis->addWidget(makeKpi(QStringLiteral("Refunds"), &m_kpiRefunds));

    // --- Sales table card --------------------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Receipt"), QStringLiteral("When"),
                                        QStringLiteral("Cashier"), QStringLiteral("Subtotal"),
                                        QStringLiteral("Discount"), QStringLiteral("Total"),
                                        QStringLiteral("Mode"), QStringLiteral("Status")});
    // Right-align headers over the numeric/money/date columns so the header text
    // sits above its values (the cells themselves are right-aligned below).
    for (int col : {1, 3, 4, 5}) {
        if (auto *hi = m_table->horizontalHeaderItem(col))
            hi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setWordWrap(false);

    auto *csvBtn = new QPushButton(QStringLiteral("Export CSV"), this);
    csvBtn->setProperty("variant", "secondary");
    auto *reprintBtn = new QPushButton(QStringLiteral("Reprint receipt"), this);

    auto *tableCard = new QFrame(this);
    tableCard->setObjectName(QStringLiteral("card"));
    auto *tableCardLayout = new QVBoxLayout(tableCard);
    tableCardLayout->setContentsMargins(20, 16, 20, 16);
    tableCardLayout->setSpacing(12);
    auto *tableHeader = new QLabel(QStringLiteral("Sales"), tableCard);
    tableHeader->setObjectName(QStringLiteral("h2"));
    auto *tableDesc
        = new QLabel(QStringLiteral("Double-click a row to reprint its receipt."), tableCard);
    tableDesc->setObjectName(QStringLiteral("muted"));
    tableCardLayout->addWidget(tableHeader);
    tableCardLayout->addWidget(tableDesc);
    tableCardLayout->addWidget(m_table, 1);

    // Right-aligned footer: secondary export, then primary reprint right-most.
    auto *footer = new QHBoxLayout;
    footer->setSpacing(8);
    footer->addStretch();
    footer->addWidget(csvBtn);
    footer->addWidget(reprintBtn);
    tableCardLayout->addLayout(footer);

    // Sales content lives in its own tab; analytics reports are sibling tabs.
    auto *salesTab = new QWidget(this);
    auto *salesLayout = new QVBoxLayout(salesTab);
    salesLayout->setContentsMargins(0, 0, 0, 0);
    salesLayout->setSpacing(16);
    salesLayout->addWidget(filterCard);
    salesLayout->addLayout(kpis);
    salesLayout->addWidget(tableCard, 1);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(salesTab, QStringLiteral("Sales"));
    tabs->addTab(new PlReportWidget(m_db, this), QStringLiteral("Profit && Loss"));
    tabs->addTab(new TopSellersWidget(m_db, this), QStringLiteral("Top sellers"));
    tabs->addTab(new TaxReportWidget(m_db, this), QStringLiteral("Tax"));
    tabs->addTab(new RefundsReportWidget(m_db, this), QStringLiteral("Refunds"));
    tabs->addTab(new ComplianceReportWidget(m_db, this), QStringLiteral("Compliance"));
    tabs->addTab(new VelocityReportWidget(m_db, this), QStringLiteral("Velocity"));
    tabs->addTab(new NarcoticRegisterWidget(m_db, this), QStringLiteral("Narcotic register"));

    // Wrap the whole page in a scroll area so nothing clips on small/short
    // screens. The themed page background must still show through, so the scroll
    // area and its content widget are kept transparent.
    auto *content = new QWidget;
    content->setObjectName(QStringLiteral("reportsScrollContent"));
    content->setStyleSheet(QStringLiteral("#reportsScrollContent { background: transparent; }"));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addWidget(tabs, 1);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("reportsScroll"));
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setStyleSheet(QStringLiteral("#reportsScroll, #reportsScroll > QWidget > QWidget "
                                         "{ background: transparent; border: none; }"));
    scroll->viewport()->setAutoFillBackground(false);

    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    pageLayout->addWidget(scroll);

    connect(filterBtn, &QPushButton::clicked, this, &ReportsPage::reload);
    connect(csvBtn, &QPushButton::clicked, this, &ReportsPage::exportCsv);
    connect(reprintBtn, &QPushButton::clicked, this, &ReportsPage::reprintSelected);
    connect(m_table, &QTableWidget::doubleClicked, this, &ReportsPage::reprintSelected);

    reload();
}

void ReportsPage::reload()
{
    SalesReportRepository repo(m_db);
    const SalesReport rep = repo.report(m_from->date(), m_to->date());
    const SalesSummary &s = rep.summary;

    m_kpiCount->setText(QString::number(s.count));
    m_kpiNet->setText(Money::fromString(s.net).display());
    m_kpiCash->setText(Money::fromString(s.netCash).display());
    m_kpiCard->setText(Money::fromString(s.netCard).display());
    m_kpiRefunds->setText(
        QStringLiteral("PKR %1 (%2)").arg(Money::fromString(s.refunds).fmt()).arg(s.refundCount));

    UiUtil::beginFill(m_table);
    m_table->setRowCount(rep.rows.size());
    for (int i = 0; i < rep.rows.size(); ++i) {
        const SalesReportRow &r = rep.rows.at(i);
        auto *rc = new QTableWidgetItem(r.receiptNumber);
        rc->setData(Qt::UserRole, r.saleId);
        m_table->setItem(i, 0, rc);
        auto *when = new QTableWidgetItem(r.soldAt);
        UiUtil::rightAlign(when);
        m_table->setItem(i, 1, when);
        m_table->setItem(i, 2, new QTableWidgetItem(r.cashierName));
        for (int c = 3; c <= 5; ++c) {
            const QString v = c == 3 ? r.subtotal : (c == 4 ? r.discountTotal : r.grandTotal);
            auto *it = new QTableWidgetItem(Money::fromString(v).display());
            UiUtil::rightAlign(it);
            m_table->setItem(i, c, it);
        }
        m_table->setItem(i, 6, new QTableWidgetItem(r.paymentMode));
        m_table->setItem(i, 7, new QTableWidgetItem(r.status));
    }
    UiUtil::emptyState(m_table, QStringLiteral("No sales in this date range."));
}

qint64 ReportsPage::selectedSaleId() const
{
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 0)) {
        return 0;
    }
    return m_table->item(row, 0)->data(Qt::UserRole).toLongLong();
}

void ReportsPage::exportCsv()
{
    SalesReportRepository repo(m_db);
    const SalesReport rep = repo.report(m_from->date(), m_to->date());
    const QString def
        = QStringLiteral("sales_%1_to_%2.csv")
              .arg(m_from->date().toString(Qt::ISODate), m_to->date().toString(Qt::ISODate));
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export CSV"), def,
                                                      QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    if (!SalesReportRepository::exportCsv(rep, path)) {
        QMessageBox::warning(this, QStringLiteral("Export"),
                             QStringLiteral("Could not write the file."));
        return;
    }
    QMessageBox::information(this, QStringLiteral("Export"), QStringLiteral("Saved %1").arg(path));
}

void ReportsPage::reprintSelected()
{
    const qint64 id = selectedSaleId();
    if (id == 0) {
        QMessageBox::information(this, QStringLiteral("Reprint"),
                                 QStringLiteral("Select a sale first."));
        return;
    }
    SaleRepository repo(m_db);
    const SaleResult sale = repo.loadReceipt(id);
    if (!sale.ok) {
        QMessageBox::warning(this, QStringLiteral("Reprint"), sale.error);
        return;
    }
    SettingsRepository settings(m_db);
    ReceiptDialog dlg(sale, settings.get(SettingsKeys::PharmacyName),
                      settings.get(SettingsKeys::LogoPath), m_db, 0, this);
    dlg.exec();
}
