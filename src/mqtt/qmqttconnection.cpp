// MIT License - see LICENSE
#include "qmqttconnection_p.h"

#include <QtCore/qrandom.h>
#include <QtNetwork/qtcpsocket.h>
#ifndef QT_NO_SSL
#include <QtNetwork/qsslsocket.h>
#endif

namespace {

constexpr quint32 kMaxLength = 268435455; // the largest remaining length that four bytes encode

quint16 readU16(const QByteArray &data, qsizetype at) {
    return static_cast<quint16>((static_cast<quint8>(data[at]) << 8) | static_cast<quint8>(data[at + 1]));
}

void appendU16(QByteArray &out, quint16 value) {
    out.append(static_cast<char>(value >> 8));
    out.append(static_cast<char>(value & 0xff));
}

QString randomClientId() {
    static const char kAlphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    QString id = QStringLiteral("nt");
    for (int i = 0; i < 12; ++i)
        id.append(QChar(kAlphabet[QRandomGenerator::global()->bounded(36)]));
    return id;
}

} // namespace

QMqttConnection::QMqttConnection(QMqttClient *client) : q(client) {
    m_keepAliveTimer.setSingleShot(false);
    QObject::connect(&m_keepAliveTimer, &QTimer::timeout, q, [this] { onKeepAlive(); });
}

QMqttConnection::~QMqttConnection() {
    m_closing = true;
    detach();
    if (transport && state != QMqttClient::Disconnected && transport->isOpen())
        transport->close();
}

const QString &QMqttConnection::effectiveClientId() const {
    if (!clientIdSet) {
        clientId = randomClientId();
        clientIdSet = true;
    }
    return clientId;
}

void QMqttConnection::appendLength(QByteArray &out, quint32 length) {
    do {
        quint8 byte = length % 128;
        length /= 128;
        if (length > 0)
            byte |= 0x80;
        out.append(static_cast<char>(byte));
    } while (length > 0);
}

bool QMqttConnection::appendString(QByteArray &out, const QByteArray &utf8) {
    if (utf8.size() > 65535)
        return false;
    appendU16(out, static_cast<quint16>(utf8.size()));
    out.append(utf8);
    return true;
}

void QMqttConnection::attach() {
    detach();
    m_connections.append(QObject::connect(transport, &QIODevice::readyRead, q, [this] { onReadyRead(); }));
    m_connections.append(QObject::connect(transport, &QIODevice::aboutToClose, q, [this] { onTransportClosed(); }));
    m_connections.append(
        QObject::connect(transport, &QIODevice::readChannelFinished, q, [this] { onTransportClosed(); }));
    if (auto *socket = qobject_cast<QAbstractSocket *>(transport)) {
        m_connections.append(
            QObject::connect(socket, &QAbstractSocket::disconnected, q, [this] { onTransportClosed(); }));
        m_connections.append(QObject::connect(socket, &QAbstractSocket::errorOccurred, q,
                                              [this](QAbstractSocket::SocketError) { onTransportClosed(); }));
    }
}

void QMqttConnection::detach() {
    for (const QMetaObject::Connection &c : std::as_const(m_connections))
        QObject::disconnect(c);
    m_connections.clear();
}

