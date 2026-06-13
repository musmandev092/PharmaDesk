#include "domain/Log.h"

#include "domain/AppPaths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QTextStream>

#include <cstdio>

namespace Log {
namespace {

constexpr qint64 kMaxBytes = 1 * 1024 * 1024; // rotate at ~1 MB
QString g_path;
QMutex g_mutex;

const char *levelName(QtMsgType t)
{
    switch (t) {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
        return "ERROR";
    case QtFatalMsg:
        return "FATAL";
    }
    return "INFO";
}

void rotateIfNeeded()
{
    QFileInfo fi(g_path);
    if (fi.exists() && fi.size() > kMaxBytes) {
        const QString prev = g_path + QStringLiteral(".1");
        QFile::remove(prev);
        QFile::rename(g_path, prev);
    }
}

void handler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    QMutexLocker lock(&g_mutex);
    const QString line = QStringLiteral("%1 [%2] %3")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODate),
                                  QString::fromLatin1(levelName(type)), msg);

#ifndef QT_NO_DEBUG
    fprintf(stderr, "%s\n", line.toLocal8Bit().constData());
#endif

    if (g_path.isEmpty()) {
        return;
    }
    rotateIfNeeded();
    QFile f(g_path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream(&f) << line << '\n';
    }
    if (type == QtFatalMsg) {
        abort();
    }
}

} // namespace

void init()
{
    AppPaths::ensureDirs();
    g_path = AppPaths::logsDir() + QStringLiteral("/pharmadesk.log");
    qInstallMessageHandler(handler);
    qInfo("PharmaDesk started");
}

QString filePath()
{
    return g_path;
}

void fatal(const QString &what)
{
    // Use the message handler path (already locked internally) but never abort
    // here — the caller decides whether to terminate.
    QMutexLocker lock(&g_mutex);
    if (g_path.isEmpty()) {
        return;
    }
    QFile f(g_path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream(&f) << QDateTime::currentDateTime().toString(Qt::ISODate)
                        << QStringLiteral(" [FATAL] ") << what << '\n';
    }
}

} // namespace Log
