// MIT License - see LICENSE
#include "qcommssession.h"

#include "qcommsprotocol_p.h"
#include "qcommsreliable_p.h"

#include <QtCore/qcborarray.h>
#include <QtCore/qcborvalue.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qhash.h>
#include <QtCore/qmap.h>
#include <QtCore/qrandom.h>
#include <QtCore/qtimer.h>
#include <QtNetwork/qnetworkdatagram.h>
#include <QtNetwork/qudpsocket.h>

#include <algorithm>
#include <limits>
#include <map>

namespace QtComms {

using Protocol::Header;
using Protocol::Message;
using Protocol::Type;

namespace {

constexpr int kTickInterval = 25;
constexpr qint64 kChannelLinger = 10000;
constexpr int kClockSamples = 8;

QString endpointKey(const QHostAddress &address, quint16 port) {
    // An IPv4 address may arrive mapped into IPv6; key both the same way.
    bool ok = false;
    const quint32 v4 = address.toIPv4Address(&ok);
    return (ok ? QHostAddress(v4).toString() : address.toString()) + QLatin1Char(':') + QString::number(port);
}

QHostAddress updateGroupFor(quint32 session) {
    return QHostAddress(QStringLiteral("239.255.78.%1").arg(128 + session % 64));
}

} // namespace

class SessionPrivate {
public:
    struct PeerEntry {
        Peer peer;
        qint64 lastSeen = 0;
    };
    struct Channel {
        ReliableChannel reliable;
        QHostAddress address;
        quint16 port = 0;
        qint64 lastUsed = 0;
    };
    struct ClockSample {
        qint64 rtt = 0;
        qint64 offset = 0;
    };

    explicit SessionPrivate(Session *session) : q(session) {}

    Session *q;
    SessionConfiguration config;
    Session::State state = Session::State::Idle;
    bool hosting = false;
    SessionInfo info;
    QCborMap sessionAttributes;
    quint16 localPeer = 0;
    quint16 hostPeer = 0;
    quint16 nextPeer = 2;
    QString localName;
    Peer::Role localRole = Peer::Role::Participant;
    QCborMap localAttributes;
    QMap<quint16, PeerEntry> peers;
    std::map<QString, Channel> channels;
    QHash<QString, quint16> endpointPeer;
    QByteArray key;
    QUdpSocket *control = nullptr;
    QUdpSocket *update = nullptr;
    QUdpSocket *discovery = nullptr;
    QTimer *tickTimer = nullptr;
    QTimer *heartbeatTimer = nullptr;
    QTimer *beaconTimer = nullptr;
    QElapsedTimer clock;
    qint64 offset = 0;
    QList<ClockSample> samples;
    Session::AdmissionPolicy policy;
    QString error;
    QList<QHostAddress> localAddresses;
    QString hostKey;
    qint64 joinStarted = 0;

    qint64 now() const { return clock.elapsed(); }
    qint64 sessionTime() const { return now() + offset; }

    void setState(Session::State s) {
        if (state == s)
            return;
        state = s;
        emit q->stateChanged(s);
    }

    bool openSockets(const QHostAddress &group, quint16 updatePort);
    bool openDiscovery();
    void closeSockets();
    void startTimers();
    void reset();

    Header header(Type type, quint16 flags = 0) const {
        Header h;
        h.type = type;
        h.flags = flags;
        h.session = info.id;
        h.peer = localPeer;
        h.epoch = info.epoch;
        return h;
    }
    void sendPacket(const QHostAddress &address, quint16 port, const Header &h, const QByteArray &payload,
                    bool signedPacket = true);
    Channel &channel(const QHostAddress &address, quint16 port);
    bool sendReliable(const QHostAddress &address, quint16 port, const QCborMap &message);
    bool sendToPeer(quint16 number, const QCborMap &message);
    void sendToAllPeers(const QCborMap &message, quint16 except = 0);
    void sendAck(Channel &c);
    void sendBeacon();
    SessionInfo beaconInfo() const;

    void readSocket(QUdpSocket *socket, bool updateSocket);
    void handleDatagram(const QNetworkDatagram &datagram, bool updateSocket);
    void handlePacket(const Header &h, const QByteArray &body, const QHostAddress &from, quint16 port);
    void handleMessage(const QCborMap &message, const QString &endpoint, quint16 claimedPeer, const QHostAddress &from,
                       quint16 port);
    void handleJoinRequest(const QCborMap &message, const QHostAddress &from, quint16 port);
    void handleJoinAccept(const QCborMap &message, const QHostAddress &from, quint16 port);
    void fail(Session::RejectReason reason, const QString &text, const QCborMap &details = QCborMap());

    void addPeer(const Peer &peer, bool announce);
    void removePeer(quint16 number, bool broadcast);
    void tick();
    void heartbeat();
    void migrateHost();
    void becomeHost();
    bool isLocal(const QHostAddress &address, quint16 port) const;
};

bool SessionPrivate::openSockets(const QHostAddress &group, quint16 updatePort) {
    control = new QUdpSocket(q);
    update = new QUdpSocket(q);
    if (!control->bind(QHostAddress(QHostAddress::AnyIPv4), config.controlPort)) {
        error = QStringLiteral("cannot bind the control port: %1").arg(control->errorString());
        return false;
    }
    if (!update->bind(QHostAddress(QHostAddress::AnyIPv4), updatePort,
                      QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint)) {
        error = QStringLiteral("cannot bind the update port %1: %2").arg(updatePort).arg(update->errorString());
        return false;
    }
    const bool joinedGroup = config.networkInterface.isValid()
                                 ? update->joinMulticastGroup(group, config.networkInterface)
                                 : update->joinMulticastGroup(group);
    if (!joinedGroup) {
        error = QStringLiteral("cannot join the multicast group %1").arg(group.toString());
        return false;
    }
    control->setSocketOption(QAbstractSocket::MulticastTtlOption, config.multicastTtl);
    control->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);
    if (config.networkInterface.isValid())
        control->setMulticastInterface(config.networkInterface);
    QObject::connect(control, &QUdpSocket::readyRead, q, [this] { readSocket(control, false); });
    QObject::connect(update, &QUdpSocket::readyRead, q, [this] { readSocket(update, true); });
    localAddresses = QNetworkInterface::allAddresses();
    return true;
}

