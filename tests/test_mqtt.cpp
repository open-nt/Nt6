// MIT License - see LICENSE
//
// Nt Mqtt: topic names and filters (the examples of the MQTT 3.1.1 standard), the exact bytes of the packets the
// client sends, and QMqttClient against a small broker inside the test: QoS 0, 1 and 2 in both directions, retained
// messages, wildcards, unsubscribing, refused connections, the keep-alive and the last will. When a mosquitto binary
// is on the PATH (or in NT_MOSQUITTO), the client also runs against it, which checks the protocol against an
// implementation of others.
#include <QtCore>
#include <QtMqtt>
#include <QtNetwork>

#include <cstdio>
#include <functional>
#include <memory>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

void spinEvents(int msecs) {
    QEventLoop loop;
    QTimer::singleShot(msecs, [&] { loop.quit(); });
    loop.exec();
}

template <typename F> bool waitFor(F cond, int timeoutMs = 5000) {
    QElapsedTimer timer;
    timer.start();
    while (!cond() && timer.elapsed() < timeoutMs)
        spinEvents(5);
    return cond();
}

void testTopics() {
    CHECK(QMqttTopicName(QStringLiteral("sport/tennis/player1")).isValid());
    CHECK(QMqttTopicName(QStringLiteral("/")).isValid());
    CHECK(QMqttTopicName(QStringLiteral("/")).levelCount() == 2);
    CHECK(!QMqttTopicName(QString()).isValid());
    CHECK(!QMqttTopicName(QStringLiteral("sport/+")).isValid());
    CHECK(!QMqttTopicName(QStringLiteral("sport/#")).isValid());
    CHECK(QMqttTopicName(QStringLiteral("a/b/c")).levels() == (QStringList{"a", "b", "c"}));
    CHECK(QMqttTopicName(QLatin1String("x")) == QMqttTopicName(QStringLiteral("x")));

    const auto matches = [](const char *filter, const char *name) {
        return QMqttTopicFilter(QString::fromUtf8(filter)).match(QMqttTopicName(QString::fromUtf8(name)));
    };
    // The examples of section 4.7 of MQTT 3.1.1.
    CHECK(matches("sport/tennis/player1/#", "sport/tennis/player1"));
    CHECK(matches("sport/tennis/player1/#", "sport/tennis/player1/ranking"));
    CHECK(matches("sport/tennis/player1/#", "sport/tennis/player1/score/wimbledon"));
    CHECK(matches("sport/#", "sport"));
    CHECK(matches("#", "sport/tennis"));
    CHECK(matches("sport/tennis/+", "sport/tennis/player1"));
    CHECK(!matches("sport/tennis/+", "sport/tennis/player1/ranking"));
    CHECK(matches("sport/+", "sport/"));
    CHECK(!matches("sport/+", "sport"));
    CHECK(matches("+/+", "/finance"));
    CHECK(matches("/+", "/finance"));
    CHECK(!matches("+", "/finance"));
    CHECK(matches("+/monitor/Clients", "$SYS/monitor/Clients")); // Qt's default
    CHECK(!QMqttTopicFilter(QStringLiteral("+/monitor/Clients"))
               .match(QMqttTopicName(QStringLiteral("$SYS/monitor/Clients")),
                      QMqttTopicFilter::WildcardsDontMatchDollarTopicMatchOption));
    CHECK(QMqttTopicFilter(QStringLiteral("$SYS/#"))
              .match(QMqttTopicName(QStringLiteral("$SYS/broker")),
                     QMqttTopicFilter::WildcardsDontMatchDollarTopicMatchOption));

    CHECK(!QMqttTopicFilter(QStringLiteral("sport/tennis#")).isValid());
    CHECK(!QMqttTopicFilter(QStringLiteral("sport/tennis/#/ranking")).isValid());
    CHECK(!QMqttTopicFilter(QStringLiteral("sport+")).isValid());
    CHECK(QMqttTopicFilter(QStringLiteral("+/tennis/#")).isValid());
    CHECK(!QMqttTopicFilter(QString()).isValid());

    const QMqttTopicFilter shared(QStringLiteral("$share/group/sensors/+"));
    CHECK(shared.isValid() && shared.sharedSubscriptionName() == QStringLiteral("group"));
    CHECK(shared.match(QMqttTopicName(QStringLiteral("sensors/t1"))));
    CHECK(!QMqttTopicFilter(QStringLiteral("$share/group")).isValid());
    CHECK(QMqttTopicFilter(QStringLiteral("a/b")).sharedSubscriptionName().isEmpty());
}

