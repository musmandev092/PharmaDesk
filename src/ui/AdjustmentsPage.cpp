#include "ui/AdjustmentsPage.h"

#include "data/MedicineRepository.h"
#include "data/StockAdjustmentRepository.h"
#include "data/UserRepository.h"
#include "domain/Money.h"
#include "service/StockAdjuster.h"
#include "ui/ManagerOverrideDialog.h"
#include "ui/UiUtil.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

constexpr int kQtyMin = 1;
constexpr int kQtyMax = 100000;
constexpr int kSearchLimit = 20;
constexpr int kRecentLimit = 200;
constexpr int kBatchIdRole = Qt::UserRole;
constexpr int kMedicineIdRole = Qt::UserRole;

// The 7 adjustment reasons accepted by StockAdjuster, with human captions. The
// stored code is the data; the caption is display-only.
struct Reason
{
    const char *code;
    const char *caption;
};
const Reason kReasons[] = {
    {"DAMAGE", "Damage"},
    {"EXPIRY_WRITEOFF", "Expiry write-off"},
    {"SHRINKAGE", "Shrinkage"},
    {"COUNT_CORRECTION", "Count correction"},
    {"SAMPLE", "Sample"},
    {"DONATION", "Donation"},
    {"OTHER", "Other (notes required)"},
};
constexpr const char *kReasonOther = "OTHER";

QTableWidget *makeTable(const QStringList &headers)
{
    auto *t = new QTableWidget;
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->verticalHeader()->setVisible(false);
    t->setWordWrap(false);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    return t;
}

QString medicineDisplay(const MedicineRow &m)
{
    QString s = m.brandName;
    if (!m.strength.isEmpty()) s += QStringLiteral(" %1").arg(m.strength);
    if (!m.genericName.isEmpty()) s += QStringLiteral(" — %1").arg(m.genericName);
    return s;
}

QString reasonCaption(const QString &code)
{
    for (const Reason &r : kReasons)
        if (code == QLatin1String(r.code)) return QString::fromLatin1(r.caption);
    return code;
}

} // namespace

AdjustmentsPage::AdjustmentsPage(QSqlDatabase db, qint64 userId, QWidget *parent)
    : QWidget(parent), m_db(std::move(db)), m_userId(userId)
{
    buildUi();
    reload();
}

void AdjustmentsPage::buildUi()
{
    auto *title = new QLabel(QStringLiteral("Stock adjustments"), this);
    title->setObjectName(QStringLiteral("h1"));

    // ── Medicine search ────────────────────────────────────────────────────
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("Search brand, generic, SKU…"));
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged, this, &AdjustmentsPage::searchMedicines);

    m_results
        = makeTable({QStringLiteral("Medicine"), QStringLiteral("SKU"), QStringLiteral("On hand")});
    m_results->setMaximumHeight(220);
    connect(m_results, &QTableWidget::itemSelectionChanged, this,
            &AdjustmentsPage::onMedicineSelected);

    auto *searchBox = new QVBoxLayout;
    searchBox->setSpacing(8);
    auto *sh = new QLabel(QStringLiteral("1. Find a medicine"), this);
    sh->setObjectName(QStringLiteral("h2"));
    searchBox->addWidget(sh);
    searchBox->addWidget(m_search);
    searchBox->addWidget(m_results);

    // ── Adjustment form ────────────────────────────────────────────────────
    auto *form = new QFrame(this);
    form->setObjectName(QStringLiteral("card"));
    auto *formLayout = new QVBoxLayout(form);
    formLayout->setContentsMargins(16, 16, 16, 16);
    formLayout->setSpacing(8);

    auto *fh = new QLabel(QStringLiteral("2. Adjust a batch"), form);
    fh->setObjectName(QStringLiteral("h2"));
    formLayout->addWidget(fh);

    m_medicineLabel = new QLabel(QStringLiteral("No medicine selected."), form);
    m_medicineLabel->setObjectName(QStringLiteral("muted"));
    formLayout->addWidget(m_medicineLabel);

    auto *batchRow = new QHBoxLayout;
    batchRow->addWidget(new QLabel(QStringLiteral("Batch"), form));
    m_batchCombo = new QComboBox(form);
    m_batchCombo->setMinimumWidth(280);
    connect(m_batchCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &AdjustmentsPage::onBatchSelected);
    batchRow->addWidget(m_batchCombo, 1);
    formLayout->addLayout(batchRow);

    auto *qtyRow = new QHBoxLayout;
    qtyRow->addWidget(new QLabel(QStringLiteral("Action"), form));
    m_direction = new QComboBox(form);
    m_direction->addItem(QStringLiteral("Remove (−)"), -1);
    m_direction->addItem(QStringLiteral("Add (+)"), 1);
    qtyRow->addWidget(m_direction);
    qtyRow->addWidget(new QLabel(QStringLiteral("Quantity"), form));
    m_qty = new QSpinBox(form);
    m_qty->setRange(kQtyMin, kQtyMax);
    m_qty->setValue(kQtyMin);
    qtyRow->addWidget(m_qty);
    qtyRow->addStretch(1);
    formLayout->addLayout(qtyRow);

    auto *reasonRow = new QHBoxLayout;
    reasonRow->addWidget(new QLabel(QStringLiteral("Reason"), form));
    m_reason = new QComboBox(form);
    for (const Reason &r : kReasons)
        m_reason->addItem(QString::fromLatin1(r.caption), QString::fromLatin1(r.code));
    reasonRow->addWidget(m_reason, 1);
    formLayout->addLayout(reasonRow);

    formLayout->addWidget(new QLabel(QStringLiteral("Notes"), form));
    m_notes = new QPlainTextEdit(form);
    m_notes->setPlaceholderText(QStringLiteral("Optional — required when reason is Other."));
    m_notes->setMaximumHeight(80);
    formLayout->addWidget(m_notes);

    auto *applyRow = new QHBoxLayout;
    m_apply = new QPushButton(QStringLiteral("Apply adjustment"), form);
    m_apply->setObjectName(QStringLiteral("primary"));
    connect(m_apply, &QPushButton::clicked, this, &AdjustmentsPage::applyAdjustment);
    applyRow->addWidget(m_apply);
    m_status = new QLabel(QString(), form);
    m_status->setObjectName(QStringLiteral("muted"));
    m_status->setWordWrap(true);
    applyRow->addWidget(m_status, 1);
    formLayout->addLayout(applyRow);

    // ── Top: search (left) + form (right) ──────────────────────────────────
    auto *top = new QHBoxLayout;
    top->setSpacing(16);
    top->addLayout(searchBox, 1);
    top->addWidget(form, 1);

    // ── Recent adjustments ─────────────────────────────────────────────────
    auto *rh = new QLabel(QStringLiteral("Recent adjustments"), this);
    rh->setObjectName(QStringLiteral("h2"));
    m_recent
        = makeTable({QStringLiteral("#"), QStringLiteral("Medicine"), QStringLiteral("Batch"),
                     QStringLiteral("Δ Qty"), QStringLiteral("Reason"),
                     QStringLiteral("Cost impact"), QStringLiteral("By"), QStringLiteral("When")});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    layout->addWidget(title);
    layout->addLayout(top);
    layout->addWidget(rh);
    layout->addWidget(m_recent, 1);

    onBatchSelected();
}

