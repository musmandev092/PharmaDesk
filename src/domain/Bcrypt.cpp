#include "domain/Bcrypt.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <crypt.h>

#include <QByteArray>

namespace Bcrypt {

QString hash(const QString &plain, int cost)
{
    // Generate a random $2b$ salt at the given cost. crypt_gensalt_rn seeds
    // from the kernel RNG when entropy == NULL/0.
    char saltBuf[64] = {0};
    char *salt = crypt_gensalt_rn("$2b$", static_cast<unsigned long>(cost), nullptr, 0, saltBuf,
                                  sizeof(saltBuf));
    if (!salt) {
        return {};
    }

    crypt_data data;
    data.initialized = 0;
    const QByteArray plainBytes = plain.toUtf8();
    char *result = crypt_r(plainBytes.constData(), salt, &data);
    if (!result || result[0] == '*') { // '*' / "*0" / "*1" signal failure
        return {};
    }
    return QString::fromLatin1(result);
}

bool verify(const QString &plain, const QString &storedHash)
{
    if (storedHash.isEmpty()) {
        return false;
    }
    crypt_data data;
    data.initialized = 0;
    const QByteArray plainBytes = plain.toUtf8();
    const QByteArray hashBytes = storedHash.toLatin1();

    char *result = crypt_r(plainBytes.constData(), hashBytes.constData(), &data);
    if (!result || result[0] == '*') {
        return false;
    }

    // Length-aware, value-comparison. The recomputed hash must equal the stored
    // one exactly. crypt re-derives the same salt/cost from storedHash.
    const QByteArray recomputed(result);
    if (recomputed.size() != hashBytes.size()) {
        return false;
    }
    unsigned char diff = 0;
    for (int i = 0; i < recomputed.size(); ++i) {
        diff |= static_cast<unsigned char>(recomputed[i] ^ hashBytes[i]);
    }
    return diff == 0;
}

} // namespace Bcrypt
