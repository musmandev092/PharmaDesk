#include "ui/VelocityReportWidget.h"

#include "data/AnalyticsRepository.h"
#include "ui/UiUtil.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

VelocityReportWidget::VelocityReportWidget(QSqlDatabase db, QWidget *parent)
    : QWidget(parent), m_db(std::move(db))
{
    // --- Page header -------------------------------------------------------
    auto *title = new QLabel(QStringLiteral("Sales velocity"), this);
    title->setObjectName(QStringLiteral("h1"));
    auto *subtitle = new QLabel(
        QStringLiteral("Units sold per medicine over rolling 7, 30, and 90-day windows."), this);
    subtitle->setObjectName(QStringLiteral("muted"));

    // --- Table card --------------------------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Medicine"), QStringLiteral("Generic"),
                                        QStringLiteral("7d"), QStringLiteral("30d"),
                                        QStringLiteral("90d")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setWordWrap(false);
    m_table->setShowGrid(true);
    m_table->setAlternatingRowColors(false);
    // Right-align every numeric window header (the 7d / 30d / 90d columns).
    for (int col : {2, 3, 4}) {
        if (auto *h = m_table->horizontalHeaderItem(col)) {
            h->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    auto *refreshBtn = new QPushButton(QStringLiteral("Refresh"), this);
    refreshBtn->setProperty("variant", "secondary");

    auto *footer = new QHBoxLayout;
    footer->setContentsMargins(0, 0, 0, 0);
    footer->setSpacing(8);
    footer->addStretch();
    footer->addWidget(refreshBtn);

    auto *tableCard = new QFrame(this);
    tableCard->setObjectName(QStringLiteral("card"));
    auto *tableCardLayout = new QVBoxLayout(tableCard);
    tableCardLayout->setContentsMargins(20, 16, 20, 16);
    tableCardLayout->setSpacing(12);
    auto *tableHeader = new QLabel(QStringLiteral("Velocity by medicine"), tableCard);
    tableHeader->setObjectName(QStringLiteral("h2"));
    auto *tableDesc = new QLabel(
        QStringLiteral("All-time rolling windows; counts are net units sold."), tableCard);
    tableDesc->setObjectName(QStringLiteral("muted"));
    tableCardLayout->addWidget(tableHeader);
    tableCardLayout->addWidget(tableDesc);
    tableCardLayout->addWidget(m_table, 1);
    tableCardLayout->addLayout(footer);

    // --- Page root ---------------------------------------------------------
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addWidget(tableCard, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &VelocityReportWidget::reload);

    reload();
}

void VelocityReportWidget::reload()
{
    AnalyticsRepository repo(m_db);
    const QVector<VelocityRow> rows = repo.velocityReport();

    UiUtil::beginFill(m_table);
    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const VelocityRow &r = rows.at(i);
        m_table->setItem(i, 0, new QTableWidgetItem(r.brandName));
        m_table->setItem(i, 1, new QTableWidgetItem(r.genericName));
        auto *d7 = new QTableWidgetItem(QString::number(r.d7));
        UiUtil::rightAlign(d7);
        m_table->setItem(i, 2, d7);
        auto *d30 = new QTableWidgetItem(QString::number(r.d30));
        UiUtil::rightAlign(d30);
        m_table->setItem(i, 3, d30);
        auto *d90 = new QTableWidgetItem(QString::number(r.d90));
        UiUtil::rightAlign(d90);
        m_table->setItem(i, 4, d90);
    }
    UiUtil::emptyState(m_table, QStringLiteral("No sales recorded yet."));
}
