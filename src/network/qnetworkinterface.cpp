// MIT License - see LICENSE
#include <QtNetwork/qnetworkinterface.h>

#include "sockaddr_p.h"

#include <ifaddrs.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

namespace {

QHostAddress toAddress(const sockaddr *sa) {
    if (!sa)
        return QHostAddress();
    sockaddr_storage ss = {};
    std::memcpy(&ss, sa, sa->sa_family == AF_INET6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in));
    return QtPrivate::fromSockaddr(ss, nullptr);
}

int prefixFromMask(const QHostAddress &mask) {
    if (mask.isNull())
        return -1;
    const int bytes = mask.protocol() == QAbstractSocket::IPv4Protocol ? 4 : 16;
    const quint8 *p = mask.bytes();
    const quint32 v4 = bytes == 4 ? mask.toIPv4Address() : 0;
    int bits = 0;
    for (int i = 0; i < bytes; ++i) {
        const quint8 b = bytes == 4 ? static_cast<quint8>(v4 >> (24 - 8 * i)) : p[i];
        for (int k = 7; k >= 0 && (b >> k & 1); --k)
            ++bits;
        if (b != 0xff)
            break;
    }
    return bits;
}

QHostAddress maskFromPrefix(int length, bool v6) {
    if (v6) {
        Q_IPV6ADDR a;
        for (int i = 0; i < 16; ++i) {
            const int bits = qBound(0, length - i * 8, 8);
            a[i] = static_cast<quint8>(0xff00 >> bits);
        }
        return QHostAddress(a);
    }
    return QHostAddress(length <= 0 ? 0u : 0xffffffffu << (32 - qMin(length, 32)));
}

int interfaceMtu(const char *name) {
    const int fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return 0;
    ifreq req = {};
    std::strncpy(req.ifr_name, name, IFNAMSIZ - 1);
    const int mtu = ioctl(fd, SIOCGIFMTU, &req) == 0 ? req.ifr_mtu : 0;
    ::close(fd);
    return mtu;
}

bool isWireless(const QString &name) {
    struct stat st;
    return ::stat((QString("/sys/class/net/") + name + QString("/wireless")).toUtf8().constData(), &st) == 0;
}

QNetworkInterface::InterfaceFlags toFlags(unsigned f) {
    QNetworkInterface::InterfaceFlags r;
    if (f & IFF_UP) r |= QNetworkInterface::IsUp;
    if (f & IFF_RUNNING) r |= QNetworkInterface::IsRunning;
    if (f & IFF_BROADCAST) r |= QNetworkInterface::CanBroadcast;
    if (f & IFF_LOOPBACK) r |= QNetworkInterface::IsLoopBack;
    if (f & IFF_POINTOPOINT) r |= QNetworkInterface::IsPointToPoint;
    if (f & IFF_MULTICAST) r |= QNetworkInterface::CanMulticast;
    return r;
}

} // namespace

/*!
    \class QNetworkAddressEntry
    \brief The QNetworkAddressEntry class stores one IP address of a network
    interface, with its netmask and broadcast address.

    \differences
    There is no DNS eligibility or address lifetime information.
    \enddifferences

    \sa QNetworkInterface
*/

/*!
    \fn QNetworkAddressEntry::QNetworkAddressEntry()

    Constructs an empty entry.
*/

/*!
    Returns the IP address.

    \sa setIp()
*/
QHostAddress QNetworkAddressEntry::ip() const { return m_ip; }

/*!
    Sets the IP address to \a newIp.

    \sa ip()
*/
void QNetworkAddressEntry::setIp(const QHostAddress &newIp) { m_ip = newIp; }

/*!
    Returns the netmask.

    \sa setNetmask(), prefixLength()
*/
QHostAddress QNetworkAddressEntry::netmask() const { return m_netmask; }

/*!
    Sets the netmask to \a newNetmask.

    \sa netmask()
*/
void QNetworkAddressEntry::setNetmask(const QHostAddress &newNetmask) { m_netmask = newNetmask; }

