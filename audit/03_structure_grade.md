# 03 — Structure Grade

_Phase 0 verdict on code structure only (architecture/security/etc. graded in Phase 1)._

## Grade: **B−**

A clean, deliberately-layered codebase with real domain separation and a disciplined data
layer, held back from a B+/A by a handful of god-files, two long trust-core functions, and
a few reverse/skip layer edges.

### Rubric

| Dimension | Grade | Evidence |
|---|---|---|
| Layer separation | B | 4 layers, domain nearly a pure leaf. Violations L1 (`data→service`, High), L2 (`domain→data`), pervasive UI→data reads. |
| File cohesion | C+ | 5 god-files (ReturnService 893, AdminPage 845, ReturnsPage 821, AnalyticsRepository 644, SaleService 450). |
| Function length | C+ | `SaleService::commit` 397 lines; 12 functions >100 lines. |
| Cyclic dependencies | B+ | No compile cycles; one header-coupling knot (L1). |
| Duplication | B | Localized, mechanical (doc-numbers, ledger writes, report boilerplate). |
| Dead code | A− | Minimal; only redundant barcode target + unexercised CUPS code. |
| Naming / idiom | A− | Consistent, idiomatic Qt/C++; clear names; good header comments. |

### Why not higher

- The two **highest-stakes functions** (`SaleService::commit`, `ReturnService::*`) are also
  the longest — risk concentrates exactly where decomposition is hardest. They must get
  characterization tests (Phase 3) *before* being split (Phase 6).
- `data/→service/` reverse edge (L1) is a genuine architectural defect, not just length.

### Why not lower

- Genuine domain layer with pure logic and its own tests.
- Transactional, ledgered data layer with trigger-protected audit table.
- Real-object test suite already covering ~19 modules — decomposition can be done safely
  behind it.

### Path to A (tracked in FINAL_REPORT roadmap)

1. Break L1 by extracting shared sale DTOs to a neutral header (Phase 6).
2. Decompose `SaleService::commit` and `ReturnService` into named, tested steps behind the
   Phase-3 characterization tests.
3. Split `AdminPage`/`ReturnsPage` tab-builders into separate TUs (mechanical, low risk).
4. Add a header-include linter that ratchets layer direction so structure can't regress.
