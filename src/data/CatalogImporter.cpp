#include "data/CatalogImporter.h"

#include "data/MedicineRepository.h"

#include <QFile>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QStringList>
#include <QVector>

namespace CatalogImporter {
namespace {

// RFC-4180-ish parser: handles quoted fields, doubled "" escapes, and commas or
// newlines inside quotes. Returns one QStringList per record.
QVector<QStringList> parseCsv(const QString &text)
{
    QVector<QStringList> rows;
    QStringList row;
    QString cur;
    bool inQuotes = false;
    bool fieldStarted = false;

    auto endField = [&]() {
        row << cur;
        cur.clear();
        fieldStarted = false;
    };
    auto endRow = [&]() {
        endField();
        rows << row;
        row.clear();
    };

    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (inQuotes) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('"')) {
                    cur += QLatin1Char('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                cur += c;
            }
            continue;
        }
        if (c == QLatin1Char('"')) {
            inQuotes = true;
            fieldStarted = true;
        } else if (c == QLatin1Char(',')) {
            endField();
        } else if (c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
            if (c == QLatin1Char('\r') && i + 1 < text.size()
                && text.at(i + 1) == QLatin1Char('\n')) {
                ++i;
            }
            if (fieldStarted || !cur.isEmpty() || !row.isEmpty()) {
                endRow();
            }
        } else {
            cur += c;
            fieldStarted = true;
        }
    }
    if (fieldStarted || !cur.isEmpty() || !row.isEmpty()) {
        endRow();
    }
    return rows;
}

bool toBool(const QString &v)
{
    const QString s = v.trimmed().toLower();
    return s == QLatin1String("t") || s == QLatin1String("true") || s == QLatin1String("1");
}

QString cell(const QStringList &row, const QHash<QString, int> &idx, const char *name)
{
    const int i = idx.value(QString::fromLatin1(name), -1);
    return (i >= 0 && i < row.size()) ? row.at(i).trimmed() : QString();
}

} // namespace

Result importFromCsv(QSqlDatabase &db, const QString &csvPath, qint64 userId)
{
    Result result;

    QFile f(csvPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.error = QStringLiteral("Could not open catalog file: %1").arg(csvPath);
        return result;
    }
    const QVector<QStringList> rows = parseCsv(QString::fromUtf8(f.readAll()));
    if (rows.isEmpty()) {
        result.error = QStringLiteral("Catalog file is empty: %1").arg(csvPath);
        return result;
    }

    // Header → column-index map (order-independent).
    QHash<QString, int> idx;
    const QStringList &header = rows.first();
    for (int i = 0; i < header.size(); ++i) {
        idx.insert(header.at(i).trimmed(), i);
    }
    for (const char *required : {"sku", "brand_name", "generic_name", "form", "purchase_unit",
                                 "base_unit", "units_per_purchase"}) {
        if (!idx.contains(QString::fromLatin1(required))) {
            result.error = QStringLiteral("Catalog file missing required column: %1")
                               .arg(QString::fromLatin1(required));
            return result;
        }
    }

    if (!db.transaction()) {
        result.error = db.lastError().text();
        return result;
    }

    MedicineRepository repo(db);
    for (int r = 1; r < rows.size(); ++r) {
        const QStringList &row = rows.at(r);
        if (row.size() < header.size()) {
            continue; // ragged / blank trailing line
        }
        // Never import rows that were soft-deleted in the source system.
        if (!cell(row, idx, "deleted_at").isEmpty()) {
            ++result.skipped;
            continue;
        }
        const QString sku = cell(row, idx, "sku");
        if (sku.isEmpty() || repo.skuInUse(sku)) {
            ++result.skipped;
            continue;
        }

        MedicineDraft d;
        d.sku = sku;
        d.primaryBarcode = cell(row, idx, "primary_barcode");
        d.brandName = cell(row, idx, "brand_name");
        d.genericName = cell(row, idx, "generic_name");
        d.strength = cell(row, idx, "strength");
        d.manufacturer = cell(row, idx, "manufacturer");
        d.therapeuticCategory = cell(row, idx, "therapeutic_category");
        d.form = cell(row, idx, "form");
        d.formCustom = cell(row, idx, "form_custom");
        d.purchaseUnit = cell(row, idx, "purchase_unit");
        d.baseUnit = cell(row, idx, "base_unit");
        d.unitsPerPurchase = qMax(1, cell(row, idx, "units_per_purchase").toInt());
        d.controlledSchedule = cell(row, idx, "controlled_schedule");
        if (d.controlledSchedule.isEmpty()) {
            d.controlledSchedule = QStringLiteral("NONE");
        }
        d.taxCode = cell(row, idx, "tax_code_value");
        if (d.taxCode.isEmpty()) {
            d.taxCode = QStringLiteral("EXEMPT");
        }
        d.reorderLevel = cell(row, idx, "reorder_level").toInt();
        d.reorderQuantity = cell(row, idx, "reorder_quantity").toInt();
        d.reorderUnit = cell(row, idx, "reorder_unit");
        if (d.reorderUnit.isEmpty()) {
            d.reorderUnit = QStringLiteral("PURCHASE");
        }
        d.prescriptionRequired = toBool(cell(row, idx, "prescription_required"));
        d.isActive = idx.contains(QStringLiteral("is_active")) ? toBool(cell(row, idx, "is_active"))
                                                               : true;

        if (repo.create(d, userId) > 0) {
            ++result.imported;
        } else {
            ++result.failed;
        }
    }

    if (!db.commit()) {
        result.error = db.lastError().text();
        db.rollback();
        return result;
    }
    result.ok = true;
    return result;
}

} // namespace CatalogImporter
