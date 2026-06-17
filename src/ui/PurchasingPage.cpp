#include "ui/PurchasingPage.h"

#include "data/GrnRepository.h"
#include "data/SupplierRepository.h"
#include "domain/Money.h"
#include "ui/GrnDetailDialog.h"
#include "ui/GrnDialog.h"
#include "ui/SupplierDialog.h"
#include "ui/UiUtil.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

PurchasingPage::PurchasingPage(QSqlDatabase db, qint64 userId, QWidget *parent)
    : QWidget(parent), m_db(std::move(db)), m_userId(userId)
{
    auto *content = new QWidget;
    content->setAttribute(Qt::WA_StyledBackground, false);
    content->setAutoFillBackground(false);

    auto *title = new QLabel(QStringLiteral("Purchasing"), content);
    title->setObjectName(QStringLiteral("h1"));

    m_tabs = new QTabWidget(content);

    // ── Suppliers tab ──────────────────────────────────────────────────────
    auto *supTab = new QWidget;
    m_supplierSearch = new QLineEdit(supTab);
    m_supplierSearch->setPlaceholderText(QStringLiteral("Search suppliers…"));
    m_supplierSearch->setClearButtonEnabled(true);
    m_supplierSearch->setMaximumWidth(560);
    auto *addSupBtn = new QPushButton(QStringLiteral("Add supplier"), supTab);
    auto *editSupBtn = new QPushButton(QStringLiteral("Edit"), supTab);
    editSupBtn->setProperty("variant", "secondary");
    auto *delSupBtn = new QPushButton(QStringLiteral("Delete"), supTab);
    delSupBtn->setProperty("variant", "destructive");

    m_suppliers = new QTableWidget(supTab);
    m_suppliers->setColumnCount(5);
    m_suppliers->setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Phone"),
                                            QStringLiteral("NTN"), QStringLiteral("Terms"),
                                            QStringLiteral("Active")});
    m_suppliers->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_suppliers->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_suppliers->setSelectionMode(QAbstractItemView::SingleSelection);
    m_suppliers->verticalHeader()->setVisible(false);
    // Name is the only stretch column; the others size to their content so a
    // normal phone number / NTN / terms value renders without ellipsis.
    QHeaderView *supHeader = m_suppliers->horizontalHeader();
    supHeader->setSectionResizeMode(0, QHeaderView::Stretch);
    supHeader->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    supHeader->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    supHeader->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    supHeader->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    supHeader->setMinimumSectionSize(96);

    auto *supTop = new QHBoxLayout;
    supTop->setSpacing(8);
    supTop->addWidget(m_supplierSearch, 1);
    supTop->addWidget(addSupBtn);
    auto *supBot = new QHBoxLayout;
    supBot->setSpacing(8);
    supBot->addWidget(editSupBtn);
    supBot->addStretch();
    supBot->addWidget(delSupBtn);
    auto *supLayout = new QVBoxLayout(supTab);
    supLayout->setContentsMargins(16, 16, 16, 16);
    supLayout->setSpacing(12);
    supLayout->addLayout(supTop);
    supLayout->addWidget(m_suppliers, 1);
    supLayout->addLayout(supBot);

    // ── Goods receipts tab ─────────────────────────────────────────────────
    auto *grnTab = new QWidget;
    auto *newGrnBtn = new QPushButton(QStringLiteral("New GRN"), grnTab);
    m_grns = new QTableWidget(grnTab);
    m_grns->setColumnCount(7);
    m_grns->setHorizontalHeaderLabels({QStringLiteral("GRN #"), QStringLiteral("Supplier"),
                                       QStringLiteral("Invoice"), QStringLiteral("Posted"),
                                       QStringLiteral("Status"), QStringLiteral("Lines"),
                                       QStringLiteral("Total")});
    m_grns->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_grns->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_grns->verticalHeader()->setVisible(false);
    m_grns->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    auto *grnTop = new QHBoxLayout;
    grnTop->setSpacing(8);
    grnTop->addStretch();
    grnTop->addWidget(newGrnBtn);
    auto *grnLayout = new QVBoxLayout(grnTab);
    grnLayout->setContentsMargins(16, 16, 16, 16);
    grnLayout->setSpacing(12);
    grnLayout->addLayout(grnTop);
    grnLayout->addWidget(m_grns, 1);

    m_tabs->addTab(supTab, QStringLiteral("Suppliers"));
    m_tabs->addTab(grnTab, QStringLiteral("Goods receipts"));

    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addWidget(title);
    layout->addWidget(m_tabs, 1);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setStyleSheet(QStringLiteral("QScrollArea{background:transparent;border:none;}"));
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setWidget(content);

    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    pageLayout->addWidget(scroll);

    connect(m_supplierSearch, &QLineEdit::textChanged, this, &PurchasingPage::reloadSuppliers);
    connect(addSupBtn, &QPushButton::clicked, this, &PurchasingPage::addSupplier);
    connect(editSupBtn, &QPushButton::clicked, this, &PurchasingPage::editSupplier);
    connect(m_suppliers, &QTableWidget::doubleClicked, this, &PurchasingPage::editSupplier);
    connect(delSupBtn, &QPushButton::clicked, this, &PurchasingPage::deleteSupplier);
    connect(newGrnBtn, &QPushButton::clicked, this, &PurchasingPage::newGrn);
    connect(m_grns, &QTableWidget::doubleClicked, this, &PurchasingPage::openGrn);

    reload();
}