bool SessionPrivate::openDiscovery() {
    if (discovery)
        return true;
    discovery = new QUdpSocket(q);
    if (!discovery->bind(QHostAddress(QHostAddress::AnyIPv4), config.discoveryPort,
                         QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint)) {
        error = QStringLiteral("cannot bind the discovery port %1: %2")
                    .arg(config.discoveryPort)
                    .arg(discovery->errorString());
        return false;
    }
    const bool joinedGroup = config.networkInterface.isValid()
                                 ? discovery->joinMulticastGroup(config.discoveryGroup, config.networkInterface)
                                 : discovery->joinMulticastGroup(config.discoveryGroup);
    if (!joinedGroup) {
        error = QStringLiteral("cannot join the discovery group %1").arg(config.discoveryGroup.toString());
        return false;
    }
    QObject::connect(discovery, &QUdpSocket::readyRead, q, [this] { readSocket(discovery, false); });
    return true;
}

void SessionPrivate::closeSockets() {
    for (QUdpSocket **s : {&control, &update, &discovery}) {
        if (*s) {
            (*s)->close();
            (*s)->deleteLater();
            *s = nullptr;
        }
    }
    for (QTimer **t : {&tickTimer, &heartbeatTimer, &beaconTimer}) {
        if (*t) {
            (*t)->stop();
            (*t)->deleteLater();
            *t = nullptr;
        }
    }
}

void SessionPrivate::startTimers() {
    tickTimer = new QTimer(q);
    QObject::connect(tickTimer, &QTimer::timeout, q, [this] { tick(); });
    tickTimer->start(kTickInterval);
    heartbeatTimer = new QTimer(q);
    QObject::connect(heartbeatTimer, &QTimer::timeout, q, [this] { heartbeat(); });
    heartbeatTimer->start(config.heartbeatInterval);
}

void SessionPrivate::reset() {
    closeSockets();
    hosting = false;
    info = SessionInfo();
    sessionAttributes = QCborMap();
    localPeer = 0;
    hostPeer = 0;
    nextPeer = 2;
    peers.clear();
    channels.clear();
    endpointPeer.clear();
    key.clear();
    offset = 0;
    samples.clear();
    hostKey.clear();
}

void SessionPrivate::sendPacket(const QHostAddress &address, quint16 port, const Header &h, const QByteArray &payload,
                                bool signedPacket) {
    if (!control)
        return;
    Header out = h;
    if (signedPacket && !key.isEmpty())
        out.flags |= Protocol::Authenticated;
    QByteArray datagram = Protocol::encodeHeader(out) + payload;
    if (signedPacket)
        datagram = Protocol::sign(key, datagram);
    control->writeDatagram(datagram, address, port);
}

SessionPrivate::Channel &SessionPrivate::channel(const QHostAddress &address, quint16 port) {
    Channel &c = channels[endpointKey(address, port)];
    if (c.port == 0) {
        c.address = address;
        c.port = port;
    }
    c.lastUsed = now();
    return c;
}

bool SessionPrivate::sendReliable(const QHostAddress &address, quint16 port, const QCborMap &message) {
    Channel &c = channel(address, port);
    return c.reliable.send(Protocol::encodeCbor(message), now(), [this, &c](const ReliableChannel::Fragment &f) {
        quint16 flags = f.more ? Protocol::More : 0;
        if (c.reliable.hasReceived())
            flags |= Protocol::AckValid;
        QByteArray payload;
        Protocol::putU16(&payload, f.seq);
        Protocol::putU16(&payload, c.reliable.ack());
        Protocol::putU32(&payload, c.reliable.ackBits());
        payload.append(f.data);
        sendPacket(c.address, c.port, header(Type::Reliable, flags), payload);
    });
}

bool SessionPrivate::sendToPeer(quint16 number, const QCborMap &message) {
    const auto it = peers.constFind(number);
    if (it == peers.cend() || number == localPeer || it->peer.controlPort == 0)
        return false;
    return sendReliable(it->peer.address, it->peer.controlPort, message);
}

void SessionPrivate::sendToAllPeers(const QCborMap &message, quint16 except) {
    for (const PeerEntry &e : std::as_const(peers)) {
        if (e.peer.number != except)
            sendToPeer(e.peer.number, message);
    }
}

void SessionPrivate::sendAck(Channel &c) {
    QByteArray payload;
    Protocol::putU16(&payload, c.reliable.ack());
    Protocol::putU32(&payload, c.reliable.ackBits());
    sendPacket(c.address, c.port, header(Type::Ack, Protocol::AckValid), payload);
}

SessionInfo SessionPrivate::beaconInfo() const {
    SessionInfo b = info;
    b.peerCount = static_cast<int>(peers.size());
    b.attributes = sessionAttributes;
    b.controlPort = control ? control->localPort() : 0;
    return b;
}

void SessionPrivate::sendBeacon() {
    if (!hosting || !control)
        return;
    const QByteArray payload = Protocol::encodeCbor(Protocol::encodeBeacon(beaconInfo()));
    // Beacons are not signed: a browser without the key must still be able to list the session.
    sendPacket(config.discoveryGroup, config.discoveryPort, header(Type::Beacon), payload, false);
}

void SessionPrivate::readSocket(QUdpSocket *socket, bool updateSocket) {
    while (socket && socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = socket->receiveDatagram(Protocol::kMaxMessage);
        handleDatagram(datagram, updateSocket);
        if (!control)
            return; // the session ended while handling the datagram
    }
}

bool SessionPrivate::isLocal(const QHostAddress &address, quint16 port) const {
    if (!control || port != control->localPort())
        return false;
    bool ok = false;
    const quint32 v4 = address.toIPv4Address(&ok);
    const QHostAddress plain = ok ? QHostAddress(v4) : address;
    return plain.isLoopback() || localAddresses.contains(plain);
}

void SessionPrivate::handleDatagram(const QNetworkDatagram &datagram, bool updateSocket) {
    QByteArray data = datagram.data();
    const QHostAddress from = datagram.senderAddress();
    const auto port = static_cast<quint16>(datagram.senderPort());
    if (!Protocol::isSessionPacket(data)) {
        // Anything else on the update group is application traffic, such as DIS PDUs.
        if (!updateSocket || isLocal(from, port) || !Protocol::verify(key, &data))
            return;
        emit q->datagramReceived(data, endpointPeer.value(endpointKey(from, port)));
        return;
    }
    Header h;
    if (!Protocol::decodeHeader(data, &h))
        return;
    if (h.type == Type::Query) {
        if (hosting)
            sendBeacon();
        return;
    }
    if (h.type == Type::Beacon || h.session != info.id || (h.peer != 0 && h.peer == localPeer))
        return;
    if (h.type == Type::Refused) {
        if (state == Session::State::Joining && endpointKey(from, port) == hostKey &&
            data.size() >= Protocol::kHeaderSize + 2) {
            const auto reason = static_cast<Session::RejectReason>(Protocol::getU16(data, Protocol::kHeaderSize));
            fail(reason, QStringLiteral("The session refused the connection: wrong or missing session key."));
        }
        return;
    }
    if (!key.isEmpty() && ((h.flags & Protocol::Authenticated) == 0 || !Protocol::verify(key, &data))) {
        if (hosting && h.type == Type::Reliable && h.peer == 0) {
            QByteArray payload;
            Protocol::putU16(&payload, static_cast<quint16>(Session::RejectReason::AuthenticationFailed));
            sendPacket(from, port, header(Type::Refused), payload, false);
            emit q->joinRefused(
                Peer(), Session::RejectReason::AuthenticationFailed,
                QStringLiteral("A peer at %1 used a wrong or missing session key.").arg(from.toString()));
        }
        return;
    }
    handlePacket(h, data.mid(Protocol::kHeaderSize), from, port);
}

