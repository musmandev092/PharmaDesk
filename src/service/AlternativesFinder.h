#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>

// One alternative medicine: a different brand of the same generic, with its
// current saleable on-hand quantity.
struct AlternativeMedicine
{
    qint64 id = 0;
    QString brandName;
    QString strength;
    QString form;
    QString manufacturer;
    int onHand = 0;  // saleable base units (valid, in-date, non-quarantined)
    QString unitMrp; // soonest-expiry valid batch MRP, decimal string (may be empty)
};

// Internal, offline drug-alternatives lookup (port of the PHP
// medicine-alternatives API, but sourced entirely from our own catalog rather
// than an external DB). Finds other active medicines sharing the same
// generic_name, ranked by in-stock availability.
class AlternativesFinder
{
public:
    explicit AlternativesFinder(QSqlDatabase db) : m_db(std::move(db)) {}

    // Alternatives to `medicineId` (excludes itself). Empty if the medicine has
    // no generic_name or no live alternatives.
    QVector<AlternativeMedicine> forMedicine(qint64 medicineId) const;

private:
    QSqlDatabase m_db;
};