void PurchasingPage::openGrn()
{
    const int row = m_grns->currentRow();
    if (row < 0 || !m_grns->item(row, 0)) {
        return;
    }
    const qint64 id = m_grns->item(row, 0)->data(Qt::UserRole).toLongLong();
    if (id <= 0) {
        return;
    }
    GrnDetailDialog dlg(m_db, id, this);
    dlg.exec();
}

void PurchasingPage::reload()
{
    reloadSuppliers();
    reloadGrns();
}

void PurchasingPage::reloadSuppliers()
{
    SupplierRepository repo(m_db);
    const QVector<SupplierRow> rows = repo.list(m_supplierSearch->text());
    UiUtil::beginFill(m_suppliers);
    m_suppliers->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const SupplierRow &s = rows.at(i);
        auto *name = new QTableWidgetItem(s.name);
        name->setData(Qt::UserRole, s.id);
        m_suppliers->setItem(i, 0, name);
        m_suppliers->setItem(i, 1, new QTableWidgetItem(s.contactPhone));
        m_suppliers->setItem(i, 2, new QTableWidgetItem(s.ntn));
        m_suppliers->setItem(i, 3, new QTableWidgetItem(s.paymentTerms));
        m_suppliers->setItem(
            i, 4, new QTableWidgetItem(s.isActive ? QStringLiteral("Yes") : QStringLiteral("No")));
    }
    UiUtil::emptyState(m_suppliers, m_supplierSearch->text().isEmpty()
                                        ? QStringLiteral("No suppliers yet — click “Add supplier”.")
                                        : QStringLiteral("No suppliers match your search."));
}

void PurchasingPage::reloadGrns()
{
    GrnRepository repo(m_db);
    const QVector<GrnDocRow> rows = repo.list();
    UiUtil::beginFill(m_grns);
    m_grns->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const GrnDocRow &g = rows.at(i);
        auto *grn0 = new QTableWidgetItem(g.grnNumber);
        grn0->setData(Qt::UserRole, g.id);
        m_grns->setItem(i, 0, grn0);
        m_grns->setItem(i, 1, new QTableWidgetItem(g.supplierName));
        m_grns->setItem(i, 2, new QTableWidgetItem(g.invoiceNumber));
        auto *posted = new QTableWidgetItem(g.postedAt.left(10));
        UiUtil::rightAlign(posted);
        m_grns->setItem(i, 3, posted);
        const QColor stColor = g.status == QLatin1String("POSTED")      ? UiUtil::Palette::Success
                               : g.status == QLatin1String("CANCELLED") ? UiUtil::Palette::Danger
                                                                        : UiUtil::Palette::Muted;
        UiUtil::setBadge(m_grns, i, 4, g.status, stColor);
        auto *lines = new QTableWidgetItem(QString::number(g.lineCount));
        UiUtil::rightAlign(lines);
        m_grns->setItem(i, 5, lines);
        auto *total = new QTableWidgetItem(Money::fromString(g.grandTotal).display());
        UiUtil::rightAlign(total);
        m_grns->setItem(i, 6, total);
    }
    UiUtil::emptyState(m_grns, QStringLiteral("No goods receipts yet — click “New GRN”."));
}

qint64 PurchasingPage::selectedSupplierId() const
{
    const int row = m_suppliers->currentRow();
    if (row < 0 || !m_suppliers->item(row, 0)) {
        return 0;
    }
    return m_suppliers->item(row, 0)->data(Qt::UserRole).toLongLong();
}

void PurchasingPage::addSupplier()
{
    SupplierRepository repo(m_db);
    SupplierDialog dlg(&repo, m_userId, 0, this);
    if (dlg.exec() == QDialog::Accepted) {
        reloadSuppliers();
    }
}

void PurchasingPage::editSupplier()
{
    const qint64 id = selectedSupplierId();
    if (id == 0) {
        QMessageBox::information(this, QStringLiteral("Edit"),
                                 QStringLiteral("Select a supplier first."));
        return;
    }
    SupplierRepository repo(m_db);
    SupplierDialog dlg(&repo, m_userId, id, this);
    if (dlg.exec() == QDialog::Accepted) {
        reloadSuppliers();
    }
}

void PurchasingPage::deleteSupplier()
{
    const qint64 id = selectedSupplierId();
    if (id == 0) {
        QMessageBox::information(this, QStringLiteral("Delete"),
                                 QStringLiteral("Select a supplier first."));
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("Delete supplier"),
                              QStringLiteral("Delete this supplier? (soft delete)"))
        != QMessageBox::Yes) {
        return;
    }
    SupplierRepository repo(m_db);
    if (!repo.softDelete(id, m_userId)) {
        QMessageBox::warning(this, QStringLiteral("Delete"),
                             QStringLiteral("Could not delete: %1").arg(repo.errorString()));
    }
    reloadSuppliers();
}

void PurchasingPage::newGrn()
{
    GrnDialog dlg(m_db, m_userId, this);
    if (dlg.exec() == QDialog::Accepted) {
        reloadGrns();
    }
}
