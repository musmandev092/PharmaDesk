#!/usr/bin/env python3
"""Generate the medicine-import templates (Excel .xlsx + CSV) for PharmaDesk.

Dependency-free: an .xlsx file is just a ZIP of XML parts, so we build it with the
Python standard library (zipfile + minimal OOXML). No openpyxl/pandas required.

Run from the repo root:  python3 tools/gen_import_templates.py
Outputs:
  resources/templates/medicine_import_template.xlsx   (Medicines + Reference sheets)
  resources/templates/medicine_import_template.csv    (same columns, header + examples)

The column NAMES below must match what data/CatalogImporter.cpp reads (it maps by
header name, order-independent). Keep the two in sync.
"""
import csv
import io
import os
import zipfile

# ── The import format (single source of truth for both outputs) ──────────────
# (header, required?, help text, example value for each of the 3 sample rows)
COLUMNS = [
    ("sku",                  True,  "Unique stock-keeping code you assign (no duplicates).",
        ["ACE1125-SYR", "ACVL400-TAB", "MORPH10-TAB"]),
    ("brand_name",           True,  "Trade/brand name printed on the pack.",
        ["Acefyl-125", "Acvlex", "Morfine"]),
    ("generic_name",         True,  "Active ingredient(s).",
        ["Acefylline Piperazine", "Aciclovir", "Morphine Sulphate"]),
    ("strength",             False, "e.g. 500mg, 125ml, 10mg/5ml.",
        ["125ml", "400mg", "10mg"]),
    ("form",                 True,  "Dosage form — see the Reference sheet for valid values.",
        ["SYRUP", "TABLET", "TABLET"]),
    ("manufacturer",         False, "Maker / supplier company.",
        ["Nabiqasim Industries", "Ferozsons Laboratories", "Searle Pakistan"]),
    ("primary_barcode",      False, "Main scan barcode (EAN-13/GTIN). Leave blank if none.",
        ["08964002675821", "08961100510047", ""]),
    ("purchase_unit",        True,  "Unit you BUY in (e.g. BOX, CARTON, PACK).",
        ["BOX", "BOX", "BOX"]),
    ("base_unit",            True,  "Unit you SELL in (e.g. TABLET, BOTTLE, STRIP).",
        ["BOTTLE", "TABLET", "TABLET"]),
    ("units_per_purchase",   True,  "How many base units in one purchase unit (whole number >= 1).",
        ["1", "20", "10"]),
    ("prescription_required",False, "Yes or No.",
        ["No", "Yes", "Yes"]),
    ("controlled_schedule",  False, "NONE / SCHEDULE_G / SCHEDULE_H / NARCOTIC (see Reference).",
        ["NONE", "NONE", "NARCOTIC"]),
    ("tax_code_value",       False, "EXEMPT / STANDARD_18 / REDUCED / ZERO_RATED (see Reference).",
        ["EXEMPT", "EXEMPT", "EXEMPT"]),
    ("reorder_level",        False, "Reorder when stock falls to this (whole number).",
        ["3", "40", "5"]),
    ("reorder_quantity",     False, "Suggested quantity to reorder.",
        ["0", "0", "0"]),
    ("reorder_unit",         False, "PURCHASE or BASE (default PURCHASE).",
        ["PURCHASE", "PURCHASE", "PURCHASE"]),
]

FORMS = [
    "TABLET", "CAPSULE", "SYRUP", "SUSPENSION", "INJECTION", "INJECTION_VIAL",
    "INJECTION_AMPOULE", "IV_FLUID", "INFUSION", "DROPS", "EYE_DROPS", "EAR_DROPS",
    "NASAL_DROPS", "NASAL_SPRAY", "INHALER", "INHALER_MDI", "INHALER_DPI",
    "NEBULIZER_SOLUTION", "CREAM", "OINTMENT", "GEL", "LOTION", "SOLUTION",
    "ORAL_SOLUTION", "DRY_SYRUP", "ELIXIR", "EMULSION", "SACHET", "POWDER",
    "SUPPOSITORY", "PESSARY", "ENEMA", "LOZENGE", "SUBLINGUAL_TABLET",
    "CHEWABLE_TABLET", "DISPERSIBLE_TABLET", "EFFERVESCENT_TABLET",
    "SOFT_GEL_CAPSULE", "PATCH", "TRANSDERMAL_PATCH", "SPRAY", "SHAMPOO",
    "MOUTHWASH", "DEVICE", "TEST_STRIPS", "OTHER",
]
SCHEDULES = ["NONE", "SCHEDULE_G", "SCHEDULE_H", "NARCOTIC"]
TAX_CODES = ["EXEMPT", "STANDARD_18", "REDUCED", "ZERO_RATED"]

