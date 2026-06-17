#include "domain/Licensing.h"

#include "domain/AppPaths.h"
#include "domain/MachineFingerprint.h"

#include <QByteArray>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QMessageAuthenticationCode>

#include <openssl/evp.h>

#include <unistd.h>

namespace Licensing {
namespace {

// ── The vendor's Ed25519 PUBLIC key (base64 of 32 bytes). EMPTY = licensing not
//    configured (the app runs unlocked). scripts/licensing/generate_keys.py fills
//    this in. The matching PRIVATE key stays on the vendor's machine, NEVER shipped.
const QString kEmbeddedPublicKeyB64 = QStringLiteral("");

// The active public key — the embedded one in production; tests may override it via
// setPublicKeyForTesting (production code never does).
QString g_publicKeyB64 = kEmbeddedPublicKeyB64;
QString publicKeyB64()
{
    return g_publicKeyB64;
}

const QString kLicenseName = QStringLiteral("license.lic");
const QString kSeenName = QStringLiteral(".license-seen");

QString licenseDir()
{
    // Beside the DB in the app-data dir (same idiom as audit.key / zreport.key).
    return QDir(AppPaths::logosDir()).filePath(QStringLiteral(".."));
}
QString licensePath()
{
    return QDir(licenseDir()).filePath(kLicenseName);
}
QString seenPath()
{
    return QDir(licenseDir()).filePath(kSeenName);
}

QByteArray publicKey()
{
    return QByteArray::fromBase64(publicKeyB64().trimmed().toLatin1());
}

bool ed25519Verify(const QByteArray &pub32, const QByteArray &msg, const QByteArray &sig)
{
    if (pub32.size() != 32 || sig.size() != 64) {
        return false;
    }
    EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519, nullptr, reinterpret_cast<const unsigned char *>(pub32.constData()), 32);
    if (!pkey) {
        return false;
    }
    bool ok = false;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (ctx && EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey) == 1) {
        ok = EVP_DigestVerify(ctx, reinterpret_cast<const unsigned char *>(sig.constData()),
                              static_cast<size_t>(sig.size()),
                              reinterpret_cast<const unsigned char *>(msg.constData()),
                              static_cast<size_t>(msg.size()))
             == 1;
    }
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return ok;
}

// ── clock-rollback high-water mark ───────────────────────────────────────────
// Keyed digest binding a date to this build's public key, so a user can't hand-edit
// the high-water file to an earlier date undetected.
QString seenStamp(const QString &isoDate)
{
    QByteArray key = publicKeyB64().trimmed().toLatin1();
    if (key.isEmpty()) {
        key = QByteArrayLiteral("pharmadesk-seen");
    }
    return QString::fromLatin1(
               QMessageAuthenticationCode::hash(isoDate.toLatin1(), key, QCryptographicHash::Sha256)
                   .toHex())
        .left(16);
}

QDate highWaterDate()
{
    QFile f(seenPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QDate();
    }
    const QString raw = QString::fromUtf8(f.readAll()).trimmed();
    const int bar = raw.indexOf(QLatin1Char('|'));
    if (bar < 0) {
        return QDate();
    }
    const QString iso = raw.left(bar).trimmed().left(10);
    const QString stamp = raw.mid(bar + 1).trimmed();
    if (stamp.isEmpty() || stamp != seenStamp(iso)) {
        return QDate(); // unstamped / hand-edited → ignore (falls back to the real clock)
    }
    return QDate::fromString(iso, Qt::ISODate);
}

QDate effectiveToday()
{
    const QDate today = QDate::currentDate();
    const QDate hw = highWaterDate();
    return (hw.isValid() && hw > today) ? hw : today;
}

void recordSeen()
{
    const QDate today = QDate::currentDate();
    const QDate hw = highWaterDate();
    const QDate newest = (hw.isValid() && hw > today) ? hw : today;
    const QString iso = newest.toString(Qt::ISODate);
    AppPaths::ensureDirs();
    QFile f(seenPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write((iso + QLatin1Char('|') + seenStamp(iso)).toUtf8());
        f.close();
        f.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    }
}

bool signalsMatch(const QJsonObject &licSignals)
{
    // A license must bind ≥2 signals; activation needs max(2, n-1) of n to match,
    // so at most one hardware change is tolerated (and only when ≥3 were bound).
    if (licSignals.size() < 2) {
        return false;
    }
    const QMap<QString, QString> cur = MachineFingerprint::collectSignals();
    int matched = 0;
    for (auto it = licSignals.constBegin(); it != licSignals.constEnd(); ++it) {
        const auto cit = cur.constFind(it.key());
        if (cit != cur.constEnd() && it.value().isString()
            && cit.value() == it.value().toString()) {
            ++matched;
        }
    }
    const int needed = qMax(2, licSignals.size() - 1);
    return matched >= needed;
}

} // namespace

bool configured()
{
    return !publicKeyB64().trimmed().isEmpty();
}

bool enforced()
{
    if (!configured()) {
        return false;
    }
#ifdef PHARMADESK_ENFORCE_LICENSE
    return true; // shipped release: always enforced
#else
    return !qEnvironmentVariableIsEmpty("PHARMADESK_ENFORCE_LICENSE");
#endif
}

