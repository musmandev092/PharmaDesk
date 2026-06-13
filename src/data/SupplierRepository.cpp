#include "data/SupplierRepository.h"

#include "data/Audit.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
QVariant nz(const QString &s)
{
    return s.isEmpty() ? QVariant() : QVariant(s);
}
} // namespace

QVector<SupplierRow> SupplierRepository::list(const QString &query) const
{
    QString sql = QStringLiteral(
        "SELECT id, name, contact_phone, ntn, address, booker_name, salesman_name, "
        "       payment_terms, notes, is_active "
        "  FROM suppliers WHERE deleted_at IS NULL");
    const QString q = query.trimmed();
    if (!q.isEmpty()) {
        sql += QStringLiteral(" AND (lower(name) LIKE :like ESCAPE '\\' "
                              "      OR lower(COALESCE(contact_phone,'')) LIKE :like ESCAPE '\\' "
                              "      OR lower(COALESCE(ntn,'')) LIKE :like ESCAPE '\\')");
    }
    sql += QStringLiteral(" ORDER BY name LIMIT 300");

    QSqlQuery sq(m_db);
    sq.prepare(sql);
    if (!q.isEmpty()) {
        QString esc = q.toLower();
        esc.replace(QLatin1Char('%'), QStringLiteral("\\%"))
            .replace(QLatin1Char('_'), QStringLiteral("\\_"));
        sq.bindValue(QStringLiteral(":like"), QStringLiteral("%%%1%%").arg(esc));
    }

    QVector<SupplierRow> out;
    if (sq.exec()) {
        while (sq.next()) {
            SupplierRow r;
            r.id = sq.value(0).toLongLong();
            r.name = sq.value(1).toString();
            r.contactPhone = sq.value(2).toString();
            r.ntn = sq.value(3).toString();
            r.address = sq.value(4).toString();
            r.bookerName = sq.value(5).toString();
            r.salesmanName = sq.value(6).toString();
            r.paymentTerms = sq.value(7).toString();
            r.notes = sq.value(8).toString();
            r.isActive = sq.value(9).toInt() != 0;
            out.push_back(r);
        }
    }
    return out;
}

bool SupplierRepository::find(qint64 id, SupplierRow *out) const
{
    QSqlQuery q(m_db);
    q.prepare(
        QStringLiteral("SELECT id, name, contact_phone, ntn, address, booker_name, salesman_name, "
                       "       payment_terms, notes, is_active FROM suppliers WHERE id = ? AND "
                       "deleted_at IS NULL"));
    q.addBindValue(id);
    if (!q.exec() || !q.next()) {
        return false;
    }
    out->id = q.value(0).toLongLong();
    out->name = q.value(1).toString();
    out->contactPhone = q.value(2).toString();
    out->ntn = q.value(3).toString();
    out->address = q.value(4).toString();
    out->bookerName = q.value(5).toString();
    out->salesmanName = q.value(6).toString();
    out->paymentTerms = q.value(7).toString();
    out->notes = q.value(8).toString();
    out->isActive = q.value(9).toInt() != 0;
    return true;
}

qint64 SupplierRepository::create(const SupplierRow &s, qint64 userId)
{
    {
        QSqlQuery dup(m_db);
        dup.prepare(QStringLiteral(
            "SELECT 1 FROM suppliers WHERE lower(name)=lower(?) AND deleted_at IS NULL LIMIT 1"));
        dup.addBindValue(s.name);
        if (dup.exec() && dup.next()) {
            m_error = QStringLiteral("A supplier named '%1' already exists.").arg(s.name);
            return -1;
        }
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO suppliers (name, contact_phone, ntn, address, booker_name, "
        " salesman_name, payment_terms, notes, is_active) VALUES (?,?,?,?,?,?,?,?,?)"));
    q.addBindValue(s.name);
    q.addBindValue(nz(s.contactPhone));
    q.addBindValue(nz(s.ntn));
    q.addBindValue(nz(s.address));
    q.addBindValue(nz(s.bookerName));
    q.addBindValue(nz(s.salesmanName));
    q.addBindValue(s.paymentTerms.isEmpty() ? QStringLiteral("CASH") : s.paymentTerms);
    q.addBindValue(nz(s.notes));
    q.addBindValue(s.isActive ? 1 : 0);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return -1;
    }
    const qint64 id = q.lastInsertId().toLongLong();
    Audit::write(m_db, userId, QStringLiteral("SUPPLIER_CREATED"), QStringLiteral("suppliers"), id);
    return id;
}

bool SupplierRepository::update(qint64 id, const SupplierRow &s, qint64 userId)
{
    {
        QSqlQuery dup(m_db);
        dup.prepare(QStringLiteral(
            "SELECT 1 FROM suppliers WHERE lower(name)=lower(?) AND deleted_at IS NULL "
            "AND id <> ? LIMIT 1"));
        dup.addBindValue(s.name);
        dup.addBindValue(id);
        if (dup.exec() && dup.next()) {
            m_error = QStringLiteral("Another supplier named '%1' already exists.").arg(s.name);
            return false;
        }
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE suppliers SET name=?, contact_phone=?, ntn=?, address=?, booker_name=?, "
        " salesman_name=?, payment_terms=?, notes=?, is_active=?, updated_at=CURRENT_TIMESTAMP "
        "WHERE id=? AND deleted_at IS NULL"));
    q.addBindValue(s.name);
    q.addBindValue(nz(s.contactPhone));
    q.addBindValue(nz(s.ntn));
    q.addBindValue(nz(s.address));
    q.addBindValue(nz(s.bookerName));
    q.addBindValue(nz(s.salesmanName));
    q.addBindValue(s.paymentTerms.isEmpty() ? QStringLiteral("CASH") : s.paymentTerms);
    q.addBindValue(nz(s.notes));
    q.addBindValue(s.isActive ? 1 : 0);
    q.addBindValue(id);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    Audit::write(m_db, userId, QStringLiteral("SUPPLIER_UPDATED"), QStringLiteral("suppliers"), id);
    return true;
}

bool SupplierRepository::softDelete(qint64 id, qint64 userId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE suppliers SET deleted_at=CURRENT_TIMESTAMP, is_active=0, "
                             "updated_at=CURRENT_TIMESTAMP WHERE id=? AND deleted_at IS NULL"));
    q.addBindValue(id);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    Audit::write(m_db, userId, QStringLiteral("SUPPLIER_DELETED"), QStringLiteral("suppliers"), id);
    return true;
}
