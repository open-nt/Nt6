// MIT License - see LICENSE
#pragma once

#include <QtCore/qeventloop.h>
#include <QtCore/qobject.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

class QSocketNotifier;
class QAbstractEventDispatcher;

namespace QtPrivate {

// Posted-event queue, timer table, fd notifiers and loop registry of one thread (POSIX). Any
// thread may post events and wake it up; the owning thread runs process().
class Dispatcher {
public:
    using Clock = std::chrono::steady_clock;

    Dispatcher();
    ~Dispatcher();
    Dispatcher(const Dispatcher &) = delete;
    Dispatcher &operator=(const Dispatcher &) = delete;

    // Queues the event unless `receiver` no longer belongs to `owner`; returns false then and
    // leaves the event with the caller.
    bool post(QObject *receiver, QEvent *event, const void *owner);
    // type 0 matches every event.
    void removePosted(QObject *receiver, int type);

    // Timer and notifier ids are unique across all dispatchers.
    static int allocateTimerId();
    int startTimer(QObject *obj, std::chrono::milliseconds interval);
    // Starts a timer under an id from allocateTimerId().
    void startTimerWithId(int id, QObject *obj, std::chrono::milliseconds interval);
    // Both return whether a timer was removed.
    bool killTimer(int id);
    bool killTimers(QObject *obj);
    struct TimerInfo {
        int id;
        std::chrono::milliseconds interval;
    };
    std::vector<TimerInfo> timersOf(QObject *obj);
    // Milliseconds until the timer fires; -1 if there is no such timer.
    long long remainingTime(int id);

    // type: 0 read, 1 write, 2 exception (QSocketNotifier::Type).
    int addNotifier(QSocketNotifier *obj, int fd, int type);
    void removeNotifier(int id);
    void setNotifierEnabled(int id, bool enabled);

    // One pass over fd activity, due timers and queued events; with `wait`, blocks until there is work or `*stop`.
    bool process(bool wait, const std::atomic<bool> *stop);
    // Delivers the queued events of `receiver` (every receiver if null) that match `type` (0 matches all),
    // including DeferredDelete.
    void sendPosted(QObject *receiver, int type);
    void wakeUp();
    // Makes the blocking process() that runs, or the next one, return without waiting for work.
    void interrupt();
    // The object that reports aboutToBlock() and awake(); it must outlive the dispatcher's use.
    void setObserver(QAbstractEventDispatcher *observer) { observer_.store(observer); }

    void addLoop(QEventLoop *loop);
    void removeLoop(QEventLoop *loop);
    void exitAll(int code);
    int loopLevel();

    // Moves the timers, notifiers and queued events of `objs` to `to`.
    void migrate(Dispatcher &to, const std::vector<QObject *> &objs);

private:
    struct Posted {
        QObject *receiver;
        QEvent *event;
        std::uint64_t seq;
    };
    struct Timer {
        int id;
        QObject *obj;
        std::chrono::milliseconds interval;
        Clock::time_point next;
    };
    struct Notifier {
        int id;
        QSocketNotifier *obj;
        int fd;
        int type;
        bool enabled;
    };

    static void deliver(const Posted &p);

    std::mutex m_;
    std::deque<Posted> queue_;
    std::vector<Timer> timers_;
    std::vector<Notifier> notifiers_;
    std::vector<QEventLoop *> loops_;
    std::uint64_t seq_ = 0;
    std::atomic<bool> interrupted_{false};
    std::atomic<QAbstractEventDispatcher *> observer_{nullptr};
    int wakeRead_ = -1;
    int wakeWrite_ = -1;
};

} // namespace QtPrivate
