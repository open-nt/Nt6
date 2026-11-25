// MIT License - see LICENSE
#include <QtMqtt/qmqttclient.h>

#include "qmqttconnection_p.h"

/*!
    \module NtMqtt
    \title Nt Mqtt C++ Classes
    \brief Classes for MQTT, the protocol of publish and subscribe for devices.
    \cmaketarget Nt::Mqtt

    Nt Mqtt has the client of Qt MQTT: QMqttClient connects to a broker, publishes messages to
    QMqttTopicName topics and subscribes to QMqttTopicFilter patterns, whose messages arrive at a
    QMqttSubscription as QMqttMessage objects. It speaks MQTT 3.1.1 and 3.1 over TCP, TLS or any
    QIODevice, on every platform of Nt Network, Zephyr included.

    See \l{Using Nt Mqtt} for a walk through.
*/

/*!
    \namespace QMqtt
    \brief The QMqtt namespace holds the status of a message that the client published.
    \since 6.0
*/

/*!
    \enum QMqtt::MessageStatus

    The stages of the acknowledgement of a message of QoS 1 or 2 that the client published, which
    QMqttClient::messageStatusChanged() reports.

    \value Unknown The status is not known.
    \value Published The message was published. Not reported by Nt.
    \value Acknowledged The broker acknowledged a message of QoS 1 (PUBACK).
    \value Received The broker received a message of QoS 2 (PUBREC).
    \value Released The message of QoS 2 was released (PUBREL). Not reported by Nt.
    \value Completed The broker completed a message of QoS 2 (PUBCOMP).
*/

/*!
    \class QMqttClient
    \brief The QMqttClient class connects to an MQTT broker, publishes messages and subscribes to topics.
    \since 6.0

    QMqttClient speaks MQTT 3.1.1 (or 3.1) to a broker such as Mosquitto over TCP, over TLS, or
    over any QIODevice set with setTransport(). Set the host name and port, connect, and publish
    or subscribe once connected() is emitted:

    \code
    QMqttClient client;
    client.setHostname(QStringLiteral("broker.local"));
    client.setPort(1883);
    QObject::connect(&client, &QMqttClient::connected, [&client] {
        QMqttSubscription *sub = client.subscribe(QMqttTopicFilter(QStringLiteral("sensors/+/temperature")), 1);
        QObject::connect(sub, &QMqttSubscription::messageReceived, [](const QMqttMessage &msg) {
            qDebug() << msg.topic().name() << msg.payload();
        });
        client.publish(QMqttTopicName(QStringLiteral("devices/board1/status")), "online", 1, true);
    });
    client.connectToHost();
    \endcode

    Messages of QoS 1 and 2 are acknowledged as the protocol asks, in both directions; messageSent()
    reports when the broker has taken one that the client published. With autoKeepAlive, the
    client sends a ping every keepAlive() seconds and drops the connection, with
    \c ServerUnavailable, when the broker leaves one unanswered for a whole interval.

    \differences
    MQTT 5 is not available: setting \c MQTT_5_0 makes connectToHost() fail with
    \c InvalidProtocolVersion, and the classes of its properties (QMqttConnectionProperties,
    QMqttPublishProperties, QMqttSubscriptionProperties and the like), authenticate() and the
    overloads that take them are missing; messageStatusChanged() has no properties argument.
    Messages of QoS 1 and 2 that were in flight when the connection was lost are not sent again
    after a reconnect, even without a clean session. QMqtt::MessageStatus is not a meta-enum.
    WebSockets need a QIODevice adapter set with setTransport().
    \enddifferences

    \sa QMqttSubscription, QMqttTopicName, QMqttTopicFilter, {Using Nt Mqtt}
*/

/*!
    \enum QMqttClient::TransportType

    The kind of transport that setTransport() is given.

    \value IODevice An open QIODevice of any kind, such as a QLocalSocket or a serial port.
    \value AbstractSocket A QAbstractSocket, such as a QTcpSocket, which connectToHost() connects.
    \value SecureSocket A QSslSocket, which connectToHost() connects with TLS.
*/

