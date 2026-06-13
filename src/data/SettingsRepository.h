#pragma once

#include <QMap>
#include <QSqlDatabase>
#include <QString>

// Key/value access to the `settings` table (upsert semantics, mirroring the
// PHP "INSERT ... ON CONFLICT(key) DO UPDATE").
class SettingsRepository
{
public:
    explicit SettingsRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    QString get(const QString &key, const QString &fallback = QString()) const;
    QMap<QString, QString> getAll() const;

    // Upsert a single key. updatedBy may be -1 when no user context (rare).
    bool set(const QString &key, const QString &value, qint64 updatedBy);

    // Upsert many keys. NOTE: does not open its own transaction — callers that
    // need atomicity with other writes (the wizard) wrap it themselves.
    bool setMany(const QMap<QString, QString> &values, qint64 updatedBy);

    QString errorString() const { return m_error; }

private:
    QSqlDatabase m_db;
    mutable QString m_error;
};