QByteArray canonicalPayload(const QJsonObject &payload)
{
    QJsonObject body = payload;
    body.remove(QStringLiteral("sig"));
    // QJsonObject keeps keys sorted (ASCII) and Compact emits no spaces / UTF-8 —
    // byte-identical to the Python issuer's json.dumps(sort_keys=True,
    // separators=(",",":"), ensure_ascii=False).
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

bool verifySignature(const QJsonObject &payload)
{
    if (!configured()) {
        return false;
    }
    const QJsonValue sig = payload.value(QStringLiteral("sig"));
    if (!sig.isString()) {
        return false;
    }
    const QByteArray sigBytes = QByteArray::fromBase64(sig.toString().toLatin1());
    return ed25519Verify(publicKey(), canonicalPayload(payload), sigBytes);
}

Evaluation evaluate(const QJsonObject &payload)
{
    if (payload.isEmpty()) {
        return {QStringLiteral("invalid"), QStringLiteral("License file is not valid.")};
    }
    if (!verifySignature(payload)) {
        return {
            QStringLiteral("bad_signature"),
            QStringLiteral("License signature is invalid (not issued by the vendor, or edited).")};
    }
    if (!signalsMatch(payload.value(QStringLiteral("signals")).toObject())) {
        return {QStringLiteral("wrong_machine"),
                QStringLiteral("This license is for a different computer.")};
    }
    const QString expiry = payload.value(QStringLiteral("expiry")).toString().trimmed();
    if (!expiry.isEmpty()) {
        const QDate exp = QDate::fromString(expiry, Qt::ISODate);
        if (exp.isValid() && effectiveToday() > exp) {
            return {QStringLiteral("expired"),
                    QStringLiteral("This license expired on %1.").arg(expiry)};
        }
    }
    return {QStringLiteral("ok"), QStringLiteral("Licensed.")};
}

QString currentCode()
{
    return MachineFingerprint::fingerprintCode();
}

QString buildRequest()
{
    QJsonObject signals_;
    const QMap<QString, QString> sig = MachineFingerprint::collectSignals();
    for (auto it = sig.constBegin(); it != sig.constEnd(); ++it) {
        signals_.insert(it.key(), it.value());
    }
    char host[256] = {0};
    const QString hostName
        = (gethostname(host, sizeof(host) - 1) == 0) ? QString::fromUtf8(host).left(64) : QString();
    QJsonObject payload;
    payload.insert(QStringLiteral("v"), 1);
    payload.insert(QStringLiteral("host"), hostName);
    payload.insert(QStringLiteral("signals"), signals_);
    const QByteArray raw = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    return QString::fromLatin1(raw.toBase64());
}

QJsonObject parseLicenseText(const QString &text, bool *ok)
{
    const auto fail = [&]() {
        if (ok) {
            *ok = false;
        }
        return QJsonObject();
    };
    const QString t = text.trimmed();
    if (t.isEmpty()) {
        return fail();
    }
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(t.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        if (ok) {
            *ok = true;
        }
        return doc.object();
    }
    // base64-wrapped JSON.
    const QByteArray decoded = QByteArray::fromBase64(t.toLatin1());
    doc = QJsonDocument::fromJson(decoded, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        if (ok) {
            *ok = true;
        }
        return doc.object();
    }
    return fail();
}

CheckResult check()
{
    CheckResult res;
    QFile f(licensePath());
    if (!f.exists()) {
        res.state = QStringLiteral("unactivated");
        return res;
    }
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        res.state = QStringLiteral("invalid");
        return res;
    }
    bool parsed = false;
    const QJsonObject payload = parseLicenseText(QString::fromUtf8(f.readAll()), &parsed);
    if (!parsed) {
        res.state = QStringLiteral("invalid");
        return res;
    }
    res.payload = payload;
    res.hasPayload = true;
    res.state = evaluate(payload).state;
    if (res.state == QLatin1String("ok")) {
        recordSeen();
    }
    return res;
}

InstallResult installLicense(const QString &text)
{
    bool parsed = false;
    const QJsonObject payload = parseLicenseText(text, &parsed);
    if (!parsed) {
        return {false, QStringLiteral("That doesn't look like a license file.")};
    }
    const Evaluation ev = evaluate(payload);
    if (ev.state != QLatin1String("ok")) {
        return {false, ev.message};
    }
    AppPaths::ensureDirs();
    QFile f(licensePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return {false, QStringLiteral("Could not save the license file.")};
    }
    f.write(QJsonDocument(payload).toJson(QJsonDocument::Compact));
    f.close();
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    recordSeen();
    return {true, QStringLiteral("Activated.")};
}

QJsonObject licenseInfo()
{
    return check().payload;
}

bool verifyDetached(const QString &publicKeyB64Arg, const QJsonObject &payload)
{
    const QJsonValue sig = payload.value(QStringLiteral("sig"));
    if (!sig.isString()) {
        return false;
    }
    return ed25519Verify(QByteArray::fromBase64(publicKeyB64Arg.trimmed().toLatin1()),
                         canonicalPayload(payload),
                         QByteArray::fromBase64(sig.toString().toLatin1()));
}

void setPublicKeyForTesting(const QString &publicKeyB64Arg)
{
    g_publicKeyB64 = publicKeyB64Arg;
}

} // namespace Licensing
