#include "ui/PosTerminalPage.h"

#include "data/MedicineRepository.h"
#include "data/ParkedCartRepository.h"
#include "data/SettingsRepository.h"
#include "data/UserRepository.h"
#include "domain/ControlledSubstancePolicy.h"
#include "domain/DiscountAuthorizationPolicy.h"
#include "domain/Money.h"
#include "domain/SaleCalculator.h"
#include "domain/SettingsKeys.h"
#include "service/SaleService.h"
#include "ui/AlternativesDialog.h"
#include "ui/ManagerOverrideDialog.h"
#include "ui/ParkedBillsDialog.h"
#include "ui/ReceiptDialog.h"
#include "ui/UiUtil.h"

#include <QSet>
#include <QSqlQuery>

#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QInputDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QShortcut>
#include <QSqlError>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

PosTerminalPage::PosTerminalPage(QSqlDatabase db, qint64 userId, QWidget *parent)
    : QWidget(parent), m_db(std::move(db)), m_userId(userId)
{
    // Role drives discount self-authorization (managerial cashiers skip the
    // override prompt; the service still re-validates server-side).
    m_role = UserRepository(m_db).roleOf(m_userId);

    // ── Left: search + results ─────────────────────────────────────────────
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("Search a medicine to sell…"));
    m_search->setClearButtonEnabled(true);
    // Free-standing search input: cap so it doesn't stretch full-bleed on 4K.
    m_search->setMaximumWidth(560);

    m_results = new QTableWidget(this);
    m_results->setColumnCount(3);
    m_results->setHorizontalHeaderLabels(
        {QStringLiteral("Medicine"), QStringLiteral("In stock"), QStringLiteral("MRP")});
    // Right-align headers over the numeric "In stock" and "MRP" columns so the
    // header text sits over its right-aligned values (column 0 stays left).
    for (int col : {1, 2})
        m_results->horizontalHeaderItem(col)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_results->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_results->setSelectionMode(QAbstractItemView::SingleSelection);
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->verticalHeader()->setVisible(false);
    m_results->setWordWrap(false);
    m_results->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_results->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    auto *addBtn = new QPushButton(QStringLiteral("Add unit"), this);
    auto *addPackBtn = new QPushButton(QStringLiteral("Add pack"), this);
    addPackBtn->setProperty("variant", "secondary");
    m_alternatives = new QPushButton(QStringLiteral("Alternatives"), this);
    m_alternatives->setProperty("variant", "ghost");
    m_alternatives->setEnabled(false); // enabled once a result row is selected
    auto *addRow = new QHBoxLayout;
    addRow->setSpacing(8);
    addRow->addWidget(addBtn, 2);
    addRow->addWidget(addPackBtn, 1);
    addRow->addWidget(m_alternatives, 1);

    auto *left = new QVBoxLayout;
    left->setSpacing(12);
    auto *lh = new QLabel(QStringLiteral("Find product"), this);
    lh->setObjectName(QStringLiteral("h2"));
    left->addWidget(lh);
    left->addWidget(m_search);
    left->addWidget(m_results, 1);
    left->addLayout(addRow);

    // ── Right: cart + checkout ─────────────────────────────────────────────
    m_cart = new QTableWidget(this);
    m_cart->setColumnCount(4);
    m_cart->setHorizontalHeaderLabels({QStringLiteral("Item"), QStringLiteral("Qty"),
                                       QStringLiteral("MRP"), QStringLiteral("Total")});
    // Right-align headers over the numeric Qty/MRP/Total columns to match their
    // right-aligned cell values (column 0 "Item" stays left-aligned).
    for (int col : {1, 2, 3})
        m_cart->horizontalHeaderItem(col)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_cart->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_cart->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_cart->verticalHeader()->setVisible(false);
    // Item gets the slack; qty/MRP/total size to their content so the medicine
    // name is never clipped by the qty spinbox.
    auto *cartHeader = m_cart->horizontalHeader();
    cartHeader->setSectionResizeMode(0, QHeaderView::Stretch);
    cartHeader->setSectionResizeMode(1, QHeaderView::Fixed);
    cartHeader->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    cartHeader->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    // Wide enough that the qty spinbox (and both up/down steppers) renders fully
    // without clipping the right-hand arrows.
    m_cart->setColumnWidth(1, 96);
    m_cart->setWordWrap(true);

    auto *removeBtn = new QPushButton(QStringLiteral("Remove line"), this);
    removeBtn->setProperty("variant", "secondary");
    auto *clearBtn = new QPushButton(QStringLiteral("Clear"), this);
    clearBtn->setProperty("variant", "ghost");
    auto *parkBtn = new QPushButton(QStringLiteral("Park"), this);
    parkBtn->setProperty("variant", "secondary");
    auto *resumeBtn = new QPushButton(QStringLiteral("Parked…"), this);
    resumeBtn->setProperty("variant", "secondary");
    auto *cartBtns = new QHBoxLayout;
    cartBtns->setSpacing(8);
    cartBtns->addWidget(removeBtn);
    cartBtns->addWidget(parkBtn);
    cartBtns->addWidget(resumeBtn);
    cartBtns->addWidget(clearBtn);
    cartBtns->addStretch();

    m_subtotal = new QLabel(QStringLiteral("0.00"), this);
    m_discount = new QLineEdit(this);
    m_discount->setText(QStringLiteral("0"));
    m_discount->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^\\d{1,9}(\\.\\d{1,2})?$")), m_discount));
    m_grand = new QLabel(QStringLiteral("0.00"), this);
    m_grand->setObjectName(QStringLiteral("h2"));
    m_payment = new QComboBox(this);
    m_payment->addItems({QStringLiteral("CASH"), QStringLiteral("CARD"), QStringLiteral("OTHER")});
    m_tendered = new QLineEdit(this);
    m_tendered->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^\\d{1,9}(\\.\\d{1,2})?$")), m_tendered));
    m_change = new QLabel(QStringLiteral("0.00"), this);

    auto *totals = new QFormLayout;
    totals->setSpacing(8);
    totals->addRow(QStringLiteral("Subtotal"), m_subtotal);
    totals->addRow(QStringLiteral("Discount"), m_discount);
    totals->addRow(QStringLiteral("Grand total"), m_grand);
    totals->addRow(QStringLiteral("Payment"), m_payment);
    totals->addRow(QStringLiteral("Tendered"), m_tendered);
    totals->addRow(QStringLiteral("Change"), m_change);

    m_complete = new QPushButton(QStringLiteral("Complete sale"), this);

    auto *right = new QVBoxLayout;
    right->setSpacing(12);
    auto *rh = new QLabel(QStringLiteral("Cart"), this);
    rh->setObjectName(QStringLiteral("h2"));
    right->addWidget(rh);
    right->addWidget(m_cart, 1);
    right->addLayout(cartBtns);
    right->addLayout(totals);
    right->addWidget(m_complete);

    // ── Responsive wrapper ─────────────────────────────────────────────────
    // Move the two-column split onto a content widget that carries the page's
    // 24px margins, then host it in a scroll area so nothing clips on short or
    // narrow screens. The split (two data tables) keeps the full width — POS
    // benefits from width, so we do not cap the content.
    auto *content = new QWidget;
    content->setObjectName(QStringLiteral("scrollContent"));
    auto *grid = new QHBoxLayout(content);
    grid->setContentsMargins(24, 24, 24, 24);
    grid->setSpacing(16);
    grid->addLayout(left, 3);
    grid->addLayout(right, 2);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("scrollContent"));
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // Keep the themed page background visible behind the transparent scroll area.
    scroll->setStyleSheet(QStringLiteral("QScrollArea#scrollContent, QWidget#scrollContent { "
                                         "background: transparent; border: none; }"));
    content->setAutoFillBackground(false);

    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    pageLayout->addWidget(scroll);

    connect(m_search, &QLineEdit::textChanged, this, &PosTerminalPage::runSearch);
    connect(addBtn, &QPushButton::clicked, this, &PosTerminalPage::addUnit);
    connect(addPackBtn, &QPushButton::clicked, this, &PosTerminalPage::addPack);
    connect(m_results, &QTableWidget::doubleClicked, this, &PosTerminalPage::addUnit);
    connect(m_alternatives, &QPushButton::clicked, this, &PosTerminalPage::showAlternatives);
    // The Alternatives action needs a highlighted result to act on.
    connect(m_results, &QTableWidget::itemSelectionChanged, this, [this]() {
        m_alternatives->setEnabled(m_results->currentRow() >= 0
                                   && m_results->item(m_results->currentRow(), 0) != nullptr);
    });
    connect(parkBtn, &QPushButton::clicked, this, &PosTerminalPage::parkBill);
    connect(resumeBtn, &QPushButton::clicked, this, &PosTerminalPage::resumeBill);
    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        const int row = m_cart->currentRow();
        if (row >= 0 && row < m_cartLines.size()) {
            m_cartLines.remove(row);
            renderCart();
        }
    });
    connect(clearBtn, &QPushButton::clicked, this, &PosTerminalPage::clearCart);
    connect(m_discount, &QLineEdit::textChanged, this, &PosTerminalPage::recompute);
    connect(m_tendered, &QLineEdit::textChanged, this, &PosTerminalPage::recompute);
    connect(m_complete, &QPushButton::clicked, this, &PosTerminalPage::completeSale);

    // Keyboard flow: Enter in the search box adds the highlighted (or first)
    // result; Ctrl+Enter completes the sale.
    connect(m_search, &QLineEdit::returnPressed, this, [this]() {
        if (m_results->rowCount() == 0) {
            return;
        }
        if (m_results->currentRow() < 0) {
            m_results->selectRow(0);
        }
        addUnit();
    });
    auto *completeSc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    connect(completeSc, &QShortcut::activated, this, &PosTerminalPage::completeSale);

    m_search->setFocus();
    renderCart();
}

