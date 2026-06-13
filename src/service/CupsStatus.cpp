#include "service/CupsStatus.h"

#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

namespace {

constexpr int kTimeoutMs = 4000;

// Run a CUPS client binary with a bounded timeout. argv form (never a shell
// string) so a queue/printer name can't inject commands. Returns whether the
// process exited 0 and captures stdout.
struct RunResult
{
    bool ok = false;
    QString out;
};

RunResult run(const QString &program, const QStringList &args, int timeoutMs = kTimeoutMs)
{
    RunResult r;
    QProcess p;
    p.start(program, args);
    if (!p.waitForStarted(timeoutMs)) {
        return r; // binary missing / not on PATH
    }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(500);
        return r;
    }
    r.out = QString::fromUtf8(p.readAllStandardOutput());
    r.ok = p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    return r;
}

} // namespace

QString CupsStatus::defaultQueue()
{
    // Matches the queue the printer/ host-install scripts create.
    return QStringLiteral("PharmaDesk_Receipt_80mm");
}

bool CupsStatus::cupsResponsive()
{
    // `lpstat -r` prints "scheduler is running" when cupsd is up.
    const RunResult r = run(QStringLiteral("lpstat"), {QStringLiteral("-r")}, 2000);
    return r.ok && r.out.contains(QStringLiteral("is running"));
}

bool CupsStatus::driverInstalled(const QString &queue)
{
    // 1. The rastertozj filter binary (driver core) on either common path.
    for (const QString &p : {QStringLiteral("/usr/lib/cups/filter/rastertozj"),
                             QStringLiteral("/usr/libexec/cups/filter/rastertozj")}) {
        if (QFileInfo::exists(p)) {
            return true;
        }
    }
    // 2. The queue's PPD references a ZJ-80 / Zijiang driver.
    if (!queue.trimmed().isEmpty()) {
        const QString ppd = QStringLiteral("/etc/cups/ppd/") + queue + QStringLiteral(".ppd");
        QFile f(ppd);
        if (f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString head = QString::fromUtf8(f.read(4096)).toLower();
            for (const QString &needle :
                 {QStringLiteral("zj-80"), QStringLiteral("zj80"), QStringLiteral("zijiang"),
                  QStringLiteral("rastertozj")}) {
                if (head.contains(needle)) {
                    return true;
                }
            }
        }
    }
    // 3. Last resort: lpinfo -m lists a zj / zijiang model.
    const RunResult r = run(QStringLiteral("lpinfo"), {QStringLiteral("-m")});
    if (r.ok) {
        const QString lower = r.out.toLower();
        if (lower.contains(QStringLiteral("zj")) || lower.contains(QStringLiteral("zijiang"))) {
            return true;
        }
    }
    return false;
}

bool CupsStatus::queueExists(const QString &queue)
{
    if (queue.trimmed().isEmpty()) {
        return false;
    }
    const RunResult r = run(QStringLiteral("lpstat"), {QStringLiteral("-p"), queue}, 2000);
    return r.ok;
}

QStringList CupsStatus::listQueues()
{
    // `lpstat -e` prints one configured destination name per line.
    const RunResult r = run(QStringLiteral("lpstat"), {QStringLiteral("-e")});
    QStringList queues;
    if (!r.ok) {
        return queues;
    }
    const QStringList lines
        = r.out.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString name = line.trimmed();
        if (!name.isEmpty()) {
            queues << name;
        }
    }
    return queues;
}

QVector<UsbPrinter> CupsStatus::usbPrinters()
{
    const RunResult r = run(QStringLiteral("lpinfo"),
                            {QStringLiteral("--include-schemes=usb"), QStringLiteral("-v")}, 5000);
    QVector<UsbPrinter> devices;
    if (!r.ok) {
        return devices;
    }
    // Lines look like: "direct usb://Zijiang/ZJ-80?serial=... \"Zijiang ZJ-80\""
    static const QRegularExpression re(QStringLiteral("^\\s*\\S+\\s+(usb:\\S+)\\s*(.*)$"));
    const QStringList lines
        = r.out.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QRegularExpressionMatch m = re.match(line);
        if (m.hasMatch()) {
            UsbPrinter d;
            d.uri = m.captured(1);
            d.description = m.captured(2).trimmed();
            d.description.remove(QRegularExpression(QStringLiteral("^[\"']|[\"']$")));
            devices.append(d);
        }
    }
    return devices;
}

CupsSnapshot CupsStatus::snapshot(const QString &queue)
{
    CupsSnapshot s;
    s.queueName = queue;
    s.cupsActive = cupsResponsive();
    if (s.cupsActive) {
        s.driverInstalled = driverInstalled(queue);
        s.queueExists = queueExists(queue);
    }
    return s;
}
