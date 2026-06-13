#include "ui/NarcoticRegisterWidget.h"

#include "data/AnalyticsRepository.h"
#include "ui/UiUtil.h"

#include <QDateEdit>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

NarcoticRegisterWidget::NarcoticRegisterWidget(QSqlDatabase db, QWidget *parent)
    : QWidget(parent), m_db(std::move(db))
{
    // Page header.
    auto *title = new QLabel(QStringLiteral("Narcotic register"), this);
    title->setObjectName(QStringLiteral("h1"));

    auto *subtitle
        = new QLabel(QStringLiteral("DRAP-style register of controlled-drug dispenses with "
                                    "prescriber, patient and witness details."),
                     this);
    subtitle->setObjectName(QStringLiteral("muted"));
    subtitle->setWordWrap(true);

    // --- Filter card -------------------------------------------------------
    auto *filterCard = new QFrame(this);
    filterCard->setObjectName(QStringLiteral("card"));

    auto *filterHeader = new QLabel(QStringLiteral("Date range"), filterCard);
    filterHeader->setObjectName(QStringLiteral("h2"));

    m_from = new QDateEdit(filterCard);
    m_from->setCalendarPopup(true);
    m_from->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_from->setDate(QDate::currentDate().addDays(-180));
    m_from->setMinimumWidth(140);
    m_to = new QDateEdit(filterCard);
    m_to->setCalendarPopup(true);
    m_to->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_to->setDate(QDate::currentDate());
    m_to->setMinimumWidth(140);

    auto *filterBtn = new QPushButton(QStringLiteral("Filter"), filterCard);
    filterBtn->setDefault(true);

    auto *fromLabel = new QLabel(QStringLiteral("From"), filterCard);
    auto *toLabel = new QLabel(QStringLiteral("To"), filterCard);

    auto *filterRow = new QHBoxLayout;
    filterRow->setSpacing(16);
    filterRow->addWidget(fromLabel);
    filterRow->addWidget(m_from);
    filterRow->addSpacing(8);
    filterRow->addWidget(toLabel);
    filterRow->addWidget(m_to);
    filterRow->addStretch(1);
    filterRow->addWidget(filterBtn);

    auto *filterLayout = new QVBoxLayout(filterCard);
    filterLayout->setContentsMargins(20, 16, 20, 16);
    filterLayout->setSpacing(12);
    filterLayout->addWidget(filterHeader);
    filterLayout->addLayout(filterRow);

    // --- Register card -----------------------------------------------------
    auto *tableCard = new QFrame(this);
    tableCard->setObjectName(QStringLiteral("card"));

    auto *tableHeader = new QLabel(QStringLiteral("Dispense register"), tableCard);
    tableHeader->setObjectName(QStringLiteral("h2"));

    auto *tableDesc = new QLabel(
        QStringLiteral("One row per controlled-drug sale in the selected period."), tableCard);
    tableDesc->setObjectName(QStringLiteral("muted"));

    m_table = new QTableWidget(tableCard);
    m_table->setColumnCount(11);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("Receipt"), QStringLiteral("When"), QStringLiteral("Items"),
         QStringLiteral("Doctor"), QStringLiteral("Licence"), QStringLiteral("Patient"),
         QStringLiteral("Phone"), QStringLiteral("Address"), QStringLiteral("Cashier"),
         QStringLiteral("Witness"), QStringLiteral("Witness at")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setWordWrap(false);

    // Right-align the date/time column headers to match their cells.
    for (int col : {1, 10}) {
        if (auto *header = m_table->horizontalHeaderItem(col))
            header->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    auto *tableLayout = new QVBoxLayout(tableCard);
    tableLayout->setContentsMargins(20, 16, 20, 16);
    tableLayout->setSpacing(12);
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

    connect(filterBtn, &QPushButton::clicked, this, &NarcoticRegisterWidget::reload);

    reload();
}

void NarcoticRegisterWidget::reload()
{
    AnalyticsRepository repo(m_db);
    const QVector<NarcoticRow> rows = repo.narcoticRegister(m_from->date(), m_to->date());

    UiUtil::beginFill(m_table);
    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const NarcoticRow &r = rows.at(i);
        m_table->setItem(i, 0, new QTableWidgetItem(r.receiptNumber));
        auto *soldAtItem = new QTableWidgetItem(r.soldAt);
        UiUtil::rightAlign(soldAtItem);
        m_table->setItem(i, 1, soldAtItem);
        m_table->setItem(i, 2, new QTableWidgetItem(r.items));
        m_table->setItem(i, 3, new QTableWidgetItem(r.doctorName));
        m_table->setItem(i, 4, new QTableWidgetItem(r.prescriberLicense));
        m_table->setItem(i, 5, new QTableWidgetItem(r.patientName));
        m_table->setItem(i, 6, new QTableWidgetItem(r.patientPhone));
        m_table->setItem(i, 7, new QTableWidgetItem(r.patientAddress));
        m_table->setItem(i, 8, new QTableWidgetItem(r.cashierName));
        m_table->setItem(i, 9, new QTableWidgetItem(r.witnessName));
        auto *witnessAtItem = new QTableWidgetItem(r.witnessAt);
        UiUtil::rightAlign(witnessAtItem);
        m_table->setItem(i, 10, witnessAtItem);
    }
    UiUtil::emptyState(m_table, QStringLiteral("No narcotic dispenses in this date range."));
}
