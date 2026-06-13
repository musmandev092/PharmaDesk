#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>

// One line of a parked (suspended) cart. Mirrors the POS CartLine shape: the
// medicine, its display name + base unit, the per-unit MRP (decimal string),
// and the quantity. Stored serialized as JSON in parked_carts.items_json.
struct ParkedItem
{
    qint64 medicineId = 0;
    QString name;
    QString baseUnit;
    QString unitMrp; // decimal string
    int qty = 0;
};

// A parked cart header. `items` is populated by load(); list() leaves it empty
// but fills the summary fields (itemCount, total) for the picker table.
struct ParkedCart
{
    qint64 id = 0;
    QString label;
    QString createdAt;
    int itemCount = 0;
    QString total; // money, scale-2 decimal string
    QVector<ParkedItem> items;
};

// Suspend & resume a POS cart. Cart lines are serialized to JSON; the summary
// total is computed with Money (sum of qty * unitMrp, rounded HALF_UP to 2 dp)
// to match the sale path. Park/discard are audited (CART_PARKED /
// CART_DISCARDED). Resuming is done by the caller: load() the items, hand them
// back to the POS, then discard() the row.
class ParkedCartRepository
{
public:
    explicit ParkedCartRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    // Serialize `items` to JSON and insert a parked_carts row. Returns the new
    // id, or -1 on failure (see errorString()). Empty carts are rejected.
    qint64 park(qint64 cashierId, const QString &label, const QVector<ParkedItem> &items,
                qint64 userId);

    // All parked carts for the cashier, newest first, with itemCount + total
    // summaries filled (items left empty — call load() for the lines).
    QVector<ParkedCart> list(qint64 cashierId) const;

    // Full parked cart including its item lines. Returns false if not found.
    bool load(qint64 id, ParkedCart *out) const;

    // Delete a parked cart row (audited CART_DISCARDED). Used both for an
    // explicit discard and after a successful resume.
    bool discard(qint64 id, qint64 userId);

    QString errorString() const { return m_error; }

private:
    QSqlDatabase m_db;
    mutable QString m_error;
};
