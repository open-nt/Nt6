// MIT License - see LICENSE
#include "qdbusmarshaller_p.h"

#include <algorithm>
#include <cstring>

// Names, signatures and the check that received data is well formed.
namespace QDBusUtil {

namespace {

bool isElementChar(ushort c, bool allowDash, bool allowDigit) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (allowDigit && c >= '0' && c <= '9') ||
           (allowDash && c == '-');
}

// Dot-separated elements, at least `minElements` of them, each with the given characters.
bool validDotted(const QString &name, int start, int minElements, bool allowDash, bool leadingDigit) {
    int elements = 0;
    int length = 0;
    for (qsizetype i = start; i < name.size(); ++i) {
        const ushort c = name.at(i).unicode();
        if (c == '.') {
            if (!length)
                return false;
            ++elements;
            length = 0;
            continue;
        }
        if (!isElementChar(c, allowDash, length > 0 || leadingDigit))
            return false;
        ++length;
    }
    return length > 0 && elements + 1 >= minElements;
}

bool isBasicCode(char c) {
    switch (c) {
    case 'y':
    case 'b':
    case 'n':
    case 'q':
    case 'i':
    case 'u':
    case 'x':
    case 't':
    case 'd':
    case 's':
    case 'o':
    case 'g':
    case 'h':
        return true;
    default:
        return false;
    }
}

int skipTypeAt(const char *s, int size, int pos, int arrays, int structs) {
    if (pos >= size)
        return -1;
    const char c = s[pos];
    if (isBasicCode(c) || c == 'v')
        return pos + 1;
    if (c == 'a') {
        if (arrays >= kMaxArrayDepth)
            return -1;
        if (pos + 1 < size && s[pos + 1] == '{') {
            int p = pos + 2;
            if (p >= size || !isBasicCode(s[p]))
                return -1;
            p = skipTypeAt(s, size, p + 1, arrays + 1, structs + 1);
            return p < 0 || p >= size || s[p] != '}' ? -1 : p + 1;
        }
        return skipTypeAt(s, size, pos + 1, arrays + 1, structs);
    }
    if (c == '(') {
        if (structs >= kMaxStructDepth)
            return -1;
        int p = pos + 1;
        if (p >= size || s[p] == ')')
            return -1;
        while (p >= 0 && p < size && s[p] != ')')
            p = skipTypeAt(s, size, p, arrays, structs + 1);
        return p < 0 || p >= size ? -1 : p + 1;
    }
    return -1;
}

bool validUtf8(const char *s, int size) {
    int i = 0;
    while (i < size) {
        const auto c = static_cast<uchar>(s[i]);
        int extra;
        uint min, cp;
        if (c < 0x80) {
            ++i;
            continue;
        }
        if ((c & 0xe0) == 0xc0) {
            extra = 1;
            min = 0x80;
            cp = c & 0x1fU;
        } else if ((c & 0xf0) == 0xe0) {
            extra = 2;
            min = 0x800;
            cp = c & 0x0fU;
        } else if ((c & 0xf8) == 0xf0) {
            extra = 3;
            min = 0x10000;
            cp = c & 0x07U;
        } else {
            return false;
        }
        if (i + extra >= size)
            return false;
        for (int k = 1; k <= extra; ++k) {
            const auto n = static_cast<uchar>(s[i + k]);
            if ((n & 0xc0) != 0x80)
                return false;
            cp = (cp << 6) | (n & 0x3fU);
        }
        if (cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
            return false;
        i += extra + 1;
    }
    return true;
}

class Validator {
public:
    Validator(const QByteArray &data, const QByteArray &signature, bool swapped, int unixFds)
        : m_d(data.constData()), m_size(static_cast<int>(data.size())), m_sig(signature), m_swapped(swapped),
          m_fds(unixFds) {}

    bool values(int &pos, const QByteArray &signature, int depth) {
        int s = 0;
        while (s < signature.size())
            if (!value(pos, signature, s, depth))
                return false;
        return true;
    }
    bool run(int pos) { return values(pos, m_sig, 0); }

private:
    bool pad(int &pos, int n) const {
        const int aligned = (pos + n - 1) / n * n;
        if (aligned > m_size)
            return false;
        for (int i = pos; i < aligned; ++i)
            if (m_d[i])
                return false;
        pos = aligned;
        return true;
    }
    bool u32(int &pos, uint &v) const {
        if (!pad(pos, 4) || m_size - pos < 4)
            return false;
        std::memcpy(&v, m_d + pos, 4);
        if (m_swapped)
            std::reverse(reinterpret_cast<char *>(&v), reinterpret_cast<char *>(&v) + 4);
        pos += 4;
        return true;
    }
    bool text(int &pos, uint size) const {
        if (size >= static_cast<uint>(m_size) || static_cast<uint>(m_size - pos) < size + 1)
            return false;
        const char *s = m_d + pos;
        if (s[size] != 0 || std::memchr(s, 0, size) || !validUtf8(s, static_cast<int>(size)))
            return false;
        pos += static_cast<int>(size) + 1;
        return true;
    }

    bool value(int &pos, const QByteArray &sig, int &s, int depth) {
        if (depth > kMaxTotalDepth)
            return false;
        const char c = sig.at(s);
        switch (c) {
        case 'y':
            if (m_size - pos < 1)
                return false;
            ++pos;
            ++s;
            return true;
        case 'n':
        case 'q':
            if (!pad(pos, 2) || m_size - pos < 2)
                return false;
            pos += 2;
            ++s;
            return true;
        case 'b':
        case 'i':
        case 'u':
        case 'h': {
            uint v = 0;
            if (!u32(pos, v) || (c == 'b' && v > 1) || (c == 'h' && v >= static_cast<uint>(m_fds)))
                return false;
            ++s;
            return true;
        }
        case 'x':
        case 't':
        case 'd':
            if (!pad(pos, 8) || m_size - pos < 8)
                return false;
            pos += 8;
            ++s;
            return true;
        case 's':
        case 'o': {
            uint size = 0;
            if (!u32(pos, size))
                return false;
            const int from = pos;
            if (!text(pos, size))
                return false;
            if (c == 'o' && !isValidObjectPath(QString::fromLatin1(m_d + from, static_cast<qsizetype>(size))))
                return false;
            ++s;
            return true;
        }
        case 'g': {
            if (m_size - pos < 1)
                return false;
            const auto size = static_cast<uchar>(m_d[pos]);
            ++pos;
            const int from = pos;
            if (!text(pos, size))
                return false;
            if (!isValidSignature(QByteArray(m_d + from, size)))
                return false;
            ++s;
            return true;
        }
        case 'v': {
            if (m_size - pos < 1)
                return false;
            const auto size = static_cast<uchar>(m_d[pos]);
            ++pos;
            const int from = pos;
            if (!text(pos, size))
                return false;
            const QByteArray inner(m_d + from, size);
            if (!isValidSingleSignature(inner))
                return false;
            ++s;
            return values(pos, inner, depth + 1);
        }
        case '(': {
            if (!pad(pos, 8))
                return false;
            ++s;
            while (s < sig.size() && sig.at(s) != ')')
                if (!value(pos, sig, s, depth + 1))
                    return false;
            ++s;
            return true;
        }
        case '{': {
            if (!pad(pos, 8))
                return false;
            ++s;
            while (s < sig.size() && sig.at(s) != '}')
                if (!value(pos, sig, s, depth + 1))
                    return false;
            ++s;
            return true;
        }
        case 'a': {
            uint size = 0;
            if (!u32(pos, size) || size > static_cast<uint>(kMaxArrayBytes))
                return false;
            const int element = s + 1;
            const int after = skipType(sig, s);
            if (after < 0 || !pad(pos, alignmentOf(sig.at(element))) || static_cast<uint>(m_size - pos) < size)
                return false;
            const int end = pos + static_cast<int>(size);
            while (pos < end) {
                int e = element;
                if (!value(pos, sig, e, depth + 1))
                    return false;
            }
            if (pos != end)
                return false;
            s = after;
            return true;
        }
        default:
            return false;
        }
    }

    const char *m_d;
    int m_size;
    QByteArray m_sig;
    bool m_swapped;
    int m_fds;
};

} // namespace

bool isValidObjectPath(const QString &path) {
    if (path == QLatin1String("/"))
        return true;
    if (!path.startsWith(QLatin1Char('/')) || path.endsWith(QLatin1Char('/')) || path.size() > 0xfffffff)
        return false;
    bool empty = true;
    for (qsizetype i = 1; i < path.size(); ++i) {
        const ushort c = path.at(i).unicode();
        if (c == '/') {
            if (empty)
                return false;
            empty = true;
        } else {
            if (!isElementChar(c, false, true))
                return false;
            empty = false;
        }
    }
    return !empty;
}

bool isValidInterfaceName(const QString &name) { return name.size() <= 255 && validDotted(name, 0, 2, false, false); }

bool isValidErrorName(const QString &name) { return isValidInterfaceName(name); }

bool isValidMemberName(const QString &name) {
    if (name.isEmpty() || name.size() > 255)
        return false;
    for (qsizetype i = 0; i < name.size(); ++i)
        if (!isElementChar(name.at(i).unicode(), false, i > 0))
            return false;
    return true;
}

bool isValidUniqueConnectionName(const QString &name) {
    return name.size() <= 255 && name.startsWith(QLatin1Char(':')) && validDotted(name, 1, 2, true, true);
}

bool isValidBusName(const QString &name) {
    if (name.startsWith(QLatin1Char(':')))
        return isValidUniqueConnectionName(name);
    return name.size() <= 255 && validDotted(name, 0, 2, true, false);
}

int skipType(const QByteArray &signature, int pos) {
    return skipTypeAt(signature.constData(), static_cast<int>(signature.size()), pos, 0, 0);
}

bool isValidSignature(const QByteArray &signature) {
    if (signature.size() > kMaxSignature)
        return false;
    int pos = 0;
    while (pos < signature.size()) {
        pos = skipType(signature, pos);
        if (pos < 0)
            return false;
    }
    return true;
}

bool isValidSingleSignature(const QByteArray &signature) {
    return signature.size() <= kMaxSignature && !signature.isEmpty() && skipType(signature, 0) == signature.size();
}

int alignmentOf(char typeCode) {
    switch (typeCode) {
    case 'n':
    case 'q':
        return 2;
    case 'b':
    case 'i':
    case 'u':
    case 'h':
    case 's':
    case 'o':
    case 'a':
        return 4;
    case 'x':
    case 't':
    case 'd':
    case '(':
    case '{':
        return 8;
    default:
        return 1;
    }
}

bool validateValues(const QByteArray &data, int pos, const QByteArray &signature, bool swapped, int unixFds) {
    if (!isValidSignature(signature))
        return false;
    return Validator(data, signature, swapped, unixFds).run(pos);
}

} // namespace QDBusUtil
