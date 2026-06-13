#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>

struct GrnDocRow
{
    qint64 id = 0;
    QString grnNumber;
    QString supplierName;
    QString invoiceNumber;
    QString postedAt;
    QString grandTotal;
    QString status;
    int lineCount = 0;
};

// Read model for the goods-receipt list.
class GrnRepository
{
public:
    explicit GrnRepository(QSqlDatabase db) : m_db(std::move(db)) {}
    QVector<GrnDocRow> list(int limit = 200) const;

private:
    QSqlDatabase m_db;
};
