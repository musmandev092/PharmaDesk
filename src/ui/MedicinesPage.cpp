#include "ui/MedicinesPage.h"

#include "data/BatchRepository.h"
#include "data/CatalogImporter.h"
#include "data/MedicineRepository.h"
#include "domain/MedicineForm.h"
#include "ui/MedicineDialog.h"
#include "ui/StockInDialog.h"
#include "ui/UiUtil.h"

#include <QBrush>

#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

MedicinesPage::MedicinesPage(QSqlDatabase db, qint64 userId, QWidget *parent)
    : QWidget(parent), m_db(std::move(db)), m_userId(userId)
{
    auto *content = new QWidget;
    content->setObjectName(QStringLiteral("scrollContent"));
    content->setAutoFillBackground(false);

    auto *title = new QLabel(QStringLiteral("Medicines"), content);
    title->setObjectName(QStringLiteral("h1"));

    m_search = new QLineEdit(content);
    m_search->setPlaceholderText(QStringLiteral("Search brand, generic, SKU, manufacturer…"));
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(560);

    auto *addBtn = new QPushButton(QStringLiteral("Add medicine"), content);
    auto *importBtn = new QPushButton(QStringLiteral("Import…"), content);
    importBtn->setProperty("variant", "secondary");
    importBtn->setToolTip(
        QStringLiteral("Import medicines from an Excel (.xlsx) or CSV file.\n"
                       "Use the template in resources/templates/medicine_import_template.xlsx.\n"
                       "Rows whose SKU already exists are skipped (safe to re-run)."));

    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(12);
    topRow->addWidget(m_search, 1);
    topRow->addWidget(importBtn);
    topRow->addWidget(addBtn);

    m_table = new QTableWidget(content);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({QStringLiteral("SKU"), QStringLiteral("Brand / Generic"),
                                        QStringLiteral("Form"), QStringLiteral("Schedule"),
                                        QStringLiteral("On hand"), QStringLiteral("Active")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setWordWrap(false);
    if (auto *onHandHeader = m_table->horizontalHeaderItem(4)) {
        onHandHeader->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    auto *editBtn = new QPushButton(QStringLiteral("Edit"), content);
    auto *stockBtn = new QPushButton(QStringLiteral("Add stock"), content);
    auto *delBtn = new QPushButton(QStringLiteral("Delete"), content);
    editBtn->setProperty("variant", "secondary");
    delBtn->setProperty("variant", "destructive");

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(12);
    btnRow->addWidget(editBtn);
    btnRow->addWidget(stockBtn);
    btnRow->addStretch();
    btnRow->addWidget(delBtn);

    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addWidget(title);
    layout->addLayout(topRow);
    layout->addWidget(m_table, 1);
    layout->addLayout(btnRow);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("pageScroll"));
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setAutoFillBackground(false);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setStyleSheet(QStringLiteral("QScrollArea#pageScroll, QWidget#scrollContent { "
                                         "background: transparent; border: none; }"));

    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    pageLayout->addWidget(scroll);

    connect(addBtn, &QPushButton::clicked, this, &MedicinesPage::addMedicine);
    connect(importBtn, &QPushButton::clicked, this, &MedicinesPage::importCatalog);
    connect(editBtn, &QPushButton::clicked, this, &MedicinesPage::editSelected);
    connect(stockBtn, &QPushButton::clicked, this, &MedicinesPage::addStockSelected);
    connect(delBtn, &QPushButton::clicked, this, &MedicinesPage::deleteSelected);
    connect(m_search, &QLineEdit::textChanged, this, &MedicinesPage::reload);
    connect(m_table, &QTableWidget::doubleClicked, this, &MedicinesPage::editSelected);

    reload();
}

void MedicinesPage::reload()
{
    MedicineRepository repo(m_db);
    const QVector<MedicineRow> rows = repo.list(m_search->text());

    UiUtil::beginFill(m_table);
    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const MedicineRow &r = rows.at(i);
        auto *skuItem = new QTableWidgetItem(r.sku);
        skuItem->setData(Qt::UserRole, r.id);
        skuItem->setData(Qt::UserRole + 1, r.brandName);
        m_table->setItem(i, 0, skuItem);
        m_table->setItem(
            i, 1,
            new QTableWidgetItem(
                QStringLiteral("%1\n%2 %3").arg(r.brandName, r.genericName, r.strength).trimmed()));
        m_table->setItem(i, 2, new QTableWidgetItem(MedicineForm::label(r.form)));
        if (r.controlledSchedule == QLatin1String("NARCOTIC")) {
            UiUtil::setBadge(m_table, i, 3, r.controlledSchedule, UiUtil::Palette::Danger);
        } else if (r.controlledSchedule != QLatin1String("NONE")) {
            UiUtil::setBadge(m_table, i, 3, r.controlledSchedule, UiUtil::Palette::Warning);
        } else {
            m_table->setItem(i, 3, new QTableWidgetItem(QStringLiteral("—")));
        }
        auto *onHand = new QTableWidgetItem(QStringLiteral("%1 %2").arg(r.onHand).arg(r.baseUnit));
        UiUtil::rightAlign(onHand);
        m_table->setItem(i, 4, onHand);
        m_table->setItem(
            i, 5, new QTableWidgetItem(r.isActive ? QStringLiteral("Yes") : QStringLiteral("No")));
        if (!r.isActive) {
            for (int c = 0; c < m_table->columnCount(); ++c) {
                if (auto *it = m_table->item(i, c)) {
                    it->setForeground(QBrush(UiUtil::Palette::MutedFaint));
                }
            }
        }
    }
    UiUtil::emptyState(
        m_table, m_search->text().isEmpty()
                     ? QStringLiteral("No medicines yet — click \"Add medicine\" to create one.")
                     : QStringLiteral("No medicines match your search."));
    m_table->resizeRowsToContents();
}

qint64 MedicinesPage::selectedId() const
{
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 0)) {
        return 0;
    }
    return m_table->item(row, 0)->data(Qt::UserRole).toLongLong();
}

