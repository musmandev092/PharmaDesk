#include "data/SettingsRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

QString SettingsRepository::get(const QString &key, const QString &fallback) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    q.addBindValue(key);
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    return fallback;
}

QMap<QString, QString> SettingsRepository::getAll() const
{
    QMap<QString, QString> out;
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral("SELECT key, value FROM settings"))) {
        while (q.next()) {
            out.insert(q.value(0).toString(), q.value(1).toString());
        }
    }
    return out;
}

bool SettingsRepository::set(const QString &key, const QString &value, qint64 updatedBy)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO settings (key, value, updated_at, updated_by) "
                             "VALUES (?, ?, CURRENT_TIMESTAMP, ?) "
                             "ON CONFLICT(key) DO UPDATE SET "
                             "  value = excluded.value, "
                             "  updated_at = CURRENT_TIMESTAMP, "
                             "  updated_by = excluded.updated_by"));
    q.addBindValue(key);
    q.addBindValue(value);
    q.addBindValue(updatedBy < 0 ? QVariant() : QVariant(updatedBy));
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

bool SettingsRepository::setMany(const QMap<QString, QString> &values, qint64 updatedBy)
{
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (!set(it.key(), it.value(), updatedBy)) {
            return false;
        }
    }
    return true;
}
