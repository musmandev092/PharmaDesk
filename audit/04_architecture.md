# 04 — Architecture Audit

## Summary — Grade **B**

A textbook 4-layer desktop architecture (UI → service → data, with a pure-ish domain leaf)
implemented with idiomatic Qt Widgets. The bones are good. The defects are concentrated:
one reverse layer edge, a too-thin service layer that lets UI reach the DB directly, and
business logic that occasionally lives in widgets.

## Strengths

- **Real domain layer.** `Money`, `SaleCalculator`, `Fefo`, `CostBlender`, `PinPolicy`,
  `BarcodeParser`, and the policy classes are pure, side-effect-free, and unit-tested in
  isolation.
- **Transactional data layer.** Every mutating path (sale, return, void, adjust, GRN,
  reconcile, catalog import) opens `QSqlDatabase::transaction()`, commits once, and rolls
  back on any error or exception. Verified at all entry points (see 06).
- **Single composition root.** `main.cpp` builds the DB, runs `bootstrap()`+`migrate()`,
  authenticates, and constructs `MainWindow`. Dependencies flow in via constructors
  (`QSqlDatabase`, `UserRecord`, repos) — DI by hand, no globals for state.
- **Static core lib** shared by app + tests means tests exercise the *same* code the app
  runs (no test-only reimplementation).

## Defects

| ID | Finding | Severity |
|---|---|---|
| A1 | `data/ → service/` reverse edge (`EscPosRenderer.h`, `SaleRepository.h` include `SaleService.h` for DTOs). | High |
| A2 | Service layer is "thin": ~33 UI widgets hold a `QSqlDatabase` and run `QSqlQuery` directly for reads/reports, bypassing any service. Couples UI to schema. | Medium |
| A3 | Business logic in UI: `PosTerminalPage::completeSale` (123 lines) and parts of `ReturnsPage` orchestrate domain decisions a service should own. | Medium |
| A4 | `domain/DesktopIntegration` reaches into `data/SettingsRepository` — infra mislabeled as domain (L2). | Medium |
| A5 | No explicit application/use-case layer; "service" classes are a mix of true use-cases (`SaleService`) and infra wrappers (`CupsRawPrinter`, `BackupService`). | Low |
| A6 | Authorization is enforced almost entirely at the **UI** layer (menu visibility / PIN dialogs), not at the service boundary — so any code path reaching a repo is unguarded. This is the keystone problem (see 05, Phase 4). | High |

## Target architecture (incremental, not a rewrite)

```
ui/         widgets — presentation + input only; call services for ALL writes
service/    use-cases — own authorization + transaction + audit (the write boundary)
data/       repositories — SQL only, no upward deps; shared DTOs in a neutral header
domain/     pure logic — leaf, no Qt SQL
```

Moves (all behind the Phase-3 tests, public headers kept stable):
1. Extract shared sale DTOs → neutral header; delete the `data→service` edge (A1).
2. Introduce a thin **authorization + audit** seam every privileged write must pass
   through (A6 / keystone, Phase 4).
3. Move `DesktopIntegration` to `service/` (A4).
4. Leave read/report queries in widgets short-term, but freeze the boundary with an
   include linter so it can't get worse, then migrate hot reports to repositories.

## ADRs to record (Phase 7)

- ADR-0001 Qt Widgets over QML. ADR-0002 SQLite + fixed-point decimal money.
- ADR-0003 Service-owned authorized+audited write boundary. ADR-0004 Append-only,
  HMAC-chained audit log. ADR-0005 `user_version` forward-only migrations.
