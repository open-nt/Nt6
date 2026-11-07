// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace QtPrivate {

struct ThreadData;

struct ConnectionData {
    QObject *sender = nullptr;
    QObject *receiver = nullptr;
    SignalId signal;
    std::function<void(void *const *)> invoke;
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

} // namespace QtPrivate

// Delivers a queued signal emission to its receiver's event loop.
class QMetaCallEvent : public QEvent {
public:
    QMetaCallEvent(std::shared_ptr<QtPrivate::ConnectionData> conn, QtPrivate::QueuedArgs args,
                   std::shared_ptr<QtPrivate::BlockingLatch> latch = nullptr)
        : QEvent(MetaCall), m_conn(std::move(conn)), m_args(std::move(args)), m_latch(std::move(latch)) {}
    ~QMetaCallEvent() override;
    void place();

private:
    std::shared_ptr<QtPrivate::ConnectionData> m_conn;
    QtPrivate::QueuedArgs m_args;
    std::shared_ptr<QtPrivate::BlockingLatch> m_latch;
};
