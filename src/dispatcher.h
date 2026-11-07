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

namespace QtPrivate {

// Process-wide posted-event queue, timer table, fd notifiers and loop registry (POSIX).
class Dispatcher {
public:
    using Clock = std::chrono::steady_clock;

    static Dispatcher &instance();

    void post(QObject *receiver, QEvent *event);
    // type 0 matches every event.
    void removePosted(QObject *receiver, int type);

    int startTimer(QObject *obj, std::chrono::milliseconds interval);
    void killTimer(int id);
    void killTimers(QObject *obj);

    // type: 0 read, 1 write, 2 exception (QSocketNotifier::Type).
    int addNotifier(QSocketNotifier *obj, int fd, int type);
    void removeNotifier(int id);
    void setNotifierEnabled(int id, bool enabled);

    // One pass over fd activity, due timers and queued events; with `wait`, blocks until there is work or `*stop`.
    bool process(bool wait, const std::atomic<bool> *stop);
    void wakeUp();

    void addLoop(QEventLoop *loop);
    void removeLoop(QEventLoop *loop);
    void exitAll(int code);

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

    Dispatcher();

    std::mutex m_;
    std::deque<Posted> queue_;
    std::vector<Timer> timers_;
    std::vector<Notifier> notifiers_;
    std::vector<QEventLoop *> loops_;
    std::uint64_t seq_ = 0;
    int nextTimerId_ = 1;
    int nextNotifierId_ = 1;
    int wakeRead_ = -1;
    int wakeWrite_ = -1;
};

} // namespace QtPrivate
