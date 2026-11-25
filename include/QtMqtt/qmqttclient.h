// MIT License - see LICENSE
#pragma once

#include <QtCore/qiodevice.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtMqtt/qmqttglobal.h>
#include <QtMqtt/qmqttmessage.h>
#include <QtMqtt/qmqttsubscription.h>
#include <QtMqtt/qmqtttopicfilter.h>
#include <QtMqtt/qmqtttopicname.h>

#ifndef QT_NO_SSL
#include <QtNetwork/qsslconfiguration.h>
#endif

#include <memory>

class QMqttConnection;

class QMqttClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString clientId READ clientId WRITE setClientId NOTIFY clientIdChanged)
    Q_PROPERTY(QString hostname READ hostname WRITE setHostname NOTIFY hostnameChanged)
    Q_PROPERTY(quint16 port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(quint16 keepAlive READ keepAlive WRITE setKeepAlive NOTIFY keepAliveChanged)
    Q_PROPERTY(
        ProtocolVersion protocolVersion READ protocolVersion WRITE setProtocolVersion NOTIFY protocolVersionChanged)
    Q_PROPERTY(ClientState state READ state WRITE setState NOTIFY stateChanged)
    Q_PROPERTY(ClientError error READ error WRITE setError NOTIFY errorChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY usernameChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(bool cleanSession READ cleanSession WRITE setCleanSession NOTIFY cleanSessionChanged)
    Q_PROPERTY(QString willTopic READ willTopic WRITE setWillTopic NOTIFY willTopicChanged)
    Q_PROPERTY(QByteArray willMessage READ willMessage WRITE setWillMessage NOTIFY willMessageChanged)
    Q_PROPERTY(quint8 willQoS READ willQoS WRITE setWillQoS NOTIFY willQoSChanged)
    Q_PROPERTY(bool willRetain READ willRetain WRITE setWillRetain NOTIFY willRetainChanged)
    Q_PROPERTY(bool autoKeepAlive READ autoKeepAlive WRITE setAutoKeepAlive NOTIFY autoKeepAliveChanged)
public:
    enum TransportType { IODevice, AbstractSocket, SecureSocket };
    Q_ENUM(TransportType)

    enum ClientState { Disconnected = 0, Connecting, Connected };
    Q_ENUM(ClientState)

    enum ClientError {
        NoError = 0,
        InvalidProtocolVersion = 1,
        IdRejected = 2,
        ServerUnavailable = 3,
        BadUsernameOrPassword = 4,
        NotAuthorized = 5,
        TransportInvalid = 256,
        ProtocolViolation,
        UnknownError,
        Mqtt5SpecificError
    };
    Q_ENUM(ClientError)

    enum ProtocolVersion { MQTT_3_1 = 3, MQTT_3_1_1 = 4, MQTT_5_0 = 5 };
    Q_ENUM(ProtocolVersion)

    explicit QMqttClient(QObject *parent = nullptr);
    ~QMqttClient() override;

    void setTransport(QIODevice *device, TransportType transport);
    QIODevice *transport() const;

    QMqttSubscription *subscribe(const QMqttTopicFilter &topic, quint8 qos = 0);
    void unsubscribe(const QMqttTopicFilter &topic);

    qint32 publish(const QMqttTopicName &topic, const QByteArray &message = QByteArray(), quint8 qos = 0,
                   bool retain = false);

    bool requestPing();

    QString hostname() const;
    quint16 port() const;
    QString clientId() const;
    quint16 keepAlive() const;
    ProtocolVersion protocolVersion() const;

    Q_INVOKABLE void connectToHost();
#ifndef QT_NO_SSL
    void connectToHostEncrypted(const QSslConfiguration &conf);
#endif
    Q_INVOKABLE void disconnectFromHost();

    ClientState state() const;
    ClientError error() const;

    QString username() const;
    QString password() const;
    bool cleanSession() const;

    QString willTopic() const;
    quint8 willQoS() const;
    QByteArray willMessage() const;
    bool willRetain() const;

    bool autoKeepAlive() const;

Q_SIGNALS:
    void connected();
    void disconnected();
    void messageReceived(const QByteArray &message, const QMqttTopicName &topic = QMqttTopicName());
    void messageStatusChanged(qint32 id, QMqtt::MessageStatus s);
    void messageSent(qint32 id);
    void pingResponseReceived();
    void brokerSessionRestored();

    void hostnameChanged(QString hostname);
    void portChanged(quint16 port);
    void clientIdChanged(QString clientId);
    void keepAliveChanged(quint16 keepAlive);
    void protocolVersionChanged(QMqttClient::ProtocolVersion protocolVersion);
    void stateChanged(QMqttClient::ClientState state);
    void errorChanged(QMqttClient::ClientError error);
    void usernameChanged(QString username);
    void passwordChanged(QString password);
    void cleanSessionChanged(bool cleanSession);

    void willTopicChanged(QString willTopic);
    void willQoSChanged(quint8 willQoS);
    void willMessageChanged(QByteArray willMessage);
    void willRetainChanged(bool willRetain);

    void autoKeepAliveChanged(bool autoKeepAlive);

public Q_SLOTS:
    void setHostname(const QString &hostname);
    void setPort(quint16 port);
    void setClientId(const QString &clientId);
    void setKeepAlive(quint16 keepAlive);
    void setProtocolVersion(QMqttClient::ProtocolVersion protocolVersion);
    void setState(QMqttClient::ClientState state);
    void setError(QMqttClient::ClientError error);
    void setUsername(const QString &username);
    void setPassword(const QString &password);
    void setCleanSession(bool cleanSession);

    void setWillTopic(const QString &willTopic);
    void setWillQoS(quint8 willQoS);
    void setWillMessage(const QByteArray &willMessage);
    void setWillRetain(bool willRetain);

    void setAutoKeepAlive(bool autoKeepAlive);

private:
    friend class QMqttConnection;
    friend class QMqttSubscription;
    std::unique_ptr<QMqttConnection> d;
};
