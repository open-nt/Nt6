// MIT License - see LICENSE
#pragma once

#include <QtCore/qeventloop.h>
#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>

#include <chrono>

class QAbstractNativeEventFilter;
class QByteArray;
class QSocketNotifier;
class QThread;

namespace QtPrivate {
struct ThreadData;
} // namespace QtPrivate

class QAbstractEventDispatcher : public QObject {
    Q_OBJECT
public:
    /*!
        \typedef QAbstractEventDispatcher::Duration
        \since 6.8
        The type of the timer intervals of the chrono-based functions: \c{std::chrono::nanoseconds}.
    */
    using Duration = std::chrono::nanoseconds;

    struct TimerInfo {
        int timerId;
        int interval;
        Qt::TimerType timerType;

        TimerInfo(int id, int i, Qt::TimerType type) : timerId(id), interval(i), timerType(type) {}
    };

    struct TimerInfoV2 {
        Duration interval;
        Qt::TimerId timerId;
        Qt::TimerType timerType;
    };

    explicit QAbstractEventDispatcher(QObject *parent = nullptr);
    ~QAbstractEventDispatcher() override;

    static QAbstractEventDispatcher *instance(QThread *thread = nullptr);

    virtual bool processEvents(QEventLoop::ProcessEventsFlags flags);

    virtual void registerSocketNotifier(QSocketNotifier *notifier);
    virtual void unregisterSocketNotifier(QSocketNotifier *notifier);

    int registerTimer(qint64 interval, Qt::TimerType timerType, QObject *object);
    Qt::TimerId registerTimer(Duration interval, Qt::TimerType timerType, QObject *object);
    virtual void registerTimer(int timerId, qint64 interval, Qt::TimerType timerType, QObject *object);
    void registerTimer(Qt::TimerId timerId, Duration interval, Qt::TimerType timerType, QObject *object);
    virtual bool unregisterTimer(int timerId);
    bool unregisterTimer(Qt::TimerId timerId);
    virtual bool unregisterTimers(QObject *object);
    virtual QList<TimerInfo> registeredTimers(QObject *object) const;
    QList<TimerInfoV2> timersForObject(QObject *object) const;
    virtual int remainingTime(int timerId);
    Duration remainingTime(Qt::TimerId timerId) const;

    virtual void wakeUp();
    virtual void interrupt();
    void flush();

    virtual void startingUp();
    virtual void closingDown();

    void installNativeEventFilter(QAbstractNativeEventFilter *filterObj);
    void removeNativeEventFilter(QAbstractNativeEventFilter *filterObj);
    bool filterNativeEvent(const QByteArray &eventType, void *message, qintptr *result);

    static int allocateTimerId();
    static void releaseTimerId(int timerId);

signals:
    /*!
        This signal is emitted before the event dispatcher waits for events, from the thread that waits.
        Slots must be quick, since the thread is about to sleep and does not handle events before they return.

        \sa awake()
    */
    void aboutToBlock() { emitSignal(&QAbstractEventDispatcher::aboutToBlock); }
    /*!
        This signal is emitted after the event dispatcher woke up from waiting for events, from the thread
        that waited, and before it handles them.

        \sa aboutToBlock()
    */
    void awake() { emitSignal(&QAbstractEventDispatcher::awake); }

private:
    friend class QAbstractEventDispatcherV2;
    QAbstractEventDispatcher(QtPrivate::ThreadData *owner, int);
    QAbstractEventDispatcher(QObject *parent, bool v2);

    bool m_owned = false;
    bool m_isV2 = false;
};

class QAbstractEventDispatcherV2 : public QAbstractEventDispatcher {
    Q_OBJECT
public:
    explicit QAbstractEventDispatcherV2(QObject *parent = nullptr);
    ~QAbstractEventDispatcherV2() override;

    using QAbstractEventDispatcher::registerTimer;
    using QAbstractEventDispatcher::remainingTime;
    using QAbstractEventDispatcher::unregisterTimer;

    void registerTimer(int timerId, qint64 interval, Qt::TimerType timerType, QObject *object) final;
    virtual void registerTimer(Qt::TimerId timerId, Duration interval, Qt::TimerType timerType, QObject *object);
    bool unregisterTimer(int timerId) final;
    virtual bool unregisterTimer(Qt::TimerId timerId);
    QList<TimerInfo> registeredTimers(QObject *object) const final;
    virtual QList<TimerInfoV2> timersForObject(QObject *object) const;
    int remainingTime(int timerId) final;
    virtual Duration remainingTime(Qt::TimerId timerId) const;

    virtual bool processEventsWithDeadline(QEventLoop::ProcessEventsFlags flags, QDeadlineTimer deadline);
};
