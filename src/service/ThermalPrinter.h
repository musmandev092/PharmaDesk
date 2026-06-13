#pragma once

#include <QByteArray>
#include <QString>

struct ThermalPrintResult
{
    bool ok = false;
    QString error;
};

// Abstraction over a raw byte-stream (ESC/POS) printer, injected where receipts
// are printed so the UI and tests can mock it. Implementations MUST NOT modify
// the host print system's configuration.
class ThermalPrinter
{
public:
    virtual ~ThermalPrinter() = default;

    // Submit raw bytes to the printer. `label` is a human job name.
    virtual ThermalPrintResult printRaw(const QByteArray &bytes, const QString &label) = 0;
};
