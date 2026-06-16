# 01 — Dependency / Include Graph

_Phase 0. Include-edge analysis across `src/`._

## Intended layer direction

```
ui  →  service  →  domain
 │         │         ▲
 └────► data ────────┘        domain is a leaf (depends on nothing internal)
```

Rules that *should* hold:
- `domain/` depends on **nothing** internal (pure logic, leaf).
- `data/` depends on `domain/` only.
- `service/` depends on `data/` + `domain/`.
- `ui/` depends on `service/` + `data/` + `domain/` (it is the top).

## Measured violations (reverse / skip edges)

| # | Edge | Files | Severity | Why it's wrong |
|---|---|---|---|---|
| L1 | `data/ → service/` | `data/EscPosRenderer.h:3`, `data/SaleRepository.h:3` both `#include "service/SaleService.h"` | **High** | Data layer depends *up* on service. Creates a `data ↔ service` knot: `SaleService` → `SaleRepository` → `SaleService.h`. The headers reuse `SaleResult`/`SaleResultLine` structs declared in the service header. |
| L2 | `domain/ → data/` | `domain/DesktopIntegration.cpp:4` `#include "data/SettingsRepository.h"` | **Medium** | Domain is supposed to be a leaf. `DesktopIntegration` is really infra glue mislabeled as domain. |
| L3 | `ui/ → data/` (skip service) | ~33 UI files include `data/*` directly | **Medium** | UI talks to repositories directly, bypassing a service layer. Pervasive but consistent with the app's "thin service" design; the real risk is the **write** paths (see 05/keystone), not the read paths. |
| L4 | UI holds `QSqlDatabase` + raw `QSqlQuery` | ~33 UI files reference `QSqlQuery`/`QSqlDatabase` | **Medium** | Widgets construct queries inline for reads/reports. No injection (all parameterized), but it couples UI to SQL and blocks a clean DB swap. |

No `service/ → ui/` edges (good). No include **cycles at the file level** other than the
`data ↔ service` struct-sharing knot (L1), which is a header coupling, not a compile cycle
(it resolves because the structs live in `SaleService.h`).

## Hub headers (most-included internal headers)

- `data/Database.h` / the repository headers — included by services and UI.
- `domain/Money.h` — the single most depended-on domain header (every money/cost path).
- `service/SaleService.h` — leaked downward into `data/` (L1).

## Fix direction (Phase 6)

1. **L1:** extract the shared sale DTOs (`SaleResult`, `SaleResultLine`, `SaleInput`) into a
   neutral header — `domain/SaleTypes.h` or `data/SaleDtos.h` — that both `service/` and
   `data/` can include downward. Removes the reverse edge without touching call sites.
2. **L2:** move `DesktopIntegration` to `service/` (it is infra), or inject the settings
   value instead of having domain reach into a repository.
3. **L3/L4:** acceptable for read/report paths short-term; enforce a **header-include linter**
   (Phase 6) that *blocks new* `data/→service/` and `domain/→{data,service,ui}` edges
   (ratchet), and route all **writes** through services (keystone, Phase 4).