/*!
    Returns the number of leading one bits in the netmask, or \c -1 if the
    netmask is unset.

    \sa setPrefixLength()
*/
int QNetworkAddressEntry::prefixLength() const { return prefixFromMask(m_netmask); }

/*!
    Sets the netmask from a prefix of \a length bits. The IP address must be set
    first, as it decides between an IPv4 and an IPv6 mask.

    \sa prefixLength(), setIp()
*/
void QNetworkAddressEntry::setPrefixLength(int length) {
    if (!m_ip.isNull())
        m_netmask = maskFromPrefix(length, m_ip.protocol() == QAbstractSocket::IPv6Protocol);
}

/*!
    Returns the broadcast address, which is null for IPv6.

    \sa setBroadcast()
*/
QHostAddress QNetworkAddressEntry::broadcast() const { return m_broadcast; }

/*!
    Sets the broadcast address to \a newBroadcast.

    \sa broadcast()
*/
void QNetworkAddressEntry::setBroadcast(const QHostAddress &newBroadcast) { m_broadcast = newBroadcast; }

/*!
    \class QNetworkInterface
    \brief The QNetworkInterface class lists the host's network interfaces and
    their addresses.

    allInterfaces() returns every interface, and allAddresses() is a shortcut
    for the addresses of all of them.

    \differences
    Linux only for now. Interface data is a snapshot taken when the list is
    requested. Type detection covers loopback, Ethernet, Wi-Fi and PPP.
    \enddifferences

    \sa QNetworkAddressEntry, QHostAddress
*/

/*!
    \enum QNetworkInterface::InterfaceFlag

    Describes the state and abilities of an interface.

    \value IsUp The interface is administratively up.
    \value IsRunning The interface has resources allocated and is operational.
    \value CanBroadcast The interface works in broadcast mode.
    \value IsLoopBack The interface is a loopback interface.
    \value IsPointToPoint The interface is a point-to-point link.
    \value CanMulticast The interface supports multicast.
*/

/*!
    \enum QNetworkInterface::InterfaceType

    Describes the link technology of an interface.

    \value Unknown The type could not be determined.
    \value Loopback A loopback interface.
    \value Virtual A virtual interface.
    \value Ethernet A wired Ethernet interface.
    \value Wifi A wireless interface.
    \value Ppp A point-to-point protocol interface.
*/

/*!
    \fn QNetworkInterface::QNetworkInterface()

    Constructs an invalid interface object.
*/

/*!
    Returns \c true if this object describes an existing interface.
*/
bool QNetworkInterface::isValid() const { return !m_name.isEmpty(); }

/*!
    Returns the system's index for the interface, or \c 0 if it is unknown.
*/
int QNetworkInterface::index() const { return m_index; }

/*!
    Returns the interface's maximum transmission unit in bytes, or \c 0 if it
    is unknown.
*/
int QNetworkInterface::maximumTransmissionUnit() const { return m_mtu; }

/*!
    Returns the interface's name, such as \c eth0.

    \sa humanReadableName()
*/
QString QNetworkInterface::name() const { return m_name; }

/*!
    Returns a name for display. On Linux this is the same as name().
*/
QString QNetworkInterface::humanReadableName() const { return m_name; }

/*!
    Returns the interface's state flags.
*/
QNetworkInterface::InterfaceFlags QNetworkInterface::flags() const { return m_flags; }

/*!
    Returns the interface's link type.
*/
QNetworkInterface::InterfaceType QNetworkInterface::type() const { return m_type; }

/*!
    Returns the hardware address as colon-separated hex, such as
    \c 00:1A:2B:3C:4D:5E, or an empty string if there is none.
*/
QString QNetworkInterface::hardwareAddress() const { return m_hardware; }

/*!
    Returns the interface's IP addresses.
*/
QList<QNetworkAddressEntry> QNetworkInterface::addressEntries() const { return m_entries; }