void SessionPrivate::handlePacket(const Header &h, const QByteArray &body, const QHostAddress &from, quint16 port) {
    const QString endpoint = endpointKey(from, port);
    // A known peer may reach us from an address the host did not see; learn it from its port and number.
    if (h.peer != 0 && !endpointPeer.contains(endpoint)) {
        const auto it = peers.constFind(h.peer);
        if (it != peers.cend() && it->peer.controlPort == port)
            endpointPeer.insert(endpoint, h.peer);
    }
    switch (h.type) {
    case Type::Reliable: {
        if (body.size() < 8)
            return;
        Channel &c = channel(from, port);
        if (h.flags & Protocol::AckValid)
            c.reliable.acknowledge(Protocol::getU16(body, 2), Protocol::getU32(body, 4), now());
        ReliableChannel::Fragment f;
        f.seq = Protocol::getU16(body, 0);
        f.more = (h.flags & Protocol::More) != 0;
        f.data = body.mid(8);
        const QList<QByteArray> messages = c.reliable.receive(f);
        sendAck(c);
        for (const QByteArray &bytes : messages) {
            QCborMap message;
            if (Protocol::decodeCbor(bytes, &message))
                handleMessage(message, endpoint, h.peer, from, port);
            if (!control)
                return;
        }
        break;
    }
    case Type::Ack: {
        const auto it = channels.find(endpoint);
        if (it != channels.end() && body.size() >= 6)
            it->second.reliable.acknowledge(Protocol::getU16(body, 0), Protocol::getU32(body, 2), now());
        break;
    }
    case Type::Heartbeat: {
        const auto it = peers.find(h.peer);
        if (it == peers.end())
            return;
        it->lastSeen = now();
        if (it->peer.state == Peer::State::Unresponsive) {
            it->peer.state = Peer::State::Active;
            emit q->peerRecovered(h.peer);
        }
        break;
    }
    case Type::Ping: {
        if (!hosting || body.size() < 8)
            return;
        QByteArray payload = body.left(8);
        Protocol::putI64(&payload, sessionTime());
        sendPacket(from, port, header(Type::Pong), payload);
        break;
    }
    case Type::Pong: {
        if (hosting || body.size() < 16 || endpointPeer.value(endpoint) != hostPeer)
            return;
        const qint64 sent = Protocol::getI64(body, 0);
        const qint64 hostTime = Protocol::getI64(body, 8);
        const qint64 received = now();
        const qint64 rtt = received - sent;
        if (rtt < 0)
            return;
        samples.append(ClockSample{rtt, hostTime + rtt / 2 - received});
        while (samples.size() > kClockSamples)
            samples.removeFirst();
        const auto best = std::min_element(samples.cbegin(), samples.cend(),
                                           [](const ClockSample &a, const ClockSample &b) { return a.rtt < b.rtt; });
        offset = best->offset;
        break;
    }
    default:
        break;
    }
}

void SessionPrivate::handleMessage(const QCborMap &message, const QString &endpoint, quint16 claimedPeer,
                                   const QHostAddress &from, quint16 port) {
    const auto type = static_cast<Message>(message.value(0).toInteger());
    const quint16 sender = endpointPeer.value(endpoint);
    Q_UNUSED(claimedPeer);
    switch (type) {
    case Message::JoinRequest:
        if (hosting && sender == 0)
            handleJoinRequest(message, from, port);
        return;
    case Message::JoinAccept:
        if (state == Session::State::Joining && endpoint == hostKey)
            handleJoinAccept(message, from, port);
        return;
    case Message::JoinReject:
        if (state == Session::State::Joining && endpoint == hostKey) {
            fail(static_cast<Session::RejectReason>(message.value(1).toInteger()), message.value(2).toString(),
                 message.value(3).toMap());
        }
        return;
    default:
        break;
    }
    if (sender == 0)
        return; // only admitted peers may send anything else
    switch (type) {
    case Message::PeerJoined:
        if (sender == hostPeer) {
            const Peer p = Protocol::decodePeer(message.value(1).toMap());
            if (p.isValid() && p.number != localPeer && !peers.contains(p.number))
                addPeer(p, true);
        }
        break;
    case Message::PeerLeft:
        if (sender == hostPeer) {
            const auto number = static_cast<quint16>(message.value(1).toInteger());
            if (number == localPeer) {
                fail(Session::RejectReason::HostUnreachable,
                     QStringLiteral("The host removed this peer from the session."));
                return;
            }
            removePeer(number, false);
        }
        break;
    case Message::Leave:
        if (sender == hostPeer && !hosting) {
            // The host is leaving; the next peer takes over at once instead of waiting for the timeout.
            migrateHost();
        } else {
            removePeer(sender, hosting);
        }
        break;
    case Message::HostChanged: {
        const auto number = static_cast<quint16>(message.value(1).toInteger());
        if (number == sender && number != hostPeer) {
            const quint16 old = hostPeer;
            hostPeer = number;
            info.epoch = static_cast<quint16>(message.value(2).toInteger());
            samples.clear();
            if (old != 0 && old != localPeer)
                removePeer(old, false);
            emit q->hostChanged(number);
        }
        break;
    }
    case Message::SessionAttributes:
        if (sender == hostPeer) {
            sessionAttributes = message.value(1).toMap();
            emit q->sessionAttributesChanged(sessionAttributes);
        }
        break;
    case Message::Application:
        emit q->messageReceived(sender, static_cast<int>(message.value(1).toInteger()), message.value(2).toMap());
        break;
    default:
        break;
    }
}

