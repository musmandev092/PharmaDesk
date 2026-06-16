#include "data/XlsxReader.h"

#include <QFile>
#include <QHash>
#include <QXmlStreamReader>
#include <QtEndian>

#include <zlib.h>

namespace XlsxReader {
namespace {

// Safety caps — an admin-chosen file, but never trust sizes blindly (zip bombs).
constexpr qint64 kMaxArchive = 64LL * 1024 * 1024;           // 64 MB on disk
constexpr qint64 kMaxUncompressedPart = 256LL * 1024 * 1024; // 256 MB per part
constexpr int kMaxCols = 4096;

quint16 rd16(const QByteArray &b, int off)
{
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(b.constData() + off));
}
quint32 rd32(const QByteArray &b, int off)
{
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(b.constData() + off));
}

// Raw DEFLATE inflate (zip stores method 8 with no zlib header).
bool inflateRaw(const QByteArray &in, qint64 expected, QByteArray &out)
{
    if (expected < 0 || expected > kMaxUncompressedPart) {
        return false;
    }
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
        return false;
    }
    out.resize(static_cast<int>(expected));
    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(in.constData()));
    zs.avail_in = static_cast<uInt>(in.size());
    zs.next_out = reinterpret_cast<Bytef *>(out.data());
    zs.avail_out = static_cast<uInt>(expected);
    const int rc = inflate(&zs, Z_FINISH);
    const uLong produced = zs.total_out;
    inflateEnd(&zs);
    if (rc != Z_STREAM_END || static_cast<qint64>(produced) != expected) {
        return false;
    }
    return true;
}

// Minimal ZIP central-directory reader. Returns the named entries' bytes.
class Zip
{
public:
    explicit Zip(const QByteArray &data) : m_data(data) { parse(); }
    bool ok() const { return m_ok; }
    // Case-sensitive exact path within the archive (e.g. "xl/sharedStrings.xml").
    bool has(const QString &name) const { return m_entries.contains(name); }
    QByteArray extract(const QString &name) const;

private:
    struct Entry
    {
        quint16 method = 0;
        quint32 compSize = 0;
        quint32 uncompSize = 0;
        quint32 localHeaderOff = 0;
    };
    void parse();
    const QByteArray &m_data;
    QHash<QString, Entry> m_entries;
    bool m_ok = false;
};

void Zip::parse()
{
    const QByteArray &d = m_data;
    const int n = d.size();
    // Find End Of Central Directory (0x06054b50). Scan back over the (optional)
    // comment; the fixed EOCD record is 22 bytes.
    int eocd = -1;
    for (int i = n - 22; i >= 0 && i >= n - 22 - 65535; --i) {
        if (rd32(d, i) == 0x06054b50u) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) {
        return;
    }
    const quint16 count = rd16(d, eocd + 10);
    quint32 cdOff = rd32(d, eocd + 16);
    int p = static_cast<int>(cdOff);
    for (int e = 0; e < count; ++e) {
        if (p < 0 || p + 46 > n || rd32(d, p) != 0x02014b50u) {
            return;
        }
        Entry en;
        en.method = rd16(d, p + 10);
        en.compSize = rd32(d, p + 20);
        en.uncompSize = rd32(d, p + 24);
        const quint16 nameLen = rd16(d, p + 28);
        const quint16 extraLen = rd16(d, p + 30);
        const quint16 commentLen = rd16(d, p + 32);
        en.localHeaderOff = rd32(d, p + 42);
        if (p + 46 + nameLen > n) {
            return;
        }
        const QString name = QString::fromUtf8(d.constData() + p + 46, nameLen);
        m_entries.insert(name, en);
        p += 46 + nameLen + extraLen + commentLen;
    }
    m_ok = true;
}

QByteArray Zip::extract(const QString &name) const
{
    const Entry en = m_entries.value(name);
    const QByteArray &d = m_data;
    const int lh = static_cast<int>(en.localHeaderOff);
    if (lh < 0 || lh + 30 > d.size() || rd32(d, lh) != 0x04034b50u) {
        return {};
    }
    const quint16 nameLen = rd16(d, lh + 26);
    const quint16 extraLen = rd16(d, lh + 28);
    const int dataOff = lh + 30 + nameLen + extraLen;
    if (dataOff < 0 || dataOff + static_cast<int>(en.compSize) > d.size()) {
        return {};
    }
    const QByteArray comp = d.mid(dataOff, static_cast<int>(en.compSize));
    if (en.method == 0) { // stored
        return comp;
    }
    if (en.method == 8) { // deflate
        QByteArray out;
        if (inflateRaw(comp, en.uncompSize, out)) {
            return out;
        }
    }
    return {};
}

