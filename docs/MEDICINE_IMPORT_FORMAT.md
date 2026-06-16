# Medicine Import Format

PharmaDesk imports your medicine catalog from an **Excel (`.xlsx`)** or **CSV** file via
**Medicines → Import…**. Nothing is auto-seeded; you bring your own catalog.

A ready-to-fill template lives at
**`resources/templates/medicine_import_template.xlsx`** (and a `.csv` twin). It has two
sheets: **Medicines** (fill this in) and **Reference** (allowed values).

## How import behaves

- **Idempotent / safe to re-run.** A row whose **SKU already exists** is *skipped*, not
  duplicated. Re-importing a corrected file only adds the new rows.
- **All-or-nothing per file.** The whole import runs in one database transaction; if it
  can't commit, nothing is written.
- **Excel is read natively** (first worksheet). You can also save your sheet as **CSV** and
  import that — both go through the same logic.
- After import you get a summary: **Imported / Skipped / Failed**.

## Columns

Header names are **case-insensitive** and spaces are treated as underscores, so
`Brand Name`, `brand_name`, and `BRAND NAME` are all accepted. Column **order does not
matter**.

| Column | Required | Notes |
|---|---|---|
| `sku` | ✅ | Your unique stock code. Duplicates (already in the catalog) are skipped. |
| `brand_name` | ✅ | Trade/brand name. |
| `generic_name` | ✅ | Active ingredient(s). |
| `form` | ✅ | Dosage form — must be one of the Reference values (e.g. `TABLET`, `SYRUP`). |
| `purchase_unit` | ✅ | Unit you buy in (e.g. `BOX`). |
| `base_unit` | ✅ | Unit you sell in (e.g. `TABLET`, `BOTTLE`). |
| `units_per_purchase` | ✅ | Whole number ≥ 1 — base units per purchase unit. |
| `strength` | — | e.g. `500mg`, `125ml`. |
| `manufacturer` | — | Maker/supplier. |
| `primary_barcode` | — | EAN-13/GTIN; blank if none. |
| `prescription_required` | — | `Yes`/`No` (also accepts `t`/`f`, `1`/`0`). Default No. |
| `controlled_schedule` | — | `NONE` / `SCHEDULE_G` / `SCHEDULE_H` / `NARCOTIC`. Default `NONE`. |
| `tax_code_value` | — | `EXEMPT` / `STANDARD_18` / `REDUCED` / `ZERO_RATED`. Default `EXEMPT`. |
| `reorder_level` | — | Reorder when stock falls to this. |
| `reorder_quantity` | — | Suggested reorder amount. |
| `reorder_unit` | — | `PURCHASE` or `BASE`. Default `PURCHASE`. |

Legacy Postgres-export columns (`id`, `created_at`, `updated_at`, `deleted_at`,
`form_custom`, `is_active`, `therapeutic_category`) are also recognized for backward
compatibility — a row with `deleted_at` set is skipped — so the original
`medicines_export.csv` still imports unchanged.

## Allowed values (Reference sheet)

- **form:** `TABLET, CAPSULE, SYRUP, SUSPENSION, INJECTION, INJECTION_VIAL,
  INJECTION_AMPOULE, IV_FLUID, INFUSION, DROPS, EYE_DROPS, EAR_DROPS, NASAL_DROPS,
  NASAL_SPRAY, INHALER, INHALER_MDI, INHALER_DPI, NEBULIZER_SOLUTION, CREAM, OINTMENT, GEL,
  LOTION, ORAL_SOLUTION, DRY_SYRUP, ELIXIR, EMULSION, SACHET, POWDER, SUPPOSITORY, PESSARY,
  ENEMA, LOZENGE, SUBLINGUAL_TABLET, CHEWABLE_TABLET, DISPERSIBLE_TABLET,
  EFFERVESCENT_TABLET, SOFT_GEL_CAPSULE, PATCH, TRANSDERMAL_PATCH, SPRAY, SHAMPOO,
  MOUTHWASH, DEVICE, TEST_STRIPS, OTHER` (full enum in `src/domain/MedicineForm.cpp`).
- **controlled_schedule:** `NONE, SCHEDULE_G, SCHEDULE_H, NARCOTIC`.
- **tax_code_value:** `EXEMPT, STANDARD_18, REDUCED, ZERO_RATED`.

## Regenerating the templates

The templates are generated (no external libraries) by:

```
python3 tools/gen_import_templates.py
```

Edit `COLUMNS`/`FORMS` in that script and re-run to change the format; keep the column
names in sync with `src/data/CatalogImporter.cpp`.

## Format variants considered

The shipped template is the **"standard + reference sheet"** variant. Others weighed:

1. **Raw 23-column Postgres dump** — rejected: too technical for pharmacy staff.
2. **Minimal (7 required columns only)** — rejected: omits price-relevant fields people
   expect (schedule, tax, reorder).
3. **Standard + Reference sheet (chosen)** — friendly headers, examples, and an allowed-
   values sheet; the best balance of guidance vs. simplicity.
4. **Two-row header (group + field)** — rejected: harder to parse and to fill.
5. **Per-supplier presets** — deferred: can be layered on later as saved column mappings.

## Implementation notes

- `.xlsx` reading is a small, self-contained reader (`src/data/XlsxReader.{h,cpp}`) built on
  **zlib** (raw DEFLATE) + `QXmlStreamReader`. It reads the first worksheet, resolves shared
  and inline strings, and is size-capped against malformed/zip-bomb input. No third-party
  spreadsheet library is required.
- End-to-end tests (CSV friendly-headers + a real shared-strings `.xlsx` fixture) live in
  `tests/cases/catalog_tests.cpp`.