void SessionPrivate::handleJoinRequest(const QCborMap &message, const QHostAddress &from, quint16 port) {
    Peer candidate;
    candidate.name = message.value(2).toString();
    candidate.role = message.value(3).toInteger() == 1 ? Peer::Role::Observer : Peer::Role::Participant;
    candidate.attributes = message.value(4).toMap();
    candidate.address = from;
    candidate.controlPort = port;
    const int theirVersion = static_cast<int>(message.value(1).toInteger());
    const int theirMinimum = static_cast<int>(message.value(5).toInteger(theirVersion));

    Session::Admission admission;
    if (theirVersion < Session::minimumProtocolVersion) {
        admission = Session::Admission::reject(
            Session::RejectReason::ProtocolTooOld,
            QStringLiteral(
                "This copy speaks session protocol %1, but the session needs %2 or newer. Update the application.")
                .arg(theirVersion)
                .arg(Session::minimumProtocolVersion));
    } else if (theirMinimum > Session::protocolVersion) {
        admission = Session::Admission::reject(
            Session::RejectReason::ProtocolTooNew,
            QStringLiteral(
                "The session's host speaks session protocol %1, older than this copy accepts. The host must update.")
                .arg(Session::protocolVersion));
    } else if (peers.size() >= config.maximumPeers) {
        admission =
            Session::Admission::reject(Session::RejectReason::SessionFull,
                                       QStringLiteral("The session is full (%1 peers).").arg(config.maximumPeers));
    } else if (candidate.name.isEmpty() || std::any_of(peers.cbegin(), peers.cend(), [&](const PeerEntry &e) {
                   return e.peer.name.compare(candidate.name, Qt::CaseInsensitive) == 0;
               })) {
        admission = Session::Admission::reject(
            Session::RejectReason::NameInUse,
            QStringLiteral("The name \"%1\" is empty or already used in the session.").arg(candidate.name));
    } else if (candidate.role == Peer::Role::Observer && !config.observersAllowed) {
        admission = Session::Admission::reject(Session::RejectReason::ObserversNotAllowed,
                                               QStringLiteral("The session does not accept observers."));
    } else if (policy) {
        admission = policy(candidate);
    }

    if (!admission.accepted) {
        QCborMap reject;
        reject.insert(0, static_cast<int>(Message::JoinReject));
        reject.insert(1, static_cast<int>(admission.reason));
        reject.insert(2, admission.text);
        reject.insert(3, admission.details);
        sendReliable(from, port, reject);
        emit q->joinRefused(candidate, admission.reason, admission.text);
        return;
    }

    candidate.number = nextPeer++;
    addPeer(candidate, false);
    QCborArray roster;
    for (const PeerEntry &e : std::as_const(peers))
        roster.append(Protocol::encodePeer(e.peer));
    QCborMap accept;
    accept.insert(0, static_cast<int>(Message::JoinAccept));
    accept.insert(1, candidate.number);
    accept.insert(2, info.epoch);
    accept.insert(3, sessionTime());
    accept.insert(4, roster);
    accept.insert(5, admission.acceptData);
    accept.insert(6, sessionAttributes);
    accept.insert(7, localPeer);
    sendReliable(from, port, accept);
    QCborMap joinedMessage;
    joinedMessage.insert(0, static_cast<int>(Message::PeerJoined));
    joinedMessage.insert(1, Protocol::encodePeer(candidate));
    sendToAllPeers(joinedMessage, candidate.number);
    emit q->peerJoined(candidate);
}

void SessionPrivate::handleJoinAccept(const QCborMap &message, const QHostAddress &from, quint16 port) {
    localPeer = static_cast<quint16>(message.value(1).toInteger());
    info.epoch = static_cast<quint16>(message.value(2).toInteger());
    offset = message.value(3).toInteger() - now();
    hostPeer = static_cast<quint16>(message.value(7).toInteger(1));
    sessionAttributes = message.value(6).toMap();
    const QCborArray roster = message.value(4).toArray();
    for (const auto &v : roster) {
        Peer p = Protocol::decodePeer(v.toMap());
        if (!p.isValid())
            continue;
        if (p.number == hostPeer) {
            p.address = from;
            p.controlPort = port;
        }
        if (p.number == localPeer)
            p.controlPort = control->localPort();
        addPeer(p, false);
    }
    setState(Session::State::Joined);
    heartbeat();
    emit q->joined(message.value(5).toMap());
    for (const PeerEntry &e : std::as_const(peers)) {
        if (e.peer.number != localPeer)
            emit q->peerJoined(e.peer);
    }
}

void SessionPrivate::fail(Session::RejectReason reason, const QString &text, const QCborMap &details) {
    const bool wasJoining = state == Session::State::Joining;
    error = text;
    reset();
    setState(wasJoining && reason != Session::RejectReason::HostUnreachable ? Session::State::Rejected
                                                                            : Session::State::Failed);
    emit q->rejected(reason, text, details);
}

void SessionPrivate::addPeer(const Peer &peer, bool announce) {
    PeerEntry e;
    e.peer = peer;
    e.lastSeen = now();
    peers.insert(peer.number, e);
    if (peer.controlPort != 0 && peer.number != localPeer)
        endpointPeer.insert(endpointKey(peer.address, peer.controlPort), peer.number);
    if (announce)
        emit q->peerJoined(peer);
}

void SessionPrivate::removePeer(quint16 number, bool broadcast) {
    if (number == localPeer || !peers.contains(number))
        return;
    peers.remove(number);
    for (auto it = endpointPeer.begin(); it != endpointPeer.end();) {
        if (it.value() == number) {
            channels.erase(it.key());
            it = endpointPeer.erase(it);
        } else {
            ++it;
        }
    }
    if (broadcast && hosting) {
        QCborMap left;
        left.insert(0, static_cast<int>(Message::PeerLeft));
        left.insert(1, number);
        sendToAllPeers(left);
    }
    emit q->peerLeft(number);
}

void SessionPrivate::tick() {
    const qint64 t = now();
    // Retransmissions, and peers whose channel has given up.
    QList<QString> failedEndpoints;
    for (auto &[endpoint, entry] : channels) {
        auto &c = entry; // clang cannot capture a structured binding in a lambda
        c.reliable.resend(t, [this, &c](const ReliableChannel::Fragment &f) {
            quint16 flags = f.more ? Protocol::More : 0;
            if (c.reliable.hasReceived())
                flags |= Protocol::AckValid;
            QByteArray payload;
            Protocol::putU16(&payload, f.seq);
            Protocol::putU16(&payload, c.reliable.ack());
            Protocol::putU32(&payload, c.reliable.ackBits());
            payload.append(f.data);
            sendPacket(c.address, c.port, header(Type::Reliable, flags), payload);
        });
        if (c.reliable.failed())
            failedEndpoints.append(endpoint);
    }
    for (const QString &endpoint : std::as_const(failedEndpoints)) {
        if (state == Session::State::Joining && endpoint == hostKey) {
            fail(Session::RejectReason::HostUnreachable, QStringLiteral("The host of the session does not answer."));
            return;
        }
        const quint16 number = endpointPeer.value(endpoint);
        channels.erase(endpoint);
        if (number != 0 && number == hostPeer && !hosting)
            migrateHost();
        else if (number != 0)
            removePeer(number, hosting);
        if (!control)
            return;
    }
    // Channels of peers that were refused or left, once they have nothing left to send.
    for (auto it = channels.begin(); it != channels.end();) {
        if (!endpointPeer.contains(it->first) && it->first != hostKey && it->second.reliable.idle() &&
            t - it->second.lastUsed > kChannelLinger)
            it = channels.erase(it);
        else
            ++it;
    }
    if (state == Session::State::Joining && t - joinStarted > 2 * static_cast<qint64>(config.peerTimeout)) {
        fail(Session::RejectReason::HostUnreachable, QStringLiteral("The host of the session does not answer."));
        return;
    }
    if (state != Session::State::Hosting && state != Session::State::Joined)
        return;
    // Liveness.
    QList<quint16> gone;
    bool hostLost = false;
    for (PeerEntry &e : peers) {
        if (e.peer.number == localPeer)
            continue;
        const qint64 age = t - e.lastSeen;
        if (age > config.peerTimeout && e.peer.state == Peer::State::Active) {
            e.peer.state = Peer::State::Unresponsive;
            emit q->peerUnresponsive(e.peer.number);
            if (e.peer.number == hostPeer && !hosting)
                hostLost = true;
        }
        if (age > config.peerRemoveTimeout)
            gone.append(e.peer.number);
    }
    if (hostLost) {
        migrateHost();
        return;
    }
    for (quint16 number : std::as_const(gone))
        removePeer(number, hosting);
}

