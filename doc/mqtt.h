// MIT License - see LICENSE
//
// Overview page about Nt Mqtt for the NDoc documentation. This file holds only a documentation comment and is not
// part of the library.

/*!
    \page nt-mqtt.html
    \title Using Nt Mqtt
    \brief Publishing and subscribing to an MQTT broker, from a desktop or a Zephyr board.

    Nt Mqtt is the client of Qt MQTT. It needs only Nt Network, so it runs headless and on Zephyr,
    where it sends the telemetry of a device to a broker such as Mosquitto. Link with \c Nt::Mqtt
    (or \c Qt6::Mqtt).

    \section1 Connecting

    Set the host name of the broker and connect. The state goes to \c Connecting, and to
    \c Connected, with QMqttClient::connected(), when the broker accepts the client:

    \code
    QMqttClient client;
    client.setHostname(QStringLiteral("broker.local"));
    client.setPort(1883);
    client.setUsername(QStringLiteral("device1"));
    client.setPassword(QStringLiteral("secret"));
    client.setWillTopic(QStringLiteral("devices/device1/status"));  // published if the device disappears
    client.setWillMessage("offline");
    client.setWillRetain(true);
    QObject::connect(&client, &QMqttClient::errorChanged, [](QMqttClient::ClientError error) {
        qWarning() << "MQTT error" << error;
    });
    client.connectToHost();
    \endcode

    connectToHostEncrypted() connects with TLS instead, with a QSslConfiguration that holds the
    certificate of the broker's authority. A client of its own transport, such as a QLocalSocket,
    hands it over with setTransport().

    \section1 Publishing

    publish() sends a payload to a topic at QoS 0 (at most once), 1 (at least once) or 2 (exactly
    once). For QoS 1 and 2 it returns an identifier that messageSent() reports when the broker has
    taken the message:

    \code
    QObject::connect(&client, &QMqttClient::connected, [&client] {
        client.publish(QMqttTopicName(QStringLiteral("devices/device1/status")), "online", 1, true);
    });
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&client] {
        const QByteArray reading = QByteArray::number(readTemperature(), 'f', 1);
        client.publish(QMqttTopicName(QStringLiteral("devices/device1/temperature")), reading, 0);
    });
    timer.start(10000);
    \endcode

    \section1 Subscribing

    subscribe() takes a topic filter, in which \c{+} matches one level and a final \c{#} any
    number, and returns a QMqttSubscription that emits each message whose topic matches:

    \code
    QMqttSubscription *commands =
        client.subscribe(QMqttTopicFilter(QStringLiteral("devices/device1/commands/#")), 1);
    QObject::connect(commands, &QMqttSubscription::messageReceived, [](const QMqttMessage &message) {
        qDebug() << message.topic().name() << message.payload();
    });
    \endcode

    \section1 On Zephyr

    Set \c CONFIG_NT_NETWORK and \c CONFIG_NT_MQTT, and \c CONFIG_NT_TLS for brokers on TLS. The
    client uses the TCP sockets of Zephyr's network stack, as on a desktop.

    \section1 What Is Missing

    MQTT 5 and its properties, the resending of messages that were in flight when a connection
    was lost, and a WebSocket transport of its own.

    \sa {Nt Mqtt C++ Classes}, {Nt on Zephyr}
*/