void PosTerminalPage::runSearch()
{
    MedicineRepository repo(m_db);
    const QVector<PosMedicine> hits = repo.searchForPos(m_search->text());
    UiUtil::beginFill(m_results);
    m_results->setRowCount(hits.size());
    for (int i = 0; i < hits.size(); ++i) {
        const PosMedicine &p = hits.at(i);
        const bool controlled = p.controlledSchedule != QLatin1String("NONE");
        QString label = QStringLiteral("%1 %2\n%3").arg(p.brandName, p.strength, p.genericName);
        if (controlled) {
            label += QStringLiteral("   [%1]").arg(p.controlledSchedule);
        }
        auto *name = new QTableWidgetItem(label);
        name->setData(Qt::UserRole, p.id);
        name->setData(Qt::UserRole + 1, p.brandName + QLatin1Char(' ') + p.strength);
        name->setData(Qt::UserRole + 2, p.baseUnit);
        name->setData(Qt::UserRole + 3, p.unitMrp);
        name->setData(Qt::UserRole + 4, p.inStock);
        name->setData(Qt::UserRole + 5, p.unitsPerPurchase);
        name->setData(Qt::UserRole + 6, p.purchaseUnit);
        if (p.controlledSchedule == QLatin1String("NARCOTIC")) {
            UiUtil::colorItem(name, UiUtil::Palette::Danger, true);
        } else if (controlled) {
            UiUtil::colorItem(name, UiUtil::Palette::Warning);
        }
        m_results->setItem(i, 0, name);
        auto *stock = new QTableWidgetItem(QStringLiteral("%1 %2").arg(p.inStock).arg(p.baseUnit));
        UiUtil::rightAlign(stock);
        m_results->setItem(i, 1, stock);
        auto *mrp = new QTableWidgetItem(
            p.unitMrp.isEmpty() ? QStringLiteral("—") : Money::fromString(p.unitMrp).display());
        UiUtil::rightAlign(mrp);
        m_results->setItem(i, 2, mrp);
    }
    UiUtil::emptyState(m_results, m_search->text().trimmed().isEmpty()
                                      ? QStringLiteral("Type or scan a medicine to begin.")
                                      : QStringLiteral("No medicines match — check stock."));
    m_results->resizeRowsToContents();
}

