// MIT License - see LICENSE
// Custom event dispatchers: QThread::setEventDispatcher(), QCoreApplication::setEventDispatcher(),
// QAbstractEventDispatcherV2.
#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QObject>
#include <QSocketNotifier>
#include <QThread>
#include <QTimer>
#include <QTimerEvent>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

static std::atomic<int> g_failures{0};
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                       \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

template <typename Pred> static bool spinUntil(Pred pred, int timeoutMs = 30000) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!pred()) {
        if (std::chrono::steady_clock::now() > end)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

template <typename Pred> static bool waitUntil(Pred pred, int timeoutMs = 30000) {
    QDeadlineTimer deadline(timeoutMs);
    while (!pred()) {
        if (deadline.hasExpired())
            return false;
        QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
    }
    return true;
}

// Counts the calls and leaves the work to Nt's dispatcher.
class RecordingDispatcher : public QAbstractEventDispatcher {
public:
    static std::atomic<int> startingUps;
    static std::atomic<int> closingDowns;
    static std::atomic<int> destroyed;

    ~RecordingDispatcher() override { ++destroyed; }

    bool processEvents(QEventLoop::ProcessEventsFlags flags) override {
        ++calls;
        if (flags.testFlag(QEventLoop::EventLoopExec))
            ++execCalls;
        if (flags.testFlag(QEventLoop::ApplicationExec))
            ++appExecCalls;
        return QAbstractEventDispatcher::processEvents(flags);
    }
    void registerTimer(int id, qint64 interval, Qt::TimerType type, QObject *object) override {
        ++timers;
        QAbstractEventDispatcher::registerTimer(id, interval, type, object);
    }
    void registerSocketNotifier(QSocketNotifier *notifier) override {
        ++notifiers;
        QAbstractEventDispatcher::registerSocketNotifier(notifier);
    }
    void unregisterSocketNotifier(QSocketNotifier *notifier) override {
        ++notifierRemovals;
        QAbstractEventDispatcher::unregisterSocketNotifier(notifier);
    }
    void wakeUp() override {
        ++wakeUps;
        QAbstractEventDispatcher::wakeUp();
    }
    void startingUp() override { ++startingUps; }
    void closingDown() override { ++closingDowns; }

    std::atomic<int> calls{0};
    std::atomic<int> execCalls{0};
    std::atomic<int> appExecCalls{0};
    std::atomic<int> timers{0};
    std::atomic<int> notifiers{0};
    std::atomic<int> notifierRemovals{0};
    std::atomic<int> wakeUps{0};
};
std::atomic<int> RecordingDispatcher::startingUps{0};
std::atomic<int> RecordingDispatcher::closingDowns{0};
std::atomic<int> RecordingDispatcher::destroyed{0};

// Does all the work itself, without Nt's dispatcher: its own timer table, poll() on its own wake pipe and on the
// sockets of its notifiers, and QCoreApplication::sendPostedEvents() for the posted events.
class ManualDispatcher : public QAbstractEventDispatcher {
public:
    static std::atomic<int> startingUps;
    static std::atomic<int> closingDowns;
    static std::atomic<int> destroyed;

    ManualDispatcher() {
        if (::pipe(m_wake) == 0) {
            ::fcntl(m_wake[0], F_SETFL, O_NONBLOCK);
            ::fcntl(m_wake[1], F_SETFL, O_NONBLOCK);
        }
    }
    ~ManualDispatcher() override {
        ::close(m_wake[0]);
        ::close(m_wake[1]);
        ++destroyed;
    }

    bool processEvents(QEventLoop::ProcessEventsFlags flags) override {
        ++calls;
        bool did = deliver(std::vector<QSocketNotifier *>());
        if (did || !flags.testFlag(QEventLoop::WaitForMoreEvents))
            return pollOnce(0) || did;
        return pollOnce(nextTimeout());
    }
    void registerSocketNotifier(QSocketNotifier *notifier) override {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (std::find(m_notifiers.begin(), m_notifiers.end(), notifier) == m_notifiers.end())
            m_notifiers.push_back(notifier);
    }
    void unregisterSocketNotifier(QSocketNotifier *notifier) override {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_notifiers.erase(std::remove(m_notifiers.begin(), m_notifiers.end(), notifier), m_notifiers.end());
    }
    void registerTimer(int id, qint64 interval, Qt::TimerType, QObject *object) override {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_timers[id] =
            Timer{object, std::chrono::milliseconds(interval), Clock::now() + std::chrono::milliseconds(interval)};
    }
    bool unregisterTimer(int id) override {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_timers.erase(id) != 0;
    }
    bool unregisterTimers(QObject *object) override {
        std::lock_guard<std::mutex> lk(m_mutex);
        bool found = false;
        for (auto it = m_timers.begin(); it != m_timers.end();) {
            if (it->second.object == object) {
                it = m_timers.erase(it);
                found = true;
            } else {
                ++it;
            }
        }
        return found;
    }
    QList<TimerInfo> registeredTimers(QObject *object) const override {
        std::lock_guard<std::mutex> lk(m_mutex);
        QList<TimerInfo> out;
        for (const auto &t : m_timers)
            if (t.second.object == object)
                out.append(TimerInfo(t.first, static_cast<int>(t.second.interval.count()), Qt::CoarseTimer));
        return out;
    }
    int remainingTime(int id) override {
        std::lock_guard<std::mutex> lk(m_mutex);
        const auto it = m_timers.find(id);
        if (it == m_timers.end())
            return -1;
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(it->second.next - Clock::now());
        return std::max(0, static_cast<int>(left.count()));
    }
    void wakeUp() override {
        ++wakeUps;
        const char c = 1;
        const ssize_t n = ::write(m_wake[1], &c, 1);
        (void)n;
    }
    void interrupt() override { wakeUp(); }
    void startingUp() override { ++startingUps; }
    void closingDown() override { ++closingDowns; }

    std::atomic<int> calls{0};
    std::atomic<int> wakeUps{0};

private:
    using Clock = std::chrono::steady_clock;
    struct Timer {
        QObject *object;
        std::chrono::milliseconds interval;
        Clock::time_point next;
    };

    int nextTimeout() {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_timers.empty())
            return -1;
        auto next = Clock::time_point::max();
        for (const auto &t : m_timers)
            next = std::min(next, t.second.next);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(next - Clock::now()).count() + 1;
        return static_cast<int>(std::max<long long>(0, ms));
    }

    // Waits up to `timeout` milliseconds for the wake pipe or a socket, then delivers what is ready.
    bool pollOnce(int timeout) {
        std::vector<pollfd> fds{{m_wake[0], POLLIN, 0}};
        std::vector<QSocketNotifier *> watched;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            watched = m_notifiers;
        }
        for (QSocketNotifier *n : watched)
            fds.push_back({static_cast<int>(n->socket()),
                           n->type() == QSocketNotifier::Read ? short(POLLIN) : short(POLLOUT), 0});
        emit aboutToBlock();
        ::poll(fds.data(), static_cast<nfds_t>(fds.size()), timeout);
        emit awake();
        char buf[64];
        while (::read(m_wake[0], buf, sizeof buf) > 0) {
        }
        std::vector<QSocketNotifier *> ready;
        for (std::size_t i = 1; i < fds.size(); ++i)
            if (fds[i].revents != 0)
                ready.push_back(watched[i - 1]);
        return deliver(ready);
    }

    bool deliver(const std::vector<QSocketNotifier *> &ready) {
        bool did = false;
        for (QSocketNotifier *n : ready) {
            {
                // A notifier that an earlier one destroyed or disabled is gone from the list.
                std::lock_guard<std::mutex> lk(m_mutex);
                if (std::find(m_notifiers.begin(), m_notifiers.end(), n) == m_notifiers.end())
                    continue;
            }
            QEvent e(QEvent::SockAct);
            QCoreApplication::sendEvent(n, &e);
            did = true;
        }
        std::vector<std::pair<int, QObject *>> due;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            const auto now = Clock::now();
            for (auto &t : m_timers) {
                if (t.second.next <= now) {
                    due.emplace_back(t.first, t.second.object);
                    t.second.next = now + t.second.interval;
                }
            }
        }
        for (const auto &d : due) {
            {
                std::lock_guard<std::mutex> lk(m_mutex);
                if (m_timers.find(d.first) == m_timers.end())
                    continue;
            }
            QTimerEvent e(d.first);
            QCoreApplication::sendEvent(d.second, &e);
            did = true;
        }
        QCoreApplication::sendPostedEvents();
        return did;
    }

    mutable std::mutex m_mutex;
    std::map<int, Timer> m_timers;
    std::vector<QSocketNotifier *> m_notifiers;
    int m_wake[2] = {-1, -1};
};
std::atomic<int> ManualDispatcher::startingUps{0};
std::atomic<int> ManualDispatcher::closingDowns{0};
std::atomic<int> ManualDispatcher::destroyed{0};

