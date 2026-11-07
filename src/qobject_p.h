// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>

#include <functional>
#include <memory>
#include <vector>

namespace QtPrivate {

struct ConnectionData {
    QObject *sender = nullptr;
    QObject *receiver = nullptr;
    SignalId signal;
    std::function<void(void *const *)> invoke;
    Qt::ConnectionType type = Qt::AutoConnection;
    bool alive = true;
};

struct ObjectData {
    QObject *parent = nullptr;
    QObjectList children;
    std::vector<std::shared_ptr<ConnectionData>> outgoing;
    std::vector<std::shared_ptr<ConnectionData>> incoming;
    bool blocked = false;
    QString objectName;
    std::shared_ptr<PointerData> guard;
};

} // namespace QtPrivate

// Delivers a queued signal emission to its receiver's event loop.
class QMetaCallEvent : public QEvent {
public:
    QMetaCallEvent(std::shared_ptr<QtPrivate::ConnectionData> conn, QtPrivate::QueuedArgs args)
        : QEvent(MetaCall), m_conn(std::move(conn)), m_args(std::move(args)) {}
    void place();

private:
    std::shared_ptr<QtPrivate::ConnectionData> m_conn;
    QtPrivate::QueuedArgs m_args;
};