// Parse <sst> shared strings into a list. Each <si> may contain a plain <t> or
// several <r><t> runs (rich text) which we concatenate.
QStringList parseSharedStrings(const QByteArray &xml)
{
    QStringList out;
    QXmlStreamReader r(xml);
    QString cur;
    bool inSi = false;
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            if (r.name() == QLatin1String("si")) {
                inSi = true;
                cur.clear();
            } else if (inSi && r.name() == QLatin1String("t")) {
                cur += r.readElementText();
            }
        } else if (r.isEndElement() && r.name() == QLatin1String("si")) {
            out << cur;
            inSi = false;
        }
    }
    return out;
}

int colIndexFromRef(const QString &ref)
{
    int col = 0;
    for (const QChar c : ref) {
        if (c.isLetter()) {
            col = col * 26 + (c.toUpper().unicode() - 'A' + 1);
        } else {
            break;
        }
    }
    return col - 1; // 0-based
}

} // namespace

Result read(const QString &path)
{
    Result res;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        res.error = QStringLiteral("Could not open file: %1").arg(path);
        return res;
    }
    if (f.size() > kMaxArchive) {
        res.error = QStringLiteral("File is too large to import.");
        return res;
    }
    const QByteArray data = f.readAll();

    Zip zip(data);
    if (!zip.ok()) {
        res.error = QStringLiteral("Not a valid .xlsx workbook (corrupt or unsupported ZIP).");
        return res;
    }
    // The first worksheet is conventionally xl/worksheets/sheet1.xml.
    const QString sheetPart = QStringLiteral("xl/worksheets/sheet1.xml");
    if (!zip.has(sheetPart)) {
        res.error = QStringLiteral("Workbook has no readable first worksheet.");
        return res;
    }
    QStringList shared;
    if (zip.has(QStringLiteral("xl/sharedStrings.xml"))) {
        shared = parseSharedStrings(zip.extract(QStringLiteral("xl/sharedStrings.xml")));
    }
    const QByteArray sheet = zip.extract(sheetPart);
    if (sheet.isEmpty()) {
        res.error = QStringLiteral("Could not read the first worksheet.");
        return res;
    }

    QXmlStreamReader r(sheet);
    QStringList curRow;
    int nextCol = 0;
    QString cellType;
    QString cellRef;
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            if (r.name() == QLatin1String("row")) {
                curRow.clear();
                nextCol = 0;
            } else if (r.name() == QLatin1String("c")) {
                cellType = r.attributes().value(QLatin1String("t")).toString();
                cellRef = r.attributes().value(QLatin1String("r")).toString();
            } else if (r.name() == QLatin1String("v")) {
                const QString raw = r.readElementText();
                QString value = raw;
                if (cellType == QLatin1String("s")) { // shared-string index
                    bool ok = false;
                    const int i = raw.toInt(&ok);
                    value = (ok && i >= 0 && i < shared.size()) ? shared.at(i) : QString();
                }
                const int col = cellRef.isEmpty() ? nextCol : colIndexFromRef(cellRef);
                if (col >= 0 && col < kMaxCols) {
                    while (curRow.size() < col) {
                        curRow << QString();
                    }
                    if (curRow.size() == col) {
                        curRow << value.trimmed();
                    } else {
                        curRow[col] = value.trimmed();
                    }
                    nextCol = col + 1;
                }
            } else if (r.name() == QLatin1String("t") && cellType == QLatin1String("inlineStr")) {
                const QString value = r.readElementText().trimmed();
                const int col = cellRef.isEmpty() ? nextCol : colIndexFromRef(cellRef);
                if (col >= 0 && col < kMaxCols) {
                    while (curRow.size() < col) {
                        curRow << QString();
                    }
                    if (curRow.size() == col) {
                        curRow << value;
                    } else {
                        curRow[col] = value;
                    }
                    nextCol = col + 1;
                }
            }
        } else if (r.isEndElement() && r.name() == QLatin1String("row")) {
            res.rows << curRow;
        }
    }
    if (r.hasError()) {
        res.error = QStringLiteral("Malformed worksheet XML: %1").arg(r.errorString());
        return res;
    }
    res.ok = true;
    return res;
}

} // namespace XlsxReader