// Records the durations that it is given.
class DurationDispatcher : public QAbstractEventDispatcherV2 {
public:
    using QAbstractEventDispatcherV2::registerTimer;
    using QAbstractEventDispatcherV2::unregisterTimer;
    void registerTimer(Qt::TimerId id, Duration interval, Qt::TimerType type, QObject *object) override {
        {
            std::lock_guard<std::mutex> lk(mutex);
            intervals.push_back(interval);
        }
        QAbstractEventDispatcherV2::registerTimer(id, interval, type, object);
    }
    bool unregisterTimer(Qt::TimerId id) override {
        ++removals;
        return QAbstractEventDispatcherV2::unregisterTimer(id);
    }

    std::mutex mutex;
    std::vector<Duration> intervals;
    std::atomic<int> removals{0};
};

class TimerCounter : public QObject {
public:
    std::atomic<int> ticks{0};
    std::atomic<QThread *> where{nullptr};

protected:
    void timerEvent(QTimerEvent *) override {
        where = QThread::currentThread();
        ++ticks;
    }
};

// The main thread runs a RecordingDispatcher that QCoreApplication::setEventDispatcher() installed.
static void testMainThread(RecordingDispatcher *main) {
    CHECK(QCoreApplication::eventDispatcher() == main);
    CHECK(QAbstractEventDispatcher::instance() == main);
    CHECK(QThread::currentThread()->eventDispatcher() == main);
    CHECK(RecordingDispatcher::startingUps == 1);

    const int timersBefore = main->timers;
    bool fired = false;
    QTimer::singleShot(5, [&] { fired = true; });
    CHECK(main->timers > timersBefore);
    CHECK(waitUntil([&] { return fired; }));

    // A loop calls the dispatcher with EventLoopExec, and QCoreApplication::exec() adds ApplicationExec.
    const int execBefore = main->execCalls;
    QEventLoop loop;
    QTimer::singleShot(5, &loop, [&] { loop.quit(); });
    CHECK(loop.exec() == 0);
    CHECK(main->execCalls > execBefore);
    QTimer::singleShot(5, [] { QCoreApplication::quit(); });
    CHECK(QCoreApplication::exec() == 0);
    CHECK(main->appExecCalls > 0);

    // Posting from another thread wakes the dispatcher.
    const int wakesBefore = main->wakeUps;
    std::atomic<bool> delivered{false};
    QObject receiver;
    std::thread poster([&] { QMetaObject::invokeMethod(&receiver, [&] { delivered = true; }, Qt::QueuedConnection); });
    CHECK(waitUntil([&] { return delivered.load(); }));
    poster.join();
    CHECK(main->wakeUps > wakesBefore);

    // Socket notifiers register with the dispatcher while they are enabled.
    int fds[2];
    CHECK(::pipe(fds) == 0);
    {
        const int registered = main->notifiers;
        const int removed = main->notifierRemovals;
        QSocketNotifier notifier(fds[0], QSocketNotifier::Read);
        CHECK(main->notifiers == registered + 1);
        int activations = 0;
        QObject::connect(&notifier, &QSocketNotifier::activated, [&](QSocketDescriptor) {
            ++activations;
            char c = 0;
            const ssize_t n = ::read(fds[0], &c, 1);
            (void)n;
        });
        const char byte = 1;
        ssize_t n = ::write(fds[1], &byte, 1);
        CHECK(waitUntil([&] { return activations == 1; }));
        notifier.setEnabled(false);
        CHECK(main->notifierRemovals == removed + 1);
        n = ::write(fds[1], &byte, 1);
        (void)n;
        QCoreApplication::processEvents();
        CHECK(activations == 1);
        notifier.setEnabled(true);
        CHECK(main->notifiers == registered + 2);
        CHECK(waitUntil([&] { return activations == 2; }));
        // Unregistering through the dispatcher leaves the notifier enabled.
        main->unregisterSocketNotifier(&notifier);
        CHECK(notifier.isEnabled());
        main->registerSocketNotifier(&notifier);
    }
    ::close(fds[0]);
    ::close(fds[1]);

    // Too late for the main thread, and for a thread that has a dispatcher already.
    auto *late = new RecordingDispatcher;
    QCoreApplication::setEventDispatcher(late);
    CHECK(QCoreApplication::eventDispatcher() == main);
    delete late;
    {
        QThread idle;
        QAbstractEventDispatcher *own = idle.eventDispatcher();
        auto *refused = new RecordingDispatcher;
        idle.setEventDispatcher(refused);
        CHECK(idle.eventDispatcher() == own);
        CHECK(refused->thread() == QThread::currentThread());
        delete refused;
    }
}

