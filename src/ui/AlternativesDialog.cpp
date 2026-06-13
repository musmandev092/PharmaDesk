#include "ui/AlternativesDialog.h"

#include "domain/Money.h"
#include "service/AlternativesFinder.h"
#include "ui/UiUtil.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

AlternativesDialog::AlternativesDialog(QSqlDatabase db, qint64 medicineId,
                                       const QString &medicineName, QWidget *parent)
    : QDialog(parent), m_db(std::move(db)), m_medicineId(medicineId)
{
    setWindowTitle(QStringLiteral("Alternatives"));
    setModal(true);
    setMinimumWidth(560);
    resize(640, 420);

    auto *heading = new QLabel(QStringLiteral("Alternatives for %1").arg(medicineName), this);
    heading->setObjectName(QStringLiteral("h2"));
    heading->setWordWrap(true);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Brand"), QStringLiteral("Strength"),
                                        QStringLiteral("Form"), QStringLiteral("Manufacturer"),
                                        QStringLiteral("On hand"), QStringLiteral("MRP")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->setWordWrap(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    const QVector<AlternativeMedicine> alts = AlternativesFinder(m_db).forMedicine(m_medicineId);
    UiUtil::beginFill(m_table);
    m_table->setRowCount(alts.size());
    for (int i = 0; i < alts.size(); ++i) {
        const AlternativeMedicine &a = alts.at(i);
        m_table->setItem(i, 0, new QTableWidgetItem(a.brandName));
        m_table->setItem(i, 1, new QTableWidgetItem(a.strength));
        m_table->setItem(i, 2, new QTableWidgetItem(a.form));
        m_table->setItem(i, 3, new QTableWidgetItem(a.manufacturer));

        auto *onHand = new QTableWidgetItem(QString::number(a.onHand));
        UiUtil::rightAlign(onHand);
        m_table->setItem(i, 4, onHand);

        const QString mrp
            = a.unitMrp.isEmpty() ? QStringLiteral("—") : Money::fromString(a.unitMrp).fmt();
        auto *mrpItem = new QTableWidgetItem(mrp);
        UiUtil::rightAlign(mrpItem);
        m_table->setItem(i, 5, mrpItem);
    }
    UiUtil::emptyState(m_table, QStringLiteral("No in-stock alternatives of the same generic."));
    m_table->resizeRowsToContents();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addWidget(heading);
    layout->addWidget(m_table, 1);
    layout->addWidget(buttons);
}
