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
#include <QtCore/qlogging.h>
#include <QtCore/qsocketnotifier.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace QtPrivate {

namespace {

std::atomic<int> g_nextTimerId{1};
std::atomic<int> g_nextNotifierId{1};

// How much one poll() can wait for. Zephyr fixes it when the system is configured: CONFIG_ZVFS_POLL_MAX, only 3 by
// default, counts the events the kernel waits for, and makes poll() fail with more. A descriptor takes one event
// for each of POLLIN and POLLOUT that it is polled for (the latter for TCP sockets only, so counting it is safe),
// and the wake-up descriptor takes one. Elsewhere poll() is not limited.
#if defined(__ZEPHYR__) && defined(CONFIG_ZVFS_POLL_MAX)
constexpr std::size_t kPollLimit = CONFIG_ZVFS_POLL_MAX > 2 ? CONFIG_ZVFS_POLL_MAX : 3; // wake-up and a socket
#else
constexpr std::size_t kPollLimit = static_cast<std::size_t>(-1);
#endif

// How long a wait on one group of descriptors lasts before the next group is tried, when there are too many
// descriptors for one poll().
constexpr int kSliceMs = 10;

std::size_t eventCost(const pollfd &f) { return ((f.events & POLLIN) ? 1u : 0u) + ((f.events & POLLOUT) ? 1u : 0u); }

// poll() for any number of descriptors. `fds[0]` is the descriptor that wakes the loop up. If they fit in one call
// this is just that. Otherwise the descriptors are polled in groups, each with the wake-up descriptor: first all
// groups without waiting, then, until something is ready or the time is up, one group at a time for a short slice,
// so that a descriptor is served within a few slices at worst. Returns the number of descriptors that are ready, or
// -1 with errno set.
int pollMany(std::vector<pollfd> &fds, int timeoutMs) {
    std::size_t total = 0;
    for (const pollfd &f : fds)
        total += eventCost(f);
    if (total <= kPollLimit)
        return ::poll(fds.data(), static_cast<nfds_t>(fds.size()), timeoutMs);

    // The groups, as indexes into fds. The wake-up descriptor costs one event in each, and a descriptor never
    // costs more than two, which fits as the limit is at least 3.
    std::vector<std::vector<std::size_t>> groups;
    std::size_t used = kPollLimit; // forces the first group to start
    for (std::size_t i = 1; i < fds.size(); ++i) {
        const std::size_t cost = eventCost(fds[i]);
        if (used + cost > kPollLimit) {
            groups.emplace_back();
            used = eventCost(fds[0]);
        }
        groups.back().push_back(i);
        used += cost;
    }

    std::vector<pollfd> group;
    const auto pollGroup = [&](const std::vector<std::size_t> &members, int wait) {
        group.assign(1, fds[0]);
        for (std::size_t index : members)
            group.push_back(fds[index]);
        const int r = ::poll(group.data(), static_cast<nfds_t>(group.size()), wait);
        if (r < 0)
            return r;
        fds[0].revents = static_cast<short>(fds[0].revents | group[0].revents);
        for (std::size_t k = 0; k < members.size(); ++k)
            fds[members[k]].revents = group[k + 1].revents;
        return r;
    };
    const auto countReady = [&fds] {
        int n = 0;
        for (const pollfd &f : fds)
            n += f.revents != 0 ? 1 : 0;
        return n;
    };

    const auto start = std::chrono::steady_clock::now();
    std::size_t next = 0;
    for (;;) {
        for (pollfd &f : fds)
            f.revents = 0;
        for (const auto &members : groups) {
            if (pollGroup(members, 0) < 0)
                return -1;
        }
        if (int n = countReady(); n > 0)
            return n;
        int slice = kSliceMs;
        if (timeoutMs >= 0) {
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeoutMs)
                return 0;
            slice = static_cast<int>(std::min<long long>(slice, timeoutMs - elapsed));
        }
        // Blocks on one group, and on the wake-up descriptor, for the slice.
        if (pollGroup(groups[next], slice) < 0)
            return -1;
        if (int n = countReady(); n > 0)
            return n;
        next = (next + 1) % groups.size();
    }
}

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
    notifiers_.erase(
        std::remove_if(notifiers_.begin(), notifiers_.end(), [id](const Notifier &n) { return n.id == id; }),
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
            next = std::min_element(timers_.begin(), timers_.end(), [](const Timer &a, const Timer &b) {
                       return a.next < b.next;
                   })->next;
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

        // One entry per descriptor, however many notifiers watch it: a socket usually has a notifier for reading
        // and another for writing, and poll() may take few entries (see kPollLimit).
        struct Watch {
            std::size_t slot; // in fds
            int id;
            short events;
        };
        std::vector<pollfd> fds;
        std::vector<Watch> watches;
        std::unordered_map<int, std::size_t> slotOf;
        fds.push_back({wakeRead_, POLLIN, 0});
        for (const Notifier &n : notifiers_) {
            if (!n.enabled)
                continue;
            const short ev = n.type == 0 ? POLLIN : n.type == 1 ? POLLOUT : POLLPRI;
            const auto found = slotOf.find(n.fd);
            std::size_t slot;
            if (found == slotOf.end()) {
                slot = fds.size();
                slotOf.emplace(n.fd, slot);
                fds.push_back({n.fd, 0, 0});
            } else {
                slot = found->second;
            }
            fds[slot].events = static_cast<short>(fds[slot].events | ev);
            watches.push_back({slot, n.id, ev});
        }

        const bool needPoll = wait || fds.size() > 1;
        int r = 0;
        if (needPoll) {
            lk.unlock();
            QAbstractEventDispatcher *observer = timeout != 0 ? observer_.load() : nullptr;
            if (observer)
                observer->aboutToBlock();
            r = pollMany(fds, timeout);
            if (observer)
                observer->awake();
            if (r < 0 && errno != EINTR) {
                // No socket is served while this lasts, so say why once.
                static std::atomic<bool> reported{false};
                if (!reported.exchange(true))
                    qWarning("QEventLoop: poll() of %d descriptors failed: %s", static_cast<int>(fds.size()),
                             std::strerror(errno));
            }
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
            // An error or a hang-up is for every notifier of the descriptor; otherwise a notifier is told only
            // about the events that it asked for.
            constexpr short kProblem = POLLERR | POLLHUP | POLLNVAL;
            for (const Watch &w : watches) {
                const short happened = fds[w.slot].revents;
                if (happened & (w.events | kProblem))
                    ready.push_back(w.id);
            }
        }
        if (!wait)
            break;
        if (!ready.empty() || !queue_.empty())
            break;
        if (!timers_.empty()) {
            const auto soonest = std::min_element(timers_.begin(), timers_.end(), [](const Timer &a, const Timer &b) {
                                     return a.next < b.next;
                                 })->next;
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