// A thread that runs a dispatcher which does everything itself.
static void testManualThread() {
    ManualDispatcher::startingUps = 0;
    ManualDispatcher::closingDowns = 0;
    ManualDispatcher::destroyed = 0;

    QThread thread;
    auto *dispatcher = new ManualDispatcher;
    thread.setEventDispatcher(dispatcher);
    CHECK(thread.eventDispatcher() == dispatcher);
    CHECK(QAbstractEventDispatcher::instance(&thread) == dispatcher);
    CHECK(dispatcher->thread() == &thread);
    CHECK(QAbstractEventDispatcher::instance() != dispatcher);
    thread.start();
    CHECK(spinUntil([] { return ManualDispatcher::startingUps.load() == 1; }));
    // A running thread keeps its dispatcher.
    auto *second = new ManualDispatcher;
    thread.setEventDispatcher(second);
    CHECK(thread.eventDispatcher() == dispatcher);
    delete second;
    ManualDispatcher::destroyed = 0;

    // Queued calls, a QTimer and a socket notifier, all served by the dispatcher.
    QObject context;
    context.moveToThread(&thread);
    std::atomic<QThread *> ranIn{nullptr};
    std::atomic<int> ticks{0};
    std::atomic<int> activations{0};
    int fds[2];
    CHECK(::pipe(fds) == 0);
    QMetaObject::invokeMethod(
        &context,
        [&] {
            ranIn = QThread::currentThread();
            auto *timer = new QTimer(&context);
            QObject::connect(timer, &QTimer::timeout, [&ticks, timer] {
                if (++ticks == 3)
                    timer->stop();
            });
            timer->start(2);
            auto *notifier = new QSocketNotifier(fds[0], QSocketNotifier::Read, &context);
            QObject::connect(notifier, &QSocketNotifier::activated, [&activations, &fds](QSocketDescriptor) {
                char c = 0;
                const ssize_t n = ::read(fds[0], &c, 1);
                (void)n;
                ++activations;
            });
        },
        Qt::QueuedConnection);
    CHECK(spinUntil([&] { return ticks.load() == 3; }));
    CHECK(ranIn.load() == &thread);
    const char byte = 1;
    const ssize_t n = ::write(fds[1], &byte, 1);
    (void)n;
    CHECK(spinUntil([&] { return activations.load() == 1; }));
    CHECK(dispatcher->calls > 0 && dispatcher->wakeUps > 0);

    // A timer of an object that moves into the thread goes on there, in the thread's dispatcher.
    auto *counter = new TimerCounter;
    const int id = counter->startTimer(2);
    CHECK(id > 0);
    counter->moveToThread(&thread);
    CHECK(spinUntil([&] { return counter->ticks.load() >= 2 && counter->where.load() == &thread; }));
    QMetaObject::invokeMethod(counter, [counter, id] { counter->killTimer(id); }, Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(counter, [counter] { delete counter; }, Qt::QueuedConnection);

    // quit() reaches the loop through the dispatcher's wakeUp(); the thread then closes the dispatcher down and
    // deletes it.
    QMetaObject::invokeMethod(&context, [&context] { context.moveToThread(nullptr); }, Qt::BlockingQueuedConnection);
    thread.quit();
    CHECK(thread.wait(30000));
    CHECK(ManualDispatcher::closingDowns == 1);
    CHECK(ManualDispatcher::destroyed == 1);
    CHECK(thread.eventDispatcher() != nullptr);
    ::close(fds[0]);
    ::close(fds[1]);
}

// A QAbstractEventDispatcherV2 gets the intervals as durations.
static void testV2() {
    QThread thread;
    auto *dispatcher = new DurationDispatcher;
    thread.setEventDispatcher(dispatcher);
    thread.start();
    QObject context;
    context.moveToThread(&thread);
    std::atomic<int> ticks{0};
    std::atomic<int> listed{0};
    QMetaObject::invokeMethod(
        &context,
        [&] {
            auto *timer = new QTimer(&context);
            QObject::connect(timer, &QTimer::timeout, [&, timer] {
                if (++ticks == 2) {
                    listed = static_cast<int>(dispatcher->timersForObject(timer).size());
                    timer->stop();
                }
            });
            timer->start(7);
        },
        Qt::QueuedConnection);
    CHECK(spinUntil([&] { return ticks.load() == 2; }));
    CHECK(listed == 1);
    {
        std::lock_guard<std::mutex> lk(dispatcher->mutex);
        CHECK(!dispatcher->intervals.empty() && dispatcher->intervals[0] == std::chrono::milliseconds(7));
    }
    CHECK(spinUntil([&] { return dispatcher->removals.load() >= 1; }));
    QMetaObject::invokeMethod(&context, [&context] { context.moveToThread(nullptr); }, Qt::BlockingQueuedConnection);
    thread.quit();
    CHECK(thread.wait(30000));

    // The chrono overloads of a dispatcher that is not installed work on the calling thread's own dispatcher.
    DurationDispatcher local;
    TimerCounter counter;
    const Qt::TimerId id = local.registerTimer(std::chrono::milliseconds(1000), Qt::CoarseTimer, &counter);
    CHECK(id != Qt::TimerId::Invalid);
    const auto timers = local.timersForObject(&counter);
    CHECK(timers.size() == 1 && timers[0].timerId == id && timers[0].interval == std::chrono::seconds(1));
    CHECK(local.remainingTime(id) > std::chrono::milliseconds(0));
    CHECK(local.remainingTime(static_cast<int>(id)) > 0);
    CHECK(local.registeredTimers(&counter).size() == 1);
    CHECK(local.unregisterTimer(id));
    CHECK(local.remainingTime(id) < std::chrono::nanoseconds(0));

    // Waiting with a deadline returns by the deadline, though a wakeUp() may end a wait sooner.
    QElapsedTimer elapsed;
    elapsed.start();
    const QDeadlineTimer deadline(40);
    int waits = 0;
    while (!deadline.hasExpired() && waits < 1000) {
        local.processEventsWithDeadline(QEventLoop::WaitForMoreEvents, deadline);
        ++waits;
    }
    CHECK(deadline.hasExpired());
    CHECK(waits < 100);
    CHECK(elapsed.elapsed() >= 35 && elapsed.elapsed() < 20000);
}

int main(int argc, char **argv) {
    auto *main = new RecordingDispatcher;
    QCoreApplication::setEventDispatcher(main);
    {
        QCoreApplication app(argc, argv);
        testMainThread(main);
        testManualThread();
        testV2();
    }
    // The application object closes the main thread's dispatcher down and deletes it.
    CHECK(RecordingDispatcher::closingDowns == 1);
    CHECK(RecordingDispatcher::destroyed >= 3);
    if (g_failures == 0)
        std::printf("All event dispatcher tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
