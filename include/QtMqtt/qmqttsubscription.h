// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtMqtt/qmqttmessage.h>
#include <QtMqtt/qmqtttopicfilter.h>

class QMqttClient;
class QMqttConnection;

class QMqttSubscription : public QObject {
    Q_OBJECT
    Q_PROPERTY(SubscriptionState state READ state NOTIFY stateChanged)
    Q_PROPERTY(quint8 qos READ qos NOTIFY qosChanged)
    Q_PROPERTY(QMqttTopicFilter topic READ topic)
    Q_PROPERTY(bool sharedSubscription READ isSharedSubscription)
    Q_PROPERTY(QString sharedSubscriptionName READ sharedSubscriptionName)
public:
    ~QMqttSubscription() override;

    enum SubscriptionState { Unsubscribed = 0, SubscriptionPending, Subscribed, UnsubscriptionPending, Error };
    Q_ENUM(SubscriptionState)

    SubscriptionState state() const;
    QMqttTopicFilter topic() const;
    quint8 qos() const;
    bool isSharedSubscription() const;
    QString sharedSubscriptionName() const;

Q_SIGNALS:
    void stateChanged(QMqttSubscription::SubscriptionState state);
    void qosChanged(quint8 qos);
    void messageReceived(QMqttMessage msg);

public Q_SLOTS:
    void unsubscribe();

private:
    friend class QMqttConnection;
    explicit QMqttSubscription(QObject *parent = nullptr);
    void setState(SubscriptionState state);
    void setQos(quint8 qos);

    QMqttClient *m_client = nullptr;
    QMqttTopicFilter m_topic;
    SubscriptionState m_state = Unsubscribed;
    quint8 m_qos = 0;
};
