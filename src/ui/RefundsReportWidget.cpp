#include "ui/RefundsReportWidget.h"

#include "data/AnalyticsRepository.h"
#include "domain/Money.h"
#include "ui/UiUtil.h"

#include <QDateEdit>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
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

RefundsReportWidget::RefundsReportWidget(QSqlDatabase db, QWidget *parent)
    : QWidget(parent), m_db(std::move(db))
{
    // --- Page header -------------------------------------------------------
    auto *title = new QLabel(QStringLiteral("Refunds"), this);
    title->setObjectName(QStringLiteral("h1"));
    auto *subtitle
        = new QLabel(QStringLiteral("Returned items, refund value and refund rate for the "
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
    kpis->addWidget(makeKpi(QStringLiteral("Refunds"), &m_kpiCount));
    kpis->addWidget(makeKpi(QStringLiteral("Amount"), &m_kpiAmount));
    kpis->addWidget(makeKpi(QStringLiteral("Refund rate"), &m_kpiRate));

    // --- Table card --------------------------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Medicine"), QStringLiteral("Count"),
                                        QStringLiteral("Qty returned"), QStringLiteral("Amount")});
    rightAlignHeader(m_table, 1);
    rightAlignHeader(m_table, 2);
    rightAlignHeader(m_table, 3);
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
        = new QLabel(QStringLiteral("Per-medicine refund count, quantity returned and refunded "
                                    "amount."),
                     tableCard);
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

    connect(filterBtn, &QPushButton::clicked, this, &RefundsReportWidget::reload);

    reload();
}

void RefundsReportWidget::reload()
{
    AnalyticsRepository repo(m_db);
    const RefundsReport rep = repo.refundsReport(m_from->date(), m_to->date());

    m_kpiCount->setText(QLocale().toString(rep.totalCount));
    m_kpiAmount->setText(Money::fromString(rep.totalAmount).display());
    m_kpiRate->setText(QStringLiteral("%1%").arg(rep.refundRatePct));

    UiUtil::beginFill(m_table);
    m_table->setRowCount(rep.byMedicine.size());
    for (int i = 0; i < rep.byMedicine.size(); ++i) {
        const RefundMedicineRow &r = rep.byMedicine.at(i);
        m_table->setItem(i, 0, new QTableWidgetItem(r.brandName));
        auto *count = new QTableWidgetItem(QString::number(r.count));
        UiUtil::rightAlign(count);
        m_table->setItem(i, 1, count);
        auto *qty = new QTableWidgetItem(QString::number(r.qtyReturned));
        UiUtil::rightAlign(qty);
        m_table->setItem(i, 2, qty);
        auto *amount = new QTableWidgetItem(Money::fromString(r.amount).display());
        UiUtil::rightAlign(amount);
        m_table->setItem(i, 3, amount);
    }
    UiUtil::emptyState(m_table, QStringLiteral("No refunds in this date range."));
}
