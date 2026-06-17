"""Shared canonical-JSON form for the PharmaDesk license payload.

This MUST byte-match the C++ verifier (src/domain/Licensing.cpp::canonicalPayload),
which uses Qt's QJsonDocument::toJson(Compact): keys sorted (ASCII), no spaces,
UTF-8 (NOT \\u-escaped). So here we use sort_keys=True, the compact separators, and
ensure_ascii=False. The "sig" field is excluded — it signs everything else.
"""

from __future__ import annotations

import json


def canonical_payload(payload: dict[str, object]) -> bytes:
    body = {k: payload[k] for k in payload if k != "sig"}
    return json.dumps(
        body, sort_keys=True, separators=(",", ":"), ensure_ascii=False
    ).encode("utf-8")
