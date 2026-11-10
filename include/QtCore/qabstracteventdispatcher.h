// MIT License - see LICENSE
#pragma once

#include <QtCore/qeventloop.h>
#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>

class QSocketNotifier;
class QThread;

namespace QtPrivate {
struct ThreadData;
} // namespace QtPrivate

class QAbstractEventDispatcher : public QObject {
    Q_OBJECT
public:
    struct TimerInfo {
        int timerId;
        int interval;
        Qt::TimerType timerType;

        TimerInfo(int id, int i, Qt::TimerType type) : timerId(id), interval(i), timerType(type) {}
    };

    explicit QAbstractEventDispatcher(QObject *parent = nullptr);
    ~QAbstractEventDispatcher() override;

    static QAbstractEventDispatcher *instance(QThread *thread = nullptr);

    bool processEvents(QEventLoop::ProcessEventsFlags flags);

    void registerSocketNotifier(QSocketNotifier *notifier);
    void unregisterSocketNotifier(QSocketNotifier *notifier);

    int registerTimer(qint64 interval, Qt::TimerType timerType, QObject *object);
    void registerTimer(int timerId, qint64 interval, Qt::TimerType timerType, QObject *object);
    bool unregisterTimer(int timerId);
    bool unregisterTimers(QObject *object);
    QList<TimerInfo> registeredTimers(QObject *object) const;
    int remainingTime(int timerId);

    void wakeUp();
    void interrupt();
    void flush();

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
    QAbstractEventDispatcher(QtPrivate::ThreadData *owner, int);

    bool m_owned = false;
};