void PosTerminalPage::addUnit()
{
    addSelected(false);
}
void PosTerminalPage::addPack()
{
    addSelected(true);
}

void PosTerminalPage::addSelected(bool asPack)
{
    const int row = m_results->currentRow();
    if (row < 0 || !m_results->item(row, 0)) {
        return;
    }
    QTableWidgetItem *it = m_results->item(row, 0);
    const qint64 id = it->data(Qt::UserRole).toLongLong();
    const QString mrp = it->data(Qt::UserRole + 3).toString();
    const int inStock = it->data(Qt::UserRole + 4).toInt();
    const int upp = qMax(1, it->data(Qt::UserRole + 5).toInt());
    const QString baseUnit = it->data(Qt::UserRole + 2).toString();
    const QString purchaseUnit = it->data(Qt::UserRole + 6).toString();
    if (mrp.isEmpty() || inStock <= 0) {
        QMessageBox::information(
            this, QStringLiteral("Out of stock"),
            QStringLiteral("This medicine has no sellable stock. Add stock first."));
        return;
    }
    const int factor = asPack ? upp : 1;
    const QString label = asPack ? purchaseUnit : baseUnit;

    // Merge only if same medicine AND same sold unit (factor).
    bool merged = false;
    for (CartLine &c : m_cartLines) {
        if (c.medicineId == id && c.factor == factor) {
            c.qty += 1;
            merged = true;
            break;
        }
    }
    if (!merged) {
        m_cartLines.push_back({id, it->data(Qt::UserRole + 1).toString(), mrp, 1, factor, label});
    }
    renderCart();
    // Ready for the next scan/search.
    m_search->clear();
    m_search->setFocus();
}