OUT_DIR = os.path.join("resources", "templates")
HEADERS = [c[0] for c in COLUMNS]
EXAMPLES = [[c[3][i] for c in COLUMNS] for i in range(3)]


# ── Minimal OOXML (.xlsx) writer ─────────────────────────────────────────────
def _esc(s):
    return (str(s).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
            .replace('"', "&quot;"))


def _col_ref(c0):
    s, n = "", c0 + 1
    while n:
        n, r = divmod(n - 1, 26)
        s = chr(65 + r) + s
    return s


def _sheet_xml(rows, bold_first_row=True):
    out = ['<?xml version="1.0" encoding="UTF-8" standalone="yes"?>',
           '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">',
           '<sheetViews><sheetView workbookViewId="0">',
           '<pane ySplit="1" topLeftCell="A2" activePane="bottomLeft" state="frozen"/>',
           '</sheetView></sheetViews>', '<sheetData>']
    for r, row in enumerate(rows):
        out.append(f'<row r="{r + 1}">')
        for c, val in enumerate(row):
            if val == "" or val is None:
                continue
            ref = f"{_col_ref(c)}{r + 1}"
            style = ' s="1"' if (bold_first_row and r == 0) else ""
            out.append(f'<c r="{ref}"{style} t="inlineStr"><is><t xml:space="preserve">'
                       f'{_esc(val)}</t></is></c>')
        out.append('</row>')
    out.append('</sheetData></worksheet>')
    return "".join(out)


def write_xlsx(path, sheets):
    """sheets: list of (name, rows)."""
    styles = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
              '<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'
              '<fonts count="2"><font><sz val="11"/><name val="Calibri"/></font>'
              '<font><b/><sz val="11"/><name val="Calibri"/></font></fonts>'
              '<fills count="1"><fill><patternFill patternType="none"/></fill></fills>'
              '<borders count="1"><border/></borders>'
              '<cellStyleXfs count="1"><xf/></cellStyleXfs>'
              '<cellXfs count="2"><xf/><xf fontId="1" applyFont="1"/></cellXfs>'
              '</styleSheet>')
    content_types = ['<?xml version="1.0" encoding="UTF-8" standalone="yes"?>',
                     '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">',
                     '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>',
                     '<Default Extension="xml" ContentType="application/xml"/>',
                     '<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>',
                     '<Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>']
    for i in range(len(sheets)):
        content_types.append(f'<Override PartName="/xl/worksheets/sheet{i + 1}.xml" '
                             'ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>')
    content_types.append('</Types>')
    rels = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
            '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>'
            '</Relationships>')
    wb_sheets, wb_rels = [], []
    for i, (name, _rows) in enumerate(sheets):
        wb_sheets.append(f'<sheet name="{_esc(name)}" sheetId="{i + 1}" r:id="rId{i + 1}"/>')
        wb_rels.append(f'<Relationship Id="rId{i + 1}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet{i + 1}.xml"/>')
    workbook = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
                '<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" '
                'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">'
                '<sheets>' + "".join(wb_sheets) + '</sheets></workbook>')
    workbook_rels = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
                     '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
                     + "".join(wb_rels) +
                     '<Relationship Id="rIdStyles" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>'
                     '</Relationships>')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", "".join(content_types))
        z.writestr("_rels/.rels", rels)
        z.writestr("xl/workbook.xml", workbook)
        z.writestr("xl/_rels/workbook.xml.rels", workbook_rels)
        z.writestr("xl/styles.xml", styles)
        for i, (_name, rows) in enumerate(sheets):
            z.writestr(f"xl/worksheets/sheet{i + 1}.xml", _sheet_xml(rows))


def build():
    medicines = [HEADERS] + EXAMPLES
    reference = [["Column", "Required?", "What to enter"]]
    for name, req, help_text, _ex in COLUMNS:
        reference.append([name, "REQUIRED" if req else "optional", help_text])
    reference += [
        [], ["Valid 'form' values:"], *[[f] for f in FORMS],
        [], ["Valid 'controlled_schedule' values:"], *[[s] for s in SCHEDULES],
        [], ["Valid 'tax_code_value' values:"], *[[t] for t in TAX_CODES],
        [], ["'prescription_required':", "Yes or No"],
        [], ["'reorder_unit':", "PURCHASE or BASE"],
    ]
    xlsx = os.path.join(OUT_DIR, "medicine_import_template.xlsx")
    write_xlsx(xlsx, [("Medicines", medicines), ("Reference", reference)])

    buf = io.StringIO()
    w = csv.writer(buf)
    w.writerow(HEADERS)
    for row in EXAMPLES:
        w.writerow(row)
    csv_path = os.path.join(OUT_DIR, "medicine_import_template.csv")
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        f.write(buf.getvalue())
    print("wrote", xlsx)
    print("wrote", csv_path)


if __name__ == "__main__":
    build()
