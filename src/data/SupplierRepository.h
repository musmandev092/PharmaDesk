#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>

struct SupplierRow
{
    qint64 id = 0;
    QString name;
    QString contactPhone;
    QString ntn;
    QString address;
    QString bookerName;
    QString salesmanName;
    QString paymentTerms = QStringLiteral("CASH");
    QString notes;
    bool isActive = true;
};

class SupplierRepository
{
public:
    explicit SupplierRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    QVector<SupplierRow> list(const QString &query = QString()) const;
    bool find(qint64 id, SupplierRow *out) const;
    qint64 create(const SupplierRow &s, qint64 userId);
    bool update(qint64 id, const SupplierRow &s, qint64 userId);
    bool softDelete(qint64 id, qint64 userId);

    QString errorString() const { return m_error; }

private:
    QSqlDatabase m_db;
    mutable QString m_error;
};
