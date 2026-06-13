#include "ui/InventoryPage.h"

#include "data/InventoryRepository.h"
#include "domain/Money.h"
#include "ui/AdjustmentsPage.h"
#include "ui/UiUtil.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <initializer_list>

namespace {
// Batch status label + colour, ported from stock.php's badge ladder.
struct Status
{
    QString text;
    QColor color;
};
Status statusFor(const StockBatchRow &b)
{
    if (b.expired || b.days <= 0) return {QStringLiteral("EXPIRED"), UiUtil::Palette::Danger};
    if (b.quarantined) return {QStringLiteral("QUARANTINED"), UiUtil::Palette::Danger};
    if (b.days <= 30) return {QStringLiteral("CONFIRM"), UiUtil::Palette::Warning};
    if (b.days <= 60) return {QStringLiteral("AMBER"), UiUtil::Palette::Info};
    if (b.days <= 90) return {QStringLiteral("INFO"), UiUtil::Palette::Muted};
    return {QStringLiteral("OK"), UiUtil::Palette::Success};
}

QTableWidget *makeTable(const QStringList &headers)
{
    auto *t = new QTableWidget;
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->verticalHeader()->setVisible(false);
    t->setWordWrap(false);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    return t;
}

// Right-align the given header columns so the header text sits over its
// (already right-aligned) numeric/money/qty values.
void rightAlignHeaders(QTableWidget *t, std::initializer_list<int> cols)
{
    for (int c : cols) {
        if (QTableWidgetItem *h = t->horizontalHeaderItem(c))
            h->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
}
} // namespace

InventoryPage::InventoryPage(QSqlDatabase db, qint64 userId, QWidget *parent)
    : QWidget(parent), m_db(std::move(db)), m_userId(userId)
{
    // Responsive: the entire page content lives inside a scroll area so nothing
    // clips on small/short screens; the page's own layout holds only the scroll
    // area with zero margins.
    auto *content = new QWidget;
    content->setObjectName(QStringLiteral("scrollContent"));
    content->setAutoFillBackground(false);

    auto *title = new QLabel(QStringLiteral("Inventory"), content);
    title->setObjectName(QStringLiteral("h1"));

    m_tabs = new QTabWidget(content);
    auto *overview = new QWidget;
    auto *batches = new QWidget;
    auto *low = new QWidget;
    auto *expiring = new QWidget;
    auto *adjust = new AdjustmentsPage(m_db, m_userId, this);
    buildOverview(overview);
    buildBatches(batches);
    buildLowStock(low);
    buildExpiring(expiring);
    m_tabs->addTab(overview, QStringLiteral("Overview"));
    m_tabs->addTab(batches, QStringLiteral("Batches"));
    m_tabs->addTab(low, QStringLiteral("Low stock"));
    m_tabs->addTab(expiring, QStringLiteral("Expiring soon"));
    m_tabs->addTab(adjust, QStringLiteral("Adjust stock"));

    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addWidget(title);
    layout->addWidget(m_tabs, 1);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("scrollArea"));
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setStyleSheet(
        QStringLiteral("#scrollArea, #scrollContent { background: transparent; border: none; }"));
    scroll->viewport()->setAutoFillBackground(false);

    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    pageLayout->addWidget(scroll);

    reload();
}

void InventoryPage::buildOverview(QWidget *tab)
{
    auto makeKpi = [tab](const QString &caption, QLabel **valueOut) {
        auto *card = new QFrame(tab);
        card->setObjectName(QStringLiteral("card"));
        card->setMinimumWidth(150); // keep KPI labels readable down to 1024px
        auto *l = new QVBoxLayout(card);
        l->setContentsMargins(16, 16, 16, 16);
        l->setSpacing(8);
        auto *cap = new QLabel(caption, card);
        cap->setObjectName(QStringLiteral("muted"));
        auto *val = new QLabel(QStringLiteral("—"), card);
        val->setObjectName(QStringLiteral("h2"));
        l->addWidget(cap);
        l->addWidget(val);
        *valueOut = val;
        return card;
    };

    auto *kpiRow = new QHBoxLayout;
    kpiRow->setSpacing(12);
    kpiRow->addWidget(makeKpi(QStringLiteral("Active medicines"), &m_kpiMedicines));
    kpiRow->addWidget(makeKpi(QStringLiteral("Active batches"), &m_kpiBatches));
    kpiRow->addWidget(makeKpi(QStringLiteral("Stock at cost"), &m_kpiCost));
    kpiRow->addWidget(makeKpi(QStringLiteral("Stock at MRP"), &m_kpiMrp));

    m_ovExpiring = makeTable({QStringLiteral("Medicine"), QStringLiteral("Batch"),
                              QStringLiteral("Expiry"), QStringLiteral("Qty")});
    rightAlignHeaders(m_ovExpiring, {2, 3}); // Expiry, Qty
    m_ovLow = makeTable(
        {QStringLiteral("Medicine"), QStringLiteral("On hand"), QStringLiteral("Reorder at")});
    rightAlignHeaders(m_ovLow, {1, 2}); // On hand, Reorder at

    auto *expBox = new QVBoxLayout;
    expBox->setSpacing(8);
    auto *eh = new QLabel(QStringLiteral("Expiring in 60 days"), tab);
    eh->setObjectName(QStringLiteral("h2"));
    expBox->addWidget(eh);
    expBox->addWidget(m_ovExpiring);

    auto *lowBox = new QVBoxLayout;
    lowBox->setSpacing(8);
    auto *lh = new QLabel(QStringLiteral("Low stock"), tab);
    lh->setObjectName(QStringLiteral("h2"));
    lowBox->addWidget(lh);
    lowBox->addWidget(m_ovLow);

    auto *lists = new QHBoxLayout;
    lists->setSpacing(16);
    lists->addLayout(expBox);
    lists->addLayout(lowBox);

    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addLayout(kpiRow);
    layout->addLayout(lists, 1);
}