// The bytes the client writes, captured through a QBuffer transport, against packets worked out by hand from the
// standard.
void testWireFormat() {
    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    QMqttClient client;
    client.setClientId(QStringLiteral("nt"));
    client.setKeepAlive(30);
    client.setUsername(QStringLiteral("u"));
    client.setPassword(QStringLiteral("p"));
    client.setWillTopic(QStringLiteral("w"));
    client.setWillMessage("bye");
    client.setWillQoS(1);
    client.setWillRetain(true);
    client.setTransport(&buffer, QMqttClient::IODevice);
    client.connectToHost();
    CHECK(client.state() == QMqttClient::Connecting);
    // CONNECT: "MQTT" level 4, flags user|password|will retain|will QoS 1|will|clean session = 0xEE, keep-alive 30,
    // then client id, will topic, will message, user name and password.
    const QByteArray connect = QByteArray::fromHex("101c"
                                                   "00044d5154540" // "MQTT"
                                                   "4ee001e"       // level 4, flags, keep-alive
                                                   "0002"
                                                   "6e74" // "nt"
                                                   "0001"
                                                   "77" // "w"
                                                   "0003"
                                                   "627965" // "bye"
                                                   "0001"
                                                   "75" // "u"
                                                   "0001"
                                                   "70"); // "p"
    CHECK(buffer.data() == connect);
    if (buffer.data() != connect)
        std::printf("  got %s\n", buffer.data().toHex().constData());

    // The broker accepts: CONNACK with session present. The buffer is read where the client wrote, so the reply goes
    // at the end and the read position is moved back to it.
    const qint64 at = buffer.pos();
    bool restored = false;
    QObject::connect(&client, &QMqttClient::brokerSessionRestored, [&] { restored = true; });
    buffer.buffer().append(QByteArray::fromHex("20020100"));
    buffer.seek(at);
    buffer.readyRead();
    CHECK(client.state() == QMqttClient::Connected);
    CHECK(restored);

    // PUBLISH QoS 1 retained on "a/b" with packet id 1: 0x33, length 2+3+2+2.
    buffer.buffer().clear();
    buffer.seek(0);
    const qint32 id = client.publish(QMqttTopicName(QStringLiteral("a/b")), "hi", 1, true);
    CHECK(id == 1);
    CHECK(buffer.data() == QByteArray::fromHex("33090003612f6200016869"));

    // A payload of 200 bytes needs two bytes of remaining length: 2+1+200 = 203 = 0xcb 0x01.
    buffer.buffer().clear();
    buffer.seek(0);
    CHECK(client.publish(QMqttTopicName(QStringLiteral("t")), QByteArray(200, 'x')) == 0);
    CHECK(buffer.data().left(5) == QByteArray::fromHex("30cb010001"));

    // SUBSCRIBE (flags 0x2) to "s/#" at QoS 2 with packet id 2.
    buffer.buffer().clear();
    buffer.seek(0);
    QMqttSubscription *sub = client.subscribe(QMqttTopicFilter(QStringLiteral("s/#")), 2);
    CHECK(sub && sub->state() == QMqttSubscription::SubscriptionPending);
    CHECK(buffer.data() == QByteArray::fromHex("82080002"
                                               "0003732f23"
                                               "02"));

    // DISCONNECT is 0xe0 0x00.
    buffer.buffer().clear();
    buffer.seek(0);
    client.disconnectFromHost();
    CHECK(buffer.data() == QByteArray::fromHex("e000"));
    CHECK(client.state() == QMqttClient::Disconnected);
    CHECK(sub->state() == QMqttSubscription::Unsubscribed);

    // MQTT 3.1 sends "MQIsdp" at level 3, and needs a client id of 1 to 23 bytes.
    QBuffer old;
    old.open(QIODevice::ReadWrite);
    QMqttClient legacy;
    legacy.setProtocolVersion(QMqttClient::MQTT_3_1);
    legacy.setClientId(QStringLiteral("c"));
    legacy.setKeepAlive(0);
    legacy.setTransport(&old, QMqttClient::IODevice);
    legacy.connectToHost();
    CHECK(old.data() == QByteArray::fromHex("100f00064d514973647003020000000163"));
    legacy.disconnectFromHost();
    legacy.setClientId(QString(24, QChar(u'x')));
    legacy.connectToHost();
    CHECK(legacy.state() == QMqttClient::Disconnected && legacy.error() == QMqttClient::IdRejected);

    QMqttClient v5;
    v5.setProtocolVersion(QMqttClient::MQTT_5_0);
    v5.connectToHost();
    CHECK(v5.state() == QMqttClient::Disconnected && v5.error() == QMqttClient::InvalidProtocolVersion);

    // Nothing can be published or subscribed while disconnected.
    QMqttClient idle;
    CHECK(idle.publish(QMqttTopicName(QStringLiteral("a")), "x") == -1);
    CHECK(idle.subscribe(QMqttTopicFilter(QStringLiteral("a"))) == nullptr);
    CHECK(!idle.clientId().isEmpty() && idle.clientId().size() <= 23);
}

