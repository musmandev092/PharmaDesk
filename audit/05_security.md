# 05 — Security Audit

_Classified Critical / High / Medium / Low. Cites `file:line` at commit `12a6316`._

## Summary — Grade **C+**

The cryptographic primitives are correct (real bcrypt cost-12, account lockout, signed
Z-reports, trigger-protected append-only audit table, fully parameterized SQL). The
weakness is **authorization architecture**: privileged writes are gated at the UI, not the
service boundary, leaving at least one exploitable hole (any cashier can void any sale),
and the **audit log's tamper-evidence (HMAC chain) is declared in the schema but never
populated**. Fixing the write boundary (Phase 4) and the HMAC chain (Phase 5) moves this
to A−.

## Critical

_None that are remotely exploitable on a single-PC deployment, but treat **H1** as
effectively critical for an insider-threat / compliance model._

## High

| ID | Finding | Location | Fix |
|---|---|---|---|
| H1 | **Void sale has zero authorization.** `ReturnService::voidSale` restocks every line and marks the sale VOIDED (financially a full refund), but the only gate is a confirm dialog; the Void button is enabled by sale *status*, not role, and the Returns page is primary nav for **every** role incl. cashier. Any cashier can void any completed sale. (It *is* audited atomically — the gate is missing.) | `ui/ReturnsPage.cpp:629,448`, `MainWindow.cpp:147`, `service/ReturnService.cpp:767` | Require manager re-auth (PIN) like stock-adjust; enforce the role check **in the service**, not the UI. (Phase 4) |
| H2 | **Audit log is append-only but NOT tamper-evident.** `prev_hmac`/`row_hmac` columns exist and default to `''`; `Audit::write` never populates them. Triggers block in-place UPDATE/DELETE, but a wholesale DB-file substitution leaves no detectable break. | `sql/schema_sqlite.sql:386-387`, `data/Audit.cpp:13-23` | Implement the deferred HMAC chain: `row_hmac = HMAC(key, canonical(row) ‖ prev_hmac)`, key off-DB; add a chain verifier + invariant test. (Phase 5) |
| H3 | **Non-atomic, best-effort audit on three mutating paths.** `MedicineRepository::update/create` and `UserRepository::updateUser` do the data UPDATE and the `Audit::write` as **separate autocommits** and discard the audit return value — a price change or role change can commit with no audit trail and still report success. | `data/MedicineRepository.cpp:209`, `data/UserRepository.cpp:222` | Wrap write+audit in one `transaction()` and use `writeOrThrow`. (Phase 4) |
| H4 | **Settings changes are entirely unaudited** (and `setMany` is non-atomic). Return policy, backup folder, printer queue, NTN, theme — no `audit_log` row despite `updated_by` on the row. | `data/SettingsRepository.cpp:30` | Audit setting changes through the authorized write seam. (Phase 4) |
| H5 | **SQLite float arithmetic on a TEXT money column.** `bumpSessionRefund` does `total_refunds_paid = total_refunds_paid + ?` — SQLite coerces the TEXT decimal to IEEE-754 double, re-introducing float drift into cash reconciliation. | `service/ReturnService.cpp:99-104` | Read → add with `Money` → write `toString()`, mirroring `SessionRepository::bumpForSale`. (Phase 5) |

## Medium

| ID | Finding | Location |
|---|---|---|
| M1 | Authorization is UI-only for medicine edit, GRN posting, user management, settings — no service/repo role check; defense relies on menu visibility (`isMgr`/`isAdmin`). | `MainWindow.cpp:97-98,182`, repos |
| M2 | Refund/return authz for cashiers is a UI manager-PIN prompt, not enforced in the service. | `ui/ReturnsPage.cpp:557-602` |
| M3 | GRN posting (high-value write) has no credential prompt at all — weaker than stock-adjust. | `service/GrnService.cpp`, `ui/GrnDialog.cpp` |
| M4 | `OVERRIDE_GRANTED` audit row is committed pre-transaction; if the sale rolls back the override audit orphans. | `ui/PosTerminalPage.cpp:491` vs `SaleService.cpp:215` |
| M5 | `registerFailedLogin`/`clearFailedLogins` write no audit row. | `data/UserRepository.cpp:348,399` |
| M6 | `PRAGMA table_info(%1)` / `PRAGMA user_version = %1` interpolate (not bound). Table name is an internal constant and version an int, so not exploitable today — but the only non-parameterized SQL in the tree. | `data/Database.cpp:277,271` |

## Low

| ID | Finding | Location |
|---|---|---|
| L1 | `BatchRepository::addStock` uses non-throwing `Audit::write` and ignores the result. | `data/BatchRepository.cpp:55` |
| L2 | `audit_log.user_id` has no FK (intentional for an immutable log) — reports must LEFT JOIN. | `sql/schema_sqlite.sql:374-388` |
| L3 | Backup is a full DB copy (PIN hashes, patient names); perms set owner-only — good — but no encryption at rest. | `service/BackupService.cpp:37-40` |

## What is already done well (do not regress)

- **Passwords & PINs:** real bcrypt via OS libxcrypt, `$2b$`, **cost 12** (matches PHP);
  `crypt_r` reentrant; verify via constant-work `crypt_r`. `data/UserRepository`, `domain/Bcrypt.cpp`.
- **Account lockout:** consecutive-failure lockout with **exponential backoff** (30/60/120s…
  capped). `data/UserRepository.cpp:358-403`.
- **SQL injection:** all queries use bound parameters (`prepare`+`bindValue`); only the two
  internal PRAGMAs interpolate (M6). No string-built WHERE clauses.
- **Command execution:** every `QProcess` call passes an **argument list** (not a shell
  string): `update-desktop-database`, `gtk-update-icon-cache`, `lp`, `lpstat`. No injection.
  `domain/DesktopIntegration.cpp:152-153`, `service/Cups*`.
- **Deserialization:** parked-cart JSON parsed with Qt's `QJsonDocument` (no code exec).
- **Append-only audit:** DB triggers `audit_log_no_update`/`no_delete` + the same on
  `z_report_archive` (migration v5) — raw SQL edits are blocked.
- **Z-report signing:** `zreport.key` (owner-only, off the DB) signs the Z-report archive —
  the tamper-anchor pattern that the audit log still needs (H2).
- **Memory safety:** Qt parent/child ownership throughout; no raw `new`/`delete` leaks
  found; `Money` arithmetic is overflow-checked (`__builtin_*_overflow`, throws).

## Severity count

Critical 0 · High 5 · Medium 6 · Low 3.
