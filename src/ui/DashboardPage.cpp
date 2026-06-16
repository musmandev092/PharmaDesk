#include "ui/DashboardPage.h"

#include "data/InventoryRepository.h"
#include "data/SalesReportRepository.h"
#include "domain/Money.h"
#include "ui/UiUtil.h"

#include <QDate>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
// A KPI card: caption + big value + optional accent colour on the value.
QFrame *kpiCard(const QString &caption, QLabel **valueOut, QWidget *parent,
                const QString &accent = QString())
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("card"));
    card->setMinimumWidth(150);
    auto *l = new QVBoxLayout(card);
    l->setContentsMargins(16, 16, 16, 16);
    l->setSpacing(8);
    auto *cap = new QLabel(caption, card);
    cap->setObjectName(QStringLiteral("kpiLabel"));
    auto *val = new QLabel(QStringLiteral("—"), card);
    val->setObjectName(QStringLiteral("kpiValue"));
    if (!accent.isEmpty()) {
        val->setStyleSheet(QStringLiteral("color:%1;").arg(accent));
    }
    l->addWidget(cap);
    l->addWidget(val);
    *valueOut = val;
    return card;
}
} // namespace

DashboardPage::DashboardPage(QSqlDatabase db, QWidget *parent)
    : QWidget(parent), m_db(std::move(db))
{
    auto *title = new QLabel(QStringLiteral("Dashboard"), this);
    title->setObjectName(QStringLiteral("h1"));
    auto *subtitle
        = new QLabel(QStringLiteral("Today at a glance — jump to any area below."), this);
    subtitle->setObjectName(QStringLiteral("muted"));

    // ── KPI row ────────────────────────────────────────────────────────────
    auto *kpis = new QHBoxLayout;
    kpis->setSpacing(12);
    kpis->addWidget(kpiCard(QStringLiteral("Sales today"), &m_kpiTodaySales, this));
    kpis->addWidget(kpiCard(QStringLiteral("Revenue today"), &m_kpiTodayNet, this,
                            UiUtil::Palette::Success.name()));
    kpis->addWidget(kpiCard(QStringLiteral("Stock value (cost)"), &m_kpiStockValue, this));
    kpis->addWidget(
        kpiCard(QStringLiteral("Low stock"), &m_kpiLowStock, this, UiUtil::Palette::Danger.name()));
    kpis->addWidget(kpiCard(QStringLiteral("Expiring in 60 days"), &m_kpiExpiring, this,
                            UiUtil::Palette::Warning.name()));
    kpis->addWidget(kpiCard(QStringLiteral("Active medicines"), &m_kpiMedicines, this));

    // ── Quick-action tiles (navigate to each section) ──────────────────────
    auto *quick = new QGridLayout;
    quick->setSpacing(12);
    struct Tile
    {
        const char *label;
        const char *section;
        const char *variant;
    };
    const Tile tiles[] = {
        {"New sale", "Point of sale", nullptr},
        {"Returns and refunds", "Returns & refunds", "secondary"},
        {"Cashier sessions", "Cashier sessions", "secondary"},
        {"Medicines", "Medicines", "secondary"},
        {"Inventory", "Inventory", "secondary"},
        {"Purchasing", "Purchasing", "secondary"},
        {"Reports", "Reports", "secondary"},
        {"Admin and settings", "Admin & settings", "secondary"},
    };
    int n = 0;
    for (const Tile &t : tiles) {
        auto *b = new QPushButton(QString::fromUtf8(t.label), this);
        if (t.variant) b->setProperty("variant", QString::fromLatin1(t.variant));
        b->setMinimumHeight(48);
        const QString section = QString::fromLatin1(t.section);
        connect(b, &QPushButton::clicked, this, [this, section]() { emit openSection(section); });
        quick->addWidget(b, n / 4, n % 4);
        ++n;
    }

    // ── Shortlists ─────────────────────────────────────────────────────────
    auto makeList = [this](const QStringList &headers) {
        auto *t = new QTableWidget(this);
        t->setColumnCount(headers.size());
        t->setHorizontalHeaderLabels(headers);
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        t->setSelectionMode(QAbstractItemView::NoSelection);
        t->setSelectionBehavior(QAbstractItemView::SelectRows);
        t->verticalHeader()->setVisible(false);
        t->setWordWrap(false);
        t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        return t;
    };
    m_lowStock = makeList({QStringLiteral("Low-stock medicine"), QStringLiteral("On hand"),
                           QStringLiteral("Reorder at")});
    m_expiring = makeList(
        {QStringLiteral("Expiring soon"), QStringLiteral("Expiry"), QStringLiteral("Qty")});

    auto *lowBox = new QVBoxLayout;
    lowBox->setSpacing(12);
    auto *lowH = new QLabel(QStringLiteral("Low stock"), this);
    lowH->setObjectName(QStringLiteral("h2"));
    lowBox->addWidget(lowH);
    lowBox->addWidget(m_lowStock);
    auto *expBox = new QVBoxLayout;
    expBox->setSpacing(12);
    auto *expH = new QLabel(QStringLiteral("Expiring in 60 days"), this);
    expH->setObjectName(QStringLiteral("h2"));
    expBox->addWidget(expH);
    expBox->addWidget(m_expiring);
    auto *lists = new QHBoxLayout;
    lists->setSpacing(16);
    lists->addLayout(lowBox);
    lists->addLayout(expBox);

    auto *content = new QWidget(this);
    content->setObjectName(QStringLiteral("scrollContent"));
    content->setAutoFillBackground(false);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addLayout(kpis);
    layout->addLayout(quick);
    layout->addLayout(lists, 1);

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

    reload();
}