// A broker of MQTT 3.1.1 that is just big enough for the tests: sessions are always clean, and QoS 2 is completed in
// one exchange.
class MiniBroker : public QObject {
public:
    quint8 connackCode = 0;
    bool answerPings = true;
    QStringList connectedIds;
    QByteArray lastConnect;

    explicit MiniBroker() {
        m_server.listen(QHostAddress::LocalHost, 0);
        QObject::connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = m_server.nextPendingConnection()) {
                auto session = std::make_shared<Session>();
                session->socket = socket;
                m_sessions.append(session);
                QObject::connect(socket, &QTcpSocket::readyRead, this, [this, session] { read(session); });
                QObject::connect(socket, &QTcpSocket::disconnected, this, [this, session] { gone(session); });
            }
        });
    }
    quint16 port() const { return m_server.serverPort(); }

    // Drops every client without a DISCONNECT, so that their wills are published.
    void dropAll() {
        for (const auto &session : m_sessions)
            session->socket->abort();
    }

private:
    struct Session {
        QTcpSocket *socket = nullptr;
        QByteArray buffer;
        QString clientId;
        QList<QPair<QMqttTopicFilter, quint8>> filters;
        QString willTopic;
        QByteArray willMessage;
        bool willRetain = false;
        bool clean = true;
    };

    static QByteArray packet(quint8 header, const QByteArray &body) {
        QByteArray out(1, static_cast<char>(header));
        quint32 length = static_cast<quint32>(body.size());
        do {
            quint8 byte = length % 128;
            length /= 128;
            if (length)
                byte |= 0x80;
            out.append(static_cast<char>(byte));
        } while (length);
        return out + body;
    }
    static QByteArray u16(quint16 v) {
        QByteArray out;
        out.append(static_cast<char>(v >> 8));
        out.append(static_cast<char>(v & 0xff));
        return out;
    }
    static quint16 readU16(const QByteArray &d, qsizetype at) {
        return static_cast<quint16>((static_cast<quint8>(d[at]) << 8) | static_cast<quint8>(d[at + 1]));
    }
    static QByteArray readString(const QByteArray &d, qsizetype &at) {
        const quint16 n = readU16(d, at);
        const QByteArray s = d.mid(at + 2, n);
        at += 2 + n;
        return s;
    }

    void route(const QString &topic, const QByteArray &payload, quint8 qos, bool retain) {
        if (retain) {
            if (payload.isEmpty())
                m_retained.remove(topic);
            else
                m_retained.insert(topic, payload);
        }
        for (const auto &session : std::as_const(m_sessions)) {
            for (const auto &[filter, granted] : std::as_const(session->filters)) {
                if (filter.match(QMqttTopicName(topic))) {
                    deliver(session, topic, payload, qMin(qos, granted), false);
                    break;
                }
            }
        }
    }
    void deliver(const std::shared_ptr<Session> &session, const QString &topic, const QByteArray &payload, quint8 qos,
                 bool retain) {
        QByteArray body;
        const QByteArray name = topic.toUtf8();
        body += u16(static_cast<quint16>(name.size())) + name;
        if (qos > 0)
            body += u16(m_nextId++);
        body += payload;
        session->socket->write(packet(static_cast<quint8>(0x30 | (qos << 1) | (retain ? 1 : 0)), body));
    }

    void gone(const std::shared_ptr<Session> &session) {
        m_sessions.removeAll(session);
        if (!session->willTopic.isEmpty())
            route(session->willTopic, session->willMessage, 0, session->willRetain);
    }

    void read(const std::shared_ptr<Session> &session) {
        session->buffer += session->socket->readAll();
        for (;;) {
            QByteArray &b = session->buffer;
            if (b.size() < 2)
                return;
            quint32 length = 0, multiplier = 1;
            qsizetype at = 1;
            bool done = false;
            while (at < b.size()) {
                const auto byte = static_cast<quint8>(b[at++]);
                length += (byte & 0x7fU) * multiplier;
                multiplier *= 128;
                if (!(byte & 0x80)) {
                    done = true;
                    break;
                }
            }
            if (!done || b.size() < at + static_cast<qsizetype>(length))
                return;
            const auto header = static_cast<quint8>(b[0]);
            const QByteArray body = b.mid(at, static_cast<qsizetype>(length));
            b.remove(0, at + static_cast<qsizetype>(length));
            handle(session, header, body);
        }
    }

    void handle(const std::shared_ptr<Session> &session, quint8 header, const QByteArray &body) {
        switch (header >> 4) {
        case 1: { // CONNECT
            lastConnect = body;
            qsizetype at = 0;
            readString(body, at); // protocol name
            at += 1;              // level
            const auto flags = static_cast<quint8>(body[at++]);
            at += 2; // keep-alive
            session->clientId = QString::fromUtf8(readString(body, at));
            session->clean = flags & 0x02;
            if (flags & 0x04) {
                session->willTopic = QString::fromUtf8(readString(body, at));
                session->willMessage = readString(body, at);
                session->willRetain = flags & 0x20;
            }
            connectedIds.append(session->clientId);
            session->socket->write(packet(0x20, QByteArray(1, '\0') + QByteArray(1, static_cast<char>(connackCode))));
            if (connackCode != 0)
                session->socket->disconnectFromHost();
            break;
        }
        case 3: { // PUBLISH
            const quint8 qos = (header >> 1) & 3;
            qsizetype at = 0;
            const QString topic = QString::fromUtf8(readString(body, at));
            quint16 id = 0;
            if (qos > 0) {
                id = readU16(body, at);
                at += 2;
            }
            route(topic, body.mid(at), qos, header & 1);
            if (qos == 1)
                session->socket->write(packet(0x40, u16(id)));
            else if (qos == 2)
                session->socket->write(packet(0x50, u16(id)));
            break;
        }
        case 4: // PUBACK
        case 7: // PUBCOMP
            break;
        case 5: // PUBREC from the client for a QoS 2 delivery
            session->socket->write(packet(0x62, body));
            break;
        case 6: // PUBREL
            session->socket->write(packet(0x70, body));
            break;
        case 8: { // SUBSCRIBE
            const quint16 id = readU16(body, 0);
            qsizetype at = 2;
            QByteArray codes;
            QList<QString> subscribed;
            while (at < body.size()) {
                const QString filter = QString::fromUtf8(readString(body, at));
                const auto qos = static_cast<quint8>(body[at++]);
                if (filter == QStringLiteral("forbidden/#")) {
                    codes.append(static_cast<char>(0x80));
                    continue;
                }
                const quint8 granted = qos > 1 ? quint8(1) : qos; // this broker grants at most QoS 1
                session->filters.append({QMqttTopicFilter(filter), granted});
                codes.append(static_cast<char>(granted));
                subscribed.append(filter);
            }
            session->socket->write(packet(0x90, u16(id) + codes));
            for (const QString &filter : subscribed) {
                for (auto it = m_retained.cbegin(); it != m_retained.cend(); ++it) {
                    if (QMqttTopicFilter(filter).match(QMqttTopicName(it.key())))
                        deliver(session, it.key(), it.value(), 0, true);
                }
            }
            break;
        }
        case 10: { // UNSUBSCRIBE
            const quint16 id = readU16(body, 0);
            qsizetype at = 2;
            while (at < body.size()) {
                const QString filter = QString::fromUtf8(readString(body, at));
                session->filters.removeIf([&](const auto &entry) { return entry.first.filter() == filter; });
            }
            session->socket->write(packet(0xb0, u16(id)));
            break;
        }
        case 12: // PINGREQ
            if (answerPings)
                session->socket->write(packet(0xd0, QByteArray()));
            break;
        case 14: // DISCONNECT: no will
            session->willTopic.clear();
            session->socket->disconnectFromHost();
            break;
        default:
            break;
        }
    }

    QTcpServer m_server;
    QList<std::shared_ptr<Session>> m_sessions;
    QHash<QString, QByteArray> m_retained;
    quint16 m_nextId = 1;
};

