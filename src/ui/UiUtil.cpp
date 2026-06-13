#include "ui/UiUtil.h"

#include <QAbstractButton>
#include <QApplication>
#include <QBrush>
#include <QDialogButtonBox>
#include <QFont>
#include <QKeyEvent>
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
    if (table) {
        table->clearSpans();
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

} // namespace UiUtil
