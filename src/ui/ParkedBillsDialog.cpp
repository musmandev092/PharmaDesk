#include "ui/ParkedBillsDialog.h"

#include "domain/Money.h"
#include "ui/UiUtil.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

ParkedBillsDialog::ParkedBillsDialog(QSqlDatabase db, qint64 cashierId, QWidget *parent)
    : QDialog(parent), m_db(std::move(db)), m_cashierId(cashierId), m_repo(m_db)
{
    setWindowTitle(QStringLiteral("Parked bills"));
    setModal(true);
    setMinimumWidth(480);
    resize(560, 420);

    auto *heading = new QLabel(QStringLiteral("Parked bills"), this);
    heading->setObjectName(QStringLiteral("h2"));

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Bill"), QStringLiteral("Items"),
                                        QStringLiteral("Total"), QStringLiteral("Parked")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    m_resume = new QPushButton(QStringLiteral("Resume"), this);
    m_discard = new QPushButton(QStringLiteral("Discard"), this);
    m_discard->setProperty("variant", "destructive");
    auto *close = new QPushButton(QStringLiteral("Close"), this);
    close->setProperty("variant", "secondary");

    auto *btns = new QHBoxLayout;
    btns->setSpacing(8);
    btns->addWidget(m_discard);
    btns->addStretch();
    btns->addWidget(close);
    btns->addWidget(m_resume);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);
    root->addWidget(heading);
    root->addWidget(m_table, 1);
    root->addLayout(btns);

    connect(m_table, &QTableWidget::doubleClicked, this, &ParkedBillsDialog::resumeSelected);
    connect(m_resume, &QPushButton::clicked, this, &ParkedBillsDialog::resumeSelected);
    connect(m_discard, &QPushButton::clicked, this, &ParkedBillsDialog::discardSelected);
    connect(close, &QPushButton::clicked, this, &ParkedBillsDialog::reject);

    reload();
}

void ParkedBillsDialog::reload()
{
    const QVector<ParkedCart> bills = m_repo.list(m_cashierId);

    UiUtil::beginFill(m_table);
    m_table->setRowCount(bills.size());
    for (int i = 0; i < bills.size(); ++i) {
        const ParkedCart &b = bills.at(i);
        const QString label
            = b.label.trimmed().isEmpty() ? QStringLiteral("Bill #%1").arg(b.id) : b.label;
        auto *name = new QTableWidgetItem(label);
        name->setData(Qt::UserRole, b.id);
        m_table->setItem(i, 0, name);

        auto *items = new QTableWidgetItem(QString::number(b.itemCount));
        UiUtil::rightAlign(items);
        m_table->setItem(i, 1, items);

        auto *total = new QTableWidgetItem(Money::fromString(b.total).display());
        UiUtil::rightAlign(total);
        m_table->setItem(i, 2, total);

        auto *parked = new QTableWidgetItem(b.createdAt);
        UiUtil::rightAlign(parked);
        m_table->setItem(i, 3, parked);
    }
    UiUtil::emptyState(m_table, QStringLiteral("No parked bills."));
    m_table->resizeRowsToContents();

    const bool hasRows = !bills.isEmpty();
    m_resume->setEnabled(hasRows);
    m_discard->setEnabled(hasRows);
    if (hasRows && m_table->currentRow() < 0) {
        m_table->selectRow(0);
    }
}

qint64 ParkedBillsDialog::selectedId() const
{
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 0)) {
        return 0;
    }
    return m_table->item(row, 0)->data(Qt::UserRole).toLongLong();
}

void ParkedBillsDialog::resumeSelected()
{
    const qint64 id = selectedId();
    if (id <= 0) {
        return;
    }
    ParkedCart c;
    if (!m_repo.load(id, &c)) {
        QMessageBox::warning(this, QStringLiteral("Parked bills"), m_repo.errorString());
        reload();
        return;
    }
    m_resumed = c;
    accept();
}

void ParkedBillsDialog::discardSelected()
{
    const qint64 id = selectedId();
    if (id <= 0) {
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("Discard parked bill"),
            QStringLiteral("Discard parked bill #%1? This cannot be undone.").arg(id))
        != QMessageBox::Yes) {
        return;
    }
    if (!m_repo.discard(id, m_cashierId)) {
        QMessageBox::warning(this, QStringLiteral("Parked bills"), m_repo.errorString());
    }
    reload();
}