void AdjustmentsPage::reload()
{
    searchMedicines();
    reloadRecent();
}

void AdjustmentsPage::searchMedicines()
{
    MedicineRepository repo(m_db);
    const QVector<MedicineRow> rows = repo.list(m_search ? m_search->text() : QString());

    UiUtil::beginFill(m_results);
    const int shown = qMin<int>(rows.size(), kSearchLimit);
    m_results->setRowCount(shown);
    for (int i = 0; i < shown; ++i) {
        const MedicineRow &m = rows.at(i);
        auto *name = new QTableWidgetItem(medicineDisplay(m));
        name->setData(kMedicineIdRole, static_cast<qlonglong>(m.id));
        m_results->setItem(i, 0, name);
        m_results->setItem(i, 1, new QTableWidgetItem(m.sku));
        auto *oh = new QTableWidgetItem(QStringLiteral("%1 %2").arg(m.onHand).arg(m.baseUnit));
        UiUtil::rightAlign(oh);
        m_results->setItem(i, 2, oh);
    }
    UiUtil::emptyState(m_results, QStringLiteral("No medicines found."));
}

void AdjustmentsPage::onMedicineSelected()
{
    const QList<QTableWidgetItem *> sel = m_results->selectedItems();
    if (sel.isEmpty()) return;

    auto *first = m_results->item(sel.first()->row(), 0);
    if (!first) return;
    const qint64 id = first->data(kMedicineIdRole).toLongLong();
    if (id <= 0) return;

    m_medicineId = id;
    m_medicineLabel->setText(first->text());
    loadBatches(id);
}

void AdjustmentsPage::loadBatches(qint64 medicineId)
{
    StockAdjustmentRepository repo(m_db);
    const QVector<AdjustableBatch> batches = repo.batchesForMedicine(medicineId);

    QSignalBlocker block(m_batchCombo);
    m_batchCombo->clear();
    for (const AdjustableBatch &b : batches) {
        QString flag;
        if (b.expired)
            flag = QStringLiteral("  [EXPIRED]");
        else if (b.quarantined)
            flag = QStringLiteral("  [QUARANTINED]");
        const QString label = QStringLiteral("%1 · exp %2 · %3%4")
                                  .arg(b.batchNumber, b.expiry)
                                  .arg(b.currentQty)
                                  .arg(flag);
        m_batchCombo->addItem(label, static_cast<qlonglong>(b.id));
    }
    block.unblock();
    onBatchSelected();
}

