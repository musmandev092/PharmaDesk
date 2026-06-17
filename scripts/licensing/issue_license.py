#!/usr/bin/env python3
"""Issue a signed PharmaDesk license for one machine — run on YOUR machine.

Takes the activation REQUEST the pharmacy sent you, signs a license bound to that
machine's signals with your PRIVATE key, and writes a `license.lic` to send back.

Usage:
    python3 scripts/licensing/issue_license.py --request <token-or-file> \\
        --licensee "City Care Pharmacy" --expiry 2027-06-30
    python3 scripts/licensing/issue_license.py --request req.txt --days 365
"""
from __future__ import annotations

import argparse
import base64
import datetime
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import _ed25519  # noqa: E402
from _canonical import canonical_payload  # noqa: E402


def signing_key_dir() -> Path:
    import os

    env = os.environ.get("PHARMADESK_SIGNING_KEY_DIR")
    if env:
        return Path(env).expanduser()
    base = os.environ.get("XDG_DATA_HOME") or str(Path.home() / ".local" / "share")
    return Path(base) / "pharmadesk-signing-key"


def read_request(value: str) -> dict:
    token = value
    p = Path(value)
    if p.exists():
        token = p.read_text(encoding="utf-8").strip()
    data = json.loads(base64.b64decode(token.strip()).decode())
    sig = data.get("signals")
    if not isinstance(sig, dict) or not sig:
        raise ValueError("request has no machine signals")
    if len(sig) < 2:
        raise ValueError(
            f"request carries only {len(sig)} machine signal ({', '.join(sig)}); "
            "at least 2 are required to issue a node-locked license."
        )
    return data


def main() -> int:
    ap = argparse.ArgumentParser(description="Issue a signed PharmaDesk license.")
    ap.add_argument("--request", required=True, help="activation request token or file")
    ap.add_argument("--licensee", default="", help="pharmacy / customer name (recorded in the license)")
    ap.add_argument("--expiry", default="", help="expiry date YYYY-MM-DD (blank = perpetual)")
    ap.add_argument("--days", type=int, default=0, help="alternative to --expiry: valid N days")
    ap.add_argument("--out", default="license.lic", help="output file")
    args = ap.parse_args()

    key_path = signing_key_dir() / "private.key"
    if not key_path.exists():
        print(f"!! signing key not found: {key_path} — run generate_keys.py first.")
        return 1
    secret = base64.b64decode(key_path.read_text(encoding="utf-8").strip())

    req = read_request(args.request)

    expiry = args.expiry.strip()
    if not expiry and args.days > 0:
        expiry = (datetime.date.today() + datetime.timedelta(days=args.days)).isoformat()
    if expiry:
        try:
            datetime.date.fromisoformat(expiry)
        except ValueError:
            print(f"!! invalid --expiry date: {expiry} (use YYYY-MM-DD)")
            return 1

    payload: dict[str, object] = {
        "v": 1,
        "licensee": args.licensee,
        "signals": req["signals"],
        "issued": datetime.date.today().isoformat(),
        "expiry": expiry,
    }
    payload["sig"] = base64.b64encode(_ed25519.sign(secret, canonical_payload(payload))).decode()

    out = Path(args.out)
    # The app accepts compact JSON; keep it compact so it's easy to paste.
    out.write_text(json.dumps(payload, separators=(",", ":"), ensure_ascii=False), encoding="utf-8")
    print(f"✓ License written: {out}")
    print(f"  licensee: {payload['licensee'] or '(unnamed)'}")
    print(f"  expiry:   {expiry or 'perpetual'}")
    print(f"  signals:  {', '.join(req['signals'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
