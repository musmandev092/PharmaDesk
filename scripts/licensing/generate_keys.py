#!/usr/bin/env python3
"""Generate the vendor's Ed25519 license key pair for PharmaDesk — RUN ONCE.

Creates two files under a non-synced signing-key dir (OUTSIDE the repo so the secret
never sits next to the code):
  * private.key  — SECRET. Stays on YOUR machine; signs licenses (issue_license.py).
                   BACK IT UP offline. If it leaks, anyone can mint licenses.
  * public.key   — not secret. Embedded in the app so it can verify licenses.

It then patches src/domain/Licensing.cpp to embed the public key
(kEmbeddedPublicKeyB64), which ARMS licensing for the next release build.

Usage:
    python3 scripts/licensing/generate_keys.py
    python3 scripts/licensing/generate_keys.py --force   # overwrite existing
"""
from __future__ import annotations

import argparse
import base64
import contextlib
import os
import secrets
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import _ed25519  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
LICENSING_CPP = REPO / "src" / "domain" / "Licensing.cpp"


def signing_key_dir() -> Path:
    env = os.environ.get("PHARMADESK_SIGNING_KEY_DIR")
    if env:
        return Path(env).expanduser()
    base = os.environ.get("XDG_DATA_HOME") or str(Path.home() / ".local" / "share")
    return Path(base) / "pharmadesk-signing-key"


def main() -> int:
    ap = argparse.ArgumentParser(description="Generate the vendor Ed25519 key pair (run once).")
    ap.add_argument("--force", action="store_true", help="overwrite an existing private.key")
    args = ap.parse_args()

    keys_dir = signing_key_dir()
    keys_dir.mkdir(parents=True, exist_ok=True)
    with contextlib.suppress(OSError):
        os.chmod(keys_dir, 0o700)
    priv_path = keys_dir / "private.key"
    pub_path = keys_dir / "public.key"

    if priv_path.exists() and not args.force:
        print(f"!! {priv_path} already exists — refusing to overwrite your signing key.")
        print("   (--force makes a brand-new pair; doing so invalidates every issued license.)")
        return 1

    secret = secrets.token_bytes(32)
    public = _ed25519.secret_to_public(secret)
    priv_b64 = base64.b64encode(secret).decode()
    pub_b64 = base64.b64encode(public).decode()

    fd = os.open(str(priv_path), os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as fh:
        fh.write(priv_b64 + "\n")
    pub_path.write_text(pub_b64 + "\n", encoding="utf-8")

    # Embed the public key into Licensing.cpp (arms licensing for the next build).
    patched = False
    if LICENSING_CPP.exists():
        text = LICENSING_CPP.read_text(encoding="utf-8")
        import re

        new, n = re.subn(
            r'(const QString kEmbeddedPublicKeyB64 = QStringLiteral\(")[^"]*("\);)',
            rf"\g<1>{pub_b64}\g<2>",
            text,
            count=1,
        )
        if n == 1:
            LICENSING_CPP.write_text(new, encoding="utf-8")
            patched = True

    print("✓ Key pair generated.")
    print(f"  private key (SECRET, back this up): {priv_path}")
    print(f"  public  key (base64): {pub_b64}")
    if patched:
        print(f"✓ Embedded the public key into {LICENSING_CPP.relative_to(REPO)}.")
        print("  Rebuild a Release with -DPHARMADESK_ENFORCE_LICENSE to arm enforcement.")
    else:
        print(f"!! Could not auto-embed — set kEmbeddedPublicKeyB64 in {LICENSING_CPP} manually.")
    print("\nNEVER commit private.key. NEVER ship it. Keep a safe OFFLINE backup.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
