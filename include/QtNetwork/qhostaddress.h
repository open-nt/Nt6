// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>
#include <QtNetwork/qabstractsocket.h>

/*!
    \class QIPv6Address
    \brief The QIPv6Address class stores an IPv6 address in a 16-byte array.

    The bytes are in network byte order. Application code usually refers to
    this type through the \c Q_IPV6ADDR alias.

    \differences
    There is no \c operator==() or \c operator!=(); compare the bytes or the
    owning QHostAddress objects instead.
    \enddifferences

    \sa QHostAddress
*/
struct QIPv6Address {
    /*!
        The 16 bytes of the address, in network byte order.
    */
    quint8 c[16] = {};
    /*!
        Returns a reference to the byte at index \a i, from 0 to 15.
    */
    quint8 &operator[](int i) { return c[i]; }
    /*!
        \overload
    */
    quint8 operator[](int i) const { return c[i]; }
};
using Q_IPV6ADDR = QIPv6Address;

class QHostAddress {
public:
    enum SpecialAddress { Null, Broadcast, LocalHost, LocalHostIPv6, Any, AnyIPv6, AnyIPv4 };
    enum ConversionModeFlag {
        ConvertV4MappedToIPv4 = 1,
        ConvertV4CompatToIPv4 = 2,
        ConvertUnspecifiedAddress = 4,
        ConvertLocalHost = 8,
        TolerantConversion = 0xff,
        StrictConversion = 0
    };
    Q_DECLARE_FLAGS(ConversionMode, ConversionModeFlag)

    /*!
        Constructs a null address.

        \sa isNull()
    */
    QHostAddress() = default;
    /*!
        Constructs an IPv4 address from \a ip4Addr, given in host byte order.
    */
    QHostAddress(quint32 ip4Addr) { setAddress(ip4Addr); }
    QHostAddress(const quint8 *ip6Addr);
    QHostAddress(const Q_IPV6ADDR &ip6Addr);
    /*!
        Constructs an address by parsing \a address. If parsing fails, the
        address is null.

        \sa setAddress()
    */
    QHostAddress(const QString &address) { setAddress(address); }
    /*!
        Constructs the special address \a address.
    */
    QHostAddress(SpecialAddress address) { setAddress(address); }

    void setAddress(quint32 ip4Addr);
    void setAddress(const quint8 *ip6Addr);
    void setAddress(const Q_IPV6ADDR &ip6Addr);
    bool setAddress(const QString &address);
    void setAddress(SpecialAddress address);
    /*!
        Sets the address to the null address.

        \sa isNull()
    */
    void clear() { *this = QHostAddress(); }

    QAbstractSocket::NetworkLayerProtocol protocol() const noexcept;
    quint32 toIPv4Address(bool *ok = nullptr) const;
    Q_IPV6ADDR toIPv6Address() const;
    QString toString() const;

    /*!
        Returns \c true if the address is null, that is, it holds no address.

        \sa clear()
    */
    bool isNull() const noexcept { return m_kind == KindNull; }
    bool isLoopback() const;
    bool isMulticast() const;
    bool isLinkLocal() const;
    bool isGlobal() const;
    bool isEqual(const QHostAddress &other, ConversionMode mode = TolerantConversion) const;

    bool operator==(const QHostAddress &o) const noexcept;
    /*!
        Returns \c true if this address differs from \a o; otherwise returns
        \c false.
    */
    bool operator!=(const QHostAddress &o) const noexcept { return !(*this == o); }
    /*!
        \overload

        Returns \c true if this address is the special address \a s.
    */
    bool operator==(SpecialAddress s) const { return *this == QHostAddress(s); }
    /*!
        \overload

        Returns \c true if this address is not the special address \a s.
    */
    bool operator!=(SpecialAddress s) const { return !(*this == s); }

    // Raw access used by the socket layer: kind is 4, 6 or 7 (dual-stack any).
    /*!
        \internal
    */
    int kind() const noexcept { return m_kind; }
    /*!
        \internal
    */
    const quint8 *bytes() const noexcept { return m_bytes; }

private:
    enum { KindNull = 0, KindIPv4 = 4, KindIPv6 = 6, KindAnyDual = 7 };
    int m_kind = KindNull;
    quint8 m_bytes[16] = {};
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QHostAddress::ConversionMode)

inline std::size_t qHash(const QHostAddress &a, std::size_t seed = 0) noexcept {
    std::size_t h = seed ^ static_cast<std::size_t>(a.kind());
    for (int i = 0; i < 16; ++i)
        h = h * 131 + a.bytes()[i];
    return h;
}