void QMqttConnection::connectToHost(QMqttClient::TransportType type) {
    if (state != QMqttClient::Disconnected)
        return;
    if (protocolVersion == QMqttClient::MQTT_5_0) {
        q->setError(QMqttClient::InvalidProtocolVersion); // Nt speaks MQTT 3.1 and 3.1.1 only
        return;
    }
    const QString &id = effectiveClientId();
    if (protocolVersion == QMqttClient::MQTT_3_1 && (id.isEmpty() || id.toUtf8().size() > 23)) {
        q->setError(QMqttClient::IdRejected); // MQTT 3.1 requires an identifier of 1 to 23 bytes
        return;
    }
    q->setError(QMqttClient::NoError);
    m_buffer.clear();
    m_pingOutstanding = false;

    if (!transport || (ownedTransport && transportType != type)) {
#ifndef QT_NO_SSL
        if (type == QMqttClient::SecureSocket)
            ownedTransport = std::make_unique<QSslSocket>();
        else
#endif
            ownedTransport = std::make_unique<QTcpSocket>();
        transport = ownedTransport.get();
        transportType = type;
    }
    attach();
    q->setState(QMqttClient::Connecting);

    if (transportType == QMqttClient::IODevice) {
        if (!transport->isOpen()) {
            finish(QMqttClient::TransportInvalid);
            return;
        }
        sendConnect();
        return;
    }
    auto *socket = qobject_cast<QAbstractSocket *>(transport);
    if (!socket) {
        finish(QMqttClient::TransportInvalid);
        return;
    }
#ifndef QT_NO_SSL
    if (transportType == QMqttClient::SecureSocket) {
        auto *ssl = qobject_cast<QSslSocket *>(socket);
        if (!ssl) {
            finish(QMqttClient::TransportInvalid);
            return;
        }
        if (hasSslConfiguration && ownedTransport.get() == ssl)
            ssl->setSslConfiguration(sslConfiguration);
        m_connections.append(QObject::connect(ssl, &QSslSocket::encrypted, q, [this] { sendConnect(); }));
        if (ssl->state() == QAbstractSocket::ConnectedState && ssl->isEncrypted())
            sendConnect();
        else
            ssl->connectToHostEncrypted(hostname, port);
        return;
    }
#endif
    m_connections.append(QObject::connect(socket, &QAbstractSocket::connected, q, [this] { sendConnect(); }));
    if (socket->state() == QAbstractSocket::ConnectedState)
        sendConnect();
    else
        socket->connectToHost(hostname, port);
}

void QMqttConnection::sendConnect() {
    if (state != QMqttClient::Connecting)
        return;
    QByteArray body;
    const bool v31 = protocolVersion == QMqttClient::MQTT_3_1;
    appendString(body, v31 ? QByteArray("MQIsdp") : QByteArray("MQTT"));
    body.append(static_cast<char>(protocolVersion));
    quint8 flags = 0;
    if (cleanSession)
        flags |= 0x02;
    const bool will = !willTopic.isEmpty();
    if (will) {
        flags |= 0x04;
        flags |= static_cast<quint8>((willQoS & 0x03) << 3);
        if (willRetain)
            flags |= 0x20;
    }
    if (!username.isEmpty())
        flags |= 0x80;
    if (!password.isEmpty())
        flags |= 0x40;
    body.append(static_cast<char>(flags));
    appendU16(body, keepAlive);
    bool fits = appendString(body, effectiveClientId().toUtf8());
    if (will) {
        fits = fits && appendString(body, willTopic.toUtf8());
        fits = fits && appendString(body, willMessage);
    }
    if (!username.isEmpty())
        fits = fits && appendString(body, username.toUtf8());
    if (!password.isEmpty())
        fits = fits && appendString(body, password.toUtf8());
    if (!fits) {
        finish(QMqttClient::ProtocolViolation);
        return;
    }
    send(Connect << 4, body);
}

void QMqttConnection::disconnectFromHost() {
    if (state == QMqttClient::Disconnected)
        return;
    if (state == QMqttClient::Connected && transport && transport->isOpen()) {
        send(Disconnect << 4, QByteArray());
        if (auto *socket = qobject_cast<QAbstractSocket *>(transport))
            socket->flush();
    }
    finish(QMqttClient::NoError);
}