void SessionPrivate::heartbeat() {
    if (!control || (state != Session::State::Hosting && state != Session::State::Joined))
        return;
    sendPacket(info.updateGroup, info.updatePort, header(Type::Heartbeat), QByteArray());
    if (!hosting) {
        const auto it = peers.constFind(hostPeer);
        if (it != peers.cend() && it->peer.controlPort != 0) {
            QByteArray payload;
            Protocol::putI64(&payload, now());
            sendPacket(it->peer.address, it->peer.controlPort, header(Type::Ping), payload);
        }
    }
}

void SessionPrivate::migrateHost() {
    const quint16 old = hostPeer;
    quint16 next = std::numeric_limits<quint16>::max();
    for (const PeerEntry &e : std::as_const(peers)) {
        if (e.peer.number != old && e.peer.state == Peer::State::Active && e.peer.number < next)
            next = e.peer.number;
    }
    if (next == std::numeric_limits<quint16>::max())
        return;
    hostPeer = next;
    info.epoch = static_cast<quint16>(info.epoch + 1);
    samples.clear();
    removePeer(old, false);
    if (next == localPeer)
        becomeHost();
    emit q->hostChanged(next);
}

void SessionPrivate::becomeHost() {
    hosting = true;
    offset = sessionTime() - now();
    nextPeer = 2;
    for (const PeerEntry &e : std::as_const(peers))
        nextPeer = std::max<quint16>(nextPeer, static_cast<quint16>(e.peer.number + 1));
    if (!openDiscovery())
        qWarning("QtComms::Session: %s", qPrintable(error));
    info.hostName = localName;
    beaconTimer = new QTimer(q);
    QObject::connect(beaconTimer, &QTimer::timeout, q, [this] { sendBeacon(); });
    beaconTimer->start(config.beaconInterval);
    QCborMap changed;
    changed.insert(0, static_cast<int>(Message::HostChanged));
    changed.insert(1, localPeer);
    changed.insert(2, info.epoch);
    sendToAllPeers(changed);
    setState(Session::State::Hosting);
    sendBeacon();
}

/*!
    \namespace QtComms
    \inmodule NtComms
    \brief The QtComms namespace holds Nt Comms: multiplayer sessions and chat on the local network.

    \sa {Multiplayer Sessions}
*/

/*!
    \module NtComms
    \title Nt Comms C++ Classes
    \brief Multiplayer sessions on the local network, and text chat in Cursor on Target GeoChat form.
    \cmaketarget Nt::Comms

    Nt Comms lets copies of an application find each other on a LAN and share a session.
    QtComms::SessionBrowser lists the sessions that hosts announce by multicast, QtComms::Session
    hosts or joins one, with admission checks, reliable messages, multicast datagrams, heartbeats
    and a shared clock, and QtComms::Chat adds text chat that TAK devices can join. It depends only
    on Nt Core and Nt Network and knows nothing about twins; Nt Twin Network builds on it. Qt has no
    such module.

    See \l{Multiplayer Sessions} for an introduction.
*/

/*!
    \class QtComms::SessionConfiguration
    \inmodule NtComms
    \brief The SessionConfiguration struct holds the network settings of an Nt session.

    The defaults suit a LAN: discovery on the multicast group \c 239.255.78.84, port 47800;
    updates on port 47801 of a group the host chooses; heartbeats every second; a peer that is
    silent for 5 seconds is unresponsive and after 15 seconds is removed. Tests shorten the
    intervals and set \l networkInterface to the loopback interface.

    \differences
    Qt has no sessions; this struct is part of Nt Comms.
    \enddifferences

    \sa QtComms::Session
*/

/*!
    \variable QtComms::SessionConfiguration::discoveryGroup
    The multicast group of discovery beacons.
*/

/*!
    \variable QtComms::SessionConfiguration::discoveryPort
    The UDP port of discovery beacons.
*/

/*!
    \variable QtComms::SessionConfiguration::updatePort
    The UDP port of the update group, which carries heartbeats and application datagrams such as DIS PDUs.
*/

/*!
    \variable QtComms::SessionConfiguration::controlPort
    The UDP port this peer receives reliable messages on, or 0 for any free port. It is announced
    to the other peers, so several peers can run on one machine.
*/

/*!
    \variable QtComms::SessionConfiguration::networkInterface
    The interface to send and receive multicast on, or an invalid interface for the system's default.
*/

/*!
    \variable QtComms::SessionConfiguration::multicastTtl
    The time to live of multicast datagrams. 1 keeps them on the local subnet.
*/

/*!
    \variable QtComms::SessionConfiguration::key
    A passphrase shared by all peers, or empty. With a key, every datagram except discovery carries
    a 16-byte HMAC-SHA256 trailer, and datagrams with a wrong trailer are dropped. Nothing is
    encrypted.
*/

/*!
    \variable QtComms::SessionConfiguration::exercise
    The DIS exercise number announced by a host, 1 to 255.
*/

/*!
    \variable QtComms::SessionConfiguration::site
    The DIS site number announced by a host.
*/

/*!
    \variable QtComms::SessionConfiguration::maximumPeers
    The most peers a host admits, itself included.
*/

/*!
    \variable QtComms::SessionConfiguration::observersAllowed
    Whether a host admits observers.
*/

/*!
    \variable QtComms::SessionConfiguration::application
    The name and version of the application, announced in beacons, such as \c {ntsim 6.1.0}.
*/

