// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace QtPrivate {

struct ThreadData;

struct ConnectionData {
    QObject *sender = nullptr;
    QObject *receiver = nullptr;
    SignalId signal;
    std::function<void(void *const *)> invoke;
    std::string method; // normalized signature of the receiving method, for string-based connections
    Qt::ConnectionType type = Qt::AutoConnection;
    std::atomic<bool> alive{true};
};

struct ObjectData {
    QObject *parent = nullptr;
    QObjectList children;
    std::vector<std::shared_ptr<ConnectionData>> outgoing;
    std::vector<std::shared_ptr<ConnectionData>> incoming;
    std::atomic<bool> blocked{false};
    QString objectName;
    std::shared_ptr<PointerData> guard;
    std::atomic<ThreadData *> threadData{nullptr};
    bool ownsThreadRef = true;
    std::vector<std::pair<QByteArray, QVariant>> dynamicProperties;
};

} // namespace QtPrivate

namespace QtPrivate {

// Lets the emitting thread of a Qt::BlockingQueuedConnection wait until the slot has run or
// the event has been discarded.
class BlockingLatch {
public:
    void release();
    void wait();

private:
    std::mutex m_;
    std::condition_variable cv_;
    bool done_ = false;
};

// Disconnecting by signal, receiver and receiving method; a null argument matches everything.
bool disconnectConnections(const QObject *sender, const SignalId *id, const QObject *receiver,
                           const std::string *method);
void setConnectionMethod(const std::shared_ptr<ConnectionData> &c, const std::string &method);

} // namespace QtPrivate

// Delivers a queued signal emission to its receiver's event loop.
class QMetaCallEvent : public QEvent {
public:
    QMetaCallEvent(std::shared_ptr<QtPrivate::ConnectionData> conn, QtPrivate::QueuedArgs args,
                   std::shared_ptr<QtPrivate::BlockingLatch> latch = nullptr)
        : QEvent(MetaCall), m_conn(std::move(conn)), m_args(std::move(args)), m_latch(std::move(latch)) {}
    // Runs `function` in the receiver's thread; `latch`, if given, is released afterwards.
    explicit QMetaCallEvent(std::function<void()> function, std::shared_ptr<QtPrivate::BlockingLatch> latch = nullptr)
        : QEvent(MetaCall), m_latch(std::move(latch)), m_function(std::move(function)) {}
    ~QMetaCallEvent() override;
    void place();

private:
    std::shared_ptr<QtPrivate::ConnectionData> m_conn;
    QtPrivate::QueuedArgs m_args;
    std::shared_ptr<QtPrivate::BlockingLatch> m_latch;
    std::function<void()> m_function;
};
