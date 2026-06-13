#pragma once

#include <QString>

// Application logging. Installs a Qt message handler that writes every
// qDebug/qInfo/qWarning/qCritical message to a rotating log file under the
// app-data logs/ directory (and to stderr in debug builds). Without this, Qt
// routes messages to the systemd journal or nowhere, leaving field issues
// undiagnosable. Call init() once, right after QApplication is constructed.
namespace Log {

// Install the file message handler (size-capped, rotates one generation).
void init();

// Absolute path to the active log file.
QString filePath();

// Record an unhandled/fatal condition (used by the crash handler) and flush.
void fatal(const QString &what);

} // namespace Log