/*!
    \enum QMqttClient::ClientState

    The state of the connection to the broker.

    \value Disconnected The client is not connected.
    \value Connecting The transport is connecting, or the client waits for the broker to accept it.
    \value Connected The broker accepted the connection.
*/

/*!
    \enum QMqttClient::ClientError

    Why a connection failed or ended.

    \value NoError No error occurred.
    \value InvalidProtocolVersion The broker does not accept the protocol version, or it is \c MQTT_5_0.
    \value IdRejected The broker rejected the client identifier.
    \value ServerUnavailable The broker is unavailable, or it did not answer a ping in time.
    \value BadUsernameOrPassword The broker rejected the user name or password.
    \value NotAuthorized The client is not authorized to connect.
    \value TransportInvalid The transport failed or closed unexpectedly.
    \value ProtocolViolation The broker sent something that breaks the protocol.
    \value UnknownError An unknown error occurred.
    \value Mqtt5SpecificError An error of MQTT 5. Not reported by Nt.
*/

/*!
    \enum QMqttClient::ProtocolVersion

    The version of MQTT that the client speaks.

    \value MQTT_3_1 MQTT 3.1, with the protocol name \c MQIsdp.
    \value MQTT_3_1_1 MQTT 3.1.1, the default.
    \value MQTT_5_0 MQTT 5.0, which Nt does not speak.
*/

/*!
    Constructs a disconnected client with the given \a parent. Its client identifier is random, made when it is first
    needed, unless setClientId() sets one.
*/
QMqttClient::QMqttClient(QObject *parent) : QObject(parent), d(std::make_unique<QMqttConnection>(this)) {}

/*!
    Destroys the client, closing the connection without a DISCONNECT packet.
*/
QMqttClient::~QMqttClient() = default;

/*!
    Sets the transport to \a device, of the kind \a transport, which the client uses instead of a socket of its own.
    The client does not take ownership. It cannot be changed while the client is connected.
*/
void QMqttClient::setTransport(QIODevice *device, TransportType transport) {
    if (d->state != Disconnected)
        return;
    d->ownedTransport.reset();
    d->transport = device;
    d->transportType = transport;
}

/*!
    Returns the transport: the one set with setTransport() or the socket that connectToHost() made, or \c nullptr.
*/
QIODevice *QMqttClient::transport() const { return d->transport; }

/*!
    Subscribes to the topics that \a topic matches, at the given \a qos, and returns the subscription, which the client
    owns. A second subscription to the same filter returns the first. Returns \c nullptr if the client is not connected,
    or the filter or QoS is invalid.
*/
QMqttSubscription *QMqttClient::subscribe(const QMqttTopicFilter &topic, quint8 qos) {
    return d->subscribe(topic, qos);
}

/*!
    Unsubscribes from \a topic. The subscription goes to \c Unsubscribed when the broker acknowledges.
*/
void QMqttClient::unsubscribe(const QMqttTopicFilter &topic) { d->unsubscribe(topic); }

/*!
    Publishes \a message to \a topic at the given \a qos, retained by the broker if \a retain is \c true. Returns the
    identifier of the message, which messageSent() reports once the broker has taken it, 0 for QoS 0, or -1 if the
    client is not connected or the topic or QoS is invalid.
*/
qint32 QMqttClient::publish(const QMqttTopicName &topic, const QByteArray &message, quint8 qos, bool retain) {
    return d->publish(topic, message, qos, retain);
}

/*!
    Sends a ping to the broker, which answers with pingResponseReceived(). Returns \c false if the client is not
    connected or autoKeepAlive is on, since the client then pings by itself.
*/
bool QMqttClient::requestPing() { return d->requestPing(); }

/*!
    Returns the host name of the broker, which connectToHost() connects to.

    \sa setHostname()
*/
QString QMqttClient::hostname() const { return d->hostname; }

/*!
    Returns the port of the broker; 1883 by default (8883 is usual for TLS).

    \sa setPort()
*/
quint16 QMqttClient::port() const { return d->port; }