/*!
    \variable QtComms::SessionConfiguration::beaconInterval
    How often a host sends its beacon, in milliseconds.
*/

/*!
    \variable QtComms::SessionConfiguration::sessionExpiry
    How long a browser lists a session after its last beacon, in milliseconds.
*/

/*!
    \variable QtComms::SessionConfiguration::heartbeatInterval
    How often each peer sends a heartbeat, in milliseconds.
*/

/*!
    \variable QtComms::SessionConfiguration::peerTimeout
    How long a peer may be silent before it is unresponsive, in milliseconds. An unresponsive host is
    replaced.
*/

/*!
    \variable QtComms::SessionConfiguration::peerRemoveTimeout
    How long a peer may be silent before it is removed, in milliseconds.
*/

/*!
    \class QtComms::SessionInfo
    \inmodule NtComms
    \brief The SessionInfo struct describes a session found on the network.

    A host announces its session in beacons; QtComms::SessionBrowser turns them into SessionInfo
    values, and QtComms::Session::join() takes one.

    \differences
    Qt has no sessions; this struct is part of Nt Comms.
    \enddifferences
*/

/*!
    \variable QtComms::SessionInfo::id
    The random ID of the session, never 0.
*/

/*!
    \variable QtComms::SessionInfo::epoch
    How many times the session has changed host.
*/

/*!
    \variable QtComms::SessionInfo::name
    The name of the session.
*/

/*!
    \variable QtComms::SessionInfo::hostName
    The name of the peer hosting the session.
*/

/*!
    \variable QtComms::SessionInfo::hostAddress
    The address the beacon came from.
*/

/*!
    \variable QtComms::SessionInfo::controlPort
    The host's control port.
*/

/*!
    \variable QtComms::SessionInfo::updateGroup
    The multicast group of the session's updates.
*/

/*!
    \variable QtComms::SessionInfo::updatePort
    The UDP port of the session's updates.
*/

/*!
    \variable QtComms::SessionInfo::exercise
    The DIS exercise number of the session.
*/

/*!
    \variable QtComms::SessionInfo::site
    The DIS site number of the session.
*/

/*!
    \variable QtComms::SessionInfo::protocolVersion
    The session protocol version of the host.
*/

/*!
    \variable QtComms::SessionInfo::minimumProtocolVersion
    The oldest session protocol version the host admits.
*/

/*!
    \variable QtComms::SessionInfo::application
    The host's application and version.
*/

/*!
    \variable QtComms::SessionInfo::peerCount
    How many peers are in the session.
*/

/*!
    \variable QtComms::SessionInfo::maximumPeers
    The most peers the session admits.
*/

/*!
    \variable QtComms::SessionInfo::keyRequired
    Whether the session needs a key.
*/

/*!
    \variable QtComms::SessionInfo::observersAllowed
    Whether the session admits observers.
*/

/*!
    \variable QtComms::SessionInfo::attributes
    What the host's application announces, such as the world it runs and the digest of its models.
*/

/*!
    \fn bool QtComms::SessionInfo::isValid() const
    Returns \c true if the info describes a session.
*/

/*!
    \class QtComms::Peer
    \inmodule NtComms
    \brief The Peer struct describes a participant in a session.

    \differences
    Qt has no sessions; this struct is part of Nt Comms.
    \enddifferences
*/

/*!
    \enum QtComms::Peer::Role

    \value Participant The peer may own entities.
    \value Observer The peer only watches.
*/

/*!
    \enum QtComms::Peer::State

    \value Active Heartbeats arrive.
    \value Unresponsive No heartbeat has arrived for the peer timeout.
*/

/*!
    \variable QtComms::Peer::number
    The peer's number in the session, from 1 (the first host). Never reused within a session.
*/

/*!
    \variable QtComms::Peer::name
    The peer's display name, unique within the session.
*/

/*!
    \variable QtComms::Peer::role
    Whether the peer participates or observes.
*/

/*!
    \variable QtComms::Peer::address
    The peer's address as the host saw it.
*/

/*!
    \variable QtComms::Peer::controlPort
    The peer's control port.
*/

/*!
    \variable QtComms::Peer::attributes
    What the peer's application sent when it joined, such as its model catalogue.
*/

/*!
    \variable QtComms::Peer::state
    Whether heartbeats arrive from the peer.
*/

/*!
    \variable QtComms::Peer::roundTripTime
    The smoothed round-trip time to the peer in milliseconds, or -1 if unknown.
*/

/*!
    \fn bool QtComms::Peer::isValid() const
    Returns \c true if the peer has a number.
*/

/*!
    \class QtComms::Session
    \inmodule NtComms
    \brief The Session class hosts or joins a multiplayer session on the local network.

    One peer hosts a session; it announces it with multicast beacons, which
    QtComms::SessionBrowser lists. Others join it. The host decides who may join, assigns peer
    numbers and keeps the roster, and if it goes away the oldest remaining peer takes over.

    \code
    QtComms::Session session;
    session.setAdmissionPolicy([](const QtComms::Peer &candidate) {
        return candidate.attributes.value(1).toString() == myWorld
                   ? QtComms::Session::Admission::accept()
                   : QtComms::Session::Admission::reject(QtComms::Session::RejectReason::WorldMismatch,
                                                         QStringLiteral("Wrong world."));
    });
    session.host(QStringLiteral("Amesbury exercise"), QStringLiteral("alice"));
    \endcode

    Peers exchange two kinds of traffic. sendMessage() and broadcastMessage() deliver CBOR maps
    reliably and in order, over unicast UDP with acknowledgements and retransmission.
    multicast() sends a datagram to every peer at once, unreliably, over the session's update
    group; this carries DIS Entity State PDUs. Each peer also multicasts a heartbeat every second,
    so the others know it is alive even when it sends nothing else.

    sessionTime() is the shared clock of the session: the host's, estimated by each peer from
    round trips.

    \differences
    Qt has no sessions; this class is part of Nt Comms. Only IPv4 is supported, and nothing is
    encrypted.
    \enddifferences

    \sa QtComms::SessionBrowser, QtComms::Chat
*/

/*!
    \enum QtComms::Session::State

    \value Idle Not in a session.
    \value Hosting Hosting a session.
    \value Joining Waiting for the host to answer a join request.
    \value Joined In a session hosted by another peer.
    \value Rejected The host refused to admit this peer.
    \value Failed The host could not be reached, or this peer was removed.
*/