void PosTerminalPage::showAlternatives()
{
    const int row = m_results->currentRow();
    if (row < 0 || !m_results->item(row, 0)) {
        return;
    }
    QTableWidgetItem *it = m_results->item(row, 0);
    // medicineId + display name are stashed on the name cell by runSearch()
    // (UserRole = id, UserRole+1 = "brand strength").
    const qint64 id = it->data(Qt::UserRole).toLongLong();
    const QString name = it->data(Qt::UserRole + 1).toString();
    AlternativesDialog dlg(m_db, id, name, this);
    dlg.exec();
}

void PosTerminalPage::renderCart()
{
    UiUtil::beginFill(m_cart);
    // Reset to zero rows first: QTableWidget does NOT destroy cell widgets (the
    // qty spin boxes) when the row count merely shrinks/grows, which left stale
    // spin boxes overlapping the item column. Clearing to 0 deletes them.
    m_cart->setRowCount(0);
    m_cart->setRowCount(m_cartLines.size());
    for (int i = 0; i < m_cartLines.size(); ++i) {
        const CartLine &c = m_cartLines.at(i);
        // Name shows the sold unit when selling by pack (e.g. "Panadol (BOX of 100)").
        const QString itemText
            = c.factor > 1 ? QStringLiteral("%1\n(%2 of %3)").arg(c.name, c.unitLabel).arg(c.factor)
                           : c.name;
        m_cart->setItem(i, 0, new QTableWidgetItem(itemText));

        auto *qty = new QSpinBox(m_cart);
        qty->setRange(1, 9999);
        qty->setValue(c.qty);
        // Min width keeps both stepper arrows visible; don't hard-fix so the
        // spinbox can fill the (wider) qty column instead of clipping.
        qty->setMinimumWidth(72);
        connect(qty, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, i](int v) {
            if (i < m_cartLines.size()) {
                m_cartLines[i].qty = v;
                recompute();
            }
        });
        m_cart->setCellWidget(i, 1, qty);

        // MRP per sold unit = base MRP × factor; line total = qty × factor × baseMRP.
        auto *mrp = new QTableWidgetItem(Money::fromString(c.unitMrp).mul(c.factor).fmt());
        UiUtil::rightAlign(mrp);
        m_cart->setItem(i, 2, mrp);
        auto *tot = new QTableWidgetItem(
            Money::fromString(SaleCalculator::lineSubtotal(qint64(c.qty) * c.factor, c.unitMrp))
                .fmt());
        UiUtil::rightAlign(tot);
        m_cart->setItem(i, 3, tot);
    }
    UiUtil::emptyState(m_cart, QStringLiteral("Cart is empty."));
    m_cart->resizeRowsToContents();
    recompute();
}