bool connectClient(QMqttClient &client, quint16 port) {
    client.setHostname(QStringLiteral("127.0.0.1"));
    client.setPort(port);
    client.connectToHost();
    return waitFor([&] { return client.state() != QMqttClient::Connecting; }) &&
           client.state() == QMqttClient::Connected;
}

// Publishing and subscribing between two clients, through a broker on port. maxQos is the highest QoS the broker
// grants.
void exercise(quint16 port, quint8 maxQos, const QString &prefix) {
    QMqttClient sender;
    QMqttClient receiver;
    QList<QMqttClient::ClientState> states;
    QObject::connect(&receiver, &QMqttClient::stateChanged, [&](QMqttClient::ClientState s) { states.append(s); });
    int connectedSignals = 0;
    QObject::connect(&receiver, &QMqttClient::connected, [&] { ++connectedSignals; });
    CHECK(connectClient(sender, port));
    CHECK(connectClient(receiver, port));
    CHECK(states == (QList<QMqttClient::ClientState>{QMqttClient::Connecting, QMqttClient::Connected}));
    CHECK(connectedSignals == 1);

    // A retained message is delivered on subscribing, with the retain flag.
    const QMqttTopicName retainedTopic(prefix + QStringLiteral("/retained"));
    sender.publish(retainedTopic, "kept", 1, true);
    QList<QMqttMessage> got;
    QList<QPair<QByteArray, QString>> clientMessages;
    QObject::connect(&receiver, &QMqttClient::messageReceived,
                     [&](const QByteArray &m, const QMqttTopicName &t) { clientMessages.append({m, t.name()}); });
    spinEvents(100);

    QMqttSubscription *sub = receiver.subscribe(QMqttTopicFilter(prefix + QStringLiteral("/#")), 2);
    CHECK(sub != nullptr);
    if (!sub)
        return;
    QObject::connect(sub, &QMqttSubscription::messageReceived, [&](const QMqttMessage &m) { got.append(m); });
    CHECK(receiver.subscribe(QMqttTopicFilter(prefix + QStringLiteral("/#")), 2) == sub);
    CHECK(waitFor([&] { return sub->state() == QMqttSubscription::Subscribed; }));
    CHECK(sub->qos() == maxQos);
    CHECK(waitFor([&] { return !got.isEmpty(); }));
    if (!got.isEmpty()) {
        CHECK(got.first().topic() == retainedTopic && got.first().payload() == "kept" && got.first().retain());
    }
    got.clear();
    clientMessages.clear();

    // QoS 0, 1 and 2 from the sender; messageSent() and messageStatusChanged() for 1 and 2.
    QList<qint32> sent;
    QList<QPair<qint32, QMqtt::MessageStatus>> statuses;
    QObject::connect(&sender, &QMqttClient::messageSent, [&](qint32 id) { sent.append(id); });
    QObject::connect(&sender, &QMqttClient::messageStatusChanged,
                     [&](qint32 id, QMqtt::MessageStatus s) { statuses.append({id, s}); });
    CHECK(sender.publish(QMqttTopicName(prefix + QStringLiteral("/a")), "zero", 0) == 0);
    const qint32 one = sender.publish(QMqttTopicName(prefix + QStringLiteral("/b/c")), "one", 1);
    const qint32 two = sender.publish(QMqttTopicName(prefix + QStringLiteral("/d")), QByteArray(70000, 'z'), 2);
    CHECK(one > 0 && two > 0 && one != two);
    CHECK(waitFor([&] { return got.size() == 3 && sent.size() == 2; }));
    CHECK(sent == (QList<qint32>{one, two}));
    CHECK(statuses.contains({one, QMqtt::MessageStatus::Acknowledged}));
    CHECK(statuses.contains({two, QMqtt::MessageStatus::Received}));
    CHECK(statuses.contains({two, QMqtt::MessageStatus::Completed}));
    if (got.size() == 3) {
        CHECK(got[0].payload() == "zero" && got[0].qos() == 0 && got[0].id() == 0);
        CHECK(got[1].payload() == "one" && got[1].qos() == 1 && got[1].id() != 0);
        CHECK(got[2].payload().size() == 70000 && got[2].qos() == maxQos && !got[2].retain());
        CHECK(got[1].topic().name() == prefix + QStringLiteral("/b/c"));
    }
    CHECK(clientMessages.size() == 3);

    // A second subscription to a narrower filter receives its share.
    QMqttSubscription *narrow = receiver.subscribe(QMqttTopicFilter(prefix + QStringLiteral("/b/+")), 0);
    QList<QMqttMessage> narrowGot;
    QObject::connect(narrow, &QMqttSubscription::messageReceived, [&](const QMqttMessage &m) { narrowGot.append(m); });
    CHECK(waitFor([&] { return narrow->state() == QMqttSubscription::Subscribed; }));
    got.clear();
    sender.publish(QMqttTopicName(prefix + QStringLiteral("/b/x")), "both", 1);
    CHECK(waitFor([&] { return !narrowGot.isEmpty() && !got.isEmpty(); }));

    // Unsubscribed, nothing more arrives.
    sub->unsubscribe();
    narrow->unsubscribe();
    CHECK(waitFor([&] {
        return sub->state() == QMqttSubscription::Unsubscribed && narrow->state() == QMqttSubscription::Unsubscribed;
    }));
    got.clear();
    narrowGot.clear();
    sender.publish(QMqttTopicName(prefix + QStringLiteral("/a")), "late", 1);
    spinEvents(200);
    CHECK(got.isEmpty() && narrowGot.isEmpty());

    sender.publish(retainedTopic, QByteArray(), 1, true); // clears the retained message
    spinEvents(50);
    receiver.disconnectFromHost();
    sender.disconnectFromHost();
    CHECK(receiver.state() == QMqttClient::Disconnected && receiver.error() == QMqttClient::NoError);
}