/*!
    \enum QtComms::Session::RejectReason

    Why a host refused a peer, or why a session ended.

    \value None Not refused.
    \value ProtocolTooOld The peer's session protocol is older than the host admits.
    \value ProtocolTooNew The host's session protocol is older than the peer accepts.
    \value AuthenticationFailed The peer used a wrong or missing session key.
    \value WorldMismatch The peer runs a different world or revision.
    \value SessionFull The session has its maximum number of peers.
    \value NameInUse Another peer has the same name.
    \value ObserversNotAllowed The session admits no observers.
    \value MissingModels The peer cannot render models used in the session.
    \value PeersMissingModels Peers in the session cannot render models the peer brings.
    \value HostUnreachable The host does not answer, or removed this peer.
    \value Other Another reason, explained in the text.
*/

/*!
    \class QtComms::Session::Admission
    \inmodule NtComms
    \brief The Admission struct is a host's decision on a peer that wants to join.

    \differences
    Qt has no sessions; this struct is part of Nt Comms.
    \enddifferences

    \sa QtComms::Session::setAdmissionPolicy()
*/

/*!
    \variable QtComms::Session::Admission::accepted
    Whether the peer is admitted.
*/

/*!
    \variable QtComms::Session::Admission::reason
    Why the peer is refused.
*/

/*!
    \variable QtComms::Session::Admission::text
    A message for the refused peer's user.
*/

/*!
    \variable QtComms::Session::Admission::details
    Structured details for the refused peer's application, such as the models it lacks.
*/

/*!
    \variable QtComms::Session::Admission::acceptData
    What the admitted peer receives with joined(), such as the entities already in the session.
*/

/*!
    Returns an admission that accepts the peer and hands it \a acceptData.
*/
Session::Admission Session::Admission::accept(const QCborMap &acceptData) {
    Admission a;
    a.acceptData = acceptData;
    return a;
}

/*!
    Returns an admission that refuses the peer for \a reason, with \a text for its user and
    \a details for its application.
*/
Session::Admission Session::Admission::reject(RejectReason reason, const QString &text, const QCborMap &details) {
    Admission a;
    a.accepted = false;
    a.reason = reason;
    a.text = text;
    a.details = details;
    return a;
}

/*!
    \typedef QtComms::Session::AdmissionPolicy
    A function that decides whether a candidate peer may join. It receives the candidate, with the
    attributes it sent, and returns an QtComms::Session::Admission.
*/

/*!
    \variable QtComms::Session::protocolVersion
    The version of the session protocol this build speaks.
*/

/*!
    \variable QtComms::Session::minimumProtocolVersion
    The oldest version of the session protocol this build admits.
*/

/*!
    \variable QtComms::Session::firstApplicationChannel
    The first channel number free for applications. Nt Comms and Nt Twin Network use the channels
    below it and some above it: QtComms::Chat uses 16 and QtTwin::Replicator uses 32.
*/

/*!
    Constructs an idle session with the given \a parent.
*/
Session::Session(QObject *parent) : QObject(parent), d(std::make_unique<SessionPrivate>(this)) { d->clock.start(); }

/*!
    Leaves the session, if in one, and destroys the object.
*/
Session::~Session() { leave(); }

/*!
    Sets the network settings to \a configuration. They apply from the next host() or join().
*/
void Session::setConfiguration(const SessionConfiguration &configuration) { d->config = configuration; }

/*!
    Returns the network settings.
*/
SessionConfiguration Session::configuration() const { return d->config; }

/*!
    Starts hosting a session called \a sessionName, as the peer \a peerName with the attributes
    \a peerAttributes, and announces it with \a sessionAttributes in its beacons. Returns \c false,
    with errorString() set, if the sockets cannot be opened.
*/
bool Session::host(const QString &sessionName, const QString &peerName, const QCborMap &peerAttributes,
                   const QCborMap &sessionAttributes) {
    leave();
    d->error.clear();
    quint32 id = 0;
    while (id == 0)
        id = QRandomGenerator::global()->generate();
    d->info.id = id;
    d->info.name = sessionName;
    d->info.hostName = peerName;
    d->info.updateGroup = updateGroupFor(id);
    d->info.updatePort = d->config.updatePort;
    d->info.exercise = d->config.exercise;
    d->info.site = d->config.site;
    d->info.protocolVersion = protocolVersion;
    d->info.minimumProtocolVersion = minimumProtocolVersion;
    d->info.application = d->config.application;
    d->info.maximumPeers = d->config.maximumPeers;
    d->info.keyRequired = !d->config.key.isEmpty();
    d->info.observersAllowed = d->config.observersAllowed;
    d->sessionAttributes = sessionAttributes;
    d->key = Protocol::deriveKey(d->config.key, id);
    if (!d->openSockets(d->info.updateGroup, d->info.updatePort) || !d->openDiscovery()) {
        const QString error = d->error;
        d->reset();
        d->error = error;
        return false;
    }
    d->hosting = true;
    d->localName = peerName;
    d->localRole = Peer::Role::Participant;
    d->localAttributes = peerAttributes;
    d->localPeer = 1;
    d->hostPeer = 1;
    d->offset = -d->now();
    Peer self;
    self.number = 1;
    self.name = peerName;
    self.attributes = peerAttributes;
    self.controlPort = d->control->localPort();
    d->addPeer(self, false);
    d->startTimers();
    d->beaconTimer = new QTimer(this);
    connect(d->beaconTimer, &QTimer::timeout, this, [this] { d->sendBeacon(); });
    d->beaconTimer->start(d->config.beaconInterval);
    d->setState(State::Hosting);
    d->sendBeacon();
    return true;
}

/*!
    Asks the host of \a session to admit this peer as \a peerName in \a role, sending
    \a peerAttributes for its admission policy. The answer arrives as joined() or rejected().
    Returns \c false, with errorString() set, if \a session is invalid or the sockets cannot be
    opened.
*/
bool Session::join(const SessionInfo &session, const QString &peerName, Peer::Role role,
                   const QCborMap &peerAttributes) {
    leave();
    d->error.clear();
    if (!session.isValid() || session.hostAddress.isNull()) {
        d->error = QStringLiteral("invalid session");
        return false;
    }
    d->info = session;
    d->key = Protocol::deriveKey(d->config.key, session.id);
    if (!d->openSockets(session.updateGroup, session.updatePort)) {
        const QString error = d->error;
        d->reset();
        d->error = error;
        return false;
    }
    d->localName = peerName;
    d->localRole = role;
    d->localAttributes = peerAttributes;
    d->hostKey = endpointKey(session.hostAddress, session.controlPort);
    d->joinStarted = d->now();
    d->startTimers();
    d->setState(State::Joining);
    QCborMap request;
    request.insert(0, static_cast<int>(Protocol::Message::JoinRequest));
    request.insert(1, protocolVersion);
    request.insert(2, peerName);
    request.insert(3, role == Peer::Role::Observer ? 1 : 0);
    request.insert(4, peerAttributes);
    request.insert(5, minimumProtocolVersion);
    d->sendReliable(session.hostAddress, session.controlPort, request);
    return true;
}

