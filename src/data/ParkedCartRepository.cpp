#include "data/ParkedCartRepository.h"

#include "data/Audit.h"
#include "domain/Money.h"
#include "domain/SaleCalculator.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

// Serialize cart lines to a compact JSON array. Field names mirror the schema
// columns / PHP cart_json so a stored cart is self-describing.
QString itemsToJson(const QVector<ParkedItem> &items)
{
    QJsonArray arr;
    for (const ParkedItem &it : items) {
        QJsonObject o;
        o.insert(QStringLiteral("medicine_id"), static_cast<double>(it.medicineId));
        o.insert(QStringLiteral("name"), it.name);
        o.insert(QStringLiteral("base_unit"), it.baseUnit);
        o.insert(QStringLiteral("unit_mrp"), it.unitMrp);
        o.insert(QStringLiteral("qty"), it.qty);
        arr.append(o);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QVector<ParkedItem> itemsFromJson(const QString &json)
{
    QVector<ParkedItem> out;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) {
        return out;
    }
    const QJsonArray arr = doc.array();
    out.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        ParkedItem it;
        it.medicineId = static_cast<qint64>(o.value(QStringLiteral("medicine_id")).toDouble());
        it.name = o.value(QStringLiteral("name")).toString();
        it.baseUnit = o.value(QStringLiteral("base_unit")).toString();
        it.unitMrp = o.value(QStringLiteral("unit_mrp")).toString();
        it.qty = o.value(QStringLiteral("qty")).toInt();
        out.push_back(it);
    }
    return out;
}

// Sum of qty * unitMrp across the lines, rounded HALF_UP to 2 dp — same path as
// the cart subtotal in the POS.
QString summaryTotal(const QVector<ParkedItem> &items)
{
    Money total;
    for (const ParkedItem &it : items) {
        total = total + Money::fromString(SaleCalculator::lineSubtotal(it.qty, it.unitMrp));
    }
    return total.toString();
}

} // namespace

qint64 ParkedCartRepository::park(qint64 cashierId, const QString &label,
                                  const QVector<ParkedItem> &items, qint64 userId)
{
    if (items.isEmpty()) {
        m_error = QStringLiteral("Cart is empty — nothing to park.");
        return -1;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO parked_carts (cashier_id, label, items_json) VALUES (?, ?, ?)"));
    q.addBindValue(cashierId > 0 ? QVariant(cashierId) : QVariant());
    q.addBindValue(label.trimmed().isEmpty() ? QVariant() : QVariant(label.trimmed()));
    q.addBindValue(itemsToJson(items));
    if (!q.exec()) {
        m_error = q.lastError().text();
        return -1;
    }
    const qint64 id = q.lastInsertId().toLongLong();

    QJsonObject after;
    after.insert(QStringLiteral("label"), label.trimmed());
    after.insert(QStringLiteral("items_count"), items.size());
    after.insert(QStringLiteral("total"), summaryTotal(items));
    Audit::write(m_db, userId, QStringLiteral("CART_PARKED"), QStringLiteral("parked_carts"), id,
                 QString(), QString::fromUtf8(QJsonDocument(after).toJson(QJsonDocument::Compact)));
    return id;
}

QVector<ParkedCart> ParkedCartRepository::list(qint64 cashierId) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id, label, items_json, created_at FROM parked_carts "
                             "WHERE cashier_id = ? ORDER BY created_at DESC, id DESC"));
    q.addBindValue(cashierId > 0 ? QVariant(cashierId) : QVariant());

    QVector<ParkedCart> out;
    if (!q.exec()) {
        m_error = q.lastError().text();
        return out;
    }
    while (q.next()) {
        ParkedCart c;
        c.id = q.value(0).toLongLong();
        c.label = q.value(1).toString();
        const QString json = q.value(2).toString();
        c.createdAt = q.value(3).toString();
        const QVector<ParkedItem> items = itemsFromJson(json);
        c.itemCount = items.size();
        c.total = summaryTotal(items);
        out.push_back(c);
    }
    return out;
}

bool ParkedCartRepository::load(qint64 id, ParkedCart *out) const
{
    QSqlQuery q(m_db);
    q.prepare(
        QStringLiteral("SELECT id, label, items_json, created_at FROM parked_carts WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec() || !q.next()) {
        m_error = QStringLiteral("Parked bill not found.");
        return false;
    }
    out->id = q.value(0).toLongLong();
    out->label = q.value(1).toString();
    out->createdAt = q.value(3).toString();
    out->items = itemsFromJson(q.value(2).toString());
    out->itemCount = out->items.size();
    out->total = summaryTotal(out->items);
    return true;
}

bool ParkedCartRepository::discard(qint64 id, qint64 userId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM parked_carts WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    Audit::write(m_db, userId, QStringLiteral("CART_DISCARDED"), QStringLiteral("parked_carts"),
                 id);
    return true;
}
