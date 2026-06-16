#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// Infrastructure: a small, self-contained reader for the first worksheet of an
// Excel .xlsx workbook, returning one QStringList per row (mirroring the CSV
// parser in CatalogImporter) so the two import paths share the same row→draft
// logic.
//
// An .xlsx file is a ZIP of XML parts. This reader uses only zlib (raw DEFLATE)
// and QXmlStreamReader — no third-party spreadsheet library. It is intentionally
// minimal: it reads shared strings + the first sheet, resolves inline and shared
// strings, and is hardened against malformed/zip-bomb input with size caps. It is
// meant for an admin importing a catalog file they chose locally, not untrusted
// network input.
namespace XlsxReader {

struct Result
{
    bool ok = false;
    QString error;
    QVector<QStringList> rows; // row 0 is the header, as in the sheet
};

// Read <path>'s first worksheet into rows. Cells are returned as trimmed text in
// column order; empty trailing cells in a row may be omitted by Excel, so callers
// should map by header name (CatalogImporter does).
Result read(const QString &path);

} // namespace XlsxReader