/*!
    Leaves the session: tells the other peers, closes the sockets and returns to the Idle state. A
    leaving host hands the session to the next peer at once.
*/
void Session::leave() {
    if (d->state == State::Hosting || d->state == State::Joined) {
        QCborMap leaving;
        leaving.insert(0, static_cast<int>(Protocol::Message::Leave));
        d->sendToAllPeers(leaving);
    }
    d->reset();
    d->setState(State::Idle);
}

/*!
    Returns the state of the session.
*/
Session::State Session::state() const { return d->state; }

/*!
    Returns \c true if this peer hosts the session.
*/
bool Session::isHost() const { return d->hosting; }

/*!
    Returns this peer's number, or 0 if it is not in a session.
*/
quint16 Session::localPeer() const { return d->localPeer; }

/*!
    Returns the number of the peer hosting the session, or 0 if not in a session.
*/
quint16 Session::hostPeer() const { return d->hostPeer; }

/*!
    Returns the description of the session, as a beacon would announce it.
*/
SessionInfo Session::info() const { return d->beaconInfo(); }

/*!
    Returns the peers in the session, this one included, ordered by number.
*/
QList<Peer> Session::peers() const {
    QList<Peer> result;
    for (const SessionPrivate::PeerEntry &e : std::as_const(d->peers)) {
        Peer p = e.peer;
        const auto it = d->channels.find(endpointKey(p.address, p.controlPort));
        if (it != d->channels.end())
            p.roundTripTime = it->second.reliable.roundTripTime();
        result.append(p);
    }
    return result;
}

/*!
    Returns the peer numbered \a number, or an invalid peer if there is none.
*/
Peer Session::peer(quint16 number) const { return d->peers.value(number).peer; }

/*!
    Returns a description of the last error, or why the session was refused or ended.
*/
QString Session::errorString() const { return d->error; }

/*!
    Sets the attributes announced in beacons and sent to every peer to \a attributes. Only the host
    can change them.
*/
void Session::setSessionAttributes(const QCborMap &attributes) {
    if (!d->hosting)
        return;
    d->sessionAttributes = attributes;
    QCborMap message;
    message.insert(0, static_cast<int>(Protocol::Message::SessionAttributes));
    message.insert(1, attributes);
    d->sendToAllPeers(message);
    emit sessionAttributesChanged(attributes);
}

/*!
    Returns the session's attributes.
*/
QCborMap Session::sessionAttributes() const { return d->sessionAttributes; }

/*!
    Sets the function that decides, on the host, whether a peer may join to \a policy. It runs after
    the built-in checks of protocol version, key, room, name and role. A peer that becomes host keeps
    the policy, so set it on every peer.
*/
void Session::setAdmissionPolicy(const AdmissionPolicy &policy) { d->policy = policy; }

/*!
    Returns the session time in milliseconds: the time since the session was created, by the host's
    clock.
*/
qint64 Session::sessionTime() const { return d->sessionTime(); }

/*!
    Sends \a message on \a channel to the peer numbered \a peer, reliably and in order. Channels from
    firstApplicationChannel up are free for applications. Returns \c false if there is no such peer
    or the message is larger than 64 KiB.
*/
bool Session::sendMessage(quint16 peer, int channel, const QCborMap &message) {
    QCborMap m;
    m.insert(0, static_cast<int>(Protocol::Message::Application));
    m.insert(1, channel);
    m.insert(2, message);
    return d->sendToPeer(peer, m);
}

/*!
    Sends \a message on \a channel to every other peer, reliably.
*/
void Session::broadcastMessage(int channel, const QCborMap &message) {
    QCborMap m;
    m.insert(0, static_cast<int>(Protocol::Message::Application));
    m.insert(1, channel);
    m.insert(2, message);
    d->sendToAllPeers(m);
}

/*!
    Sends \a datagram to every peer over the session's update group, unreliably. The datagram must not
    start with the bytes \c NTSS. Returns \c false if not in a session.
*/
bool Session::multicast(const QByteArray &datagram) {
    if (!d->control || (d->state != State::Hosting && d->state != State::Joined) || Protocol::isSessionPacket(datagram))
        return false;
    return d->control->writeDatagram(Protocol::sign(d->key, datagram), d->info.updateGroup, d->info.updatePort) > 0;
}

/*!
    \fn void QtComms::Session::stateChanged(QtComms::Session::State state)
    This signal is emitted when the session's state changes to \a state.
*/

/*!
    \fn void QtComms::Session::joined(const QCborMap &acceptData)
    This signal is emitted when the host admits this peer, with the \a acceptData of its admission
    policy. peerJoined() follows for each peer already in the session.
*/

/*!
    \fn void QtComms::Session::rejected(QtComms::Session::RejectReason reason, const QString &text, const QCborMap &details)
    This signal is emitted when the host refuses this peer, or the session ends because the host is
    unreachable or removed this peer. \a reason says why, \a text explains it to the user and
    \a details carry the admission policy's structured data.
*/

/*!
    \fn void QtComms::Session::joinRefused(const QtComms::Peer &candidate, QtComms::Session::RejectReason reason, const QString &text)
    This signal is emitted on the host when it refuses \a candidate for \a reason, explained by
    \a text.
*/

/*!
    \fn void QtComms::Session::peerJoined(const QtComms::Peer &peer)
    This signal is emitted when \a peer joins the session.
*/

/*!
    \fn void QtComms::Session::peerLeft(quint16 peer)
    This signal is emitted when the peer numbered \a peer leaves the session or is removed.
*/

/*!
    \fn void QtComms::Session::peerUnresponsive(quint16 peer)
    This signal is emitted when no heartbeat has arrived from \a peer for the peer timeout.
*/

/*!
    \fn void QtComms::Session::peerRecovered(quint16 peer)
    This signal is emitted when heartbeats arrive again from the unresponsive \a peer.
*/

/*!
    \fn void QtComms::Session::hostChanged(quint16 peer)
    This signal is emitted when \a peer takes over as host.
*/

/*!
    \fn void QtComms::Session::sessionAttributesChanged(const QCborMap &attributes)
    This signal is emitted when the host changes the session's \a attributes.
*/

/*!
    \fn void QtComms::Session::messageReceived(quint16 peer, int channel, const QCborMap &message)
    This signal is emitted when \a message arrives from \a peer on \a channel.
*/

/*!
    \fn void QtComms::Session::datagramReceived(const QByteArray &datagram, quint16 peer)
    This signal is emitted when \a datagram arrives on the update group. \a peer is the sending
    peer, or 0 if the sender is not in the session, such as another DIS application.
*/

} // namespace QtComms

#include "moc_qcommssession.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
