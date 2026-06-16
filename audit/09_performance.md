# 09 — Performance Audit

## Summary — Grade **A−**

For a single-PC pharmacy POS (one cashier, a few-hundred-row catalog, thousands of sales/
year) the performance posture is comfortably adequate. Hot paths are indexed; the only real
items are missing FK indexes (also a data finding) and leading-wildcard catalog search.

## Findings

| ID | Finding | Severity | Notes |
|---|---|---|---|
| P1 | Missing FK / hot-join indexes — esp. `returns.sale_item_id` (per-return `SUM` scan). | Medium | Additive index migration (see 06 §2). Biggest real win. |
| P2 | Catalog search uses leading-wildcard `LIKE '%term%'` on brand/generic/sku/barcode. | Low | Can't use a B-tree; mitigated by `LIMIT 300`/`LIMIT 50`. Consider FTS5 only if the catalog grows large. |
| P3 | On-hand rollups filter `current_qty>0 AND not quarantined AND not expired` repeatedly. | Low | A partial index `ON batches(medicine_id) WHERE current_qty>0 AND is_quarantined=0 AND is_expired=0` would speed inventory/low-stock/expiring views. |
| P4 | `synchronous=FULL` + WAL on every write. | Info | Correct trade for a cash till (durability over throughput). Do **not** lower it. |
| P5 | Report widgets run several aggregate queries on tab open. | Low | Fine at current data volumes; revisit only if reports lag. |

## Non-issues (verified)

- Money math is integer-scaled (no float, no `bcmath`-style string overhead at runtime).
- FEFO allocation is index-served (`batches(medicine_id, expiry_date)`).
- Sales reporting indexed on `sold_at`, `(cashier_id, sold_at)`, `status`.
- No N+1 query loops found in the hot sale/return paths (batched within one transaction).

## Recommendation

Ship the FK-index migration (P1) with Phase 5; treat P2/P3 as "only if measured." No code
restructuring needed for performance.
