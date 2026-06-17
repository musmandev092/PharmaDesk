# Security Model

PharmaDesk is a single-PC pharmacy POS handling money, controlled substances, and a
regulatory audit trail. The primary threat model is **insider / compliance**, not network
attackers (the app is offline, single-host).

## Trust boundaries & threats

| Asset | Threat | Control |
|---|---|---|
| User credentials (PINs) | Theft, brute force | bcrypt (`$2b$`, cost 12) via OS libxcrypt; account lockout with exponential backoff. |
| Privileged operations (void, refund, price/role change, stock adjust, GRN) | Unauthorized action by a lower-privileged user | Authorization enforced at the **service boundary** (not just UI). e.g. `voidSale` requires an active manager/admin. |
| Audit trail | Silent tampering / deletion | `audit_log` is append-only (DB triggers block UPDATE/DELETE) AND HMAC-chained (`prev_hmac`/`row_hmac`, key in off-DB `audit.key`); writes are in the same transaction as the business change. `Audit::verifyChain` detects any edit/forgery/reinsertion. |
| Money / stock integrity | Rounding drift, oversell, negative stock, ledger gaps | Fixed-point Money (no float); guarded atomic decrement; `inventory_movements` ledger + consistency trigger; invariant tests. |
| Z-reports (day-close) | Tampering | Append-only archive + HMAC signature with an off-DB `zreport.key` (owner-only perms). |
| Backups | Exposure of PINs/patient data | Backup file written owner-only; WAL folded for a consistent copy. |

## AuthN

Login is username + numeric PIN, verified with bcrypt `crypt_r` (constant-work). Repeated
failures lock the account for an exponentially increasing window (`users.failed_attempts`,
`locked_until`). PIN changes go through `PinPolicy` (length, blocklist, run, history reuse).

## AuthZ

Roles: `CASHIER`, `MANAGER`, `ADMIN`. The intended rule is **defense in depth**: the UI
hides/disables what a role can't do, AND the service re-checks. Current enforcement state:

- **Enforced in the service:** sale discount authorization + controlled-substance two-person
  witness (re-validated server-side); `voidSale` manager/admin gate; stock-adjust manager
  PIN (UI) + service writes.
- **Follow-ups (tracked in DEBT.md):** push role checks into the service for medicine edit,
  GRN post, and user management (currently UI-gated); audit + atomicity for
  `UserRepository::updateUser` and `SettingsRepository::set`.

## Injection / memory safety

All SQL uses bound parameters (the only interpolated statements are two internal PRAGMAs).
Every `QProcess` call passes an argument list, not a shell string (no command injection).
The `.xlsx` importer parses with `QXmlStreamReader` (no external-entity expansion) and caps
decompressed size (zip-bomb guard). Memory is managed by Qt parent/child ownership + value
types; Money arithmetic is overflow-checked.

## Crypto & secrets

- Passwords/PINs: bcrypt (OS libxcrypt). No home-grown crypto.
- Z-report signing key: `zreport.key`, owner-only, stored off the DB (see `AppPaths`).
- Audit HMAC chain key: `audit.key`, 32 random bytes, owner-only, off the DB, generated
  once. **Rotation** is an open owner decision — a single per-install key today; rotating it
  invalidates verification of rows signed by the old key, so a rotation must archive the old
  key alongside the rows it signed.
- No hardcoded credentials or API keys in the source.

## What is out of scope

- Network attackers / multi-tenant isolation (single offline host).
- Production data migration from the legacy Postgres system (source absent).
- Full disk encryption (an OS-level concern for the deployment box).

See `audit/05_security.md` for the full findings list with severities and `file:line`.