/*!
    Returns the identifier of the client at the broker. It is random by default, made with
    QRandomGenerator::system() when it is first needed, which on Zephyr needs an entropy driver, so set
    one of the device's own there. MQTT 3.1 needs 1 to 23 bytes; MQTT 3.1.1 also takes an empty one
    with a clean session, for which the broker picks one.

    \sa setClientId()
*/
QString QMqttClient::clientId() const { return d->effectiveClientId(); }

/*!
    Returns the keep-alive interval, in seconds; 60 by default and 0 for none.

    \sa setKeepAlive()
*/
quint16 QMqttClient::keepAlive() const { return d->keepAlive; }

/*!
    Returns the version of MQTT; \c MQTT_3_1_1 by default.

    \sa setProtocolVersion()
*/
QMqttClient::ProtocolVersion QMqttClient::protocolVersion() const { return d->protocolVersion; }

/*!
    Connects to the broker at hostname() and port(), or over the transport set with setTransport(). The state goes to
    \c Connecting, and to \c Connected, with connected(), when the broker accepts the client.
*/
void QMqttClient::connectToHost() {
    d->connectToHost(d->transport && !d->ownedTransport ? d->transportType : AbstractSocket);
}

#ifndef QT_NO_SSL

/*!
    Connects to the broker over TLS, with the configuration \a conf.

    \sa connectToHost()
*/
void QMqttClient::connectToHostEncrypted(const QSslConfiguration &conf) {
    d->sslConfiguration = conf;
    d->hasSslConfiguration = true;
    d->connectToHost(d->transport && !d->ownedTransport ? d->transportType : SecureSocket);
}
#endif

/*!
    Sends DISCONNECT to the broker and closes the connection, so that the broker does not publish the last will.
*/
void QMqttClient::disconnectFromHost() { d->disconnectFromHost(); }

/*!
    Returns the state of the connection.
*/
QMqttClient::ClientState QMqttClient::state() const { return d->state; }

/*!
    Returns the error of the last connection; \c NoError while connecting succeeds.
*/
QMqttClient::ClientError QMqttClient::error() const { return d->error; }

/*!
    Returns the user name sent to the broker; none if empty.

    \sa setUsername()
*/
QString QMqttClient::username() const { return d->username; }

/*!
    Returns the password sent to the broker; none if empty.

    \sa setPassword()
*/
QString QMqttClient::password() const { return d->password; }

/*!
    Returns whether the broker discards the session of an earlier connection; \c true by default.

    \sa setCleanSession()
*/
bool QMqttClient::cleanSession() const { return d->cleanSession; }

/*!
    Returns the topic of the last will, which the broker publishes when the client disappears without disconnecting;
    none if empty.

    \sa setWillTopic()
*/
QString QMqttClient::willTopic() const { return d->willTopic; }

/*!
    Returns the QoS of the last will.

    \sa setWillQoS()
*/
quint8 QMqttClient::willQoS() const { return d->willQoS; }

/*!
    Returns the payload of the last will.

    \sa setWillMessage()
*/
QByteArray QMqttClient::willMessage() const { return d->willMessage; }

/*!
    Returns whether the broker retains the last will.

    \sa setWillRetain()
*/
bool QMqttClient::willRetain() const { return d->willRetain; }

/*!
    Returns whether the client sends the pings of the keep-alive itself; \c true by default.

    \sa requestPing()

    \sa setAutoKeepAlive()
*/
bool QMqttClient::autoKeepAlive() const { return d->autoKeepAlive; }

/*!
    Sets the host name of the broker, which connectToHost() connects to, to \a hostname.

    \sa hostname()
*/
void QMqttClient::setHostname(const QString &hostname) {
    if (d->hostname == hostname)
        return;
    d->hostname = hostname;
    hostnameChanged(hostname);
}

/*!
    Sets the port of the broker to \a port. The default is 1883; 8883 is usual for TLS.

    \sa port()
*/
void QMqttClient::setPort(quint16 port) {
    if (d->port == port)
        return;
    d->port = port;
    portChanged(port);
}