/*!
    Returns the index of the interface called \a name, or \c 0 if there is none.

    \sa interfaceNameFromIndex()
*/
int QNetworkInterface::interfaceIndexFromName(const QString &name) {
    return name.isEmpty() ? 0 : static_cast<int>(if_nametoindex(name.toUtf8().constData()));
}

/*!
    Returns the interface called \a name, or an invalid object if there is none.

    \sa interfaceFromIndex()
*/
QNetworkInterface QNetworkInterface::interfaceFromName(const QString &name) {
    for (const QNetworkInterface &i : allInterfaces())
        if (i.name() == name)
            return i;
    return QNetworkInterface();
}

/*!
    Returns the interface with the given \a index, or an invalid object if
    there is none.

    \sa interfaceFromName()
*/
QNetworkInterface QNetworkInterface::interfaceFromIndex(int index) {
    for (const QNetworkInterface &i : allInterfaces())
        if (i.index() == index)
            return i;
    return QNetworkInterface();
}

/*!
    Returns the name of the interface with the given \a index, or an empty
    string if there is none.

    \sa interfaceIndexFromName()
*/
QString QNetworkInterface::interfaceNameFromIndex(int index) {
    char name[IF_NAMESIZE] = {};
    return if_indextoname(static_cast<unsigned>(index), name) ? QString::fromUtf8(name) : QString();
}

/*!
    Returns every network interface on the host.

    \sa allAddresses()
*/
QList<QNetworkInterface> QNetworkInterface::allInterfaces() {
    QList<QNetworkInterface> result;
    ifaddrs *list = nullptr;
    if (getifaddrs(&list) != 0)
        return result;

    const auto find = [&](const char *name) -> QNetworkInterface & {
        const QString n = QString::fromUtf8(name);
        for (QNetworkInterface &i : result)
            if (i.m_name == n)
                return i;
        QNetworkInterface fresh;
        fresh.m_name = n;
        fresh.m_index = static_cast<int>(if_nametoindex(name));
        fresh.m_mtu = interfaceMtu(name);
        result.append(fresh);
        return result.last();
    };

    for (ifaddrs *a = list; a; a = a->ifa_next) {
        QNetworkInterface &i = find(a->ifa_name);
        i.m_flags = toFlags(a->ifa_flags);
        if (i.m_flags.testFlag(IsLoopBack))
            i.m_type = Loopback;
        else if (i.m_flags.testFlag(IsPointToPoint))
            i.m_type = Ppp;
        if (!a->ifa_addr)
            continue;
        const int family = a->ifa_addr->sa_family;
        if (family == AF_PACKET) {
            const auto *ll = reinterpret_cast<const sockaddr_ll *>(a->ifa_addr);
            if (ll->sll_hatype == ARPHRD_ETHER && i.m_type == Unknown)
                i.m_type = isWireless(i.m_name) ? Wifi : Ethernet;
            QString mac;
            for (int b = 0; b < ll->sll_halen; ++b) {
                char hex[4];
                std::snprintf(hex, sizeof hex, b ? ":%02X" : "%02X", ll->sll_addr[b]);
                mac.append(QString::fromUtf8(hex));
            }
            i.m_hardware = mac;
        } else if (family == AF_INET || family == AF_INET6) {
            QNetworkAddressEntry entry;
            entry.setIp(toAddress(a->ifa_addr));
            entry.setNetmask(toAddress(a->ifa_netmask));
            if (family == AF_INET && (a->ifa_flags & IFF_BROADCAST))
                entry.setBroadcast(toAddress(a->ifa_broadaddr));
            i.m_entries.append(entry);
        }
    }
    freeifaddrs(list);
    return result;
}

/*!
    Returns the IP addresses of every interface on the host.

    \sa allInterfaces()
*/
QList<QHostAddress> QNetworkInterface::allAddresses() {
    QList<QHostAddress> result;
    for (const QNetworkInterface &i : allInterfaces())
        for (const QNetworkAddressEntry &e : i.addressEntries())
            result.append(e.ip());
    return result;
}