void DashboardPage::reload()
{
    InventoryRepository inv(m_db);
    const InventoryTotals t = inv.totals();
    m_kpiStockValue->setText(Money::fromString(t.stockValueCost).display());
    m_kpiMedicines->setText(QString::number(t.medicinesCount));

    const QVector<LowStockRow> low = inv.lowStock(0);
    const QVector<ExpiringRow> exp = inv.expiringSoon(60, 0);
    m_kpiLowStock->setText(QString::number(low.size()));
    m_kpiExpiring->setText(QString::number(exp.size()));

    SalesReportRepository rep(m_db);
    const QDate today = QDate::currentDate();
    const SalesReport day = rep.report(today, today);
    m_kpiTodaySales->setText(QString::number(day.summary.count));
    m_kpiTodayNet->setText(Money::fromString(day.summary.net).display());

    // Low-stock shortlist (top 8).
    UiUtil::beginFill(m_lowStock);
    const int lowN = qMin(8, low.size());
    m_lowStock->setRowCount(lowN);
    for (int i = 0; i < lowN; ++i) {
        const LowStockRow &r = low.at(i);
        m_lowStock->setItem(i, 0, new QTableWidgetItem(r.brandName));
        auto *oh = new QTableWidgetItem(QStringLiteral("%1 %2").arg(r.onHand).arg(r.baseUnit));
        UiUtil::colorItem(oh, UiUtil::Palette::Danger, true);
        UiUtil::rightAlign(oh);
        m_lowStock->setItem(i, 1, oh);
        auto *rl = new QTableWidgetItem(QString::number(r.reorderLevel));
        UiUtil::rightAlign(rl);
        m_lowStock->setItem(i, 2, rl);
    }
    UiUtil::emptyState(m_lowStock, QStringLiteral("All medicines above reorder level."));

    // Expiring shortlist (top 8).
    UiUtil::beginFill(m_expiring);
    const int expN = qMin(8, exp.size());
    m_expiring->setRowCount(expN);
    for (int i = 0; i < expN; ++i) {
        const ExpiringRow &r = exp.at(i);
        m_expiring->setItem(i, 0, new QTableWidgetItem(r.brandName));
        auto *e = new QTableWidgetItem(
            QStringLiteral("%1 (%2d)").arg(r.expiry.toString(Qt::ISODate)).arg(r.days));
        UiUtil::colorItem(e, r.days <= 30 ? UiUtil::Palette::Warning : UiUtil::Palette::Info);
        m_expiring->setItem(i, 1, e);
        auto *q = new QTableWidgetItem(QStringLiteral("%1 %2").arg(r.currentQty).arg(r.baseUnit));
        UiUtil::rightAlign(q);
        m_expiring->setItem(i, 2, q);
    }
    UiUtil::emptyState(m_expiring, QStringLiteral("Nothing expiring in 60 days."));
}
