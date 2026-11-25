// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qobjectdefs.h>
#include <QtMqtt/qmqtttopicname.h>

class QMqttConnection;

class QMqttMessage {
    Q_GADGET
    Q_PROPERTY(QMqttTopicName topic READ topic CONSTANT)
    Q_PROPERTY(QByteArray payload READ payload CONSTANT)
    Q_PROPERTY(quint16 id READ id CONSTANT)
    Q_PROPERTY(quint8 qos READ qos CONSTANT)
    Q_PROPERTY(bool duplicate READ duplicate CONSTANT)
    Q_PROPERTY(bool retain READ retain CONSTANT)
public:
    QMqttMessage() = default;

    friend bool operator==(const QMqttMessage &a, const QMqttMessage &b) noexcept {
        return a.m_topic == b.m_topic && a.m_payload == b.m_payload && a.m_id == b.m_id && a.m_qos == b.m_qos &&
               a.m_duplicate == b.m_duplicate && a.m_retain == b.m_retain;
    }
    friend bool operator!=(const QMqttMessage &a, const QMqttMessage &b) noexcept { return !(a == b); }

    const QByteArray &payload() const;
    quint8 qos() const;
    quint16 id() const;
    QMqttTopicName topic() const;
    bool duplicate() const;
    bool retain() const;

private:
    friend class QMqttConnection;
    QMqttMessage(const QMqttTopicName &topic, const QByteArray &payload, quint16 id, quint8 qos, bool duplicate,
                 bool retain);

    QMqttTopicName m_topic;
    QByteArray m_payload;
    quint16 m_id = 0;
    quint8 m_qos = 0;
    bool m_duplicate = false;
    bool m_retain = false;
};

Q_DECLARE_METATYPE(QMqttMessage)
