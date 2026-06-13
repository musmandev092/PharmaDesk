#pragma once

#include "service/ThermalPrinter.h"

// Sends raw bytes to an EXISTING CUPS print queue via `lp -d <queue> -o raw`.
//
// Read-only with respect to CUPS: it only submits a job to a queue the operator
// has already configured — it never runs lpadmin, never edits a PPD, and never
// touches CUPS configuration. The queue name comes from settings.
class CupsRawPrinter : public ThermalPrinter
{
public:
    explicit CupsRawPrinter(QString queue) : m_queue(std::move(queue)) {}

    ThermalPrintResult printRaw(const QByteArray &bytes, const QString &label) override;

private:
    QString m_queue;
};
