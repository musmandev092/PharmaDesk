#include "ui/TaxReportWidget.h"

#include "data/AnalyticsRepository.h"
#include "domain/Money.h"
#include "ui/UiUtil.h"

#include <QDateEdit>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
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

TaxReportWidget::TaxReportWidget(QSqlDatabase db, QWidget *parent)
    : QWidget(parent), m_db(std::move(db))
{
    // --- Page header -------------------------------------------------------
    auto *title = new QLabel(QStringLiteral("Tax summary"), this);
    title->setObjectName(QStringLiteral("h1"));
    auto *subtitle
        = new QLabel(QStringLiteral("Taxable base, tax collected and totals by tax code for the "
                                    "selected period."),
                     this);
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
    kpis->addWidget(makeKpi(QStringLiteral("Base"), &m_kpiBase));
    kpis->addWidget(makeKpi(QStringLiteral("Tax"), &m_kpiTax));
    kpis->addWidget(makeKpi(QStringLiteral("Total"), &m_kpiTotal));

    // --- Table card --------------------------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Tax code"), QStringLiteral("Rate %"),
                                        QStringLiteral("Sales"), QStringLiteral("Lines"),
                                        QStringLiteral("Base"), QStringLiteral("Tax"),
                                        QStringLiteral("Total")});
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
    auto *tableHeader = new QLabel(QStringLiteral("By tax code"), tableCard);
    tableHeader->setObjectName(QStringLiteral("h2"));
    auto *tableDesc = new QLabel(
        QStringLiteral("Per tax-code base, tax and total with sale and line counts."), tableCard);
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

    connect(filterBtn, &QPushButton::clicked, this, &TaxReportWidget::reload);

    reload();
}

void TaxReportWidget::reload()
{
    AnalyticsRepository repo(m_db);
    const TaxReport rep = repo.taxReport(m_from->date(), m_to->date(), true);

    m_kpiBase->setText(Money::fromString(rep.base).display());
    m_kpiTax->setText(Money::fromString(rep.tax).display());
    m_kpiTotal->setText(Money::fromString(rep.total).display());

    UiUtil::beginFill(m_table);
    m_table->setRowCount(rep.rows.size() + (rep.rows.isEmpty() ? 0 : 1));
    for (int i = 0; i < rep.rows.size(); ++i) {
        const TaxBucketRow &r = rep.rows.at(i);
        QString code = r.taxCode;
        if (!r.label.isEmpty()) code = r.label;
        m_table->setItem(i, 0, new QTableWidgetItem(code));
        auto *rate = new QTableWidgetItem(QStringLiteral("%1%").arg(r.ratePct));
        UiUtil::rightAlign(rate);
        m_table->setItem(i, 1, rate);
        auto *sales = new QTableWidgetItem(QString::number(r.salesCount));
        UiUtil::rightAlign(sales);
        m_table->setItem(i, 2, sales);
        auto *lines = new QTableWidgetItem(QString::number(r.linesCount));
        UiUtil::rightAlign(lines);
        m_table->setItem(i, 3, lines);
        auto *base = new QTableWidgetItem(Money::fromString(r.base).display());
        UiUtil::rightAlign(base);
        m_table->setItem(i, 4, base);
        auto *tax = new QTableWidgetItem(Money::fromString(r.tax).display());
        UiUtil::rightAlign(tax);
        m_table->setItem(i, 5, tax);
        auto *total = new QTableWidgetItem(Money::fromString(r.total).display());
        UiUtil::rightAlign(total);
        m_table->setItem(i, 6, total);
    }
    if (!rep.rows.isEmpty()) {
        const int last = rep.rows.size();
        auto *label = new QTableWidgetItem(QStringLiteral("Total"));
        QFont f = label->font();
        f.setBold(true);
        label->setFont(f);
        m_table->setItem(last, 0, label);
        m_table->setItem(last, 1, new QTableWidgetItem(QString()));
        m_table->setItem(last, 2, new QTableWidgetItem(QString()));
        m_table->setItem(last, 3, new QTableWidgetItem(QString()));
        auto *base = new QTableWidgetItem(Money::fromString(rep.base).display());
        UiUtil::rightAlign(base);
        base->setFont(f);
        m_table->setItem(last, 4, base);
        auto *tax = new QTableWidgetItem(Money::fromString(rep.tax).display());
        UiUtil::rightAlign(tax);
        tax->setFont(f);
        m_table->setItem(last, 5, tax);
        auto *total = new QTableWidgetItem(Money::fromString(rep.total).display());
        UiUtil::rightAlign(total);
        total->setFont(f);
        m_table->setItem(last, 6, total);
    }
    UiUtil::emptyState(m_table, QStringLiteral("No sales in this date range."));
}
