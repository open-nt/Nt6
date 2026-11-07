// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>
#include <QtNetwork/qabstractsocket.h>

struct QIPv6Address {
    quint8 c[16] = {};
    quint8 &operator[](int i) { return c[i]; }
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

    QHostAddress() = default;
    QHostAddress(quint32 ip4Addr) { setAddress(ip4Addr); }
    QHostAddress(const quint8 *ip6Addr);
    QHostAddress(const Q_IPV6ADDR &ip6Addr);
    QHostAddress(const QString &address) { setAddress(address); }
    QHostAddress(SpecialAddress address) { setAddress(address); }

    void setAddress(quint32 ip4Addr);
    void setAddress(const quint8 *ip6Addr);
    void setAddress(const Q_IPV6ADDR &ip6Addr);
    bool setAddress(const QString &address);
    void setAddress(SpecialAddress address);
    void clear() { *this = QHostAddress(); }

    QAbstractSocket::NetworkLayerProtocol protocol() const noexcept;
    quint32 toIPv4Address(bool *ok = nullptr) const;
    Q_IPV6ADDR toIPv6Address() const;
    QString toString() const;

    bool isNull() const noexcept { return m_kind == KindNull; }
    bool isLoopback() const;
    bool isMulticast() const;
    bool isLinkLocal() const;
    bool isGlobal() const;
    bool isEqual(const QHostAddress &other, ConversionMode mode = TolerantConversion) const;

    bool operator==(const QHostAddress &o) const noexcept;
    bool operator!=(const QHostAddress &o) const noexcept { return !(*this == o); }
    bool operator==(SpecialAddress s) const { return *this == QHostAddress(s); }
    bool operator!=(SpecialAddress s) const { return !(*this == s); }

    // Raw access used by the socket layer: kind is 4, 6 or 7 (dual-stack any).
    int kind() const noexcept { return m_kind; }
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
