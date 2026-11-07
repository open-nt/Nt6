// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtNetwork/qhostaddress.h>

class QNetworkAddressEntry {
public:
    QNetworkAddressEntry() = default;

    QHostAddress ip() const;
    void setIp(const QHostAddress &newIp);
    QHostAddress netmask() const;
    void setNetmask(const QHostAddress &newNetmask);
    int prefixLength() const;
    void setPrefixLength(int length);
    QHostAddress broadcast() const;
    void setBroadcast(const QHostAddress &newBroadcast);

    friend bool operator==(const QNetworkAddressEntry &a, const QNetworkAddressEntry &b) {
        return a.m_ip == b.m_ip && a.m_netmask == b.m_netmask && a.m_broadcast == b.m_broadcast;
    }
    friend bool operator!=(const QNetworkAddressEntry &a, const QNetworkAddressEntry &b) { return !(a == b); }

private:
    QHostAddress m_ip;
    QHostAddress m_netmask;
    QHostAddress m_broadcast;
};

class QNetworkInterface {
public:
    enum InterfaceFlag {
        IsUp = 0x1,
        IsRunning = 0x2,
        CanBroadcast = 0x4,
        IsLoopBack = 0x8,
        IsPointToPoint = 0x10,
        CanMulticast = 0x20
    };
    Q_DECLARE_FLAGS(InterfaceFlags, InterfaceFlag)

    enum InterfaceType { Unknown = 0, Loopback = 1, Virtual, Ethernet, Wifi, Ppp };

    QNetworkInterface() = default;

    bool isValid() const;
    int index() const;
    int maximumTransmissionUnit() const;
    QString name() const;
    QString humanReadableName() const;
    InterfaceFlags flags() const;
    InterfaceType type() const;
    QString hardwareAddress() const;
    QList<QNetworkAddressEntry> addressEntries() const;

    static int interfaceIndexFromName(const QString &name);
    static QNetworkInterface interfaceFromName(const QString &name);
    static QNetworkInterface interfaceFromIndex(int index);
    static QString interfaceNameFromIndex(int index);
    static QList<QNetworkInterface> allInterfaces();
    static QList<QHostAddress> allAddresses();

private:
    int m_index = 0;
    int m_mtu = 0;
    QString m_name;
    InterfaceFlags m_flags;
    InterfaceType m_type = Unknown;
    QString m_hardware;
    QList<QNetworkAddressEntry> m_entries;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QNetworkInterface::InterfaceFlags)
