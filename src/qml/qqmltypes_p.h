// MIT License - see LICENSE
//
// The types of the QtQml module that are written in C++: Timer and Connections. QtObject is QObject, and
// Component is QQmlComponent.
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtCore/qtimer.h>
#include <QtQml/qqmlparserstatus.h>

#include "qqmlengine_p.h"

#include <vector>

class QQmlTimer : public QObject, public QQmlParserStatus {
    Q_OBJECT
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)
    Q_PROPERTY(bool running READ isRunning WRITE setRunning NOTIFY runningChanged)
    Q_PROPERTY(bool repeat READ isRepeating WRITE setRepeating NOTIFY repeatChanged)
    Q_PROPERTY(bool triggeredOnStart READ triggeredOnStart WRITE setTriggeredOnStart NOTIFY triggeredOnStartChanged)
public:
    explicit QQmlTimer(QObject *parent = nullptr);

    int interval() const { return m_interval; }
    void setInterval(int interval);
    bool isRunning() const { return m_running; }
    void setRunning(bool running);
    bool isRepeating() const { return m_repeat; }
    void setRepeating(bool repeat);
    bool triggeredOnStart() const { return m_triggeredOnStart; }
    void setTriggeredOnStart(bool on);

    void classBegin() override;
    void componentComplete() override;

public slots:
    void start();
    void stop();
    void restart();

signals:
    void triggered() { emitSignal(&QQmlTimer::triggered); }
    void intervalChanged() { emitSignal(&QQmlTimer::intervalChanged); }
    void runningChanged() { emitSignal(&QQmlTimer::runningChanged); }
    void repeatChanged() { emitSignal(&QQmlTimer::repeatChanged); }
    void triggeredOnStartChanged() { emitSignal(&QQmlTimer::triggeredOnStartChanged); }

private:
    void update();
    void fire();

    QTimer m_timer;
    int m_interval = 1000;
    bool m_running = false;
    bool m_repeat = false;
    bool m_triggeredOnStart = false;
    bool m_complete = true;
};

class QQmlConnections : public QObject, public QQmlParserStatus {
    Q_OBJECT
    Q_PROPERTY(QObject *target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool ignoreUnknownSignals READ ignoreUnknownSignals WRITE setIgnoreUnknownSignals)
public:
    explicit QQmlConnections(QObject *parent = nullptr);

    QObject *target() const { return m_target; }
    void setTarget(QObject *target);
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    bool ignoreUnknownSignals() const { return m_ignoreUnknown; }
    void setIgnoreUnknownSignals(bool ignore) { m_ignoreUnknown = ignore; }

    void classBegin() override;
    void componentComplete() override;

    // The handlers, "onClicked" or "function onClicked(mouse)", by signal name; set by the creator.
    std::vector<std::pair<QString, QtQmlPrivate::Handler>> handlers;
    QString location;

signals:
    void targetChanged() { emitSignal(&QQmlConnections::targetChanged); }
    void enabledChanged() { emitSignal(&QQmlConnections::enabledChanged); }

private:
    void connectAll();
    void disconnectAll();

    QPointer<QObject> m_target;
    bool m_targetSet = false;
    bool m_enabled = true;
    bool m_ignoreUnknown = false;
    bool m_complete = true;
    std::vector<QMetaObject::Connection> m_connections;
    std::vector<QString> m_declaredConnections; // handlers added to signals declared in QML
};
