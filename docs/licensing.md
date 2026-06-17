# Licensing & Activation (vendor guide)

PharmaDesk supports **offline, node-locked activation** — a copy only runs on a machine
it has been activated for. No internet, no license server. Ported from LabDesk.

## How it works

1. The app builds an activation **request** (the machine's hashed signals + hostname).
2. You (the vendor) sign a **license** for that request with your Ed25519 **private** key.
3. The app verifies the license with the **embedded public key**, checks it's for *this*
   machine (tolerating one hardware change) and not expired, then unlocks.

Copying the app to another PC fails (signals don't match). Editing the license fails (the
signature breaks — the attacker doesn't have the private key).

## One-time setup (vendor)

```bash
python3 scripts/licensing/generate_keys.py
```
- Writes `private.key` (SECRET — back it up offline, never commit/ship) and `public.key` to
  a non-synced dir (`$XDG_DATA_HOME/pharmadesk-signing-key`, override with
  `PHARMADESK_SIGNING_KEY_DIR`).
- Patches `kEmbeddedPublicKeyB64` in `src/domain/Licensing.cpp` (this **arms** licensing).
- Rebuild a **Release** with `-DPHARMADESK_ENFORCE_LICENSE` to make enforcement the default.

> Until a key is embedded, `configured()` is false and the app runs **unlocked** — so
> development, CI, tests, and the screenshot harness are never blocked.

## Activating a machine

1. On the customer's PC, launch the (armed) app → the **Activation** dialog appears. They
   send you the **activation request** (Copy/Save) and read out the computer code (e.g.
   `PD-7D2C-36F2-97E0-6CF2`).
2. You issue a license:
   ```bash
   python3 scripts/licensing/issue_license.py \
       --request <token-or-file> \
       --licensee "City Care Pharmacy" \
       --expiry 2027-06-30          # or --days 365, or omit for perpetual
   ```
   This writes `license.lic`.
3. Send `license.lic` back. They **Load license file…** (or paste) and click **Activate**.

## Enforcement gating

`Licensing::enforced()` is true only when **both** hold:
- a real public key is embedded (`configured()`), and
- enforcement is requested — the `PHARMADESK_ENFORCE_LICENSE` compile define (set by the
  Release/AppImage build) **or** the same-named environment variable (handy for testing the
  licensed flow from a dev build).

## Security notes

- **Node-lock:** a license binds ≥2 machine signals (`machine_id` /etc/machine-id, `mac`,
  `disk` serial — each SHA-256 hashed with an app salt). Activation needs `max(2, n-1)` of
  `n` to match, so at most one hardware change is tolerated (and only when ≥3 were bound).
- **Clock rollback:** a keyed, monotonic high-water file (`.license-seen`) stops winding the
  system clock back to revive an expired license.
- **Crypto:** Ed25519 (OpenSSL libcrypto in the app; the pure-Python reference signer in the
  vendor tools). The signed payload is canonical JSON; the C++ verifier and the Python issuer
  agree byte-for-byte (proved by `licensing` tests with a baked cross-language fixture).
- Files (`license.lic`, `.license-seen`) live in the app-data dir, owner-only (0600).
- **Private key:** if it leaks, anyone can mint licenses — generate a new pair and rebuild.
  Losing it means you can't issue *new* licenses (existing ones keep working). Back it up.
