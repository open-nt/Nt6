// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qcryptographichash.h>
#include <QtCore/qstring.h>

namespace QtPrivate {

// The name of the system object for a key: a slash, the prefix, the letters and digits of the key so that the
// name can be recognized, and a hash of the whole key so that different keys give different names. The name is
// short enough for every system.
inline QByteArray ipcName(const char *prefix, const QString &key) {
    if (key.isEmpty())
        return QByteArray();
    const QByteArray utf8 = key.toUtf8();
    QByteArray readable;
    for (const char c : utf8) {
        if (readable.size() < 40 && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            readable.append(c);
    }
    return QByteArray("/") + prefix + readable + QCryptographicHash::hash(utf8, QCryptographicHash::Sha1).toHex();
}

} // namespace QtPrivate
