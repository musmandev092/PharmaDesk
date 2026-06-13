#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>

// A medicine as shown in the catalog list (with computed on-hand stock).
struct MedicineRow
{
    qint64 id = 0;
    QString sku;
    QString brandName;
    QString genericName;
    QString strength;
    QString form;
    QString manufacturer;
    QString controlledSchedule;
    bool isActive = true;
    QString baseUnit;
    QString purchaseUnit;
    int unitsPerPurchase = 1;
    int onHand = 0;
};

// Editable medicine fields (create/update).
struct MedicineDraft
{
    QString sku;
    QString primaryBarcode; // empty → NULL
    QString brandName;
    QString genericName;
    QString strength;            // empty → NULL
    QString manufacturer;        // empty → NULL
    QString therapeuticCategory; // empty → NULL
    QString form = QStringLiteral("TABLET");
    QString formCustom; // empty → NULL
    QString purchaseUnit = QStringLiteral("BOX");
    QString baseUnit = QStringLiteral("TABLET");
    int unitsPerPurchase = 1;
    QString controlledSchedule = QStringLiteral("NONE");
    QString taxCode = QStringLiteral("EXEMPT");
    int reorderLevel = 0;
    int reorderQuantity = 0;
    QString reorderUnit = QStringLiteral("PURCHASE");
    bool prescriptionRequired = false;
    bool isActive = true;
};

// A POS search hit (matches the shape pos MedicineSearch returns).
struct PosMedicine
{
    qint64 id = 0;
    QString brandName;
    QString genericName;
    QString strength;
    QString form;
    QString baseUnit;
    QString purchaseUnit;
    int unitsPerPurchase = 1;
    QString controlledSchedule;
    bool prescriptionRequired = false;
    QString unitMrp; // soonest-expiry valid batch MRP (decimal string), may be empty
    int inStock = 0; // sum of valid (non-expired) batch qty
};

class MedicineRepository
{
public:
    explicit MedicineRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    // Catalog list. Empty query → all live medicines; else LIKE on
    // brand/generic/sku/manufacturer/barcode.
    QVector<MedicineRow> list(const QString &query = QString()) const;

    // Load one medicine into a draft (valid=false if not found).
    bool find(qint64 id, MedicineDraft *out) const;

    // Live-row uniqueness checks (friendly errors before the partial-unique
    // index fires). exceptId ignores a row (for edits).
    bool skuInUse(const QString &sku, qint64 exceptId = 0) const;
    bool barcodeInUse(const QString &barcode, qint64 exceptId = 0) const;

    // Create → new id (or -1). Update → bool. Both audit-write.
    qint64 create(const MedicineDraft &d, qint64 userId);
    bool update(qint64 id, const MedicineDraft &d, qint64 userId);
    bool softDelete(qint64 id, qint64 userId);

    // POS search (live medicines only) with unit_mrp + in_stock.
    QVector<PosMedicine> searchForPos(const QString &query, int limit = 20) const;

    QString errorString() const { return m_error; }

private:
    QSqlDatabase m_db;
    mutable QString m_error;
};