void InventoryPage::buildBatches(QWidget *tab)
{
    m_batchSearch = new QLineEdit(tab);
    m_batchSearch->setPlaceholderText(QStringLiteral("Search brand, generic, batch #, SKU…"));
    m_batchSearch->setClearButtonEnabled(true);
    m_batchSearch->setMaximumWidth(560); // cap free-standing search; table stays full width
    connect(m_batchSearch, &QLineEdit::textChanged, this, &InventoryPage::reloadBatches);

    m_batches = makeTable({QStringLiteral("Medicine"), QStringLiteral("Batch"),
                           QStringLiteral("Expiry"), QStringLiteral("Qty"), QStringLiteral("Cost"),
                           QStringLiteral("MRP"), QStringLiteral("Status")});
    rightAlignHeaders(m_batches, {3, 4, 5}); // Qty, Cost, MRP

    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);
    layout->addWidget(m_batchSearch);
    layout->addWidget(m_batches, 1);
}

void InventoryPage::buildLowStock(QWidget *tab)
{
    m_low = makeTable({QStringLiteral("Medicine"), QStringLiteral("Generic"),
                       QStringLiteral("On hand"), QStringLiteral("Reorder at"),
                       QStringLiteral("Short by")});
    rightAlignHeaders(m_low, {2, 3, 4}); // On hand, Reorder at, Short by
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);
    layout->addWidget(m_low, 1);
}

void InventoryPage::buildExpiring(QWidget *tab)
{
    m_expiring
        = makeTable({QStringLiteral("Medicine"), QStringLiteral("Batch"), QStringLiteral("Expiry"),
                     QStringLiteral("Days"), QStringLiteral("Qty")});
    rightAlignHeaders(m_expiring, {3, 4}); // Days, Qty
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);
    auto *note = new QLabel(QStringLiteral("Batches expiring within 90 days."), tab);
    note->setObjectName(QStringLiteral("muted"));
    layout->addWidget(note);
    layout->addWidget(m_expiring, 1);
}

