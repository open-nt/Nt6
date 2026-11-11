// MIT License - see LICENSE
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#if defined(__linux__) || defined(__ZEPHYR__)
#include <sys/eventfd.h>
#endif
// Zephyr has no exec() to close the descriptor on.
#ifndef EFD_CLOEXEC
#define EFD_CLOEXEC 0
#endif

#include "dispatcher.h"
#include "qobject_p.h"

#include <QtCore/qabstracteventdispatcher.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qsocketnotifier.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <memory>
#include <unordered_set>

namespace QtPrivate {

namespace {

std::atomic<int> g_nextTimerId{1};
std::atomic<int> g_nextNotifierId{1};

} // namespace

Dispatcher::Dispatcher() {
#if defined(__linux__) || defined(__ZEPHYR__)
    wakeRead_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    wakeWrite_ = wakeRead_;
#else
    int fds[2];
    if (::pipe(fds) == 0) {
        for (int fd : fds) {
            ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);
            ::fcntl(fd, F_SETFD, FD_CLOEXEC);
        }
        wakeRead_ = fds[0];
        wakeWrite_ = fds[1];
    }
#endif
}

Dispatcher::~Dispatcher() {
    for (const Posted &p : queue_)
        delete p.event;
    if (wakeRead_ >= 0)
        ::close(wakeRead_);
    if (wakeWrite_ >= 0 && wakeWrite_ != wakeRead_)
        ::close(wakeWrite_);
}

void Dispatcher::wakeUp() {
#if defined(__linux__) || defined(__ZEPHYR__)
    const std::uint64_t one = 1;
    const void *buf = &one;
    const std::size_t len = sizeof one;
#else
    const char one = 1;
    const void *buf = &one;
    const std::size_t len = 1;
#endif
    ssize_t r;
    do {
        r = ::write(wakeWrite_, buf, len);
    } while (r < 0 && errno == EINTR);
}

bool Dispatcher::post(QObject *receiver, QEvent *event, const void *owner) {
    {
        std::lock_guard<std::mutex> lk(m_);
        // The check happens under the lock so that migrate() either sees this event or the
        // sender sees the new owner.
        if (Access::d(receiver).threadData.load() != owner)
            return false;
        queue_.push_back({receiver, event, ++seq_});
    }
    wakeUp();
    return true;
}

