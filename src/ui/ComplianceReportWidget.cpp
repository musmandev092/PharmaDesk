#include "ui/ComplianceReportWidget.h"

#include "data/AnalyticsRepository.h"
#include "ui/UiUtil.h"

#include <QDateEdit>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

ComplianceReportWidget::ComplianceReportWidget(QSqlDatabase db, QWidget *parent)
    : QWidget(parent), m_db(std::move(db))
{
    auto *title = new QLabel(QStringLiteral("Controlled-drug compliance"), this);
    title->setObjectName(QStringLiteral("h1"));
    auto *subtitle = new QLabel(
        QStringLiteral("Audit trail of controlled-drug sales for the selected period."), this);
    subtitle->setObjectName(QStringLiteral("muted"));

    // --- Filter card -------------------------------------------------------
    auto *filterCard = new QFrame(this);
    filterCard->setObjectName(QStringLiteral("card"));
    auto *filterLayout = new QVBoxLayout(filterCard);
    filterLayout->setContentsMargins(20, 16, 20, 16);
    filterLayout->setSpacing(12);

    auto *filterHeader = new QLabel(QStringLiteral("Date range"), filterCard);
    filterHeader->setObjectName(QStringLiteral("h2"));

    m_from = new QDateEdit(filterCard);
    m_from->setCalendarPopup(true);
    m_from->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_from->setDate(QDate::currentDate().addDays(-90));
    m_from->setMinimumWidth(140);
    m_to = new QDateEdit(filterCard);
    m_to->setCalendarPopup(true);
    m_to->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_to->setDate(QDate::currentDate());
    m_to->setMinimumWidth(140);

    auto *filterBtn = new QPushButton(QStringLiteral("Apply filter"), filterCard);
    filterBtn->setObjectName(QStringLiteral("primary"));

    auto *fromLabel = new QLabel(QStringLiteral("From"), filterCard);
    auto *toLabel = new QLabel(QStringLiteral("To"), filterCard);

    auto *filters = new QHBoxLayout;
    filters->setSpacing(16);
    filters->addWidget(fromLabel);
    filters->addWidget(m_from);
    filters->addSpacing(8);
    filters->addWidget(toLabel);
    filters->addWidget(m_to);
    filters->addSpacing(8);
    filters->addWidget(filterBtn);
    filters->addStretch();

    filterLayout->addWidget(filterHeader);
    filterLayout->addLayout(filters);

    // --- Table card --------------------------------------------------------
    auto *tableCard = new QFrame(this);
    tableCard->setObjectName(QStringLiteral("card"));
    auto *tableLayout = new QVBoxLayout(tableCard);
    tableLayout->setContentsMargins(20, 16, 20, 16);
    tableLayout->setSpacing(12);

    auto *tableHeader = new QLabel(QStringLiteral("Controlled-drug sales"), tableCard);
    tableHeader->setObjectName(QStringLiteral("h2"));
    auto *tableDesc = new QLabel(
        QStringLiteral("One row per controlled-drug receipt, with prescriber and witness details."),
        tableCard);
    tableDesc->setObjectName(QStringLiteral("muted"));

    m_table = new QTableWidget(tableCard);
    m_table->setColumnCount(9);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("Receipt"), QStringLiteral("When"), QStringLiteral("Items"),
         QStringLiteral("Doctor"), QStringLiteral("Licence"), QStringLiteral("Patient"),
         QStringLiteral("Phone"), QStringLiteral("Cashier"), QStringLiteral("Witness")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setWordWrap(false);

    // Right-align headers for numeric/date columns (When, Phone).
    for (int col : {1, 6}) {
        if (auto *hi = m_table->horizontalHeaderItem(col))
            hi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

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
    layout->addWidget(tableCard, 1);

    connect(filterBtn, &QPushButton::clicked, this, &ComplianceReportWidget::reload);

    reload();
}

void ComplianceReportWidget::reload()
{
    AnalyticsRepository repo(m_db);
    const QVector<ComplianceRow> rows = repo.complianceReport(m_from->date(), m_to->date());

    UiUtil::beginFill(m_table);
    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const ComplianceRow &r = rows.at(i);
        auto *soldAtItem = new QTableWidgetItem(r.soldAt);
        auto *phoneItem = new QTableWidgetItem(r.patientPhone);
        UiUtil::rightAlign(soldAtItem);
        UiUtil::rightAlign(phoneItem);
        m_table->setItem(i, 0, new QTableWidgetItem(r.receiptNumber));
        m_table->setItem(i, 1, soldAtItem);
        m_table->setItem(i, 2, new QTableWidgetItem(r.items));
        m_table->setItem(i, 3, new QTableWidgetItem(r.doctorName));
        m_table->setItem(i, 4, new QTableWidgetItem(r.prescriberLicense));
        m_table->setItem(i, 5, new QTableWidgetItem(r.patientName));
        m_table->setItem(i, 6, phoneItem);
        m_table->setItem(i, 7, new QTableWidgetItem(r.cashierName));
        m_table->setItem(i, 8, new QTableWidgetItem(r.witnessName));
    }
    UiUtil::emptyState(m_table, QStringLiteral("No controlled-drug sales in this date range."));
}