void InventoryPage::reload()
{
    InventoryRepository repo(m_db);

    const InventoryTotals t = repo.totals();
    m_kpiMedicines->setText(QString::number(t.medicinesCount));
    m_kpiBatches->setText(QString::number(t.activeBatches));
    m_kpiCost->setText(Money::fromString(t.stockValueCost).display());
    m_kpiMrp->setText(Money::fromString(t.stockValueMrp).display());

    // Overview: expiring 60d (top 10) + low stock (top 10).
    const QVector<ExpiringRow> exp60 = repo.expiringSoon(60, 10);
    UiUtil::beginFill(m_ovExpiring);
    m_ovExpiring->setRowCount(exp60.size());
    for (int i = 0; i < exp60.size(); ++i) {
        const ExpiringRow &r = exp60.at(i);
        m_ovExpiring->setItem(i, 0, new QTableWidgetItem(r.brandName));
        m_ovExpiring->setItem(i, 1, new QTableWidgetItem(r.batchNumber));
        auto *exp = new QTableWidgetItem(
            QStringLiteral("%1 (%2d)").arg(r.expiry.toString(Qt::ISODate)).arg(r.days));
        UiUtil::rightAlign(exp);
        m_ovExpiring->setItem(i, 2, exp);
        auto *ovq = new QTableWidgetItem(QStringLiteral("%1 %2").arg(r.currentQty).arg(r.baseUnit));
        UiUtil::rightAlign(ovq);
        m_ovExpiring->setItem(i, 3, ovq);
    }

    const QVector<LowStockRow> low10 = repo.lowStock(10);
    UiUtil::beginFill(m_ovLow);
    m_ovLow->setRowCount(low10.size());
    for (int i = 0; i < low10.size(); ++i) {
        const LowStockRow &r = low10.at(i);
        m_ovLow->setItem(i, 0, new QTableWidgetItem(r.brandName));
        auto *oh = new QTableWidgetItem(QStringLiteral("%1 %2").arg(r.onHand).arg(r.baseUnit));
        UiUtil::colorItem(oh, UiUtil::Palette::Danger, true);
        UiUtil::rightAlign(oh);
        m_ovLow->setItem(i, 1, oh);
        auto *ovrl = new QTableWidgetItem(QString::number(r.reorderLevel));
        UiUtil::rightAlign(ovrl);
        m_ovLow->setItem(i, 2, ovrl);
    }

    UiUtil::emptyState(m_ovExpiring, QStringLiteral("Nothing expiring in 60 days."));
    UiUtil::emptyState(m_ovLow, QStringLiteral("All medicines above reorder level."));

    // Low-stock tab (full list).
    const QVector<LowStockRow> lowAll = repo.lowStock(0);
    UiUtil::beginFill(m_low);
    m_low->setRowCount(lowAll.size());
    for (int i = 0; i < lowAll.size(); ++i) {
        const LowStockRow &r = lowAll.at(i);
        m_low->setItem(i, 0, new QTableWidgetItem(r.brandName));
        m_low->setItem(i, 1, new QTableWidgetItem(r.genericName));
        auto *oh = new QTableWidgetItem(QStringLiteral("%1 %2").arg(r.onHand).arg(r.baseUnit));
        UiUtil::colorItem(oh, UiUtil::Palette::Danger, true);
        UiUtil::rightAlign(oh);
        m_low->setItem(i, 2, oh);
        auto *rl = new QTableWidgetItem(QString::number(r.reorderLevel));
        UiUtil::rightAlign(rl);
        m_low->setItem(i, 3, rl);
        auto *sb = new QTableWidgetItem(QString::number(r.reorderLevel - r.onHand));
        UiUtil::rightAlign(sb);
        m_low->setItem(i, 4, sb);
    }
    UiUtil::emptyState(m_low, QStringLiteral("All medicines are above their reorder level."));

    // Expiring tab (within 90 days).
    const QVector<ExpiringRow> exp90 = repo.expiringSoon(90, 0);
    UiUtil::beginFill(m_expiring);
    m_expiring->setRowCount(exp90.size());
    for (int i = 0; i < exp90.size(); ++i) {
        const ExpiringRow &r = exp90.at(i);
        m_expiring->setItem(i, 0, new QTableWidgetItem(r.brandName));
        m_expiring->setItem(i, 1, new QTableWidgetItem(r.batchNumber));
        m_expiring->setItem(i, 2, new QTableWidgetItem(r.expiry.toString(Qt::ISODate)));
        auto *days = new QTableWidgetItem(QString::number(r.days));
        UiUtil::colorItem(days, r.days <= 30 ? UiUtil::Palette::Warning : UiUtil::Palette::Info);
        UiUtil::rightAlign(days);
        m_expiring->setItem(i, 3, days);
        auto *q = new QTableWidgetItem(QStringLiteral("%1 %2").arg(r.currentQty).arg(r.baseUnit));
        UiUtil::rightAlign(q);
        m_expiring->setItem(i, 4, q);
    }
    UiUtil::emptyState(m_expiring, QStringLiteral("Nothing expiring within 90 days."));

    reloadBatches();
}

void InventoryPage::reloadBatches()
{
    InventoryRepository repo(m_db);
    const QVector<StockBatchRow> rows
        = repo.batchList(m_batchSearch ? m_batchSearch->text() : QString());
    UiUtil::beginFill(m_batches);
    m_batches->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const StockBatchRow &b = rows.at(i);
        m_batches->setItem(
            i, 0, new QTableWidgetItem(QStringLiteral("%1\n%2").arg(b.brandName, b.genericName)));
        m_batches->setItem(i, 1, new QTableWidgetItem(b.batchNumber));
        m_batches->setItem(
            i, 2,
            new QTableWidgetItem(
                QStringLiteral("%1 (%2d)").arg(b.expiry.toString(Qt::ISODate)).arg(b.days)));
        auto *q = new QTableWidgetItem(QStringLiteral("%1 %2").arg(b.currentQty).arg(b.baseUnit));
        UiUtil::rightAlign(q);
        m_batches->setItem(i, 3, q);
        auto *cost
            = new QTableWidgetItem(Money::fromString(b.costPerUnit).display(Money::ScaleCost));
        UiUtil::rightAlign(cost);
        m_batches->setItem(i, 4, cost);
        auto *mrp = new QTableWidgetItem(Money::fromString(b.mrpPerUnit).display());
        UiUtil::rightAlign(mrp);
        m_batches->setItem(i, 5, mrp);
        const Status st = statusFor(b);
        auto *sItem = new QTableWidgetItem(st.text);
        UiUtil::colorItem(sItem, st.color, true);
        m_batches->setItem(i, 6, sItem);
    }
    UiUtil::emptyState(m_batches, QStringLiteral("No batches found."));
    m_batches->resizeRowsToContents();
}
