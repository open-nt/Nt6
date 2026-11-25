// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qpointer.h>
#include <QtCore/qset.h>
#include <QtCore/qtimer.h>
#include <QtMqtt/qmqttclient.h>

#include <memory>

// The state of a QMqttClient and the MQTT 3.1 and 3.1.1 protocol on its transport: packets are encoded and decoded
// here, and the client, its subscriptions and the transport only call in.
class QMqttConnection {
public:
    enum PacketType : quint8 {
        Connect = 1,
        ConnAck,
        Publish,
        PubAck,
        PubRec,
        PubRel,
        PubComp,
        Subscribe,
        SubAck,
        Unsubscribe,
        UnsubAck,
        PingReq,
        PingResp,
        Disconnect
    };

    explicit QMqttConnection(QMqttClient *client);
    ~QMqttConnection();

    void connectToHost(QMqttClient::TransportType type);
    void disconnectFromHost();
    qint32 publish(const QMqttTopicName &topic, const QByteArray &message, quint8 qos, bool retain);
    QMqttSubscription *subscribe(const QMqttTopicFilter &topic, quint8 qos);
    void unsubscribe(const QMqttTopicFilter &topic);
    bool requestPing();

    // Encodes the remaining length of a fixed header. Exposed for the tests through the packets they see.
    static void appendLength(QByteArray &out, quint32 length);
    static bool appendString(QByteArray &out, const QByteArray &utf8);

    QMqttClient *q;

    QString hostname;
    // The identifier is made random when it is first needed, unless one was set, so that an application that sets
    // its own never needs an entropy source (which a Zephyr board may lack).
    const QString &effectiveClientId() const;
    mutable QString clientId;
    mutable bool clientIdSet = false;
    QString username;
    QString password;
    QString willTopic;
    QByteArray willMessage;
    quint16 port = 1883;
    quint16 keepAlive = 60;
    QMqttClient::ProtocolVersion protocolVersion = QMqttClient::MQTT_3_1_1;
    QMqttClient::ClientState state = QMqttClient::Disconnected;
    QMqttClient::ClientError error = QMqttClient::NoError;
    quint8 willQoS = 0;
    bool cleanSession = true;
    bool willRetain = false;
    bool autoKeepAlive = true;

    QIODevice *transport = nullptr;
    QMqttClient::TransportType transportType = QMqttClient::AbstractSocket;
    std::unique_ptr<QIODevice> ownedTransport; // the socket that connectToHost() made, when none was set
#ifndef QT_NO_SSL
    QSslConfiguration sslConfiguration; // for the socket that connectToHostEncrypted() makes
    bool hasSslConfiguration = false;
#endif

private:
    void attach();
    void detach();
    void sendConnect();
    void onReadyRead();
    void onTransportClosed();
    bool handle(quint8 header, const QByteArray &body);
    bool handlePublish(quint8 flags, const QByteArray &body);
    void deliver(const QMqttTopicName &topic, const QByteArray &payload, quint16 id, quint8 qos, bool duplicate,
                 bool retain);
    void send(quint8 header, const QByteArray &body);
    void sendId(quint8 header, quint16 id);
    quint16 takeId();
    void onKeepAlive();
    // Closes the transport and goes to Disconnected, reporting error unless it is NoError.
    void finish(QMqttClient::ClientError reason);

    QByteArray m_buffer;
    QList<QMetaObject::Connection> m_connections;
    QHash<QMqttTopicFilter, QPointer<QMqttSubscription>> m_subscriptions;
    QHash<quint16, QPointer<QMqttSubscription>> m_pendingSubscribe;
    QHash<quint16, QPointer<QMqttSubscription>> m_pendingUnsubscribe;
    QHash<quint16, quint8> m_pendingPublish; // the QoS of each message that awaits its acknowledgement
    QSet<quint16> m_releasedPublish;         // QoS 2 messages that got PUBREC and await PUBCOMP
    QSet<quint16> m_incomingQos2;            // QoS 2 messages received and not yet released
    quint16 m_nextId = 1;
    QTimer m_keepAliveTimer;
    bool m_pingOutstanding = false;
    bool m_closing = false;
};