void testMiniBroker() {
    MiniBroker broker;
    exercise(broker.port(), 1, QStringLiteral("nt"));

    // A refused connection reports the return code of CONNACK.
    broker.connackCode = 4;
    QMqttClient refused;
    int disconnects = 0;
    QObject::connect(&refused, &QMqttClient::disconnected, [&] { ++disconnects; });
    CHECK(!connectClient(refused, broker.port()));
    CHECK(refused.error() == QMqttClient::BadUsernameOrPassword);
    CHECK(disconnects == 1);
    broker.connackCode = 0;

    // A subscription the broker refuses goes to Error.
    QMqttClient client;
    CHECK(connectClient(client, broker.port()));
    QMqttSubscription *forbidden = client.subscribe(QMqttTopicFilter(QStringLiteral("forbidden/#")));
    CHECK(waitFor([&] { return forbidden->state() == QMqttSubscription::Error; }));

    // A ping by hand works only without autoKeepAlive.
    CHECK(!client.requestPing());
    client.disconnectFromHost();
    client.setAutoKeepAlive(false);
    CHECK(connectClient(client, broker.port()));
    bool pong = false;
    QObject::connect(&client, &QMqttClient::pingResponseReceived, [&] { pong = true; });
    CHECK(client.requestPing());
    CHECK(waitFor([&] { return pong; }));
    client.disconnectFromHost();

    // The keep-alive: pings every second are answered, and a broker that stops answering is dropped.
    QMqttClient alive;
    alive.setKeepAlive(1);
    int pongs = 0;
    QObject::connect(&alive, &QMqttClient::pingResponseReceived, [&] { ++pongs; });
    CHECK(connectClient(alive, broker.port()));
    CHECK(waitFor([&] { return pongs >= 2; }, 4000));
    broker.answerPings = false;
    CHECK(waitFor([&] { return alive.state() == QMqttClient::Disconnected; }, 4000));
    CHECK(alive.error() == QMqttClient::ServerUnavailable);
    broker.answerPings = true;

    // Nothing listens on a closed port.
    QTcpServer closed;
    closed.listen(QHostAddress::LocalHost, 0);
    const quint16 dead = closed.serverPort();
    closed.close();
    QMqttClient nobody;
    CHECK(!connectClient(nobody, dead));
    CHECK(nobody.error() == QMqttClient::TransportInvalid);
}

