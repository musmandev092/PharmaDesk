# PharmaDesk — Operator Runbook (single-PC AlmaLinux 10)

This is the on-site recovery guide. Keep a printed copy by the till.

## Where the data lives

All pharmacy data is in one SQLite database on this PC:

```
~/.local/share/PharmaDesk/PharmaDesk/pharmadesk.sqlite
        (plus -wal and -shm sidecar files while the app runs)
```

Other important files in that folder:
- `zreport.key` — secret key that signs the tamper-evident Z-reports. **Back this up too.** If it is lost, old Z-report signatures can no longer be verified.
- `logs/pharmadesk.log` — diagnostic log.
- `logos/` — the pharmacy logo.

All of these are owner-only (mode 0600/0700); keep them that way.

## Durability (what survives a crash / power cut)

The database runs in WAL mode with `synchronous = FULL` and `busy_timeout = 5000`. With `synchronous = FULL`, a **committed** sale survives an app crash *and* a power cut. There is **no UPS assumption baked in** — but for a cash till, fitting a small UPS is strongly recommended so an in-progress (uncommitted) sale isn't lost mid-keystroke.

Every sale, return, GRN, and stock adjustment is wrapped in a single DB transaction and writes its audit row inside that transaction: if anything fails (including the audit write), the **whole** change rolls back — stock is never left inconsistent.

## Backups

- **Set a backup folder** in Admin → Backup, pointing at a USB drive or network share (NOT this PC's disk — a disk failure must not take the backup with it).
- **Daily auto-backup** runs on startup if more than 24h have passed; **Back up now** forces one immediately.
- Each backup is a consistent single-file copy (`PRAGMA wal_checkpoint(TRUNCATE)` folds the WAL in first) named `pharmadesk_YYYYMMDD_HHMMSS.sqlite`, written mode 0600.
- **Also copy `zreport.key`** to the backup folder once (it does not change), so Z-report signatures remain verifiable after a restore.

**RPO (max data loss):** last successful backup — at most one business day with daily auto-backup; near-zero if you "Back up now" at close.
**RTO (time to a working till):** ~10–15 minutes (reinstall AppImage + copy DB back).

## Verify a backup is good

Before trusting a backup, check it opens and is structurally sound:

```sh
sqlite3 /path/to/backup/pharmadesk_YYYYMMDD_HHMMSS.sqlite "PRAGMA integrity_check;"
# expect: ok
sqlite3 /path/to/backup/pharmadesk_YYYYMMDD_HHMMSS.sqlite \
  "SELECT count(*) FROM sales; SELECT count(*) FROM users;"
```

## RESTORE — recover from a backup

> Do this only when the live database is lost or corrupt. It **overwrites** current data with the backup.

1. **Quit PharmaDesk completely** (close the window; confirm no `pharmadesk` process: `pkill -x pharmadesk` if unsure). The DB must not be open during a restore.
2. Go to the data folder:
   ```sh
   cd ~/.local/share/PharmaDesk/PharmaDesk
   ```
3. **Move the broken files aside** (don't delete — they may help diagnosis):
   ```sh
   mkdir -p broken && mv pharmadesk.sqlite* broken/ 2>/dev/null
   ```
4. **Copy the chosen backup into place** (note: the live file has NO timestamp suffix):
   ```sh
   cp /path/to/backup/pharmadesk_YYYYMMDD_HHMMSS.sqlite pharmadesk.sqlite
   ```
   A backup taken via "Back up now" is already checkpointed, so it has no `-wal`/`-shm` sidecars to copy.
5. **Restore the key** if it was lost:
   ```sh
   cp /path/to/backup/zreport.key zreport.key 2>/dev/null
   ```
6. **Fix permissions:**
   ```sh
   chmod 600 pharmadesk.sqlite zreport.key 2>/dev/null
   ```
7. **Verify, then launch:**
   ```sh
   sqlite3 pharmadesk.sqlite "PRAGMA integrity_check;"   # expect: ok
   ```
   Start PharmaDesk. On first launch it auto-applies any pending schema migrations. Sign in and confirm recent sales / stock look right.

## Rebuild this PC from scratch

1. Reinstall the OS / user account.
2. Install the latest `PharmaDesk-x86_64.AppImage` (and integrate it: just run it once — it adds itself to the menu).
3. Launch once so it creates the data folder, then quit.
4. Follow **RESTORE** above with the latest verified backup.
5. If you have no backup, you start fresh: run the setup wizard, then re-import the medicine catalog with `pharmadesk_import medicines_export.csv`.

## Updating the app in the field

1. Back up first (Admin → Backup → Back up now).
2. Replace the AppImage file with the new one (same name/location).
3. Launch — pending schema migrations apply automatically on startup. If a migration fails, the app reports the error and does not proceed; restore the pre-update backup and contact the developer.

## Who to call

Developer: **M Usman** — github.com/mosman092 — musmaniqbalbaloch@gmail.com
