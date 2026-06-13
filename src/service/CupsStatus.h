#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// A USB printer device reported by `lpinfo`.
struct UsbPrinter
{
    QString uri;
    QString description;
};

// Combined CUPS health snapshot for one queue (mirrors Cups::status() in
// pos/backend/services/Cups.php).
struct CupsSnapshot
{
    bool cupsActive = false;      // cupsd responds to lpstat -r
    bool driverInstalled = false; // rastertozj filter / zj80 PPD present
    bool queueExists = false;     // the named queue is configured
    QString queueName;
};

// Read-only CUPS diagnostics for the Admin → Printer page. Mirrors the query
// surface of pos/backend/services/Cups.php (status, queueExists, usbPrinters)
// but never mutates CUPS: it only runs lpstat/lpinfo, never lpadmin. Actual
// printing stays in CupsRawPrinter.
class CupsStatus
{
public:
    // Default queue name used as a placeholder before the operator picks one.
    static QString defaultQueue();

    // Everything the Printer page needs in one shot. queueExists/driver are
    // only probed when cupsd is actually responsive (matches the PHP guard).
    static CupsSnapshot snapshot(const QString &queue);

    static bool cupsResponsive();
    static bool driverInstalled(const QString &queue);
    static bool queueExists(const QString &queue);

    // Configured CUPS destinations (for the queue picker).
    static QStringList listQueues();

    // USB printers CUPS can see right now (for the "detected" list).
    static QVector<UsbPrinter> usbPrinters();
};