// The will, checked on its own: one watcher, one client that is aborted, and a broker that stays up.
void testWill() {
    MiniBroker broker;
    QMqttClient watcher;
    CHECK(connectClient(watcher, broker.port()));
    QList<QByteArray> wills;
    QMqttSubscription *sub = watcher.subscribe(QMqttTopicFilter(QStringLiteral("wills/#")));
    QObject::connect(sub, &QMqttSubscription::messageReceived,
                     [&](const QMqttMessage &m) { wills.append(m.payload()); });
    CHECK(waitFor([&] { return sub->state() == QMqttSubscription::Subscribed; }));
    {
        QMqttClient polite;
        polite.setWillTopic(QStringLiteral("wills/polite"));
        polite.setWillMessage("polite");
        CHECK(connectClient(polite, broker.port()));
        polite.disconnectFromHost();
    }
    {
        QMqttClient lost;
        lost.setWillTopic(QStringLiteral("wills/lost"));
        lost.setWillMessage("lost");
        CHECK(connectClient(lost, broker.port()));
        if (auto *socket = qobject_cast<QAbstractSocket *>(lost.transport()))
            socket->abort();
    }
    CHECK(waitFor([&] { return !wills.isEmpty(); }));
    spinEvents(100);
    CHECK(wills == (QList<QByteArray>{"lost"}));
    watcher.disconnectFromHost();
}