void Dispatcher::removePosted(QObject *receiver, int type) {
    std::vector<QEvent *> dead;
    {
        std::lock_guard<std::mutex> lk(m_);
        for (auto it = queue_.begin(); it != queue_.end();) {
            if (it->receiver == receiver && (type == 0 || it->event->type() == type)) {
                dead.push_back(it->event);
                it = queue_.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (QEvent *e : dead)
        delete e;
}

int Dispatcher::allocateTimerId() { return g_nextTimerId.fetch_add(1); }

int Dispatcher::startTimer(QObject *obj, std::chrono::milliseconds interval) {
    const int id = allocateTimerId();
    startTimerWithId(id, obj, interval);
    return id;
}

void Dispatcher::startTimerWithId(int id, QObject *obj, std::chrono::milliseconds interval) {
    {
        std::lock_guard<std::mutex> lk(m_);
        timers_.push_back({id, obj, interval, Clock::now() + interval});
    }
    wakeUp();
}

bool Dispatcher::killTimer(int id) {
    std::lock_guard<std::mutex> lk(m_);
    const auto it = std::remove_if(timers_.begin(), timers_.end(), [id](const Timer &t) { return t.id == id; });
    const bool found = it != timers_.end();
    timers_.erase(it, timers_.end());
    return found;
}

bool Dispatcher::killTimers(QObject *obj) {
    std::lock_guard<std::mutex> lk(m_);
    const auto it = std::remove_if(timers_.begin(), timers_.end(), [obj](const Timer &t) { return t.obj == obj; });
    const bool found = it != timers_.end();
    timers_.erase(it, timers_.end());
    return found;
}

std::vector<Dispatcher::TimerInfo> Dispatcher::timersOf(QObject *obj) {
    std::vector<TimerInfo> out;
    std::lock_guard<std::mutex> lk(m_);
    for (const Timer &t : timers_)
        if (t.obj == obj)
            out.push_back({t.id, t.interval});
    return out;
}

long long Dispatcher::remainingTime(int id) {
    std::lock_guard<std::mutex> lk(m_);
    for (const Timer &t : timers_) {
        if (t.id == id) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(t.next - Clock::now()).count();
            return left < 0 ? 0 : left;
        }
    }
    return -1;
}

void Dispatcher::interrupt() {
    interrupted_ = true;
    wakeUp();
}

int Dispatcher::addNotifier(QSocketNotifier *obj, int fd, int type) {
    const int id = g_nextNotifierId.fetch_add(1);
    {
        std::lock_guard<std::mutex> lk(m_);
        notifiers_.push_back({id, obj, fd, type, true});
    }
    wakeUp();
    return id;
}

void Dispatcher::removeNotifier(int id) {
    std::lock_guard<std::mutex> lk(m_);
    notifiers_.erase(std::remove_if(notifiers_.begin(), notifiers_.end(), [id](const Notifier &n) { return n.id == id; }),
                     notifiers_.end());
}

void Dispatcher::setNotifierEnabled(int id, bool enabled) {
    {
        std::lock_guard<std::mutex> lk(m_);
        for (Notifier &n : notifiers_)
            if (n.id == id)
                n.enabled = enabled;
    }
    wakeUp();
}

void Dispatcher::deliver(const Posted &p) {
    std::unique_ptr<QEvent> owned(p.event);
    if (p.event->type() == QEvent::DeferredDelete)
        delete p.receiver;
    else
        QCoreApplication::sendEvent(p.receiver, p.event);
}

bool Dispatcher::process(bool wait, const std::atomic<bool> *stop) {
    std::unique_lock<std::mutex> lk(m_);
    std::vector<int> ready; // notifier ids with fd activity

    for (;;) {
        if (wait && stop && stop->load())
            return false;
        if (wait && interrupted_.exchange(false))
            return false;

        const auto now = Clock::now();
        bool timerDue = false;
        Clock::time_point next{};
        if (!timers_.empty()) {
            next = std::min_element(timers_.begin(), timers_.end(),
                                    [](const Timer &a, const Timer &b) { return a.next < b.next; })->next;
            timerDue = next <= now;
        }

        int timeout;
        if (!wait || !queue_.empty() || timerDue)
            timeout = 0;
        else if (timers_.empty())
            timeout = -1;
        else {
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(next - now).count() + 1;
            timeout = ms > INT_MAX ? INT_MAX : static_cast<int>(ms);
        }

        std::vector<pollfd> fds;
        std::vector<int> ids;
        fds.push_back({wakeRead_, POLLIN, 0});
        for (const Notifier &n : notifiers_) {
            if (!n.enabled)
                continue;
            short ev = n.type == 0 ? POLLIN : n.type == 1 ? POLLOUT : POLLPRI;
            fds.push_back({n.fd, ev, 0});
            ids.push_back(n.id);
        }

        const bool needPoll = wait || fds.size() > 1;
        int r = 0;
        if (needPoll) {
            lk.unlock();
            QAbstractEventDispatcher *observer = timeout != 0 ? observer_.load() : nullptr;
            if (observer)
                observer->aboutToBlock();
            r = ::poll(fds.data(), static_cast<nfds_t>(fds.size()), timeout);
            if (observer)
                observer->awake();
            if (r > 0 && (fds[0].revents & POLLIN)) {
                std::uint64_t buf[8];
                // The wake descriptor is non-blocking and the lock is released above.
                while (::read(wakeRead_, buf, sizeof buf) > 0) { // NOLINT(clang-analyzer-unix.BlockInCriticalSection)
                }
            }
            lk.lock();
        }

        ready.clear();
        if (r > 0) {
            for (std::size_t i = 1; i < fds.size(); ++i) {
                if (fds[i].revents == 0)
                    continue;
                ready.push_back(ids[i - 1]);
            }
        }
        if (!wait)
            break;
        if (!ready.empty() || !queue_.empty())
            break;
        if (!timers_.empty()) {
            const auto soonest = std::min_element(timers_.begin(), timers_.end(),
                                                  [](const Timer &a, const Timer &b) { return a.next < b.next; })->next;
            if (soonest <= Clock::now())
                break;
        }
    }

    bool did = false;
    const std::uint64_t maxSeq = seq_;

    for (int id : ready) {
        auto it = std::find_if(notifiers_.begin(), notifiers_.end(), [id](const Notifier &n) { return n.id == id; });
        if (it == notifiers_.end() || !it->enabled)
            continue;
        QSocketNotifier *obj = it->obj;
        lk.unlock();
        QEvent ev(QEvent::SockAct);
        QCoreApplication::sendEvent(static_cast<QObject *>(obj), &ev);
        lk.lock();
        did = true;
    }

    const auto now = Clock::now();
    std::vector<int> due;
    for (Timer &t : timers_) {
        if (t.next <= now) {
            due.push_back(t.id);
            t.next = now + t.interval;
        }
    }
    for (int id : due) {
        auto it = std::find_if(timers_.begin(), timers_.end(), [id](const Timer &t) { return t.id == id; });
        if (it == timers_.end())
            continue;
        QObject *obj = it->obj;
        lk.unlock();
        QTimerEvent ev(id);
        QCoreApplication::sendEvent(obj, &ev);
        lk.lock();
        did = true;
    }

    // Events posted while handlers run wait for the next pass.
    while (!queue_.empty() && queue_.front().seq <= maxSeq) {
        const Posted p = queue_.front();
        queue_.pop_front();
        lk.unlock();
        deliver(p);
        lk.lock();
        did = true;
    }
    return did;
}

void Dispatcher::sendPosted(QObject *receiver, int type) {
    std::unique_lock<std::mutex> lk(m_);
    const std::uint64_t maxSeq = seq_;
    for (;;) {
        // Deliver one at a time: a handler may delete other receivers, which drops their queued events.
        auto it = std::find_if(queue_.begin(), queue_.end(), [&](const Posted &p) {
            if (p.seq > maxSeq || (receiver && p.receiver != receiver))
                return false;
            const int t = p.event->type();
            return type == 0 ? t != QEvent::DeferredDelete : t == type;
        });
        if (it == queue_.end())
            return;
        const Posted p = *it;
        queue_.erase(it);
        lk.unlock();
        deliver(p);
        lk.lock();
    }
}

void Dispatcher::addLoop(QEventLoop *loop) {
    std::lock_guard<std::mutex> lk(m_);
    loops_.push_back(loop);
}

void Dispatcher::removeLoop(QEventLoop *loop) {
    std::lock_guard<std::mutex> lk(m_);
    loops_.erase(std::remove(loops_.begin(), loops_.end(), loop), loops_.end());
}

// Holding the lock keeps the loops alive: removeLoop() waits for it. exit() only touches atomics
// and the wake descriptor.
void Dispatcher::exitAll(int code) {
    std::lock_guard<std::mutex> lk(m_);
    for (QEventLoop *l : loops_)
        l->exit(code);
}

int Dispatcher::loopLevel() {
    std::lock_guard<std::mutex> lk(m_);
    return static_cast<int>(loops_.size());
}

void Dispatcher::migrate(Dispatcher &to, const std::vector<QObject *> &objs) {
    const std::unordered_set<const QObject *> set(objs.begin(), objs.end());
    std::vector<Timer> timers;
    std::vector<Notifier> notifiers;
    std::vector<Posted> events;
    {
        std::lock_guard<std::mutex> lk(m_);
        auto moved = [&set](const QObject *o) { return set.count(o) != 0; };
        for (auto it = timers_.begin(); it != timers_.end();) {
            if (moved(it->obj)) {
                timers.push_back(*it);
                it = timers_.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = notifiers_.begin(); it != notifiers_.end();) {
            if (moved(it->obj)) {
                notifiers.push_back(*it);
                it = notifiers_.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = queue_.begin(); it != queue_.end();) {
            if (moved(it->receiver)) {
                events.push_back(*it);
                it = queue_.erase(it);
            } else {
                ++it;
            }
        }
    }
    {
        std::lock_guard<std::mutex> lk(to.m_);
        to.timers_.insert(to.timers_.end(), timers.begin(), timers.end());
        to.notifiers_.insert(to.notifiers_.end(), notifiers.begin(), notifiers.end());
        for (Posted &p : events) {
            p.seq = ++to.seq_;
            to.queue_.push_back(p);
        }
    }
    to.wakeUp();
}

} // namespace QtPrivate