void AdjustmentsPage::onBatchSelected()
{
    const bool haveBatch = selectedBatchId() > 0;
    if (m_apply) m_apply->setEnabled(haveBatch);
}

qint64 AdjustmentsPage::selectedBatchId() const
{
    if (!m_batchCombo || m_batchCombo->currentIndex() < 0) return -1;
    bool ok = false;
    const qint64 id = m_batchCombo->currentData().toLongLong(&ok);
    return ok ? id : -1;
}

int AdjustmentsPage::signedQty() const
{
    const int sign = m_direction->currentData().toInt(); // -1 or +1
    return sign * m_qty->value();
}

void AdjustmentsPage::applyAdjustment()
{
    const qint64 batchId = selectedBatchId();
    if (batchId <= 0) {
        setStatus(QStringLiteral("Select a medicine and batch first."), true);
        return;
    }

    const QString reason = m_reason->currentData().toString();
    const QString notes = m_notes->toPlainText().trimmed();
    if (reason == QLatin1String(kReasonOther) && notes.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Notes required"),
                             QStringLiteral("Notes are required when the reason is Other."));
        return;
    }

    // Manager/admin PIN gate — PHP adjustments.php requires
    // ManagerOverride::verify(pin,'stock_adjustment') on EVERY adjustment so an
    // unattended workstation can't be used to write off stock. Self-confirm is
    // allowed (forbidUserId = 0 → exclude nobody), matching the PHP call which
    // passes no cashierId. The grant is audited (OVERRIDE_GRANTED).
    {
        ManagerOverrideDialog dlg(
            QStringLiteral("Authorize adjustment"),
            QStringLiteral("A manager or admin PIN is required to record a stock adjustment."),
            /*needLicense=*/false, this);
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
        UserRepository users(m_db);
        const UserRepository::OverrideResult ovr
            = users.verifyManagerOverride(dlg.pin(), 0, QStringLiteral("stock_adjustment"));
        if (!ovr.ok) {
            QMessageBox::warning(this, QStringLiteral("Authorization required"), ovr.error);
            return;
        }
    }

    const int qtyDelta = signedQty();
    StockAdjuster adjuster(m_db, m_userId);
    const AdjustResult res = adjuster.adjust(batchId, qtyDelta, reason, notes);
    if (!res.ok) {
        QMessageBox::warning(this, QStringLiteral("Adjustment rejected"), res.error);
        setStatus(res.error, true);
        return;
    }

    setStatus(QStringLiteral("Recorded %1 — cost impact PKR %2.")
                  .arg(res.adjustmentNumber, Money::fromString(res.costImpact).fmt()),
              false);

    m_qty->setValue(kQtyMin);
    m_notes->clear();

    // Stock changed: refresh the batch picker (qty), the medicine on-hand list,
    // and the history table.
    if (m_medicineId > 0) loadBatches(m_medicineId);
    searchMedicines();
    reloadRecent();
}

void AdjustmentsPage::reloadRecent()
{
    StockAdjustmentRepository repo(m_db);
    const QVector<AdjustmentRow> rows = repo.listRecent(kRecentLimit);

    UiUtil::beginFill(m_recent);
    m_recent->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const AdjustmentRow &r = rows.at(i);
        m_recent->setItem(i, 0, new QTableWidgetItem(r.number));
        m_recent->setItem(i, 1, new QTableWidgetItem(r.medicineName));
        m_recent->setItem(i, 2, new QTableWidgetItem(r.batchNumber));

        auto *delta
            = new QTableWidgetItem(QStringLiteral("%1%2")
                                       .arg(r.qtyDelta > 0 ? QStringLiteral("+") : QString())
                                       .arg(r.qtyDelta));
        UiUtil::colorItem(delta,
                          r.qtyDelta < 0 ? UiUtil::Palette::Danger : UiUtil::Palette::Success);
        UiUtil::rightAlign(delta);
        m_recent->setItem(i, 3, delta);

        m_recent->setItem(i, 4, new QTableWidgetItem(reasonCaption(r.reason)));

        auto *cost = new QTableWidgetItem(Money::fromString(r.costImpact).display());
        UiUtil::rightAlign(cost);
        m_recent->setItem(i, 5, cost);

        m_recent->setItem(i, 6, new QTableWidgetItem(r.performedBy));
        m_recent->setItem(i, 7, new QTableWidgetItem(r.createdAt));
    }
    UiUtil::emptyState(m_recent, QStringLiteral("No stock adjustments recorded yet."));
}

void AdjustmentsPage::setStatus(const QString &text, bool error)
{
    if (!m_status) return;
    m_status->setText(text);
    m_status->setStyleSheet(error ? QStringLiteral("color: %1;").arg(UiUtil::Palette::Danger.name())
                                  : QString());
    m_status->setObjectName(error ? QString() : QStringLiteral("muted"));
}
