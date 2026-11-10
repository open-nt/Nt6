// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>

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
    SignalId slot; // the member function slot, when hasSlot is set, for Qt::UniqueConnection
    bool hasSlot = false;
    bool singleShot = false; // Qt::SingleShotConnection
    bool fired = false;      // a single-shot connection has been emitted; guarded by the signal-slot lock
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
    std::vector<QPointer<QObject>> eventFilters; // the most recently installed last
    bool deleting = false;                       // the destructor runs: children send it no ChildRemoved
};

// Runs the event filters for `event` to `receiver`: those of the application, then those of `receiver`. Returns true
// if a filter stopped the event.
bool runEventFilters(QObject *receiver, QEvent *event);

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
// Removes a single-shot connection after its slot was called.
void finishSingleShot(const std::shared_ptr<ConnectionData> &c);

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
