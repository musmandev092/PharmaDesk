#pragma once

#include <QDate>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

// One GRN line: how much of a medicine was received, in purchase units.
struct GrnLineInput
{
    qint64 medicineId = 0;
    QString batchNumber;
    QDate expiry;
    int paidQty = 0;            // purchase units paid for
    int focQty = 0;             // free-of-charge purchase units
    QString unitCost;           // cost per purchase unit (decimal string)
    QString mrpPerPurchaseUnit; // MRP per purchase unit (decimal string)
};

struct GrnResult
{
    bool ok = false;
    QString error;
    qint64 grnId = -1;
    QString grnNumber;
    QString subtotal;
};

// Goods-received note posting. Port of pos/backend/services/Grn.php, collapsed
// to a single-step post (create + post) for the single-PC desktop flow — the
// draft/cancel lifecycle can be layered back on later. Each line blends cost
// (CostBlender), finds-or-creates the batch keyed (medicine, batch#, expiry),
// bumps stock, and writes a GRN_RECEIPT movement. One transaction.
class GrnService
{
public:
    GrnService(QSqlDatabase db, qint64 userId) : m_db(std::move(db)), m_userId(userId) {}

    GrnResult post(qint64 supplierId, const QVector<GrnLineInput> &lines,
                   const QString &invoiceNumber = QString(), const QDate &invoiceDate = QDate(),
                   const QString &notes = QString());

private:
    QString nextGrnNumber();

    QSqlDatabase m_db;
    qint64 m_userId;
};
