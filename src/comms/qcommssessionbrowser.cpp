// MIT License - see LICENSE
#include "qcommssessionbrowser.h"

#include "qcommsprotocol_p.h"

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qmap.h>
#include <QtCore/qtimer.h>
#include <QtNetwork/qnetworkdatagram.h>
#include <QtNetwork/qudpsocket.h>

#include <algorithm>

namespace QtComms {

class SessionBrowserPrivate {
public:
    struct Entry {
        SessionInfo info;
        qint64 lastSeen = 0;
    };

    SessionConfiguration config;
    QUdpSocket *socket = nullptr;
    QTimer *timer = nullptr;
    QElapsedTimer clock;
    QMap<quint32, Entry> sessions;
    QString error;
};

/*!
    \class QtComms::SessionBrowser
    \inmodule NtComms
    \brief The SessionBrowser class lists the sessions hosted on the local network.

    A browser listens for the multicast beacons that hosts send every second on the discovery port,
    and asks for them at once when it starts, so the list fills without waiting:

    \code
    QtComms::SessionBrowser browser;
    connect(&browser, &QtComms::SessionBrowser::sessionFound, [](const QtComms::SessionInfo &s) {
        qDebug() << s.name << "hosted by" << s.hostName;
    });
    browser.start();
    \endcode

    A session is dropped, with sessionLost(), when no beacon has arrived for the configured
    expiry time. Several browsers and hosts can run on one machine.

    \differences
    Qt has no sessions; this class is part of Nt Comms.
    \enddifferences

    \sa QtComms::Session
*/

/*!
    Constructs a browser with the given \a parent. It does not listen until start().
*/
SessionBrowser::SessionBrowser(QObject *parent) : QObject(parent), d(std::make_unique<SessionBrowserPrivate>()) {
    d->clock.start();
}

/*!
    Stops the browser and destroys it.
*/
SessionBrowser::~SessionBrowser() { stop(); }

/*!
    Sets the network settings to \a configuration: the discovery group, port, interface and expiry
    time. They apply from the next start().
*/
void SessionBrowser::setConfiguration(const SessionConfiguration &configuration) { d->config = configuration; }

/*!
    Returns the network settings.
*/
SessionConfiguration SessionBrowser::configuration() const { return d->config; }

/*!
    Starts listening for beacons and asks hosts to announce themselves. Returns \c false, with
    errorString() set, if the discovery port cannot be opened.
*/
bool SessionBrowser::start() {
    stop();
    d->socket = new QUdpSocket(this);
    if (!d->socket->bind(QHostAddress(QHostAddress::AnyIPv4), d->config.discoveryPort,
                         QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint)) {
        d->error = QStringLiteral("cannot bind the discovery port %1: %2")
                       .arg(d->config.discoveryPort)
                       .arg(d->socket->errorString());
        stop();
        return false;
    }
    const bool joined = d->config.networkInterface.isValid()
                            ? d->socket->joinMulticastGroup(d->config.discoveryGroup, d->config.networkInterface)
                            : d->socket->joinMulticastGroup(d->config.discoveryGroup);
    if (!joined) {
        d->error = QStringLiteral("cannot join the discovery group %1").arg(d->config.discoveryGroup.toString());
        stop();
        return false;
    }
    d->socket->setSocketOption(QAbstractSocket::MulticastTtlOption, d->config.multicastTtl);
    d->socket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);
    if (d->config.networkInterface.isValid())
        d->socket->setMulticastInterface(d->config.networkInterface);
    connect(d->socket, &QUdpSocket::readyRead, this, [this] {
        while (d->socket && d->socket->hasPendingDatagrams()) {
            const QNetworkDatagram datagram = d->socket->receiveDatagram(Protocol::kMaxMessage);
            Protocol::Header h;
            QCborMap map;
            if (!Protocol::decodeHeader(datagram.data(), &h) || h.type != Protocol::Type::Beacon ||
                !Protocol::decodeCbor(datagram.data().mid(Protocol::kHeaderSize), &map))
                continue;
            SessionInfo info = Protocol::decodeBeacon(map);
            if (!info.isValid() || info.id != h.session)
                continue;
            info.hostAddress = datagram.senderAddress();
            const auto it = d->sessions.find(info.id);
            if (it != d->sessions.end() && info.epoch < it->info.epoch)
                continue; // a beacon from a host that has since been replaced
            const bool known = it != d->sessions.end();
            d->sessions[info.id] = SessionBrowserPrivate::Entry{info, d->clock.elapsed()};
            if (known)
                emit sessionUpdated(info);
            else
                emit sessionFound(info);
        }
    });
    d->timer = new QTimer(this);
    connect(d->timer, &QTimer::timeout, this, [this] {
        const qint64 now = d->clock.elapsed();
        for (auto it = d->sessions.begin(); it != d->sessions.end();) {
            if (now - it->lastSeen > d->config.sessionExpiry) {
                const quint32 id = it.key();
                it = d->sessions.erase(it);
                emit sessionLost(id);
            } else {
                ++it;
            }
        }
    });
    d->timer->start(std::max(50, d->config.sessionExpiry / 10));
    refresh();
    return true;
}

/*!
    Stops listening and forgets the sessions found.
*/
void SessionBrowser::stop() {
    if (d->socket) {
        d->socket->close();
        d->socket->deleteLater();
        d->socket = nullptr;
    }
    if (d->timer) {
        d->timer->stop();
        d->timer->deleteLater();
        d->timer = nullptr;
    }
    d->sessions.clear();
}

/*!
    Returns \c true if the browser is listening.
*/
bool SessionBrowser::isActive() const { return d->socket != nullptr; }

/*!
    Asks every host to send its beacon now.
*/
void SessionBrowser::refresh() {
    if (!d->socket)
        return;
    Protocol::Header h;
    h.type = Protocol::Type::Query;
    d->socket->writeDatagram(Protocol::encodeHeader(h), d->config.discoveryGroup, d->config.discoveryPort);
}

/*!
    Returns the sessions currently announced, ordered by ID.
*/
QList<SessionInfo> SessionBrowser::sessions() const {
    QList<SessionInfo> result;
    for (const SessionBrowserPrivate::Entry &e : std::as_const(d->sessions))
        result.append(e.info);
    return result;
}

/*!
    Returns the session whose name is \a nameOrId, or whose ID written in decimal or as \c 0x
    hexadecimal is \a nameOrId, or an invalid info if none is announced.
*/
SessionInfo SessionBrowser::find(const QString &nameOrId) const {
    bool ok = false;
    const quint32 id = nameOrId.toUInt(&ok, 0);
    for (const SessionBrowserPrivate::Entry &e : std::as_const(d->sessions)) {
        if (e.info.name == nameOrId || (ok && e.info.id == id))
            return e.info;
    }
    return SessionInfo();
}

/*!
    Returns a description of the last error.
*/
QString SessionBrowser::errorString() const { return d->error; }

/*!
    \fn void QtComms::SessionBrowser::sessionFound(const QtComms::SessionInfo &session)
    This signal is emitted when the first beacon of \a session arrives.
*/

/*!
    \fn void QtComms::SessionBrowser::sessionUpdated(const QtComms::SessionInfo &session)
    This signal is emitted when a later beacon of \a session arrives.
*/

/*!
    \fn void QtComms::SessionBrowser::sessionLost(quint32 id)
    This signal is emitted when the session \a id has sent no beacon for the expiry time.
*/

} // namespace QtComms

#include "moc_qcommssessionbrowser.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
