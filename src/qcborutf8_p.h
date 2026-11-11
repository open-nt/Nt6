// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>

namespace QtPrivate {

// Strict UTF-8: no overlong forms, no surrogates, nothing above U+10FFFF.
inline bool isValidUtf8(const QByteArray &bytes) {
    const auto *p = reinterpret_cast<const unsigned char *>(bytes.constData());
    const qsizetype n = bytes.size();
    qsizetype i = 0;
    while (i < n) {
        const unsigned char c = p[i];
        if (c < 0x80) {
            ++i;
            continue;
        }
        qsizetype extra = 0;
        char32_t code = 0, lowest = 0;
        if ((c & 0xe0) == 0xc0) {
            extra = 1;
            code = c & 0x1f;
            lowest = 0x80;
        } else if ((c & 0xf0) == 0xe0) {
            extra = 2;
            code = c & 0x0f;
            lowest = 0x800;
        } else if ((c & 0xf8) == 0xf0) {
            extra = 3;
            code = c & 0x07;
            lowest = 0x10000;
        } else {
            return false;
        }
        if (i + extra >= n)
            return false;
        for (qsizetype k = 1; k <= extra; ++k) {
            if ((p[i + k] & 0xc0) != 0x80)
                return false;
            code = (code << 6) | (p[i + k] & 0x3f);
        }
        if (code < lowest || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
            return false;
        i += extra + 1;
    }
    return true;
}

} // namespace QtPrivate
