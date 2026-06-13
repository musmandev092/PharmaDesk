# PharmaDesk (PMS)

Native Linux desktop port of the CPHC Pharmacy POS web app (`../pos`, plain
PHP 8.4 + Postgres 17). Target: single AlmaLinux 10 PC. Stack: C++17, Qt 6
Widgets, SQLite (Qt SQL), CMake, packaged as an AppImage.

The PHP app is the specification — see `CLAUDE.md` for the porting rules and
`docs/PHP_APP_MAP.md` for the screen/service/schema map.

## Status: Phase 1 (data layer + first-run setup)
- [x] CMake + Qt Widgets project that builds and runs
- [x] App-wide QSS theme from the PHP design tokens (`resources/theme.qss`)
- [x] PHP structure map (`docs/PHP_APP_MAP.md`)
- [x] SQLite schema port of all 17 tables (`sql/schema_sqlite.sql`)
- [x] Data layer: `Database` (bootstrap, first-run probe), `SettingsRepository`,
      `UserRepository`
- [x] Domain: `Bcrypt` (libxcrypt, PHP `$2y$`-compatible), `PinPolicy`, `AppPaths`
- [x] First-run **setup wizard** (admin user + pharmacy identity + logo + theme)
- [x] `LoginDialog` (bcrypt verify) + boot flow in `main.cpp`
- [x] Headless smoke test (`pharmadesk_smoke`, 90 checks)
- [x] Trigger parity in SQLite: audit_log immutability (UPDATE/DELETE blocked),
      partial-unique medicines (sku/barcode, live rows only)
- [x] Money (fixed-point, HALF_UP, PHP-parity) / SaleCalculator / FEFO / MedicineForm
- [x] Phase 2 core flow: Medicines CRUD + stock-in, POS terminal (search → cart →
      checkout → FEFO stock decrement → receipt preview), atomic `SaleService`
      with inventory-movement ledger + audit
- [ ] Deferred triggers (DRAP narcotic gate, supplier-returns, parked bills,
      concurrency) — for modules not yet built
- [x] Phase 3: Inventory screen — Overview KPIs (stock value at cost/MRP),
      batch list with expiry status badges, low-stock & expiring-soon tabs
- [x] Phase 4: Purchasing — Suppliers CRUD; goods-receipt notes (GRN) with
      `CostBlender` (FOC-diluted blended cost, scale-4), find-or-create batch,
      `GRN_RECEIPT` ledger + audit; GRN list. (The Phase-2 quick "Add stock"
      on the Medicines page remains as a convenience.)
- [x] Phase 5: Reports — date-range sales report (count, net/cash/card,
      refunds, per-sale table), CSV export, and receipt **printing** (QPrinter)
      + **Save-PDF** (QPrinter PdfFormat) with reprint-from-report
- [x] Phase 6: Admin & ops — Users management (admin-gated add/edit/reset PIN),
      Change-my-PIN (full PinPolicy incl. last-5 history), append-only audit-log
      viewer, and automated SQLite backup (WAL-checkpoint + copy to an
      off-machine folder; daily auto-backup on startup + Backup-now)
- [x] Phase 7: AppImage packaging — `packaging/build-appimage.sh` produces a
      single ~35 MB `PharmaDesk-x86_64.AppImage` (Qt + libs bundled, SQLite
      driver only, xcb + offscreen platform plugins). Icon + `.desktop` included.
- [x] UI audit pass (multi-agent): branded header with user box + Sign out +
      nav shortcuts; per-screen titles; numeric columns right-aligned; empty-state
      messages; consistent secondary Cancel buttons; transparent labels (fixed
      gray boxes); friendly schedule/tax labels; NARCOTIC badge; Settings screen
      to edit pharmacy identity/theme/logo after setup; PIN-rotation enforcement
      on login (must_rotate + 90-day) with forced change; login audit
      (LOGIN_SUCCESS/FAILED); last-active-admin protection; admin PIN-reset forces
      rotation; POS Enter-to-add / Ctrl+Enter complete / double-submit guard;
      GRN `batch_id` traceability fix; localtime date handling; supplier-name
      uniqueness. A screenshot harness (`pharmadesk_shots`) renders every screen to PNG.
- [x] Clean-code pass (researched best practices, then applied): centralized UI
      status colours into `UiUtil::Palette`; single-source currency formatting via
      `Money::display()`; split the largest page constructors (Admin/Returns/Sessions)
      into per-tab `buildXTab()` helpers; replaced a NUL-byte SQL sentinel with a
      NULL bind; added `.clang-format` + `.editorconfig` and formatted the whole tree.
      All behaviour-preserving — build is warning-clean (`-Wall -Wextra`) and the
      full suite passes (3060 assertions). See **Code style** below.

### Regenerate UI screenshots
```sh
QT_QPA_PLATFORM=offscreen ./build/pharmadesk_shots screenshots
```

### Run the smoke test
```sh
cmake --build build -j
QT_QPA_PLATFORM=offscreen ./build/pharmadesk_smoke
```

