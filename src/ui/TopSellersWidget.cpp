#include "ui/TopSellersWidget.h"

#include "data/AnalyticsRepository.h"
#include "domain/Money.h"
#include "ui/UiUtil.h"

#include <QDateEdit>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

TopSellersWidget::TopSellersWidget(QSqlDatabase db, QWidget *parent)
    : QWidget(parent), m_db(std::move(db))
{
    // --- Page header -------------------------------------------------------
    auto *title = new QLabel(QStringLiteral("Top sellers"), this);
    title->setObjectName(QStringLiteral("h1"));
    auto *subtitle
        = new QLabel(QStringLiteral("Best-performing medicines ranked by net revenue."), this);
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

    auto *fromLbl = new QLabel(QStringLiteral("From"), this);
    fromLbl->setObjectName(QStringLiteral("muted"));
    auto *toLbl = new QLabel(QStringLiteral("To"), this);
    toLbl->setObjectName(QStringLiteral("muted"));

    auto *filterRow = new QHBoxLayout;
    filterRow->setContentsMargins(0, 0, 0, 0);
    filterRow->setSpacing(8);
    filterRow->addWidget(fromLbl);
    filterRow->addWidget(m_from);
    filterRow->addSpacing(8);
    filterRow->addWidget(toLbl);
    filterRow->addWidget(m_to);
    filterRow->addSpacing(8);
    filterRow->addWidget(filterBtn);
    filterRow->addStretch();
    filterRow->addWidget(csvBtn);

    auto *filterCard = new QFrame(this);
    filterCard->setObjectName(QStringLiteral("card"));
    auto *filterCardLayout = new QVBoxLayout(filterCard);
    filterCardLayout->setContentsMargins(20, 16, 20, 16);
    filterCardLayout->setSpacing(12);
    auto *filterHeader = new QLabel(QStringLiteral("Date range"), filterCard);
    filterHeader->setObjectName(QStringLiteral("h2"));
    filterCardLayout->addWidget(filterHeader);
    filterCardLayout->addLayout(filterRow);

    // --- KPI cards ---------------------------------------------------------
    auto makeKpi = [this](const QString &caption, QLabel **out) {
        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("card"));
        auto *l = new QVBoxLayout(card);
        l->setContentsMargins(20, 16, 20, 16);
        l->setSpacing(8);
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
    kpis->addWidget(makeKpi(QStringLiteral("Revenue (net)"), &m_kpiRevenue));
    kpis->addWidget(makeKpi(QStringLiteral("Units (net)"), &m_kpiUnits));
    kpis->addWidget(makeKpi(QStringLiteral("Gross margin"), &m_kpiMargin));

    // --- Table card --------------------------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({QStringLiteral("#"), QStringLiteral("Medicine"),
                                        QStringLiteral("Units (net)"), QStringLiteral("Revenue"),
                                        QStringLiteral("Share"), QStringLiteral("Margin"),
                                        QStringLiteral("Margin %")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setWordWrap(false);
    m_table->setShowGrid(true);
    m_table->setAlternatingRowColors(false);
    // Right-align every numeric header (all columns except "Medicine").
    for (int col : {0, 2, 3, 4, 5, 6}) {
        if (auto *h = m_table->horizontalHeaderItem(col)) {
            h->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    auto *tableCard = new QFrame(this);
    tableCard->setObjectName(QStringLiteral("card"));
    auto *tableCardLayout = new QVBoxLayout(tableCard);
    tableCardLayout->setContentsMargins(20, 16, 20, 16);
    tableCardLayout->setSpacing(12);
    auto *tableHeader = new QLabel(QStringLiteral("Ranked medicines"), tableCard);
    tableHeader->setObjectName(QStringLiteral("h2"));
    auto *tableDesc
        = new QLabel(QStringLiteral("Sorted by net revenue over the selected range."), tableCard);
    tableDesc->setObjectName(QStringLiteral("muted"));
    tableCardLayout->addWidget(tableHeader);
    tableCardLayout->addWidget(tableDesc);
    tableCardLayout->addWidget(m_table, 1);

    // --- Page root ---------------------------------------------------------
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addWidget(filterCard);
    layout->addLayout(kpis);
    layout->addWidget(tableCard, 1);

    connect(filterBtn, &QPushButton::clicked, this, &TopSellersWidget::reload);
    connect(csvBtn, &QPushButton::clicked, this, &TopSellersWidget::exportCsv);

    reload();
}

void TopSellersWidget::reload()
{
    AnalyticsRepository repo(m_db);
    const TopSellers ts = repo.topSellers(m_from->date(), m_to->date());

    m_kpiRevenue->setText(Money::fromString(ts.totalRevenue).display());
    m_kpiUnits->setText(QLocale().toString(ts.totalUnits));
    m_kpiMargin->setText(Money::fromString(ts.totalMargin).display());

    UiUtil::beginFill(m_table);
    m_table->setRowCount(ts.rows.size());
    for (int i = 0; i < ts.rows.size(); ++i) {
        const TopSellerRow &r = ts.rows.at(i);
        auto *rank = new QTableWidgetItem(QString::number(i + 1));
        UiUtil::rightAlign(rank);
        m_table->setItem(i, 0, rank);

        QString med = r.brandName;
        if (!r.strength.isEmpty()) med += QStringLiteral(" %1").arg(r.strength);
        if (!r.genericName.isEmpty()) med += QStringLiteral(" — %1").arg(r.genericName);
        m_table->setItem(i, 1, new QTableWidgetItem(med));

        auto *units = new QTableWidgetItem(QStringLiteral("%1 %2").arg(r.units).arg(r.baseUnit));
        UiUtil::rightAlign(units);
        m_table->setItem(i, 2, units);
        auto *rev = new QTableWidgetItem(Money::fromString(r.revenue).display());
        UiUtil::rightAlign(rev);
        m_table->setItem(i, 3, rev);
        auto *share = new QTableWidgetItem(QStringLiteral("%1%").arg(r.sharePct));
        UiUtil::rightAlign(share);
        m_table->setItem(i, 4, share);
        auto *margin = new QTableWidgetItem(Money::fromString(r.margin).display());
        UiUtil::rightAlign(margin);
        m_table->setItem(i, 5, margin);
        auto *pct = new QTableWidgetItem(QStringLiteral("%1%").arg(r.marginPct));
        UiUtil::rightAlign(pct);
        m_table->setItem(i, 6, pct);
    }
    UiUtil::emptyState(m_table, QStringLiteral("No sales in this date range."));
}

void TopSellersWidget::exportCsv()
{
    AnalyticsRepository repo(m_db);
    const TopSellers ts = repo.topSellers(m_from->date(), m_to->date());
    const QString def
        = QStringLiteral("top_sellers_%1_to_%2.csv")
              .arg(m_from->date().toString(Qt::ISODate), m_to->date().toString(Qt::ISODate));
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export CSV"), def,
                                                      QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    if (!AnalyticsRepository::exportTopSellersCsv(ts, path)) {
        QMessageBox::warning(this, QStringLiteral("Export"),
                             QStringLiteral("Could not write the file."));
        return;
    }
    QMessageBox::information(this, QStringLiteral("Export"), QStringLiteral("Saved %1").arg(path));
}
