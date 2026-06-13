#include "ui/PlReportWidget.h"

#include "data/AnalyticsRepository.h"
#include "domain/Money.h"
#include "ui/UiUtil.h"

#include <QDateEdit>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

// Right-align a header section so a numeric column's header lines up with its
// right-aligned cells.
void rightAlignHeader(QTableWidget *table, int column)
{
    if (auto *h = table->horizontalHeaderItem(column)) {
        h->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
}

} // namespace

PlReportWidget::PlReportWidget(QSqlDatabase db, QWidget *parent)
    : QWidget(parent), m_db(std::move(db))
{
    // --- Page header -------------------------------------------------------
    auto *title = new QLabel(QStringLiteral("Profit & Loss"), this);
    title->setObjectName(QStringLiteral("h1"));
    auto *subtitle = new QLabel(
        QStringLiteral("Revenue, cost of goods sold and margin for the selected period."), this);
    subtitle->setObjectName(QStringLiteral("muted"));

    // --- Filter card -------------------------------------------------------
    m_from = new QDateEdit(this);
    m_from->setCalendarPopup(true);
    m_from->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_from->setDate(QDate::currentDate().addDays(-30));
    m_from->setMinimumWidth(140);
    m_to = new QDateEdit(this);
    m_to->setCalendarPopup(true);
    m_to->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_to->setDate(QDate::currentDate());
    m_to->setMinimumWidth(140);

    auto *filterBtn = new QPushButton(QStringLiteral("Filter"), this);
    auto *csvBtn = new QPushButton(QStringLiteral("Export CSV"), this);
    csvBtn->setProperty("variant", "secondary");

    auto *filterCard = new QFrame(this);
    filterCard->setObjectName(QStringLiteral("card"));
    auto *filterLayout = new QVBoxLayout(filterCard);
    filterLayout->setContentsMargins(20, 16, 20, 16);
    filterLayout->setSpacing(12);

    auto *filterHeader = new QLabel(QStringLiteral("Date range"), filterCard);
    filterHeader->setObjectName(QStringLiteral("h2"));
    filterLayout->addWidget(filterHeader);

    auto *filterRow = new QHBoxLayout;
    filterRow->setSpacing(16);
    auto *fromLabel = new QLabel(QStringLiteral("From"), filterCard);
    auto *toLabel = new QLabel(QStringLiteral("To"), filterCard);
    filterRow->addWidget(fromLabel);
    filterRow->addWidget(m_from);
    filterRow->addSpacing(8);
    filterRow->addWidget(toLabel);
    filterRow->addWidget(m_to);
    filterRow->addSpacing(8);
    filterRow->addWidget(filterBtn);
    filterRow->addStretch();
    filterRow->addWidget(csvBtn);
    filterLayout->addLayout(filterRow);

    // --- KPI cards ---------------------------------------------------------
    auto makeKpi = [this](const QString &caption, QLabel **out) {
        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("card"));
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
    kpis->addWidget(makeKpi(QStringLiteral("Revenue"), &m_kpiRevenue));
    kpis->addWidget(makeKpi(QStringLiteral("COGS"), &m_kpiCogs));
    kpis->addWidget(makeKpi(QStringLiteral("Gross margin"), &m_kpiMargin));

    // --- Table card --------------------------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Medicine"), QStringLiteral("Units (net)"),
                                        QStringLiteral("Revenue"), QStringLiteral("Refunds"),
                                        QStringLiteral("COGS"), QStringLiteral("Margin"),
                                        QStringLiteral("Margin %")});
    rightAlignHeader(m_table, 1);
    rightAlignHeader(m_table, 2);
    rightAlignHeader(m_table, 3);
    rightAlignHeader(m_table, 4);
    rightAlignHeader(m_table, 5);
    rightAlignHeader(m_table, 6);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setWordWrap(false);

    auto *tableCard = new QFrame(this);
    tableCard->setObjectName(QStringLiteral("card"));
    auto *tableLayout = new QVBoxLayout(tableCard);
    tableLayout->setContentsMargins(20, 16, 20, 16);
    tableLayout->setSpacing(12);
    auto *tableHeader = new QLabel(QStringLiteral("By medicine"), tableCard);
    tableHeader->setObjectName(QStringLiteral("h2"));
    auto *tableDesc
        = new QLabel(QStringLiteral("Per-medicine revenue, refunds, cost and margin."), tableCard);
    tableDesc->setObjectName(QStringLiteral("muted"));
    tableLayout->addWidget(tableHeader);
    tableLayout->addWidget(tableDesc);
    tableLayout->addWidget(m_table, 1);

    // --- Page root ---------------------------------------------------------
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addWidget(filterCard);
    layout->addLayout(kpis);
    layout->addWidget(tableCard, 1);

    connect(filterBtn, &QPushButton::clicked, this, &PlReportWidget::reload);
    connect(csvBtn, &QPushButton::clicked, this, &PlReportWidget::exportCsv);

    reload();
}

