// MIT License - see LICENSE
#include <arpa/inet.h>
#include <netinet/in.h>

#include <QtNetwork/qhostaddress.h>

#include <cstring>

namespace {
bool isV4Mapped(const quint8 *b) {
    static const quint8 prefix[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
    return std::memcmp(b, prefix, 12) == 0;
}
} // namespace

QHostAddress::QHostAddress(const quint8 *ip6Addr) { setAddress(ip6Addr); }
QHostAddress::QHostAddress(const Q_IPV6ADDR &ip6Addr) { setAddress(ip6Addr); }

void QHostAddress::setAddress(quint32 ip4Addr) {
    m_kind = KindIPv4;
    std::memset(m_bytes, 0, sizeof m_bytes);
    m_bytes[0] = static_cast<quint8>(ip4Addr >> 24);
    m_bytes[1] = static_cast<quint8>(ip4Addr >> 16);
    m_bytes[2] = static_cast<quint8>(ip4Addr >> 8);
    m_bytes[3] = static_cast<quint8>(ip4Addr);
}

void QHostAddress::setAddress(const quint8 *ip6Addr) {
    m_kind = KindIPv6;
    std::memcpy(m_bytes, ip6Addr, 16);
}

void QHostAddress::setAddress(const Q_IPV6ADDR &ip6Addr) { setAddress(ip6Addr.c); }

bool QHostAddress::setAddress(const QString &address) {
    clear();
    std::string s = address.trimmed().toStdString();
    const auto pct = s.find('%');
    if (pct != std::string::npos)
        s.erase(pct);
    unsigned char buf[16];
    if (!s.empty() && inet_pton(AF_INET, s.c_str(), buf) == 1) {
        m_kind = KindIPv4;
        std::memcpy(m_bytes, buf, 4);
        return true;
    }
    if (!s.empty() && inet_pton(AF_INET6, s.c_str(), buf) == 1) {
        m_kind = KindIPv6;
        std::memcpy(m_bytes, buf, 16);
        return true;
    }
    return false;
}

void QHostAddress::setAddress(SpecialAddress address) {
    std::memset(m_bytes, 0, sizeof m_bytes);
    switch (address) {
    case Null:
        m_kind = KindNull;
        break;
    case Broadcast:
        setAddress(0xffffffffu);
        break;
    case LocalHost:
        setAddress(0x7f000001u);
        break;
    case LocalHostIPv6:
        m_kind = KindIPv6;
        m_bytes[15] = 1;
        break;
    case Any:
        m_kind = KindAnyDual;
        break;
    case AnyIPv6:
        m_kind = KindIPv6;
        break;
    case AnyIPv4:
        setAddress(0u);
        break;
    }
}

QAbstractSocket::NetworkLayerProtocol QHostAddress::protocol() const noexcept {
    switch (m_kind) {
    case KindIPv4: return QAbstractSocket::IPv4Protocol;
    case KindIPv6: return QAbstractSocket::IPv6Protocol;
    case KindAnyDual: return QAbstractSocket::AnyIPProtocol;
    default: return QAbstractSocket::UnknownNetworkLayerProtocol;
    }
}

quint32 QHostAddress::toIPv4Address(bool *ok) const {
    const quint8 *b = nullptr;
    if (m_kind == KindIPv4)
        b = m_bytes;
    else if (m_kind == KindIPv6 && isV4Mapped(m_bytes))
        b = m_bytes + 12;
    if (ok)
        *ok = b != nullptr;
    if (!b)
        return 0;
    return (static_cast<quint32>(b[0]) << 24) | (static_cast<quint32>(b[1]) << 16) | (static_cast<quint32>(b[2]) << 8) | b[3];
}

Q_IPV6ADDR QHostAddress::toIPv6Address() const {
    Q_IPV6ADDR r;
    if (m_kind == KindIPv6 || m_kind == KindAnyDual) {
        std::memcpy(r.c, m_bytes, 16);
    } else if (m_kind == KindIPv4) {
        r.c[10] = r.c[11] = 0xff;
        std::memcpy(r.c + 12, m_bytes, 4);
    }
    return r;
}

QString QHostAddress::toString() const {
    char buf[INET6_ADDRSTRLEN] = {};
    switch (m_kind) {
    case KindIPv4:
        inet_ntop(AF_INET, m_bytes, buf, sizeof buf);
        return QString::fromUtf8(buf);
    case KindIPv6:
    case KindAnyDual:
        inet_ntop(AF_INET6, m_bytes, buf, sizeof buf);
        return QString::fromUtf8(buf);
    default:
        return QString();
    }
}

bool QHostAddress::isLoopback() const {
    if (m_kind == KindIPv4)
        return m_bytes[0] == 127;
    if (m_kind == KindIPv6) {
        if (isV4Mapped(m_bytes))
            return m_bytes[12] == 127;
        static const quint8 loop[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
        return std::memcmp(m_bytes, loop, 16) == 0;
    }
    return false;
}

bool QHostAddress::isMulticast() const {
    if (m_kind == KindIPv4)
        return (m_bytes[0] & 0xf0) == 0xe0;
    return m_kind == KindIPv6 && m_bytes[0] == 0xff;
}

bool QHostAddress::isLinkLocal() const {
    if (m_kind == KindIPv4)
        return m_bytes[0] == 169 && m_bytes[1] == 254;
    return m_kind == KindIPv6 && m_bytes[0] == 0xfe && (m_bytes[1] & 0xc0) == 0x80;
}

bool QHostAddress::isGlobal() const {
    if (m_kind == KindIPv4) {
        const quint8 a = m_bytes[0], b = m_bytes[1];
        return !(a == 0 || a == 10 || a == 127 || (a == 172 && (b & 0xf0) == 16) || (a == 192 && b == 168) ||
                 (a == 169 && b == 254) || a >= 224);
    }
    if (m_kind == KindIPv6)
        return !isLoopback() && !isLinkLocal() && !isMulticast() && (m_bytes[0] & 0xfe) != 0xfc;
    return false;
}

bool QHostAddress::operator==(const QHostAddress &o) const noexcept {
    return m_kind == o.m_kind && std::memcmp(m_bytes, o.m_bytes, 16) == 0;
}

bool QHostAddress::isEqual(const QHostAddress &other, ConversionMode mode) const {
    if (*this == other)
        return true;
    if (mode.testFlag(ConvertV4MappedToIPv4)) {
        bool ok1 = false, ok2 = false;
        const quint32 a = toIPv4Address(&ok1), b = other.toIPv4Address(&ok2);
        if (ok1 && ok2 && a == b)
            return true;
    }
    if (mode.testFlag(ConvertLocalHost) && isLoopback() && other.isLoopback())
        return true;
    return false;
}
