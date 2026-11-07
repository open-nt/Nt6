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

/*!
    \class QHostAddress
    \brief The QHostAddress class provides an IP address.

    QHostAddress holds an IPv4 or IPv6 address, or one of the special values
    in SpecialAddress. It does not perform name lookups; use
    QAbstractSocket::connectToHost() with a host name for that.

    \code
    QHostAddress a("192.168.0.1");
    quint32 ip = a.toIPv4Address();
    QHostAddress any(QHostAddress::Any);
    \endcode

    \differences
    Scope IDs are not stored: a \c %zone suffix is ignored by setAddress().
    There is no \c parseSubnet(), \c isInSubnet(), \c scopeId(),
    \c isUniqueLocalUnicast(), \c isSiteLocal() or \c isBroadcast().
    toIPv4Address() and toString() are the only conversions, and there is no
    QDataStream or QDebug support.
    \enddifferences

    \sa QAbstractSocket, QNetworkDatagram
*/

/*!
    \enum QHostAddress::SpecialAddress

    This enum describes address types with a special meaning.

    \value Null The null address object. Equivalent to QHostAddress().
    \value Broadcast The IPv4 broadcast address, 255.255.255.255.
    \value LocalHost The IPv4 localhost address, 127.0.0.1.
    \value LocalHostIPv6 The IPv6 localhost address, \c ::1.
    \value Any The dual-stack any-address. A socket bound to it listens on all IPv4 and IPv6 interfaces.
    \value AnyIPv6 The IPv6 any-address, \c ::. A socket bound to it listens on IPv6 interfaces only.
    \value AnyIPv4 The IPv4 any-address, 0.0.0.0. A socket bound to it listens on IPv4 interfaces only.
*/

/*!
    \enum QHostAddress::ConversionModeFlag

    This enum describes the conversions that isEqual() may apply before
    comparing two addresses.

    \value ConvertV4MappedToIPv4 Treat IPv4-mapped IPv6 addresses (\c ::ffff:a.b.c.d) as the equivalent IPv4 address.
    \value ConvertV4CompatToIPv4 Declared for source compatibility; not used.
    \value ConvertUnspecifiedAddress Declared for source compatibility; not used.
    \value ConvertLocalHost Treat any two loopback addresses as equal.
    \value TolerantConversion Enables every conversion flag.
    \value StrictConversion No conversion; the addresses must be identical.

    \sa ConversionMode, isEqual()
*/

/*!
    \typedef QHostAddress::ConversionMode

    Stores an OR combination of ConversionModeFlag values.
*/

/*!
    Constructs an IPv6 address from the 16 bytes at \a ip6Addr, in network
    byte order.
*/
QHostAddress::QHostAddress(const quint8 *ip6Addr) { setAddress(ip6Addr); }

/*!
    Constructs an IPv6 address from \a ip6Addr.
*/
QHostAddress::QHostAddress(const Q_IPV6ADDR &ip6Addr) { setAddress(ip6Addr); }

/*!
    Sets the address to the IPv4 address \a ip4Addr, given in host byte
    order.
*/
void QHostAddress::setAddress(quint32 ip4Addr) {
    m_kind = KindIPv4;
    std::memset(m_bytes, 0, sizeof m_bytes);
    m_bytes[0] = static_cast<quint8>(ip4Addr >> 24);
    m_bytes[1] = static_cast<quint8>(ip4Addr >> 16);
    m_bytes[2] = static_cast<quint8>(ip4Addr >> 8);
    m_bytes[3] = static_cast<quint8>(ip4Addr);
}

/*!
    \overload

    Sets the address to the IPv6 address given by the 16 bytes at
    \a ip6Addr, in network byte order.
*/
void QHostAddress::setAddress(const quint8 *ip6Addr) {
    m_kind = KindIPv6;
    std::memcpy(m_bytes, ip6Addr, 16);
}

/*!
    \overload

    Sets the address to the IPv6 address \a ip6Addr.
*/
void QHostAddress::setAddress(const Q_IPV6ADDR &ip6Addr) { setAddress(ip6Addr.c); }

/*!
    \overload

    Parses \a address as a dotted-decimal IPv4 or textual IPv6 address,
    ignoring surrounding whitespace and any \c %scope suffix. Returns
    \c true on success. On failure the address is cleared to a null address
    and the function returns \c false.
*/
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

/*!
    \overload

    Sets the address to the special address \a address.
*/
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

/*!
    Returns the network layer protocol of the address: IPv4Protocol,
    IPv6Protocol, AnyIPProtocol for QHostAddress::Any, or
    UnknownNetworkLayerProtocol for a null address.
*/
QAbstractSocket::NetworkLayerProtocol QHostAddress::protocol() const noexcept {
    switch (m_kind) {
    case KindIPv4: return QAbstractSocket::IPv4Protocol;
    case KindIPv6: return QAbstractSocket::IPv6Protocol;
    case KindAnyDual: return QAbstractSocket::AnyIPProtocol;
    default: return QAbstractSocket::UnknownNetworkLayerProtocol;
    }
}

/*!
    Returns the IPv4 address as a host-byte-order number. IPv4-mapped IPv6
    addresses are converted. If \a ok is not null, it is set to \c true on
    success and \c false if the address has no IPv4 form, in which case 0 is
    returned.

    \sa toIPv6Address(), toString()
*/
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

/*!
    \fn Q_IPV6ADDR QHostAddress::toIPv6Address() const

    Returns the address as an IPv6 address. An IPv4 address is returned in
    its IPv4-mapped form (\c ::ffff:a.b.c.d); a null address yields all
    zeros.

    \sa toIPv4Address(), toString()
*/
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

/*!
    Returns the address as a string, in dotted-decimal form for IPv4 or in
    the usual compressed form for IPv6. Returns a null string for a null
    address.

    \sa setAddress()
*/
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

/*!
    Returns \c true if the address is an IPv4 address in 127.0.0.0/8, or the
    IPv6 loopback address (or its IPv4-mapped form); otherwise returns
    \c false.
*/
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

/*!
    Returns \c true if the address is an IPv4 multicast address (224.0.0.0/4)
    or an IPv6 multicast address (\c ff00::/8); otherwise returns \c false.
*/
bool QHostAddress::isMulticast() const {
    if (m_kind == KindIPv4)
        return (m_bytes[0] & 0xf0) == 0xe0;
    return m_kind == KindIPv6 && m_bytes[0] == 0xff;
}

/*!
    Returns \c true if the address is an IPv4 link-local address
    (169.254.0.0/16) or an IPv6 link-local address (\c fe80::/10); otherwise
    returns \c false.
*/
bool QHostAddress::isLinkLocal() const {
    if (m_kind == KindIPv4)
        return m_bytes[0] == 169 && m_bytes[1] == 254;
    return m_kind == KindIPv6 && m_bytes[0] == 0xfe && (m_bytes[1] & 0xc0) == 0x80;
}

/*!
    Returns \c true if the address is a globally routable unicast address,
    that is, not loopback, private, link-local, multicast or unspecified;
    otherwise returns \c false.
*/
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

/*!
    Returns \c true if this address is identical to \a o; otherwise returns
    \c false. An IPv4 address does not equal its IPv4-mapped IPv6 form; use
    isEqual() for that.
*/
bool QHostAddress::operator==(const QHostAddress &o) const noexcept {
    return m_kind == o.m_kind && std::memcmp(m_bytes, o.m_bytes, 16) == 0;
}

/*!
    Returns \c true if this address is equal to \a other after applying the
    conversions enabled in \a mode; otherwise returns \c false.
*/
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
