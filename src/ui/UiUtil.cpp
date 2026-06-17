#include "ui/UiUtil.h"

#include <QAbstractButton>
#include <QApplication>
#include <QBrush>
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QWidget>

namespace UiUtil {

namespace {
// Event filter installed on a dialog's line edits: turns an Enter keypress
// (what a barcode scanner sends after the code) into a focus advance, and
// swallows it so it never reaches a default button and submits the form.
class EnterAdvances : public QObject
{
public:
    explicit EnterAdvances(QObject *parent) : QObject(parent) {}

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        if (ev->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent *>(ev);
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                if (auto *w = qobject_cast<QWidget *>(obj)) {
                    // Advance focus the same way Tab would (honours tab order).
                    QKeyEvent tab(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
                    QApplication::sendEvent(w, &tab);
                }
                return true; // consume — never trigger a Save/OK default button
            }
        }
        return QObject::eventFilter(obj, ev);
    }
};
} // namespace

void makeScanSafe(QWidget *dialog)
{
    if (!dialog) {
        return;
    }
    auto *guard = new EnterAdvances(dialog);
    for (QLineEdit *le : dialog->findChildren<QLineEdit *>()) {
        le->installEventFilter(guard);
    }
    // No button should fire on a stray Enter from a scanner.
    for (QPushButton *b : dialog->findChildren<QPushButton *>()) {
        b->setAutoDefault(false);
        b->setDefault(false);
    }
    for (QDialogButtonBox *box : dialog->findChildren<QDialogButtonBox *>()) {
        for (QAbstractButton *ab : box->buttons()) {
            auto *pb = qobject_cast<QPushButton *>(ab);
            if (!pb) {
                continue;
            }
            pb->setAutoDefault(false);
            pb->setDefault(false);
            // Standard box buttons default to the primary (teal) style. Give the
            // non-affirmative ones the secondary look and force a re-polish so the
            // dynamic property is actually picked up by the stylesheet.
            const QDialogButtonBox::ButtonRole role = box->buttonRole(ab);
            if (role == QDialogButtonBox::RejectRole || role == QDialogButtonBox::DestructiveRole) {
                if (pb->property("variant").toString().isEmpty()) {
                    pb->setProperty("variant", "secondary");
                }
            }
            if (!pb->property("variant").toString().isEmpty()) {
                pb->style()->unpolish(pb);
                pb->style()->polish(pb);
            }
        }
    }
}

void rightAlign(QTableWidgetItem *item)
{
    if (item) {
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
}

void colorItem(QTableWidgetItem *item, const QColor &color, bool bold)
{
    if (!item) {
        return;
    }
    item->setForeground(QBrush(color));
    if (bold) {
        QFont f = item->font();
        f.setBold(true);
        item->setFont(f);
    }
}

void beginFill(QTableWidget *table)
{
    if (!table) {
        return;
    }
    table->clearSpans();
    // Drop any cell widgets (e.g. status pills from setBadge) left by a previous
    // fill, so a row that no longer has a badge doesn't keep a stale one painted
    // over its new item.
    for (int r = 0; r < table->rowCount(); ++r) {
        for (int c = 0; c < table->columnCount(); ++c) {
            if (table->cellWidget(r, c)) {
                table->removeCellWidget(r, c);
            }
        }
    }
}

void emptyState(QTableWidget *table, const QString &message)
{
    if (!table || table->rowCount() > 0 || table->columnCount() == 0) {
        return;
    }
    table->setRowCount(1);
    auto *item = new QTableWidgetItem(message);
    item->setTextAlignment(Qt::AlignCenter);
    item->setFlags(Qt::ItemIsEnabled); // not selectable/editable
    item->setForeground(QBrush(Palette::Muted));
    table->setItem(0, 0, item);
    table->setSpan(0, 0, 1, table->columnCount());
    table->setRowHeight(0, 80);
}

void setBadge(QTableWidget *table, int row, int col, const QString &text, const QColor &color)
{
    if (!table || row < 0 || col < 0) {
        return;
    }
    // Tint = the status colour at ~14% over white — a soft pill background that
    // stays legible with the full-strength colour as text (matches the web app's
    // badge tints). QSS has no alpha-blend, so flatten it here.
    constexpr double a = 0.14;
    const QColor tint(static_cast<int>(255 * (1 - a) + color.red() * a),
                      static_cast<int>(255 * (1 - a) + color.green() * a),
                      static_cast<int>(255 * (1 - a) + color.blue() * a));

    auto *pill = new QLabel(text);
    pill->setAlignment(Qt::AlignCenter);
    pill->setStyleSheet(QStringLiteral("QLabel { background: %1; color: %2; border-radius: 9px; "
                                       "padding: 2px 9px; font-size: 11px; font-weight: 700; }")
                            .arg(tint.name(), color.name()));
    // QSS padding isn't reliably reflected in the label's sizeHint, so the host
    // table's ResizeToContents can under-size the column and clip the text. Pin a
    // minimum width from font metrics (text + the 2×9px padding) so it never clips.
    QFont pf = pill->font();
    pf.setPixelSize(11);
    pf.setBold(true);
    pill->setMinimumWidth(QFontMetrics(pf).horizontalAdvance(text) + 24);

    // Centre the pill in the cell (don't stretch it full-width).
    auto *cell = new QWidget;
    auto *l = new QHBoxLayout(cell);
    l->setContentsMargins(6, 3, 6, 3);
    l->addStretch();
    l->addWidget(pill);
    l->addStretch();

    table->takeItem(row, col); // drop any prior item in this cell
    table->setCellWidget(row, col, cell);
}

} // namespace UiUtil