/*!
    Sets the identifier of the client at the broker to \a clientId, which replaces the random one.

    \sa clientId()
*/
void QMqttClient::setClientId(const QString &clientId) {
    if (d->clientIdSet && d->clientId == clientId)
        return;
    d->clientIdSet = true;
    d->clientId = clientId;
    clientIdChanged(clientId);
}

/*!
    Sets the keep-alive interval to \a keepAlive seconds; 0 turns the keep-alive off.

    \sa keepAlive()
*/
void QMqttClient::setKeepAlive(quint16 keepAlive) {
    if (d->keepAlive == keepAlive)
        return;
    d->keepAlive = keepAlive;
    keepAliveChanged(keepAlive);
}

/*!
    Sets the version of MQTT to \a protocolVersion.

    \sa protocolVersion()
*/
void QMqttClient::setProtocolVersion(ProtocolVersion protocolVersion) {
    if (d->protocolVersion == protocolVersion || protocolVersion < MQTT_3_1 || protocolVersion > MQTT_5_0)
        return;
    d->protocolVersion = protocolVersion;
    protocolVersionChanged(protocolVersion);
}

/*!
    Sets the state to \a state and emits stateChanged(), and connected() or disconnected() when the state becomes
    \c Connected or \c Disconnected. The client sets its state itself; this is public as in Qt.
*/
void QMqttClient::setState(ClientState state) {
    if (d->state == state)
        return;
    d->state = state;
    QPointer<QMqttClient> guard(this);
    stateChanged(state);
    if (!guard)
        return;
    if (state == Disconnected)
        disconnected();
    else if (state == Connected)
        connected();
}

/*!
    Sets the error to \a error and emits errorChanged(). The client sets its error itself; this is public as in Qt.
*/
void QMqttClient::setError(ClientError error) {
    if (d->error == error)
        return;
    d->error = error;
    errorChanged(error);
}

/*!
    Sets the user name sent to the broker to \a username; an empty one sends none.

    \sa username()
*/
void QMqttClient::setUsername(const QString &username) {
    if (d->username == username)
        return;
    d->username = username;
    usernameChanged(username);
}

/*!
    Sets the password sent to the broker to \a password; an empty one sends none.

    \sa password()
*/
void QMqttClient::setPassword(const QString &password) {
    if (d->password == password)
        return;
    d->password = password;
    passwordChanged(password);
}

/*!
    Sets whether the broker discards the session of an earlier connection to \a cleanSession.

    \sa cleanSession()
*/
void QMqttClient::setCleanSession(bool cleanSession) {
    if (d->cleanSession == cleanSession)
        return;
    d->cleanSession = cleanSession;
    cleanSessionChanged(cleanSession);
}

/*!
    Sets the topic of the last will to \a willTopic; an empty one sends no will.

    \sa willTopic()
*/
void QMqttClient::setWillTopic(const QString &willTopic) {
    if (d->willTopic == willTopic)
        return;
    d->willTopic = willTopic;
    willTopicChanged(willTopic);
}

/*!
    Sets the QoS of the last will to \a willQoS.

    \sa willQoS()
*/
void QMqttClient::setWillQoS(quint8 willQoS) {
    if (d->willQoS == willQoS || willQoS > 2)
        return;
    d->willQoS = willQoS;
    willQoSChanged(willQoS);
}

/*!
    Sets the payload of the last will to \a willMessage.

    \sa willMessage()
*/
void QMqttClient::setWillMessage(const QByteArray &willMessage) {
    if (d->willMessage == willMessage)
        return;
    d->willMessage = willMessage;
    willMessageChanged(willMessage);
}

/*!
    Sets whether the broker retains the last will to \a willRetain.

    \sa willRetain()
*/
void QMqttClient::setWillRetain(bool willRetain) {
    if (d->willRetain == willRetain)
        return;
    d->willRetain = willRetain;
    willRetainChanged(willRetain);
}

