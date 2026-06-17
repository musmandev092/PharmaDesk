#include "domain/MachineFingerprint.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>

namespace MachineFingerprint {
namespace {

// App-specific salt so a fingerprint can't be correlated with the same machine's
// fingerprint in another product. Bump the version suffix if the scheme changes.
const QByteArray kTag = QByteArrayLiteral("pharmadesk-fingerprint-v1|");

QString hashValue(const QString &value)
{
    QByteArray data = kTag + value.trimmed().toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString readFirstLine(const QString &path)
{
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString::fromUtf8(f.readAll()).trimmed();
    }
    return QString();
}

// /etc/machine-id (fallback to the dbus copy). Primary, root-free anchor.
QString machineId()
{
    for (const QString &p :
         {QStringLiteral("/etc/machine-id"), QStringLiteral("/var/lib/dbus/machine-id")}) {
        const QString v = readFirstLine(p);
        if (!v.isEmpty()) {
            return v;
        }
    }
    return QString();
}

bool startsWithAny(const QString &s, std::initializer_list<const char *> prefixes)
{
    for (const char *p : prefixes) {
        if (s.startsWith(QLatin1String(p))) {
            return true;
        }
    }
    return false;
}

// First real NIC's MAC: skip loopback/virtual interfaces, prefer a physical
// device, deterministic (sorted) order — mirrors fingerprint.py::_primary_mac.
QString primaryMac()
{
    QDir net(QStringLiteral("/sys/class/net"));
    if (!net.exists()) {
        return QString();
    }
    struct Cand
    {
        int rank;
        QString mac;
    };
    QList<Cand> candidates;
    const QStringList ifaces = net.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &iface : ifaces) {
        if (startsWithAny(
                iface, {"lo", "docker", "veth", "virbr", "br-", "vmnet", "tun", "tap", "vnet"})) {
            continue;
        }
        const QString mac = readFirstLine(net.filePath(iface + QStringLiteral("/address")));
        if (mac.isEmpty() || mac == QLatin1String("00:00:00:00:00:00")) {
            continue;
        }
        const bool hasDevice = QFile::exists(net.filePath(iface + QStringLiteral("/device")));
        candidates.push_back({hasDevice ? 0 : 1, mac});
    }
    if (candidates.isEmpty()) {
        return QString();
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const Cand &a, const Cand &b) { return a.rank < b.rank; });
    return candidates.first().mac;
}

// Best-effort physical block-device serial (no root needed when exposed by the
// kernel). Empty when nothing readable — the fingerprint then relies on the rest.
QString diskSerial()
{
    QDir blockdir(QStringLiteral("/sys/block"));
    if (!blockdir.exists()) {
        return QString();
    }
    const QStringList devs = blockdir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &dev : devs) {
        if (startsWithAny(dev, {"loop", "ram", "dm-", "zram", "sr", "md"})) {
            continue;
        }
        for (const QString &rel : {QStringLiteral("device/serial"), QStringLiteral("device/wwid"),
                                   QStringLiteral("serial")}) {
            const QString v = readFirstLine(blockdir.filePath(dev + QLatin1Char('/') + rel));
            if (!v.isEmpty()) {
                return v;
            }
        }
    }
    return QString();
}

} // namespace

QMap<QString, QString> collectSignals()
{
    QMap<QString, QString> out;
    const QString mid = machineId();
    const QString mac = primaryMac();
    const QString disk = diskSerial();
    if (!mid.isEmpty()) {
        out.insert(QStringLiteral("machine_id"), hashValue(mid));
    }
    if (!mac.isEmpty()) {
        out.insert(QStringLiteral("mac"), hashValue(mac));
    }
    if (!disk.isEmpty()) {
        out.insert(QStringLiteral("disk"), hashValue(disk));
    }
    return out;
}

QString fingerprintCode()
{
    const QMap<QString, QString> sig = collectSignals();
    QStringList parts;
    for (auto it = sig.constBegin(); it != sig.constEnd(); ++it) {
        parts << it.key() + QLatin1Char('=') + it.value();
    }
    const QByteArray joined
        = kTag + QByteArrayLiteral("code|") + parts.join(QLatin1Char('|')).toUtf8();
    const QString digest
        = QString::fromLatin1(QCryptographicHash::hash(joined, QCryptographicHash::Sha256).toHex())
              .toUpper();
    QStringList groups;
    for (int i = 0; i < 16; i += 4) {
        groups << digest.mid(i, 4);
    }
    return QStringLiteral("PD-") + groups.join(QLatin1Char('-'));
}

} // namespace MachineFingerprint