void QMqttConnection::finish(QMqttClient::ClientError reason) {
    if (m_closing)
        return;
    m_closing = true;
    m_keepAliveTimer.stop();
    detach();
    if (transport && transport->isOpen()) {
        if (auto *socket = qobject_cast<QAbstractSocket *>(transport))
            socket->disconnectFromHost();
        else if (transportType != QMqttClient::IODevice)
            transport->close();
    }
    m_buffer.clear();
    m_pendingPublish.clear();
    m_releasedPublish.clear();
    m_incomingQos2.clear();
    m_pendingSubscribe.clear();
    m_pendingUnsubscribe.clear();
    const auto subscriptions = m_subscriptions;
    m_subscriptions.clear();
    QPointer<QMqttClient> guard(q);
    if (reason != QMqttClient::NoError)
        q->setError(reason);
    for (const QPointer<QMqttSubscription> &sub : subscriptions) {
        if (sub)
            sub->setState(QMqttSubscription::Unsubscribed);
    }
    if (guard)
        q->setState(QMqttClient::Disconnected);
    if (guard)
        m_closing = false;
}

void QMqttConnection::onTransportClosed() {
    if (m_closing || state == QMqttClient::Disconnected)
        return;
    finish(QMqttClient::TransportInvalid);
}

void QMqttConnection::send(quint8 header, const QByteArray &body) {
    if (!transport)
        return;
    QByteArray packet;
    packet.reserve(body.size() + 5);
    packet.append(static_cast<char>(header));
    appendLength(packet, static_cast<quint32>(body.size()));
    packet.append(body);
    transport->write(packet);
}

void QMqttConnection::sendId(quint8 header, quint16 id) {
    QByteArray body;
    appendU16(body, id);
    send(header, body);
}

quint16 QMqttConnection::takeId() {
    for (;;) {
        const quint16 id = m_nextId;
        m_nextId = static_cast<quint16>(m_nextId == 65535 ? 1 : m_nextId + 1);
        if (!m_pendingPublish.contains(id) && !m_pendingSubscribe.contains(id) && !m_pendingUnsubscribe.contains(id))
            return id;
    }
}

void QMqttConnection::onReadyRead() {
    if (!transport)
        return;
    m_buffer.append(transport->readAll());
    QPointer<QMqttClient> guard(q);
    while (guard && state != QMqttClient::Disconnected && m_buffer.size() >= 2) {
        quint32 length = 0;
        quint32 multiplier = 1;
        qsizetype at = 1;
        bool complete = false;
        for (; at < m_buffer.size() && at <= 4; ++at) {
            const auto byte = static_cast<quint8>(m_buffer[at]);
            length += (byte & 0x7fU) * multiplier;
            multiplier *= 128;
            if ((byte & 0x80) == 0) {
                complete = true;
                ++at;
                break;
            }
        }
        if (!complete) {
            if (at > 4 || length > kMaxLength) {
                finish(QMqttClient::ProtocolViolation);
                return;
            }
            return; // the rest of the length has not arrived
        }
        if (m_buffer.size() < at + static_cast<qsizetype>(length))
            return;
        const auto header = static_cast<quint8>(m_buffer[0]);
        const QByteArray body = m_buffer.mid(at, static_cast<qsizetype>(length));
        m_buffer.remove(0, at + static_cast<qsizetype>(length));
        if (!handle(header, body)) {
            if (guard)
                finish(QMqttClient::ProtocolViolation);
            return;
        }
    }
}

