# Onboarding & Build

## Prerequisites

- **Qt 6** (Widgets, Sql, PrintSupport). Qt 5.15 also works for dev (CMake falls back), but
  ship with Qt6 — the AlmaLinux 10 target uses Qt6.
- **CMake ≥ 3.16** and a C++17 compiler (GCC or Clang).
- **Ninja** (recommended) for fast builds.
- **libxcrypt** (`libcrypt-dev` on Ubuntu, `libxcrypt` on Arch) — bcrypt via `crypt_r`.
- **zlib** (`zlib1g-dev` / `zlib`) — used by the `.xlsx` catalog importer.

Arch: `sudo pacman -S qt6-base cmake ninja gcc` (zlib + libxcrypt are in base).
Ubuntu: `sudo apt-get install qt6-base-dev qt6-base-dev-tools libqt6sql6-sqlite ninja-build cmake g++ libcrypt-dev zlib1g-dev`.

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

Targets: `pharmadesk` (GUI app), `pharmadesk_import` (legacy CSV catalog CLI),
`pharmadesk_tests` (the suite), `pharmadesk_smoke` / `pharmadesk_shots` (harnesses).

## Run the app

```bash
./build/pharmadesk
```

First run launches the setup wizard (create the admin + pharmacy). The DB lives under the
user's app-data dir (see `domain/AppPaths`).

## Tests

```bash
ctest --test-dir build --output-on-failure          # CTest (suite + barcode), headless
QT_QPA_PLATFORM=offscreen ./build/pharmadesk_tests   # run the suite directly, verbose
```

The suite is hand-rolled (no GoogleTest): `tests/framework/TestStats.h` provides
`check(cond, "name")`; each module is `run_<module>_tests(QSqlDatabase, userId)` registered
in `tests/run_all.cpp`. It runs against **real SQLite + real repositories** (no mocking),
headless via `QT_QPA_PLATFORM=offscreen`, isolated by unique per-module identifier prefixes.

### Adding a test module

1. Add `tests/cases/<name>_tests.cpp` with
   `TestStats run_<name>_tests(QSqlDatabase db, qint64 userId)`.
2. Forward-declare it and add a `mods[]` row in `tests/run_all.cpp`.
3. Add the file to the `pharmadesk_tests` sources in `CMakeLists.txt`.

## Quality gates (run locally before every commit)

```bash
clang-format --dry-run --Werror $(git ls-files 'src/*.cpp' 'src/*.h' 'tests/*.cpp')  # blocking
python3 tools/check_layering.py                                                       # blocking
cppcheck --enable=warning,performance,portability,style -I src $(git ls-files 'src/*.cpp')  # report
run-clang-tidy -p build $(git ls-files 'src/*.cpp')                                   # report
```

CI (`.github/workflows/ci.yml`) runs these on every push/PR to `main` and `dev`. See
`DEBT.md` for the ratchet model and current baselines. Green-locally is how you avoid
red-on-push — CI runs on GitHub where you can't watch it live.

## Importing a medicine catalog

In the app: **Medicines → Import…** and choose a `.xlsx` or `.csv` file. A ready template
is at `resources/templates/medicine_import_template.xlsx`. Format details:
`docs/MEDICINE_IMPORT_FORMAT.md`. Re-running is safe (existing SKUs are skipped).

## Release

Bump the version with `scripts/bump_version.sh X.Y.Z`, merge to `main`, and the
`build.yml` workflow packages a portable tarball and cuts a GitHub Release `vX.Y.Z`.
See `docs/adr/0005-versioning-and-release.md`.
