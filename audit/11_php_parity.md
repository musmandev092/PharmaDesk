# 11 — PHP→C++ Port Parity

The original PHP app was cloned (`/home/mosman092/Projects/CPHC_Pharmacy`, the repo the
user provided) and the trust-core modules were compared **line-by-line** against the C++
port. This closes the gap the earlier audit flagged ("the PHP spec is absent"). Every claim
below was verified against the real PHP on both sides.

## Fixed to match the PHP spec (4 confirmed bugs)

| # | Divergence | PHP (spec) | C++ (was) | Fix | Sev |
|---|---|---|---|---|---|
| P1 | **CostBlender invalid input fails open** | `CostBlender.php:27-29,42` **throw** `InvalidArgumentException` on `paidQty<0 / focQty<0 / upp<1` | returned `"0.0000"` → persisted a bogus zero cost, GRN proceeds | C++ now **throws** `std::invalid_argument`; `GrnService::post` already catches → the post rolls back. Legit "nothing received" (`totalBaseUnits==0`) still returns `"0.0000"`. | High |
| P2 | **Narcotic audit verb** | `Sale.php:339` writes `NARCOTIC_DISPENSED` | wrote `CONTROLLED_DISPENSED` → DRAP compliance reports keyed on the spec verb returned nothing | C++ now emits `NARCOTIC_DISPENSED`. | High |
| P3 | **Express restock skips QA guard** | PHP has only initiate→adjudicate; `adjudicate` refuses restock into a quarantined/expired batch | C++ `commitReturn(restock=true)` re-added stock with **no** quarantine/expired check | C++ `commitReturn` now applies the same guard as `adjudicate` (refuses + rolls back). | High |
| P4 | **FEFO date clock split** | PHP filters `expiry_date > CURRENT_DATE` in a single local (Asia/Karachi) clock | `SaleService` SQL used `date('now')` (**UTC**) while the in-memory `Fefo` re-filter used `QDate::currentDate()` (**local**) → off-by-one-day on the expiry boundary in a ~5h daily window | SQL now uses `date('now','localtime')` → both layers use one local clock. | Med |

Each fix is pinned by a test (`costblender` throw cases; `returns` express-restock-quarantine
guard; `returns` NARCOTIC_DISPENSED verb). Full suite green (3131/3131).

## Verified divergences kept as-is (C++ is more correct / additive) — documented

| Divergence | Why kept |
|---|---|
| `RETURN_QUARANTINE` ledger `qty_delta`: PHP writes `baseQtyToReturn` with `qty_before==qty_after`; C++ writes `0`. | PHP's row is **internally inconsistent** (non-zero delta but unchanged before/after) and would **violate the v6 `inv_mv_consistent` trigger**. C++ (`0`) is the physically correct, ledger-consistent value. Intentional correctness improvement over the spec. |
| `has_controlled_drug` stored value: PHP trusts the client flag; C++ derives it server-side from the cart's schedules. | C++ is **fail-closed / more secure**. If strict parity were required, PHP is the side that should change. |
| Guarded stock decrement (`UPDATE … WHERE current_qty >= ?` + rows-affected check) vs PHP `SELECT … FOR UPDATE`. | Correct adaptation: SQLite has no row locks; the guard prevents oversell equivalently. |
| `voidSale` / `SALE_VOIDED`, `RETURN_PROCESSED`, `SALE_STATUS_RECOMPUTED` audit verbs. | C++-only capabilities (PHP has no void/express/standalone-recompute). `voidSale` is manager/admin-gated (keystone). Added surface, not regressions. |
| `Money` overflow throws (`std::overflow_error`) vs PHP bcmath arbitrary precision. | Deliberate int64 safety; same result for all in-range pharmacy values, throws (rolls back) rather than wrapping. |
| Barcode: C++ adds GS1 Digital Link URL decoding + a stricter GS1 AI state-machine (FNC1-aware) + month/date validation. | Additive / more robust; PHP rejected URLs and used looser regexes. |

## Prior-audit claims REFUTED by the real PHP

- ❌ "PHP validates the GTIN mod-10 check digit" — **neither** PHP nor C++ validates it (shared
  latent gap, not a parity divergence). Tracked as a quality note, not a port bug.
- ❌ "PHP `ltrim`s all leading zeros in `gtin13`" — PHP does **no** zero-stripping and has **no
  `gtin13` form at all**; the canonical GTIN-14 + `gtin13From` logic is C++-only (arguably more
  correct).
- ❌ "PHP handles HIBC / IFA PPN / China eCode / ISO 15434 that C++ drops" — **neither** side
  parses any of these.

## Open / flagged (not yet actioned — owner decision)

- **Free-text printed-label parsing** (`QrParser.php parseFreeText`: "Batch No: X  Exp: MM/YYYY")
  is **present in PHP, absent in C++**. If the POS workflow scans printed cartons, this is a real
  feature gap to port; otherwise it's an intentional scope cut. Flagged for your call.
- **>4-dp input rounding timing**: C++ rounds money inputs to 4 dp at parse (its int64-scale-4
  representation) while PHP defers rounding to the end. Identical results for inputs honoring the
  `DECIMAL(12,4)` ≤4-dp contract (all persisted values); only transient >4-dp strings differ.
  Documented as a bounded, intentional deviation (`Money.h` already notes it).
- **`Money::divByInt(0)`** returns 0 in C++ vs PHP `DivisionByZeroError`; unreachable through any
  current service (all call sites guard the divisor). Low; optional defensive alignment.