void PlReportWidget::reload()
{
    AnalyticsRepository repo(m_db);
    const ProfitAndLoss pl = repo.profitAndLoss(m_from->date(), m_to->date());

    m_kpiRevenue->setText(Money::fromString(pl.revenue).display());
    m_kpiCogs->setText(Money::fromString(pl.cogs).display());
    m_kpiMargin->setText(
        QStringLiteral("PKR %1 (%2%)").arg(Money::fromString(pl.margin).fmt(), pl.marginPct));

    UiUtil::beginFill(m_table);
    m_table->setRowCount(pl.rows.size());
    for (int i = 0; i < pl.rows.size(); ++i) {
        const PlMedicineRow &r = pl.rows.at(i);
        m_table->setItem(i, 0, new QTableWidgetItem(r.brandName));
        auto *units = new QTableWidgetItem(QString::number(r.units));
        UiUtil::rightAlign(units);
        m_table->setItem(i, 1, units);
        auto *rev = new QTableWidgetItem(Money::fromString(r.revenue).display());
        UiUtil::rightAlign(rev);
        m_table->setItem(i, 2, rev);
        const Money refunds = Money::fromString(r.refunds);
        auto *ref = new QTableWidgetItem(
            refunds.isZero() ? QStringLiteral("—") : QStringLiteral("− PKR %1").arg(refunds.fmt()));
        UiUtil::rightAlign(ref);
        if (!refunds.isZero()) UiUtil::colorItem(ref, UiUtil::Palette::Danger);
        m_table->setItem(i, 3, ref);
        auto *cogs = new QTableWidgetItem(Money::fromString(r.cogs).display());
        UiUtil::rightAlign(cogs);
        m_table->setItem(i, 4, cogs);
        auto *margin = new QTableWidgetItem(Money::fromString(r.margin).display());
        UiUtil::rightAlign(margin);
        m_table->setItem(i, 5, margin);
        auto *pct = new QTableWidgetItem(QStringLiteral("%1%").arg(r.marginPct));
        UiUtil::rightAlign(pct);
        m_table->setItem(i, 6, pct);
    }
    UiUtil::emptyState(m_table, QStringLiteral("No sales in this date range."));
}

void PlReportWidget::exportCsv()
{
    AnalyticsRepository repo(m_db);
    const ProfitAndLoss pl = repo.profitAndLoss(m_from->date(), m_to->date());
    const QString def
        = QStringLiteral("pl_%1_to_%2.csv")
              .arg(m_from->date().toString(Qt::ISODate), m_to->date().toString(Qt::ISODate));
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export CSV"), def,
                                                      QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    if (!AnalyticsRepository::exportPlCsv(pl, path)) {
        QMessageBox::warning(this, QStringLiteral("Export"),
                             QStringLiteral("Could not write the file."));
        return;
    }
    QMessageBox::information(this, QStringLiteral("Export"), QStringLiteral("Saved %1").arg(path));
}