bool QMqttConnection::handle(quint8 header, const QByteArray &body) {
    const quint8 type = header >> 4;
    const quint8 flags = header & 0x0f;
    if (state == QMqttClient::Connecting && type != ConnAck)
        return false;
    switch (type) {
    case ConnAck: {
        if (state != QMqttClient::Connecting || body.size() != 2)
            return false;
        const auto code = static_cast<quint8>(body[1]);
        if (code != 0) {
            finish(code <= 5 ? static_cast<QMqttClient::ClientError>(code) : QMqttClient::UnknownError);
            return true;
        }
        if (keepAlive > 0 && autoKeepAlive)
            m_keepAliveTimer.start(int(keepAlive) * 1000);
        QPointer<QMqttClient> guard(q);
        q->setState(QMqttClient::Connected);
        if (guard && (static_cast<quint8>(body[0]) & 0x01))
            q->brokerSessionRestored();
        return true;
    }
    case Publish:
        return handlePublish(flags, body);
    case PubAck:
    case PubComp: {
        if (body.size() != 2)
            return false;
        const quint16 id = readU16(body, 0);
        const auto it = m_pendingPublish.constFind(id);
        const quint8 expected = type == PubAck ? 1 : 2;
        if (it == m_pendingPublish.cend() || it.value() != expected)
            return true; // a stale acknowledgement; ignore it
        if (type == PubComp && !m_releasedPublish.remove(id))
            return true;
        m_pendingPublish.remove(id);
        QPointer<QMqttClient> guard(q);
        q->messageStatusChanged(id,
                                type == PubAck ? QMqtt::MessageStatus::Acknowledged : QMqtt::MessageStatus::Completed);
        if (guard)
            q->messageSent(id);
        return true;
    }
    case PubRec: {
        if (body.size() != 2)
            return false;
        const quint16 id = readU16(body, 0);
        if (m_pendingPublish.value(id) == 2) {
            m_releasedPublish.insert(id);
            sendId((PubRel << 4) | 0x02, id);
            q->messageStatusChanged(id, QMqtt::MessageStatus::Received);
        } else {
            sendId((PubRel << 4) | 0x02, id); // let the broker finish a message it holds
        }
        return true;
    }
    case PubRel: {
        if (body.size() != 2 || flags != 0x02)
            return false;
        const quint16 id = readU16(body, 0);
        m_incomingQos2.remove(id);
        sendId(PubComp << 4, id);
        return true;
    }
    case SubAck: {
        if (body.size() < 3)
            return false;
        const quint16 id = readU16(body, 0);
        const QPointer<QMqttSubscription> sub = m_pendingSubscribe.take(id);
        if (!sub)
            return true;
        const auto code = static_cast<quint8>(body[2]);
        if (code == 0x80) {
            m_subscriptions.remove(sub->m_topic);
            sub->setState(QMqttSubscription::Error);
        } else if (code <= 2) {
            sub->setQos(code);
            if (sub)
                sub->setState(QMqttSubscription::Subscribed);
        } else {
            return false;
        }
        return true;
    }
    case UnsubAck: {
        if (body.size() != 2)
            return false;
        const quint16 id = readU16(body, 0);
        const QPointer<QMqttSubscription> sub = m_pendingUnsubscribe.take(id);
        if (sub) {
            if (m_subscriptions.value(sub->m_topic) == sub)
                m_subscriptions.remove(sub->m_topic);
            sub->setState(QMqttSubscription::Unsubscribed);
        }
        return true;
    }
    case PingResp:
        if (!body.isEmpty())
            return false;
        m_pingOutstanding = false;
        q->pingResponseReceived();
        return true;
    default: // CONNECT, SUBSCRIBE, UNSUBSCRIBE, PINGREQ and DISCONNECT only go to a broker
        return false;
    }
}

bool QMqttConnection::handlePublish(quint8 flags, const QByteArray &body) {
    const quint8 qos = (flags >> 1) & 0x03;
    if (qos == 3 || body.size() < 2)
        return false;
    const quint16 topicLength = readU16(body, 0);
    qsizetype at = 2 + topicLength;
    if (body.size() < at + (qos > 0 ? 2 : 0))
        return false;
    const QMqttTopicName topic(QString::fromUtf8(body.mid(2, topicLength)));
    quint16 id = 0;
    if (qos > 0) {
        id = readU16(body, at);
        at += 2;
        if (id == 0)
            return false;
    }
    const QByteArray payload = body.mid(at);
    const bool duplicate = (flags & 0x08) != 0;
    const bool retain = (flags & 0x01) != 0;
    if (qos == 1) {
        sendId(PubAck << 4, id);
    } else if (qos == 2) {
        sendId(PubRec << 4, id);
        if (m_incomingQos2.contains(id))
            return true; // sent again before it was released; it was delivered the first time
        m_incomingQos2.insert(id);
    }
    deliver(topic, payload, id, qos, duplicate, retain);
    return true;
}

