#include "service/CupsRawPrinter.h"

#include <QProcess>

ThermalPrintResult CupsRawPrinter::printRaw(const QByteArray &bytes, const QString &label)
{
    ThermalPrintResult res;

    if (m_queue.trimmed().isEmpty()) {
        res.error = QStringLiteral("No thermal printer queue is configured.");
        return res;
    }

    // `lp -d <queue> -o raw -t <label>` — submit a job to an existing queue.
    // argv form (never a shell string) so the queue/label can't inject commands.
    QProcess lp;
    lp.start(QStringLiteral("lp"),
             {QStringLiteral("-d"), m_queue, QStringLiteral("-o"), QStringLiteral("raw"),
              QStringLiteral("-t"), label.isEmpty() ? QStringLiteral("receipt") : label});
    if (!lp.waitForStarted(5000)) {
        res.error = QStringLiteral("Could not run 'lp' — is CUPS installed?");
        return res;
    }
    lp.write(bytes);
    lp.closeWriteChannel();
    if (!lp.waitForFinished(15000)) {
        lp.kill();
        res.error = QStringLiteral("Print job timed out.");
        return res;
    }
    if (lp.exitStatus() != QProcess::NormalExit || lp.exitCode() != 0) {
        const QString err = QString::fromUtf8(lp.readAllStandardError()).trimmed();
        res.error = err.isEmpty() ? QStringLiteral("lp returned a non-zero status.") : err;
        return res;
    }
    res.ok = true;
    return res;
}