// The same exchange against mosquitto, when it is installed.
void testMosquitto() {
    QString binary = qEnvironmentVariable("NT_MOSQUITTO");
    if (binary.isEmpty())
        binary = QStandardPaths::findExecutable(QStringLiteral("mosquitto"));
    if (binary.isEmpty())
        binary = QStandardPaths::findExecutable(QStringLiteral("mosquitto"), {QStringLiteral("/usr/sbin")});
    if (binary.isEmpty()) {
        std::printf("skipped the mosquitto checks: no mosquitto on the PATH (set NT_MOSQUITTO)\n");
        return;
    }
    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost, 0);
    const quint16 port = probe.serverPort();
    probe.close();
    QTemporaryDir dir;
    QFile config(dir.filePath(QStringLiteral("mosquitto.conf")));
    config.open(QIODevice::WriteOnly);
    config.write("listener " + QByteArray::number(port) + " 127.0.0.1\nallow_anonymous true\n");
    config.close();
    QProcess broker;
    broker.setProcessChannelMode(QProcess::MergedChannels);
    broker.start(binary, {QStringLiteral("-c"), config.fileName()});
    if (!broker.waitForStarted(3000)) {
        std::printf("FAIL cannot start %s\n", qPrintable(binary));
        ++g_failures;
        return;
    }
    QTcpSocket ready;
    CHECK(waitFor([&] {
        ready.abort();
        ready.connectToHost(QHostAddress::LocalHost, port);
        return ready.waitForConnected(100);
    }));
    ready.abort();
    exercise(port, 2, QStringLiteral("nt/mosquitto"));

    QMqttClient legacy;
    legacy.setProtocolVersion(QMqttClient::MQTT_3_1);
    CHECK(connectClient(legacy, port));
    legacy.disconnectFromHost();
    broker.terminate();
    broker.waitForFinished(3000);
    std::printf("checked the client against %s\n", qPrintable(binary));
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testTopics();
    testWireFormat();
    testMiniBroker();
    testWill();
    testMosquitto();
    if (g_failures) {
        std::printf("%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("all mqtt tests passed\n");
    return 0;
}