void PosTerminalPage::recompute()
{
    Money subtotal;
    for (const CartLine &c : m_cartLines) {
        subtotal = subtotal
                   + Money::fromString(
                       SaleCalculator::lineSubtotal(qint64(c.qty) * c.factor, c.unitMrp));
    }
    const Money discount = Money::fromString(m_discount->text());
    Money grand = subtotal - discount;
    if (grand.isNegative()) {
        grand = Money();
    }
    m_subtotal->setText(subtotal.display());
    m_grand->setText(grand.display());

    if (!m_tendered->text().trimmed().isEmpty()) {
        m_change->setText(
            Money::fromString(SaleCalculator::change(m_tendered->text(), grand.toString()))
                .display());
    } else {
        m_change->setText(QStringLiteral("PKR 0.00"));
    }
}

void PosTerminalPage::clearCart()
{
    m_cartLines.clear();
    m_discount->setText(QStringLiteral("0"));
    m_tendered->clear();
    renderCart();
}

void PosTerminalPage::completeSale()
{
    if (m_cartLines.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Sale"), QStringLiteral("Cart is empty."));
        return;
    }

    SaleInput in;
    in.paymentMode = m_payment->currentText();
    in.discountTotal = m_discount->text().trimmed().isEmpty() ? QStringLiteral("0")
                                                              : m_discount->text().trimmed();
    in.amountTendered = m_tendered->text().trimmed();
    for (const CartLine &c : m_cartLines) {
        SaleLineInput li;
        li.medicineId = c.medicineId;
        li.qtySoldDisplay = c.qty;
        li.soldUnitLabel = c.unitLabel;
        li.soldUnitFactor = c.factor;
        li.unitMrp = c.unitMrp;
        in.items << li;
    }

    // ── Pre-commit authorization prompts ───────────────────────────────────
    // Collect the controlled schedules present in the cart (one query over the
    // distinct medicine ids) so we know whether a witness / license is required.
    QSet<qint64> distinctIds;
    for (const CartLine &c : m_cartLines) {
        distinctIds.insert(c.medicineId);
    }
    QStringList schedules;
    if (!distinctIds.isEmpty()) {
        QStringList placeholders;
        for (int i = 0; i < distinctIds.size(); ++i) {
            placeholders << QStringLiteral("?");
        }
        QSqlQuery q(m_db);
        q.prepare(
            QStringLiteral("SELECT DISTINCT controlled_schedule FROM medicines WHERE id IN (%1)")
                .arg(placeholders.join(QLatin1Char(','))));
        for (qint64 id : distinctIds) {
            q.addBindValue(id);
        }
        if (!q.exec()) {
            // Fail CLOSED — a DB error must never silently drop the controlled
            // gate (witness/license) and allow a narcotic to check out.
            QMessageBox::critical(
                this, QStringLiteral("Checkout blocked"),
                QStringLiteral("Could not verify controlled-substance status:\n%1\n\nThe cart is "
                               "intact — please retry.")
                    .arg(q.lastError().text()));
            return;
        }
        while (q.next()) {
            schedules << q.value(0).toString();
        }
    }
    const bool needWitness = ControlledSubstancePolicy::anyRequiresWitness(schedules);
    const bool needLicense = ControlledSubstancePolicy::anyRequiresPrescriberLicense(schedules);

    // Discount override: a cashier applying a positive discount needs a separate
    // manager/admin PIN. Managerial roles self-authorize (no prompt).
    const Money discount = Money::fromString(m_discount->text());
    const bool discountPositive = !discount.isZero() && !discount.isNegative();
    if (discountPositive && DiscountAuthorizationPolicy::requiresManagerOverride(m_role, true)) {
        ManagerOverrideDialog dlg(
            QStringLiteral("Discount approval"),
            QStringLiteral("A manager or admin PIN is required to apply a discount."), false, this);
        if (dlg.exec() != QDialog::Accepted) {
            return; // cashier cancelled — abort silently
        }
        const UserRepository::OverrideResult res = UserRepository(m_db).verifyManagerOverride(
            dlg.pin(), m_userId, QStringLiteral("discount"));
        if (!res.ok) {
            QMessageBox::warning(this, QStringLiteral("Discount approval"), res.error);
            return;
        }
        in.discountAuthorizedBy = res.userId;
    }

    // Controlled-substance gate: capture the prescriber license (when required)
    // and a witnessing manager/admin PIN (a different user than the cashier).
    if (needWitness || needLicense) {
        ManagerOverrideDialog dlg(
            QStringLiteral("Controlled substance"),
            QStringLiteral("This sale includes a controlled medicine. Enter the prescriber's "
                           "license number and a witnessing manager/admin PIN."),
            true, this);
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
        if (needLicense && dlg.license().trimmed().isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Controlled substance"),
                                 QStringLiteral("Prescriber license number is required."));
            return;
        }
        const UserRepository::OverrideResult res = UserRepository(m_db).verifyManagerOverride(
            dlg.pin(), m_userId, QStringLiteral("controlled_witness"));
        if (!res.ok) {
            QMessageBox::warning(this, QStringLiteral("Controlled substance"), res.error);
            return;
        }
        in.prescriberLicense = dlg.license().trimmed();
        in.controlledWitnessUserId = res.userId;
    }

    // Guard against a double-click committing twice.
    m_complete->setEnabled(false);
    SaleService service(m_db, m_userId);
    const SaleResult r = service.commit(in);
    m_complete->setEnabled(true);
    if (!r.ok) {
        QMessageBox::warning(this, QStringLiteral("Sale not completed"), r.error);
        return;
    }

    SettingsRepository settings(m_db);
    ReceiptDialog receipt(r, settings.get(SettingsKeys::PharmacyName),
                          settings.get(SettingsKeys::LogoPath), m_db, m_userId, this);
    receipt.exec();

    clearCart();
    runSearch(); // refresh stock figures
}