/*!
    Sets whether the client sends the pings of the keep-alive itself to \a autoKeepAlive.

    \sa autoKeepAlive()
*/
void QMqttClient::setAutoKeepAlive(bool autoKeepAlive) {
    if (d->autoKeepAlive == autoKeepAlive)
        return;
    d->autoKeepAlive = autoKeepAlive;
    autoKeepAliveChanged(autoKeepAlive);
}

/*!
    \fn void QMqttClient::connected()

    This signal is emitted when the broker has accepted the connection.
*/

/*!
    \fn void QMqttClient::disconnected()

    This signal is emitted when the state becomes \c Disconnected, after a connection or a failed attempt.
*/

/*!
    \fn void QMqttClient::messageReceived(const QByteArray &message, const QMqttTopicName &topic)

    This signal is emitted for each \a message that arrives, with the \a topic it was published to, before the
    subscriptions that match it emit QMqttSubscription::messageReceived().
*/

/*!
    \fn void QMqttClient::messageStatusChanged(qint32 id, QMqtt::MessageStatus s)

    This signal is emitted when the message \a id that the client published reaches the status \a s.
*/

/*!
    \fn void QMqttClient::messageSent(qint32 id)

    This signal is emitted when the broker has taken the message \a id, of QoS 1 or 2.
*/

/*!
    \fn void QMqttClient::pingResponseReceived()

    This signal is emitted when the broker answers a ping.
*/

/*!
    \fn void QMqttClient::brokerSessionRestored()

    This signal is emitted after connected() when the broker still had the session of an earlier connection, which
    it can only have without a clean session.
*/

/*!
    \fn void QMqttClient::hostnameChanged(QString hostname)

    This signal is emitted when the host name of the broker changes to \a hostname.
*/

/*!
    \fn void QMqttClient::portChanged(quint16 port)

    This signal is emitted when the port of the broker changes to \a port.
*/

/*!
    \fn void QMqttClient::clientIdChanged(QString clientId)

    This signal is emitted when the client identifier changes to \a clientId.
*/

/*!
    \fn void QMqttClient::keepAliveChanged(quint16 keepAlive)

    This signal is emitted when the keep-alive interval changes to \a keepAlive.
*/

/*!
    \fn void QMqttClient::protocolVersionChanged(QMqttClient::ProtocolVersion protocolVersion)

    This signal is emitted when the protocol version changes to \a protocolVersion.
*/

/*!
    \fn void QMqttClient::stateChanged(QMqttClient::ClientState state)

    This signal is emitted when the state changes to \a state.
*/

/*!
    \fn void QMqttClient::errorChanged(QMqttClient::ClientError error)

    This signal is emitted when the error changes to \a error.
*/

/*!
    \fn void QMqttClient::usernameChanged(QString username)

    This signal is emitted when the user name changes to \a username.
*/

/*!
    \fn void QMqttClient::passwordChanged(QString password)

    This signal is emitted when the password changes to \a password.
*/

/*!
    \fn void QMqttClient::cleanSessionChanged(bool cleanSession)

    This signal is emitted when the clean session setting changes to \a cleanSession.
*/

/*!
    \fn void QMqttClient::willTopicChanged(QString willTopic)

    This signal is emitted when the topic of the last will changes to \a willTopic.
*/

/*!
    \fn void QMqttClient::willQoSChanged(quint8 willQoS)

    This signal is emitted when the QoS of the last will changes to \a willQoS.
*/

/*!
    \fn void QMqttClient::willMessageChanged(QByteArray willMessage)

    This signal is emitted when the payload of the last will changes to \a willMessage.
*/

/*!
    \fn void QMqttClient::willRetainChanged(bool willRetain)

    This signal is emitted when the retain flag of the last will changes to \a willRetain.
*/

/*!
    \fn void QMqttClient::autoKeepAliveChanged(bool autoKeepAlive)

    This signal is emitted when the automatic keep-alive setting changes to \a autoKeepAlive.
*/

#include "moc_qmqttclient.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
