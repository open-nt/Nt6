// MIT License - see LICENSE
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include "dispatcher.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qsocketnotifier.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <memory>

namespace QtPrivate {

Dispatcher::Dispatcher() {
    int fds[2];
    if (::pipe2(fds, O_NONBLOCK | O_CLOEXEC) == 0) {
        wakeRead_ = fds[0];
        wakeWrite_ = fds[1];
    }
}

Dispatcher &Dispatcher::instance() {
    // Leaked on purpose: QObjects with static storage may outlive a normal singleton.
    static Dispatcher *d = new Dispatcher;
    return *d;
}

void Dispatcher::wakeUp() {
    const char b = 1;
    ssize_t r;
    do {
        r = ::write(wakeWrite_, &b, 1);
    } while (r < 0 && errno == EINTR);
}

void Dispatcher::post(QObject *receiver, QEvent *event) {
    {
        std::lock_guard<std::mutex> lk(m_);
        queue_.push_back({receiver, event, ++seq_});
    }
    wakeUp();
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

int Dispatcher::startTimer(QObject *obj, std::chrono::milliseconds interval) {
    int id;
    {
        std::lock_guard<std::mutex> lk(m_);
        id = nextTimerId_++;
        timers_.push_back({id, obj, interval, Clock::now() + interval});
    }
    wakeUp();
    return id;
}

void Dispatcher::killTimer(int id) {
    std::lock_guard<std::mutex> lk(m_);
    timers_.erase(std::remove_if(timers_.begin(), timers_.end(), [id](const Timer &t) { return t.id == id; }),
                  timers_.end());
}

void Dispatcher::killTimers(QObject *obj) {
    std::lock_guard<std::mutex> lk(m_);
    timers_.erase(std::remove_if(timers_.begin(), timers_.end(), [obj](const Timer &t) { return t.obj == obj; }),
                  timers_.end());
}

int Dispatcher::addNotifier(QSocketNotifier *obj, int fd, int type) {
    int id;
    {
        std::lock_guard<std::mutex> lk(m_);
        id = nextNotifierId_++;
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

bool Dispatcher::process(bool wait, const std::atomic<bool> *stop) {
    std::unique_lock<std::mutex> lk(m_);
    std::vector<int> ready; // notifier ids with fd activity

    for (;;) {
        if (wait && stop && stop->load())
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
            r = ::poll(fds.data(), static_cast<nfds_t>(fds.size()), timeout);
            if (r > 0 && (fds[0].revents & POLLIN)) {
                char buf[64];
                // The wake pipe is non-blocking and the lock is released above.
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
        Posted p = queue_.front();
        queue_.pop_front();
        lk.unlock();
        std::unique_ptr<QEvent> owned(p.event);
        if (p.event->type() == QEvent::DeferredDelete)
            delete p.receiver;
        else
            QCoreApplication::sendEvent(p.receiver, p.event);
        owned.reset();
        lk.lock();
        did = true;
    }
    return did;
}

void Dispatcher::addLoop(QEventLoop *loop) {
    std::lock_guard<std::mutex> lk(m_);
    loops_.push_back(loop);
}

void Dispatcher::removeLoop(QEventLoop *loop) {
    std::lock_guard<std::mutex> lk(m_);
    loops_.erase(std::remove(loops_.begin(), loops_.end(), loop), loops_.end());
}

void Dispatcher::exitAll(int code) {
    std::vector<QEventLoop *> copy;
    {
        std::lock_guard<std::mutex> lk(m_);
        copy = loops_;
    }
    for (QEventLoop *l : copy)
        l->exit(code);
}

} // namespace QtPrivate
