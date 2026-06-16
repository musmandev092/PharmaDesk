# ADR-0005: Forward-only migrations, single-source version, automated release

**Status:** Accepted

## Context
A single-PC deployment upgraded in place needs a safe schema-evolution story and a
repeatable release that produces a runnable artifact, with the version visible in the app.

## Decision
- **Schema migrations are forward-only**, via `PRAGMA user_version` stepping
  (`data/Database.cpp`): an ordered registry, each migration in its own transaction. The
  baseline `sql/schema_sqlite.sql` is version 0; changes are appended as numbered
  migrations, never folded back. Migrations are additive (ADD COLUMN / CREATE … IF NOT
  EXISTS / data updates); there is no down-path — recovery is restore-from-backup, so
  **back up before upgrading**.
- **One source of truth for the version**: CMake `project(pharmadesk VERSION x.y.z)`, baked
  into the binary as `PHARMADESK_VERSION` and shown in the UI (login + about/footer).
  `scripts/bump_version.sh X.Y.Z` updates it in one place.
- **Automated release** (`.github/workflows/build.yml`, `main` + manual only): build Release
  → CPack `TGZ` + `.sha256` → upload artifact → if `v<version>` doesn't exist, publish a
  GitHub Release tagged `v<version>`; always refresh a rolling `latest` pre-release.
  CI (`ci.yml`) runs on `main` and `dev`; only `build.yml` publishes, and only from `main`.

## Consequences
- **+** A version bump merged to `main` produces a tagged release automatically.
- **+** Upgrades converge on the same schema whether fresh or stepped.
- **−** No automated schema rollback (acceptable for a single till with backups).