QString MedicinesPage::selectedName() const
{
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 0)) {
        return QString();
    }
    return m_table->item(row, 0)->data(Qt::UserRole + 1).toString();
}

void MedicinesPage::addMedicine()
{
    MedicineRepository repo(m_db);
    MedicineDialog dlg(&repo, m_userId, 0, this);
    if (dlg.exec() == QDialog::Accepted) {
        reload();
    }
}

void MedicinesPage::importCatalog()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import medicines"), QString(),
        QStringLiteral("Spreadsheets (*.xlsx *.csv);;Excel workbook (*.xlsx);;CSV (*.csv)"));
    if (path.isEmpty()) {
        return;
    }

    const CatalogImporter::Result r = CatalogImporter::importFromFile(m_db, path, m_userId);
    if (!r.ok) {
        QMessageBox::critical(this, QStringLiteral("Import failed"),
                              r.error.isEmpty() ? QStringLiteral("The file could not be imported.")
                                                : r.error);
        return;
    }

    reload();
    QMessageBox::information(
        this, QStringLiteral("Import complete"),
        QStringLiteral("Imported: %1\nSkipped (already present / removed): %2\nFailed: %3")
            .arg(r.imported)
            .arg(r.skipped)
            .arg(r.failed));
}

void MedicinesPage::editSelected()
{
    const qint64 id = selectedId();
    if (id == 0) {
        QMessageBox::information(this, QStringLiteral("Edit"),
                                 QStringLiteral("Select a medicine first."));
        return;
    }
    MedicineRepository repo(m_db);
    MedicineDialog dlg(&repo, m_userId, id, this);
    if (dlg.exec() == QDialog::Accepted) {
        reload();
    }
}

void MedicinesPage::addStockSelected()
{
    const qint64 id = selectedId();
    if (id == 0) {
        QMessageBox::information(this, QStringLiteral("Add stock"),
                                 QStringLiteral("Select a medicine first."));
        return;
    }
    BatchRepository repo(m_db);
    StockInDialog dlg(&repo, m_userId, id, selectedName(), this);
    if (dlg.exec() == QDialog::Accepted) {
        reload();
    }
}

void MedicinesPage::deleteSelected()
{
    const qint64 id = selectedId();
    if (id == 0) {
        QMessageBox::information(this, QStringLiteral("Delete"),
                                 QStringLiteral("Select a medicine first."));
        return;
    }
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Delete medicine"),
        QStringLiteral("Delete \"%1\"? History is preserved (soft delete).").arg(selectedName()));
    if (answer != QMessageBox::Yes) {
        return;
    }
    MedicineRepository repo(m_db);
    if (!repo.softDelete(id, m_userId)) {
        QMessageBox::warning(this, QStringLiteral("Delete"),
                             QStringLiteral("Could not delete: %1").arg(repo.errorString()));
        return;
    }
    reload();
}