### Run the full test suite (CTest-gated)
```sh
ctest --test-dir build --output-on-failure   # 3000+ assertions; non-zero on any failure
```

### Operations & recovery
On-site backup, restore, durability assumptions, and PC-rebuild steps are in
[`docs/RUNBOOK.md`](docs/RUNBOOK.md). The production hardening plan (audit
findings + prioritized fixes) is in [`docs/HARDENING_PLAN.md`](docs/HARDENING_PLAN.md).

First launch shows the setup wizard; the DB lives at
`~/.local/share/PharmaDesk/PharmaDesk/pharmadesk.sqlite` (delete it to re-run setup).

## Package as an AppImage

**Portable build (recommended):**
```sh
packaging/build-appimage-portable.sh     # needs docker → PharmaDesk-x86_64.AppImage
```
This compiles inside an **Ubuntu 22.04 container (glibc 2.35)** with its distro Qt6,
then runs the native script below, giving the bundle a **glibc 2.35 floor**.

*Why a container?* glibc is forward-compatible only and is the one library an AppImage
can't bundle, so you must build against the **oldest** glibc you want to support — see
the AppImage project's [best practices](https://docs.appimage.org/reference/best-practices.html).
Building natively on Arch (glibc 2.43) instead produces an AppImage that only runs on Arch.

### Minimum requirement to run
A graphical desktop with **glibc ≥ 2.35** (verify with `ldd --version`). The GPU/GL and
core desktop libraries come from the host, so a desktop environment must be installed
(a minimal/server install without a GUI is not enough). Minimum versions per distro:

| Distro family | Minimum version (glibc) | Notes |
|---|---|---|
| RHEL / **AlmaLinux** / Rocky | **10** (2.39) | 9 = 2.34 and 8 = 2.28 are **too old** |
| Ubuntu / Pop!_OS / elementary | **22.04** (2.35) | 20.04 = 2.31 too old |
| Linux Mint | **21** (2.35) | based on Ubuntu 22.04 |
| Debian | **12** "bookworm" (2.36) | 11 = 2.31 too old |
| Fedora | **36** (2.35) | and all newer |
| openSUSE | **Tumbleweed** (rolling) | Leap 15.x = 2.31 too old |
| Arch / Manjaro / EndeavourOS | rolling | always current |

Reaching the older distros (AlmaLinux 8/9, Ubuntu 20.04, Debian 11 — glibc ≤ 2.34) would
require building Qt6 from source on that older base, since they don't package Qt6.

**Native build — matches the host's glibc (use only when building *on* your target):**
```sh
packaging/build-appimage.sh      # → PharmaDesk-x86_64.AppImage (glibc = this machine's)
```
Both download linuxdeploy + the Qt plugin (cached in `packaging/tools/`), install into
an `AppDir`, and bundle Qt + the SQLite driver. Verify the glibc floor of any build with:
```sh
./PharmaDesk-x86_64.AppImage --appimage-extract >/dev/null
find squashfs-root \( -name '*.so*' -o -name pharmadesk \) | xargs objdump -T 2>/dev/null \
  | grep -oE 'GLIBC_[0-9]+\.[0-9]+' | sort -V | tail -1     # = minimum host glibc required
```

## Build prerequisites
Dev machine is Arch (Qt 5.15 present); deployment target is AlmaLinux 10 (Qt 6).
The `CMakeLists.txt` prefers Qt6 and falls back to Qt5 so it builds either way.

```sh
# Arch (dev):       sudo pacman -S cmake qt6-base   # qt6-base covers Widgets/Sql/PrintSupport
# AlmaLinux 10:     sudo dnf install cmake gcc-c++ qt6-qtbase-devel
```

## Build & run
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/pharmadesk
```

## Code style
The whole tree is clang-format'd (config in `.clang-format`, ~Qt style: 4-space
indent, pointer/ref bound to the name, function/class braces on their own line,
control braces attached, ~100-col). Editor whitespace rules are in `.editorconfig`.
Conventions:
- **UI status colours** — use `UiUtil::Palette`; never hardcode hex in `.cpp`.
- **Currency display** — use `Money::display()` (= `"PKR " + fmt()`).
- **Large Qt page constructors** — split per-tab into `buildXTab()` methods; keep
  role/permission gates in the constructor.

Format changed files before committing (clang-format/clang-tidy ship in the Arch
`clang` package):
```sh
clang-format -i <files>     # or enable editor format-on-save / a pre-commit hook
```

## Layout
```
PMS/
├── CLAUDE.md            # porting rules (read first)
├── CMakeLists.txt
├── docs/PHP_APP_MAP.md  # screens, services, schema → port checklist
├── resources/
│   ├── theme.qss        # app-wide style (design tokens from app.css)
│   └── resources.qrc
└── src/
    ├── main.cpp         # loads theme, shows MainWindow
    ├── MainWindow.{h,cpp}
    └── (data/ domain/ ui/ added as modules are ported)
```