void QMqttConnection::deliver(const QMqttTopicName &topic, const QByteArray &payload, quint16 id, quint8 qos,
                              bool duplicate, bool retain) {
    QPointer<QMqttClient> guard(q);
    q->messageReceived(payload, topic);
    if (!guard)
        return;
    const QMqttMessage message(topic, payload, id, qos, duplicate, retain);
    QList<QPointer<QMqttSubscription>> matching;
    for (const QPointer<QMqttSubscription> &sub : std::as_const(m_subscriptions)) {
        if (sub && sub->state() == QMqttSubscription::Subscribed && sub->topic().match(topic))
            matching.append(sub);
    }
    for (const QPointer<QMqttSubscription> &sub : std::as_const(matching)) {
        if (!guard)
            return;
        if (sub)
            sub->messageReceived(message);
    }
}

qint32 QMqttConnection::publish(const QMqttTopicName &topic, const QByteArray &message, quint8 qos, bool retain) {
    if (state != QMqttClient::Connected || qos > 2 || !topic.isValid())
        return -1;
    QByteArray body;
    if (!appendString(body, topic.name().toUtf8()))
        return -1;
    quint16 id = 0;
    if (qos > 0) {
        id = takeId();
        appendU16(body, id);
        m_pendingPublish.insert(id, qos);
    }
    body.append(message);
    if (static_cast<quint32>(body.size()) > kMaxLength) {
        m_pendingPublish.remove(id);
        return -1;
    }
    quint8 header = static_cast<quint8>((Publish << 4) | (qos << 1));
    if (retain)
        header |= 0x01;
    send(header, body);
    return id;
}

QMqttSubscription *QMqttConnection::subscribe(const QMqttTopicFilter &topic, quint8 qos) {
    if (state != QMqttClient::Connected || qos > 2 || !topic.isValid())
        return nullptr;
    QPointer<QMqttSubscription> sub = m_subscriptions.value(topic);
    if (sub && sub->qos() == qos && sub->state() != QMqttSubscription::UnsubscriptionPending)
        return sub;
    if (!sub) {
        sub = new QMqttSubscription(q);
        sub->m_client = q;
        sub->m_topic = topic;
        m_subscriptions.insert(topic, sub);
    }
    sub->m_qos = qos;
    QByteArray body;
    const quint16 id = takeId();
    appendU16(body, id);
    appendString(body, topic.filter().toUtf8());
    body.append(static_cast<char>(qos));
    m_pendingSubscribe.insert(id, sub);
    sub->setState(QMqttSubscription::SubscriptionPending);
    send((Subscribe << 4) | 0x02, body);
    return sub;
}

void QMqttConnection::unsubscribe(const QMqttTopicFilter &topic) {
    const QPointer<QMqttSubscription> sub = m_subscriptions.value(topic);
    if (!sub || state != QMqttClient::Connected)
        return;
    QByteArray body;
    const quint16 id = takeId();
    appendU16(body, id);
    appendString(body, topic.filter().toUtf8());
    m_pendingUnsubscribe.insert(id, sub);
    sub->setState(QMqttSubscription::UnsubscriptionPending);
    send((Unsubscribe << 4) | 0x02, body);
}

bool QMqttConnection::requestPing() {
    if (state != QMqttClient::Connected || autoKeepAlive)
        return false;
    send(PingReq << 4, QByteArray());
    return true;
}

void QMqttConnection::onKeepAlive() {
    if (state != QMqttClient::Connected)
        return;
    if (m_pingOutstanding) {
        finish(QMqttClient::ServerUnavailable); // the broker did not answer within the keep-alive interval
        return;
    }
    m_pingOutstanding = true;
    send(PingReq << 4, QByteArray());
}