void PosTerminalPage::parkBill()
{
    if (m_cartLines.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Park bill"),
                                 QStringLiteral("Cart is empty."));
        return;
    }
    bool ok = false;
    const QString label = QInputDialog::getText(this, QStringLiteral("Park bill"),
                                                QStringLiteral("Label (customer name, optional):"),
                                                QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        return;
    }
    QVector<ParkedItem> items;
    for (const CartLine &c : m_cartLines) {
        // Park as BASE units (qty × factor) at the per-base MRP, so resuming
        // restores the exact stock quantity and total (factor folded into qty).
        items.push_back({c.medicineId, c.name, c.unitLabel, c.unitMrp, c.qty * c.factor});
    }
    ParkedCartRepository repo(m_db);
    if (repo.park(m_userId, label, items, m_userId) < 0) {
        QMessageBox::warning(this, QStringLiteral("Park bill"), repo.errorString());
        return;
    }
    clearCart();
    m_search->setFocus();
}

void PosTerminalPage::resumeBill()
{
    if (!m_cartLines.isEmpty()) {
        QMessageBox::information(
            this, QStringLiteral("Resume"),
            QStringLiteral("Clear or park the current cart before resuming another."));
        return;
    }
    ParkedBillsDialog dlg(m_db, m_userId, this);
    if (dlg.exec() != QDialog::Accepted || dlg.resumedCart().id <= 0) {
        return;
    }
    const ParkedCart &pc = dlg.resumedCart();
    m_cartLines.clear();
    for (const ParkedItem &it : pc.items) {
        // Resume as base units (factor 1); the stored unitMrp is per sold-unit,
        // but parked carts hold base lines in this build, so factor 1 is correct.
        m_cartLines.push_back({it.medicineId, it.name, it.unitMrp, it.qty, 1, it.baseUnit});
    }
    ParkedCartRepository(m_db).discard(pc.id, m_userId);
    renderCart();
}
