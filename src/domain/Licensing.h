#pragma once

#include <QJsonObject>
#include <QString>

// Offline, node-locked licensing for PharmaDesk. C++ port of LabDesk's licensing
// module. A copy of PharmaDesk only runs on a machine it has been ACTIVATED for —
// offline and signature-based, no internet, no license server:
//
//   1. The app builds an activation REQUEST (the machine's hashed signals).
//   2. The vendor signs a LICENSE for that request with their Ed25519 PRIVATE key
//      (scripts/licensing/issue_license.py) and sends back `license.lic`.
//   3. The app verifies the license with the embedded PUBLIC key, checks it is for
//      THIS machine (tolerating one hardware change) and not expired, then unlocks.
//
// Copying the app to another PC fails (signals don't match). Editing the license
// fails (the signature breaks). Enforcement is gated (see enforced()): it only bites
// when a real public key is embedded AND a release/launcher opts in — never in dev
// runs, tests, or the screenshot harness, so development and CI are never blocked.
namespace Licensing {

// ── configuration / gating ──────────────────────────────────────────────────
// True once a real public key is embedded (the machinery is armed). Empty by
// default → the app runs unlocked. generate_keys.py fills the key in.
bool configured();

// Whether this launch must be licensed: configured() AND enforcement requested
// (the PHARMADESK_ENFORCE_LICENSE compile define, set by the release/AppImage
// build, OR the same-named environment variable for testing the licensed flow).
bool enforced();

// ── activation request (app -> vendor) ──────────────────────────────────────
// A compact, copy-pasteable base64 token carrying the machine's HASHED signals +
// hostname (for the vendor's records). No raw hardware ids.
QString buildRequest();

// Short human code for this machine (quick phone reference). PD-XXXX-...
QString currentCode();

// ── verification ─────────────────────────────────────────────────────────────
// Exact bytes that get signed: canonical JSON of everything except "sig".
QByteArray canonicalPayload(const QJsonObject &payload);

// True iff payload["sig"] is a valid vendor signature over the payload.
bool verifySignature(const QJsonObject &payload);

// Classify a parsed license for THIS machine. state ∈
// ok | bad_signature | wrong_machine | expired | invalid.
struct Evaluation
{
    QString state;
    QString message;
};
Evaluation evaluate(const QJsonObject &payload);

// Accept a license as raw JSON or base64-wrapped JSON. ok=false if unparseable.
QJsonObject parseLicenseText(const QString &text, bool *ok = nullptr);

// Evaluate the installed license (if any). state is "unactivated" when no license
// file is present, else the evaluate() state. Records the clock high-water on ok.
struct CheckResult
{
    QString state;
    QJsonObject payload;
    bool hasPayload = false;
};
CheckResult check();

// Validate a pasted/loaded license for THIS machine and, if good, save it 0600.
struct InstallResult
{
    bool ok = false;
    QString message;
};
InstallResult installLicense(const QString &text);

// The installed license payload (for display), or an empty object.
QJsonObject licenseInfo();

// Verify a payload's "sig" against an EXPLICIT base64 public key (does not touch
// the embedded key or any machine state). Used to confirm a vendor-issued license
// signature in tests/tools, proving the canonical-JSON + Ed25519 round-trip.
bool verifyDetached(const QString &publicKeyB64, const QJsonObject &payload);

// TEST-ONLY: override the embedded public key for the current process. Production
// never calls this — the compiled-in key is the trust anchor. Exposed so tests can
// exercise the full evaluate()/check()/install() flow with a throwaway key pair.
void setPublicKeyForTesting(const QString &publicKeyB64);

} // namespace Licensing
