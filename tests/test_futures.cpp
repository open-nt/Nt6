// MIT License - see LICENSE
#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QCoreApplication>
#include <QEventLoop>
#include <QEventLoopLocker>
#include <QException>
#include <QFuture>
#include <QFutureSynchronizer>
#include <QFutureIterator>
#include <QFutureWatcher>
#include <QMutex>
#include <QObject>
#include <QPromise>
#include <QSocketNotifier>
#include <QString>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QTimerEvent>

#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

static std::atomic<int> g_failures{0};
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

// Every wait is condition based and generous; a healthy run never gets near the limit.
template <typename Pred> static bool waitUntil(Pred pred, int timeoutMs = 60000) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!pred()) {
        if (std::chrono::steady_clock::now() > end)
            return false;
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return true;
}

// Waits for something that other threads do, without handling events.
template <typename Pred> static bool spinUntil(Pred pred, int timeoutMs = 60000) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!pred()) {
        if (std::chrono::steady_clock::now() > end)
            return false;
        std::this_thread::yield();
    }
    return true;
}

namespace {

struct MyError : QException {
    explicit MyError(int c = 0) : code(c) {}
    void raise() const override { throw *this; }
    MyError *clone() const override { return new MyError(*this); }
    int code;
};

struct PlainError {
    int value;
};

// A thread with an event loop.
class LoopThread {
public:
    LoopThread() { m_thread.start(); }
    ~LoopThread() {
        m_thread.quit();
        m_thread.wait();
    }
    QThread *thread() { return &m_thread; }

private:
    QThread m_thread;
};

// Runs `f` in a thread of `pool` and returns its future; `f` can use the promise to report more.
template <typename R, typename F> QFuture<R> runIn(QThreadPool &pool, F f) {
    auto promise = std::make_shared<QPromise<R>>();
    QFuture<R> future = promise->future();
    pool.start([promise, f]() mutable {
        promise->start();
        f(*promise);
        promise->finish();
    });
    return future;
}

class TimerCounter : public QObject {
public:
    std::vector<int> ids;

protected:
    void timerEvent(QTimerEvent *e) override { ids.push_back(e->timerId()); }
};

} // namespace

static void testBasics() {
    QPromise<int> p;
    QFuture<int> f = p.future();
    CHECK(f.isValid());
    CHECK(!f.isStarted() && !f.isRunning() && !f.isFinished() && !f.isCanceled());
    p.start();
    CHECK(f.isStarted() && f.isRunning() && !f.isFinished());
    CHECK(p.addResult(1));
    CHECK(p.addResult(2));
    CHECK(p.emplaceResult(3));
    CHECK(f.resultCount() == 3);
    CHECK(f.isResultReadyAt(2) && !f.isResultReadyAt(3));
    CHECK(f.result() == 1);
    CHECK(f.resultAt(2) == 3);
    CHECK(p.addResults(QList<int>{4, 5, 6}));
    CHECK(f.resultCount() == 6);
    p.finish();
    CHECK(f.isFinished() && !f.isRunning() && f.isStarted() && !f.isCanceled());
    CHECK(!p.addResult(7));
    CHECK((f.results() == QList<int>{1, 2, 3, 4, 5, 6}));
    int sum = 0;
    for (int v : f)
        sum += v;
    CHECK(sum == 21);
    int count = 0;
    for (auto it = f.begin(); it != f.end(); ++it)
        ++count;
    CHECK(count == 6);
    CHECK(*f.begin() == 1);
    CHECK(int(f) == 1);
    f.waitForFinished();

    QFuture<int> empty;
    CHECK(!empty.isValid());
    CHECK(empty.isCanceled() && empty.isFinished());
    empty.waitForFinished();
    CHECK(empty.results().isEmpty());
    CHECK(empty.begin() == empty.end());

    QPromise<void> pv;
    QFuture<void> fv = pv.future();
    CHECK(!fv.isStarted());
    pv.start();
    CHECK(fv.isRunning());
    pv.finish();
    CHECK(fv.isFinished());
    fv.waitForFinished();
    QFuture<void> fromInt = f;
    CHECK(fromInt.isFinished());
    CHECK(fromInt == QFuture<void>(f));
}

static void testResultOrder() {
    QPromise<int> p;
    auto f = p.future();
    p.start();
    CHECK(p.addResult(20, 2));
    CHECK(f.resultCount() == 0);
    CHECK(f.isResultReadyAt(2) && !f.isResultReadyAt(0));
    CHECK(!p.addResult(99, 2)); // taken
    CHECK(p.addResult(0, 0));
    CHECK(f.resultCount() == 1);
    CHECK(p.addResult(10, 1));
    CHECK(f.resultCount() == 3);
    CHECK(p.emplaceResultAt(3, 30));
    CHECK(p.addResult(40)); // follows the last one
    p.finish();
    CHECK((f.results() == QList<int>{0, 10, 20, 30, 40}));
    CHECK(f.resultAt(2) == 20);

    // Batches are overlap checked and read per element.
    QPromise<int> q;
    auto g = q.future();
    q.start();
    CHECK(q.addResults(QList<int>{1, 2, 3}));
    CHECK(!q.addResults(QList<int>{9, 9}, 2));
    CHECK(!q.addResults(QList<int>{}));
    CHECK(q.addResults(QList<int>{4, 5}, 3));
    q.finish();
    CHECK(g.resultAt(4) == 5 && g.resultAt(1) == 2);
    CHECK((g.results() == QList<int>{1, 2, 3, 4, 5}));

    // A future that finishes without a result gives a default value.
    QPromise<int> n;
    auto h = n.future();
    n.start();
    n.finish();
    CHECK(h.result() == 0);
    CHECK(h.results().isEmpty());
}

static void testMoveOnlyResults() {
    QPromise<std::unique_ptr<int>> p;
    auto f = p.future();
    p.start();
    CHECK(p.emplaceResult(new int(5)));
    CHECK(p.addResult(std::make_unique<int>(7)));
    p.finish();
    CHECK(f.resultCount() == 2);
    auto a = f.takeResult();
    CHECK(a && *a == 5);

    QPromise<std::string> s;
    s.start();
    std::string text(100, 'x');
    s.addResult(std::move(text));
    s.finish();
    CHECK(s.future().result().size() == 100);
}

static void testProducerConsumer() {
    constexpr int N = 100000;
    QPromise<int> p;
    auto f = p.future();
    std::thread producer([&p] {
        p.start();
        for (int i = 0; i < N; ++i)
            p.addResult(i);
        p.finish();
    });
    long long sum = 0;
    int count = 0;
    for (int v : f) {
        CHECK(v == count);
        sum += v;
        ++count;
    }
    producer.join();
    CHECK(count == N);
    CHECK(sum == static_cast<long long>(N) * (N - 1) / 2);

    // Several consumers read the same results while they arrive.
    QPromise<int> q;
    auto g = q.future();
    std::atomic<int> good{0};
    std::vector<std::thread> consumers;
    for (int c = 0; c < 4; ++c)
        consumers.emplace_back([g, &good] {
            long long total = 0;
            for (int i = 0; i < 20000; ++i)
                total += g.resultAt(i);
            if (total == 20000LL * 19999 / 2)
                ++good;
            g.waitForFinished();
        });
    std::thread feeder([&q] {
        q.start();
        for (int i = 0; i < 20000; ++i)
            q.addResult(i);
        q.finish();
    });
    feeder.join();
    for (auto &t : consumers)
        t.join();
    CHECK(good == 4);
}

static void testLargeResultCount() {
    constexpr int N = 400000;
    QPromise<int> p;
    auto f = p.future();
    std::thread producer([&p] {
        p.start();
        for (int i = 0; i < N; ++i)
            p.addResult(i);
        QList<int> batch;
        for (int i = 0; i < 100000; ++i)
            batch.append(i);
        p.addResults(batch);
        p.finish();
    });
    f.waitForFinished();
    producer.join();
    CHECK(f.resultCount() == N + 100000);
    CHECK(f.resultAt(N - 1) == N - 1);
    CHECK(f.resultAt(N + 99999) == 99999);
    CHECK(f.results().size() == N + 100000);
}

static void testPendingWait() {
    // Waiting for a promise that was not started yet blocks until it finishes.
    QPromise<int> p;
    auto f = p.future();
    std::atomic<bool> waiting{false};
    std::atomic<bool> done{false};
    std::thread waiter([&] {
        waiting = true;
        f.waitForFinished();
        done = true;
    });
    CHECK(spinUntil([&] { return waiting.load(); }));
    p.start();
    p.addResult(1);
    CHECK(!f.isFinished());
    p.finish();
    waiter.join();
    CHECK(done);
}

static void testCancel() {
    QPromise<int> p;
    auto f = p.future();
    p.start();
    p.addResult(1);
    CHECK(!p.isCanceled());
    f.cancel();
    CHECK(p.isCanceled() && f.isCanceled());
    CHECK(!f.isFinished()); // the computation finishes when it notices
    CHECK(!p.addResult(2));
    p.finish();
    CHECK(f.isFinished() && f.isCanceled());
    CHECK(f.results().isEmpty());
    CHECK(f.resultAt(0) == 1); // what arrived before stays readable

    // Destroying a promise that did not finish cancels the future and wakes the waiters.
    QFuture<int> g;
    {
        QPromise<int> q;
        g = q.future();
        q.start();
        q.addResult(1);
    }
    CHECK(g.isCanceled() && g.isFinished());
    g.waitForFinished();

    QFuture<int> h;
    std::atomic<bool> waiting{false};
    std::atomic<bool> woke{false};
    std::thread waiter;
    {
        QPromise<int> q;
        h = q.future();
        waiter = std::thread([&] {
            waiting = true;
            h.waitForFinished();
            woke = true;
        });
        CHECK(spinUntil([&] { return waiting.load(); }));
    }
    waiter.join();
    CHECK(woke && h.isCanceled());

    // A moved-from promise has nothing to finish; the new owner does.
    QPromise<int> a;
    auto af = a.future();
    QPromise<int> b(std::move(a));
    QPromise<int> c;
    c = std::move(b);
    CHECK(!af.isFinished());
    c.start();
    c.finish();
    CHECK(af.isFinished() && !af.isCanceled());
}

static void testCancelRace() {
    for (int round = 0; round < 200; ++round) {
        QPromise<int> p;
        auto f = p.future();
        std::atomic<int> produced{0};
        std::thread producer([&] {
            p.start();
            int i = 0;
            while (!p.isCanceled() && i < 1000000) {
                p.addResult(i++);
                ++produced;
            }
            p.finish();
        });
        CHECK(spinUntil([&] { return produced > 0; }));
        f.cancel();
        f.waitForFinished();
        producer.join();
        CHECK(f.isCanceled() && f.isFinished());
        CHECK(f.results().isEmpty());
        CHECK(f.resultCount() <= produced);
    }
}

static void testExceptions() {
    {
        QPromise<int> p;
        auto f = p.future();
        p.start();
        p.addResult(1);
        p.setException(std::make_exception_ptr(std::runtime_error("boom")));
        p.setException(std::make_exception_ptr(PlainError{1})); // the first one counts
        CHECK(f.isCanceled() && f.isRunning());
        p.finish();
        bool caught = false;
        try {
            f.waitForFinished();
        } catch (const std::runtime_error &e) {
            caught = std::string(e.what()) == "boom";
        }
        CHECK(caught);
        caught = false;
        try {
            (void)f.result();
        } catch (const std::runtime_error &) {
            caught = true;
        }
        CHECK(caught);
        caught = false;
        try {
            (void)f.results();
        } catch (const std::runtime_error &) {
            caught = true;
        }
        CHECK(caught);
    }
    {
        // A QException keeps its type.
        QPromise<void> p;
        auto f = p.future();
        p.start();
        p.setException(MyError(42));
        p.finish();
        int code = 0;
        try {
            f.waitForFinished();
        } catch (const MyError &e) {
            code = e.code;
        } catch (...) {
        }
        CHECK(code == 42);
    }
    {
        // A waiter is woken by the exception, even if the promise goes on.
        QPromise<int> p;
        auto f = p.future();
        p.start();
        std::atomic<bool> threw{false};
        std::thread waiter([&] {
            try {
                (void)f.resultAt(5);
            } catch (const PlainError &e) {
                threw = e.value == 9;
            }
        });
        p.setException(std::make_exception_ptr(PlainError{9}));
        waiter.join();
        CHECK(threw);
        p.finish();
    }
    {
        QUnhandledException u(std::make_exception_ptr(PlainError{3}));
        QPromise<int> p;
        auto f = p.future();
        p.start();
        p.setException(u);
        p.finish();
        int value = 0;
        try {
            f.waitForFinished();
        } catch (const QUnhandledException &e) {
            try {
                std::rethrow_exception(e.exception());
            } catch (const PlainError &pe) {
                value = pe.value;
            }
        }
        CHECK(value == 3);
        std::unique_ptr<QException> clone(u.clone());
        CHECK(dynamic_cast<QUnhandledException *>(clone.get()) != nullptr);
        bool raised = false;
        try {
            clone->raise();
        } catch (const QUnhandledException &) {
            raised = true;
        }
        CHECK(raised);
        const std::unique_ptr<QException> plain(QException().clone());
        CHECK(plain != nullptr);
    }
    {
        auto f = QtFuture::makeExceptionalFuture<int>(std::make_exception_ptr(PlainError{5}));
        CHECK(f.isFinished() && f.isCanceled());
        bool caught = false;
        try {
            f.waitForFinished();
        } catch (const PlainError &e) {
            caught = e.value == 5;
        }
        CHECK(caught);
        auto g = QtFuture::makeExceptionalFuture(MyError(8));
        caught = false;
        try {
            g.waitForFinished();
        } catch (const MyError &e) {
            caught = e.code == 8;
        }
        CHECK(caught);
    }
}

static void testSuspend() {
    QPromise<int> p;
    auto f = p.future();
    std::atomic<int> produced{0};
    std::atomic<bool> stop{false};
    std::thread producer([&] {
        p.start();
        for (int i = 0; !stop; ++i) {
            p.suspendIfRequested();
            if (p.isCanceled())
                break;
            p.addResult(i);
            ++produced;
        }
        p.finish();
    });
    CHECK(spinUntil([&] { return produced >= 10; }));
    f.suspend();
    CHECK(f.isSuspending() || f.isSuspended());
    CHECK(f.isPaused());
    CHECK(spinUntil([&] { return f.isSuspended(); }));
    CHECK(!f.isSuspending());
    const int snapshot = produced;
    QThread::msleep(20);
    CHECK(produced == snapshot);
    f.toggleSuspended();
    CHECK(!f.isSuspended() && !f.isSuspending() && !f.isPaused());
    CHECK(spinUntil([&] { return produced > snapshot; })); // it goes on
    stop = true;
    producer.join();
    CHECK(f.resultCount() == produced);

    // Canceling wakes a suspended computation.
    QPromise<int> q;
    auto g = q.future();
    std::atomic<bool> reached{false};
    std::thread worker([&] {
        q.start();
        reached = true;
        while (!q.isCanceled()) {
            q.suspendIfRequested();
            std::this_thread::yield();
        }
        q.finish();
    });
    CHECK(spinUntil([&] { return reached.load(); }));
    g.setPaused(true);
    CHECK(spinUntil([&] { return g.isSuspended(); }));
    g.cancel();
    worker.join();
    CHECK(g.isFinished() && g.isCanceled() && !g.isSuspended());

    // Resuming something that does not suspend changes nothing.
    QPromise<void> r;
    auto h = r.future();
    h.resume();
    CHECK(!h.isSuspending());
    r.start();
    h.pause();
    CHECK(h.isSuspending() && !h.isSuspended());
    h.resume();
    CHECK(!h.isSuspending());
    r.suspendIfRequested(); // nothing to suspend for
    r.finish();
}

static void testProgress() {
    QPromise<int> p;
    auto f = p.future();
    p.start();
    CHECK(f.progressMinimum() == 0 && f.progressMaximum() == 0 && f.progressValue() == 0);
    p.setProgressRange(10, 20);
    p.setProgressValue(12);
    CHECK(f.progressMinimum() == 10 && f.progressMaximum() == 20 && f.progressValue() == 12);
    p.setProgressValueAndText(15, QStringLiteral("half"));
    CHECK(f.progressValue() == 15 && f.progressText() == QStringLiteral("half"));
    f.cancel();
    p.setProgressValue(18);
    CHECK(f.progressValue() == 15);
    p.finish();
}

static void testThenSync() {
    QPromise<int> p;
    auto f = p.future();
    std::atomic<QThread *> where{nullptr};
    auto g = f.then([&](int v) {
        where = QThread::currentThread();
        return v * 2;
    });
    auto h = g.then([](int v) { return std::to_string(v); });
    auto v = h.then([](const std::string &) {});
    CHECK(!g.isStarted() && !h.isStarted());
    std::thread t([&] {
        p.start();
        p.addResult(21);
        p.finish();
    });
    h.waitForFinished();
    v.waitForFinished();
    t.join();
    CHECK(g.result() == 42);
    CHECK(h.result() == "42");
    CHECK(v.isFinished() && !v.isCanceled());
    CHECK(where != nullptr && where != QThread::currentThread()); // ran where the future finished

    // A finished future runs the continuation at once.
    auto ready = QtFuture::makeReadyValueFuture(5);
    bool ran = false;
    auto r = ready.then([&](int x) {
        ran = true;
        return x + 1;
    });
    CHECK(ran && r.isFinished() && r.result() == 6);

    // Void futures, and continuations that take the future.
    QPromise<void> pv;
    pv.start();
    pv.finish();
    auto fv = pv.future().then([] { return 3; });
    CHECK(fv.result() == 3);
    auto byFuture = QtFuture::makeReadyValueFuture(4).then([](QFuture<int> done) { return done.result() * 10; });
    CHECK(byFuture.result() == 40);
    auto vf = QtFuture::makeReadyVoidFuture().then([](QFuture<void> done) { done.waitForFinished(); });
    CHECK(vf.isFinished());

    // Results of a move-only type are moved into the continuation.
    QPromise<std::unique_ptr<int>> pm;
    auto m = pm.future().then([](std::unique_ptr<int> x) { return *x + 1; });
    pm.start();
    pm.emplaceResult(new int(9));
    pm.finish();
    CHECK(m.result() == 10);

    // A future takes one continuation.
    QPromise<int> p2;
    auto f2 = p2.future();
    auto first = f2.then([](int x) { return x; });
    auto second = f2.then([](int x) { return x; });
    CHECK(second.isCanceled() && second.isFinished());
    p2.start();
    p2.addResult(1);
    p2.finish();
    CHECK(first.result() == 1);
}

static void testThenFailures() {
    // Exceptions and cancellation skip the continuation and travel on.
    QPromise<int> p;
    bool ran = false;
    auto f = p.future().then([&](int x) {
        ran = true;
        return x;
    });
    auto g = f.then([&](int x) {
        ran = true;
        return x;
    });
    p.start();
    p.setException(MyError(7));
    p.finish();
    CHECK(!ran && g.isFinished() && g.isCanceled());
    int code = 0;
    try {
        g.waitForFinished();
    } catch (const MyError &e) {
        code = e.code;
    }
    CHECK(code == 7);

    QPromise<int> c;
    auto cf = c.future().then([&](int x) {
        ran = true;
        return x;
    });
    c.start();
    c.future().cancel();
    c.finish();
    CHECK(!ran && cf.isCanceled() && cf.isFinished());

    // A throwing continuation fails its future.
    auto thrower = QtFuture::makeReadyValueFuture(1).then([](int) -> int { throw PlainError{11}; });
    int value = 0;
    try {
        (void)thrower.result();
    } catch (const PlainError &e) {
        value = e.value;
    }
    CHECK(value == 11);

    // A continuation that takes the future sees the failure itself.
    QPromise<int> d;
    auto df = d.future().then([](QFuture<int> done) {
        try {
            (void)done.result();
        } catch (const MyError &e) {
            return e.code;
        }
        return -1;
    });
    d.start();
    d.setException(MyError(13));
    d.finish();
    CHECK(df.result() == 13);

    // A future that finished without a result cancels the continuation.
    QPromise<int> e;
    auto ef = e.future().then([&](int x) {
        ran = true;
        return x;
    });
    e.start();
    e.finish();
    CHECK(!ran && ef.isCanceled());
}

static void testOnFailedOnCanceled() {
    {
        QPromise<int> p;
        auto f = p.future().onFailed([](const MyError &e) { return e.code; });
        p.start();
        p.setException(MyError(5));
        p.finish();
        CHECK(f.result() == 5 && !f.isCanceled());
    }
    {
        // An exception of another type passes on to the next handler.
        QPromise<int> p;
        auto f = p.future()
                     .onFailed([](const MyError &) { return 1; })
                     .onFailed([](const PlainError &e) { return e.value; });
        p.start();
        p.setException(std::make_exception_ptr(PlainError{6}));
        p.finish();
        CHECK(f.result() == 6);
    }
    {
        // A handler without arguments takes any exception.
        QPromise<int> p;
        auto f = p.future().onFailed([] { return -1; });
        p.start();
        p.setException(std::make_exception_ptr(PlainError{6}));
        p.finish();
        CHECK(f.result() == -1);
    }
    {
        // Without a failure the results pass through.
        QPromise<int> p;
        bool called = false;
        auto f = p.future().onFailed([&](const MyError &) {
            called = true;
            return 0;
        });
        p.start();
        p.addResult(1);
        p.addResult(2);
        p.finish();
        CHECK(!called && (f.results() == QList<int>{1, 2}));
    }
    {
        // A failing handler fails the future, and cancellation is not a failure.
        auto f = QtFuture::makeExceptionalFuture<int>(MyError(1)).onFailed([](const MyError &) -> int {
            throw PlainError{2};
        });
        bool caught = false;
        try {
            f.waitForFinished();
        } catch (const PlainError &) {
            caught = true;
        }
        CHECK(caught);
        QPromise<int> p;
        bool called = false;
        auto g = p.future().onFailed([&] {
            called = true;
            return 0;
        });
        p.start();
        p.future().cancel();
        p.finish();
        CHECK(!called && g.isCanceled());
    }
    {
        QPromise<int> p;
        auto f = p.future().onCanceled([] { return -5; });
        p.start();
        p.future().cancel();
        p.finish();
        CHECK(f.isFinished() && !f.isCanceled() && f.result() == -5);
    }
    {
        QPromise<int> p;
        bool called = false;
        auto f = p.future().onCanceled([&] {
            called = true;
            return 0;
        });
        p.start();
        p.addResult(8);
        p.finish();
        CHECK(!called && f.result() == 8);
    }
    {
        // An exception is not a cancellation for onCanceled().
        QPromise<void> p;
        bool called = false;
        auto f = p.future().onCanceled([&] { called = true; });
        p.start();
        p.setException(MyError(2));
        p.finish();
        CHECK(!called && f.isCanceled());
        bool caught = false;
        try {
            f.waitForFinished();
        } catch (const MyError &) {
            caught = true;
        }
        CHECK(caught);
        QPromise<void> q;
        auto g = q.future().onCanceled([&] { called = true; });
        q.start();
        q.future().cancel();
        q.finish();
        CHECK(called && !g.isCanceled());
    }
}

static void testThenLaunch() {
    const QThread *const mainThread = QThread::currentThread();
    {
        QPromise<int> p;
        std::atomic<QThread *> where{nullptr};
        auto f = p.future().then(QtFuture::Launch::Async, [&](int v) {
            where = QThread::currentThread();
            return v + 1;
        });
        p.start();
        p.addResult(1);
        p.finish();
        CHECK(f.result() == 2);
        CHECK(where != nullptr && where != mainThread);
    }
    {
        QThreadPool pool;
        pool.setMaxThreadCount(2);
        QPromise<int> p;
        std::atomic<bool> inPool{false};
        auto f = p.future().then(&pool, [&](int v) {
            inPool = pool.contains(QThread::currentThread());
            return v;
        });
        p.start();
        p.addResult(1);
        p.finish();
        CHECK(f.result() == 1);
        CHECK(pool.waitForDone());
        CHECK(inPool);
    }
    {
        // Inherit follows the pool of the previous future, and runs inline without one.
        QThreadPool pool;
        QFutureInterface<int> iface(QFutureInterfaceBase::Pending);
        iface.setThreadPool(&pool);
        QPromise<int> p(iface);
        std::atomic<bool> inPool{false};
        auto f = p.future().then(QtFuture::Launch::Inherit, [&](int v) {
            inPool = pool.contains(QThread::currentThread());
            return v;
        });
        p.start();
        p.addResult(1);
        p.finish();
        CHECK(f.result() == 1);
        CHECK(inPool);
        CHECK(f.then(QtFuture::Launch::Inherit, [&](int v) { return v; }).result() == 1);
        std::atomic<QThread *> where{nullptr};
        auto g = QtFuture::makeReadyValueFuture(1).then(QtFuture::Launch::Inherit, [&](int v) {
            where = QThread::currentThread();
            return v;
        });
        CHECK(g.isFinished() && where == mainThread);
        pool.waitForDone();
    }
}

static void testThenContext() {
    const QThread *const mainThread = QThread::currentThread();
    {
        // The context lives in another thread.
        LoopThread lt;
        QObject *ctx = new QObject;
        ctx->moveToThread(lt.thread());
        QPromise<int> p;
        std::atomic<QThread *> where{nullptr};
        auto f = p.future().then(ctx, [&](int v) {
            where = QThread::currentThread();
            return v * 3;
        });
        p.start();
        p.addResult(5);
        p.finish();
        CHECK(f.result() == 15);
        CHECK(where == lt.thread());
        ctx->deleteLater();
    }
    {
        // A context in the thread that finishes the future runs it right away, one in another thread through its loop.
        QObject ctx;
        QPromise<int> p;
        std::atomic<int> ran{0};
        std::atomic<QThread *> where{nullptr};
        auto f = p.future().then(&ctx, [&](int v) {
            where = QThread::currentThread();
            ++ran;
            return v;
        });
        std::thread t([&] {
            p.start();
            p.addResult(1);
            p.finish();
        });
        t.join();
        CHECK(ran == 0 && !f.isFinished()); // queued for the main thread
        CHECK(waitUntil([&] { return f.isFinished(); }));
        CHECK(ran == 1 && where == mainThread && f.result() == 1);
    }
    {
        // The context goes away first: the continuation does not run and its future is canceled.
        auto *ctx = new QObject;
        QPromise<int> p;
        bool ran = false;
        auto f = p.future().then(ctx, [&](int v) {
            ran = true;
            return v;
        });
        delete ctx;
        p.start();
        p.addResult(1);
        p.finish();
        CHECK(!ran && f.isFinished() && f.isCanceled());

        // ... or after the job was queued.
        auto *ctx2 = new QObject;
        QPromise<int> q;
        auto g = q.future().then(ctx2, [&](int v) {
            ran = true;
            return v;
        });
        std::thread t([&] {
            q.start();
            q.addResult(1);
            q.finish();
        });
        t.join();
        delete ctx2;
        QCoreApplication::processEvents();
        CHECK(!ran && g.isFinished() && g.isCanceled());
    }
    {
        // Handlers with a context.
        QObject ctx;
        QPromise<int> p;
        std::atomic<int> where{0};
        auto f = p.future().onFailed(&ctx, [&](const MyError &) {
            where = QThread::currentThread() == mainThread ? 1 : 2;
            return 1;
        });
        std::thread t([&] {
            p.start();
            p.setException(MyError(1));
            p.finish();
        });
        t.join();
        CHECK(waitUntil([&] { return f.isFinished(); }));
        CHECK(f.result() == 1 && where == 1);

        QPromise<int> q;
        auto g = q.future().onCanceled(&ctx, [] { return 2; });
        q.start();
        q.future().cancel();
        q.finish();
        CHECK(waitUntil([&] { return g.isFinished(); }));
        CHECK(g.result() == 2);
    }
}

static void testCancelChain() {
    QPromise<int> p;
    auto a = p.future();
    auto b = a.then([](int v) { return v; });
    auto c = b.then([](int v) { return v; });
    c.cancel();
    CHECK(c.isCanceled() && !b.isCanceled() && !a.isCanceled());
    c.cancelChain();
    CHECK(b.isCanceled() && a.isCanceled());
    CHECK(p.isCanceled());
    p.start();
    p.finish();
    CHECK(c.isFinished() && b.isFinished());

    // A canceled continuation future does not run its function.
    QPromise<int> q;
    bool ran = false;
    auto d = q.future().then([&](int v) {
        ran = true;
        return v;
    });
    d.cancel();
    q.start();
    q.addResult(1);
    q.finish();
    CHECK(!ran && d.isFinished() && d.isCanceled());

    // A chain whose first future is dropped does not leak or crash.
    {
        QPromise<int> r;
        auto e = r.future().then([](int v) { return v; }).then([](int v) { return v; });
        CHECK(!e.isFinished());
    }
}

static void testContinuationChainAcrossThreads() {
    QThreadPool pool;
    auto f = runIn<int>(pool, [](QPromise<int> &p) { p.addResult(1); });
    auto g = f.then(QtFuture::Launch::Async, [](int v) { return v + 1; })
                 .then(QtFuture::Launch::Sync, [](int v) { return v * 10; })
                 .then(QtFuture::Launch::Async, [](int v) { return std::to_string(v); });
    CHECK(g.result() == "20");
    pool.waitForDone();

    // Many chains at once.
    std::vector<QFuture<int>> chains;
    for (int i = 0; i < 200; ++i) {
        auto src = runIn<int>(pool, [i](QPromise<int> &p) { p.addResult(i); });
        chains.push_back(src.then(QtFuture::Launch::Async, [](int v) { return v + 1; }).then([](int v) { return v * 2; }));
    }
    bool ok = true;
    for (int i = 0; i < 200; ++i)
        ok = ok && chains[static_cast<std::size_t>(i)].result() == (i + 1) * 2;
    CHECK(ok);
    pool.waitForDone();

    // Attaching a continuation while the future finishes in another thread.
    for (int round = 0; round < 200; ++round) {
        QPromise<int> p;
        auto src = p.future();
        std::atomic<bool> go{false};
        std::thread t([&] {
            while (!go)
                std::this_thread::yield();
            p.start();
            p.addResult(round);
            p.finish();
        });
        go = true;
        auto next = src.then([](int v) { return v + 1; });
        CHECK(next.result() == round + 1);
        t.join();
    }
}

static void testWatcherSignals() {
    QPromise<int> p;
    auto f = p.future();
    QFutureWatcher<int> w;
    std::vector<std::string> log;
    std::vector<int> indexes;
    int lastValue = -1;
    QString lastText;
    int rangeMin = -1;
    int rangeMax = -1;
    int batchBegin = -1;
    int batchEnd = -1;
    QObject::connect(&w, &QFutureWatcherBase::started, [&] { log.push_back("started"); });
    QObject::connect(&w, &QFutureWatcherBase::finished, [&] { log.push_back("finished"); });
    QObject::connect(&w, &QFutureWatcherBase::canceled, [&] { log.push_back("canceled"); });
    QObject::connect(&w, &QFutureWatcherBase::resultReadyAt, [&](int i) { indexes.push_back(i); });
    QObject::connect(&w, &QFutureWatcherBase::resultsReadyAt, [&](int b, int e) {
        batchBegin = b;
        batchEnd = e;
    });
    QObject::connect(&w, &QFutureWatcherBase::progressValueChanged, [&](int v) { lastValue = v; });
    QObject::connect(&w, &QFutureWatcherBase::progressTextChanged, [&](const QString &t) { lastText = t; });
    QObject::connect(&w, &QFutureWatcherBase::progressRangeChanged, [&](int lo, int hi) {
        rangeMin = lo;
        rangeMax = hi;
    });
    w.setFuture(f);
    CHECK(w.future() == f);
    p.start();
    p.setProgressRange(0, 10);
    p.addResult(0);
    p.addResult(1);
    p.addResults(QList<int>{2, 3, 4});
    p.setProgressValueAndText(5, QStringLiteral("five"));
    p.finish();
    CHECK(log.empty()); // nothing arrives before the event loop runs
    CHECK(waitUntil([&] { return !log.empty() && log.back() == "finished"; }));
    CHECK((log == std::vector<std::string>{"started", "finished"}));
    CHECK((indexes == std::vector<int>{0, 1, 2, 3, 4}));
    CHECK(batchBegin == 2 && batchEnd == 5);
    CHECK(lastValue == 5 && lastText == QStringLiteral("five") && rangeMin == 0 && rangeMax == 10);
    CHECK(w.isFinished() && w.isStarted() && !w.isRunning() && w.result() == 0 && w.resultAt(4) == 4);
    CHECK(w.progressValue() == 5 && w.progressMaximum() == 10 && w.progressMinimum() == 0 &&
          w.progressText() == QStringLiteral("five"));

    // Setting a finished future replays what it did.
    log.clear();
    indexes.clear();
    QFutureWatcher<int> late;
    QObject::connect(&late, &QFutureWatcherBase::started, [&] { log.push_back("started"); });
    QObject::connect(&late, &QFutureWatcherBase::finished, [&] { log.push_back("finished"); });
    QObject::connect(&late, &QFutureWatcherBase::resultReadyAt, [&](int i) { indexes.push_back(i); });
    late.setFuture(f);
    late.setFuture(f); // no change
    CHECK(waitUntil([&] { return !log.empty() && log.back() == "finished"; }));
    CHECK((log == std::vector<std::string>{"started", "finished"}));
    CHECK(indexes.size() == 5);
}

static void testWatcherCancel() {
    QPromise<int> p;
    auto f = p.future();
    QFutureWatcher<int> w;
    std::vector<std::string> log;
    int results = 0;
    QObject::connect(&w, &QFutureWatcherBase::started, [&] { log.push_back("started"); });
    QObject::connect(&w, &QFutureWatcherBase::finished, [&] { log.push_back("finished"); });
    QObject::connect(&w, &QFutureWatcherBase::canceled, [&] { log.push_back("canceled"); });
    QObject::connect(&w, &QFutureWatcherBase::resultReadyAt, [&](int) { ++results; });
    w.setFuture(f);
    p.start();
    p.addResult(1);
    w.cancel();
    CHECK(w.isCanceled() && p.isCanceled());
    p.finish();
    CHECK(waitUntil([&] { return !log.empty() && log.back() == "finished"; }));
    CHECK((log == std::vector<std::string>{"started", "canceled", "finished"}));
    CHECK(results == 0); // results that were not delivered when it was canceled are dropped

    // An exception reports as a cancellation.
    QPromise<int> q;
    QFutureWatcher<int> x;
    int cancels = 0;
    bool fin = false;
    QObject::connect(&x, &QFutureWatcherBase::canceled, [&] { ++cancels; });
    QObject::connect(&x, &QFutureWatcherBase::finished, [&] { fin = true; });
    x.setFuture(q.future());
    q.start();
    q.setException(MyError(1));
    q.finish();
    CHECK(waitUntil([&] { return fin; }));
    CHECK(cancels == 1);
    bool caught = false;
    try {
        x.waitForFinished();
    } catch (const MyError &) {
        caught = true;
    }
    CHECK(caught);

    // A promise that is destroyed unfinished cancels.
    QFutureWatcher<int> y;
    std::vector<std::string> log2;
    QObject::connect(&y, &QFutureWatcherBase::canceled, [&] { log2.push_back("canceled"); });
    QObject::connect(&y, &QFutureWatcherBase::finished, [&] { log2.push_back("finished"); });
    {
        QPromise<int> r;
        y.setFuture(r.future());
        r.start();
    }
    CHECK(waitUntil([&] { return log2.size() == 2; }));
    CHECK((log2 == std::vector<std::string>{"canceled", "finished"}));
}

static void testWatcherSuspend() {
    QPromise<int> p;
    auto f = p.future();
    QFutureWatcher<int> w;
    std::vector<std::string> log;
    QObject::connect(&w, &QFutureWatcherBase::suspending, [&] { log.push_back("suspending"); });
    QObject::connect(&w, &QFutureWatcherBase::paused, [&] { log.push_back("paused"); });
    QObject::connect(&w, &QFutureWatcherBase::suspended, [&] { log.push_back("suspended"); });
    QObject::connect(&w, &QFutureWatcherBase::resumed, [&] { log.push_back("resumed"); });
    QObject::connect(&w, &QFutureWatcherBase::finished, [&] { log.push_back("finished"); });
    w.setFuture(f);
    std::atomic<bool> reached{false};
    std::atomic<bool> release{false};
    std::thread producer([&] {
        p.start();
        reached = true;
        while (!release) {
            p.suspendIfRequested();
            std::this_thread::yield();
        }
        p.addResult(1);
        p.finish();
    });
    CHECK(spinUntil([&] { return reached.load(); }));
    w.suspend();
    CHECK(w.isSuspending() || w.isSuspended());
    CHECK(spinUntil([&] { return w.isSuspended(); }));
    CHECK(waitUntil([&] { return log.size() >= 3; }));
    release = true;
    w.resume();
    producer.join();
    CHECK(waitUntil([&] { return !log.empty() && log.back() == "finished"; }));
    CHECK((log == std::vector<std::string>{"suspending", "paused", "suspended", "resumed", "finished"}));
    w.togglePaused();
    CHECK(f.isFinished() && !w.isPaused()); // a finished computation does not suspend
}

static void testWatcherThreads() {
    // A watcher that lives in another thread gets its signals there.
    LoopThread lt;
    QPromise<int> p;
    auto f = p.future();
    auto *w = new QFutureWatcher<int>;
    w->moveToThread(lt.thread());
    std::atomic<int> results{0};
    std::atomic<int> wrongThread{0};
    std::atomic<bool> finished{false};
    QObject::connect(w, &QFutureWatcherBase::resultReadyAt, w, [&](int) {
        ++results;
        if (QThread::currentThread() != lt.thread())
            ++wrongThread;
    });
    QObject::connect(w, &QFutureWatcherBase::finished, w, [&] {
        if (QThread::currentThread() != lt.thread())
            ++wrongThread;
        finished = true;
    });
    QMetaObject::invokeMethod(w, [&] { w->setFuture(f); }, Qt::BlockingQueuedConnection);
    p.start();
    for (int i = 0; i < 1000; ++i)
        p.addResult(i);
    p.finish();
    CHECK(spinUntil([&] { return finished.load(); }));
    CHECK(results == 1000 && wrongThread == 0);
    w->deleteLater();

    // A watcher that moves to another thread takes its pending events along.
    QPromise<int> q;
    auto *m = new QFutureWatcher<int>;
    std::atomic<QThread *> fin{nullptr};
    QObject::connect(m, &QFutureWatcherBase::finished, m, [&] { fin = QThread::currentThread(); });
    m->setFuture(q.future());
    q.start();
    q.addResult(1);
    q.finish();
    m->moveToThread(lt.thread());
    CHECK(spinUntil([&] { return fin.load() != nullptr; }));
    CHECK(fin == lt.thread());
    m->deleteLater();

    // Moved back and forth while results arrive.
    QPromise<int> r;
    auto *n = new QFutureWatcher<int>;
    std::atomic<int> got{0};
    QObject::connect(n, &QFutureWatcherBase::resultReadyAt, n, [&](int) { ++got; });
    n->setFuture(r.future());
    QThread *const mainThread = QThread::currentThread();
    std::thread feeder([&] {
        r.start();
        for (int i = 0; i < 5000; ++i)
            r.addResult(i);
        r.finish();
    });
    n->moveToThread(lt.thread());
    QMetaObject::invokeMethod(n, [n, mainThread] { n->moveToThread(mainThread); }, Qt::BlockingQueuedConnection);
    CHECK(n->thread() == mainThread);
    n->moveToThread(lt.thread());
    feeder.join();
    CHECK(spinUntil([&] { return got.load() == 5000; }));
    n->deleteLater();
}

static void testWatcherDestruction() {
    // The future and the promise outlive the watcher, and the watcher the promise.
    {
        QPromise<int> p;
        {
            QFutureWatcher<int> w;
            w.setFuture(p.future());
            p.start();
            p.addResult(1);
        }
        p.addResult(2); // no observer left to post to
        p.finish();
        QCoreApplication::processEvents();
    }
    {
        QFutureWatcher<int> w;
        {
            QPromise<int> p;
            w.setFuture(p.future());
            p.start();
        }
        CHECK(w.isFinished() && w.isCanceled());
    }

    // Watchers come and go while another thread reports.
    QPromise<int> p;
    auto f = p.future();
    std::atomic<bool> stop{false};
    std::thread producer([&] {
        p.start();
        int i = 0;
        while (!stop) {
            p.addResult(i++);
            p.setProgressValue(i);
            if (i == 100000)
                break;
        }
        p.finish();
    });
    for (int round = 0; round < 300; ++round) {
        auto *w = new QFutureWatcher<int>;
        w->setFuture(f);
        if (round % 3 == 0)
            QCoreApplication::processEvents();
        delete w;
    }
    stop = true;
    producer.join();
    QCoreApplication::processEvents();

    // A watcher destroyed from another thread while the future reports.
    QPromise<int> q;
    auto g = q.future();
    std::atomic<bool> go{false};
    std::thread feeder([&] {
        q.start();
        go = true;
        for (int i = 0; i < 20000; ++i)
            q.addResult(i);
        q.finish();
    });
    CHECK(spinUntil([&] { return go.load(); }));
    std::thread destroyer([&] {
        for (int i = 0; i < 100; ++i) {
            QFutureWatcher<int> w;
            w.setFuture(g);
        }
    });
    destroyer.join();
    feeder.join();
}

static void testWatcherThrottle() {
    QFutureInterface<int> iface(QFutureInterfaceBase::Pending);
    QPromise<int> p(iface);
    QFutureWatcher<int> w;
    int got = 0;
    QObject::connect(&w, &QFutureWatcherBase::resultReadyAt, [&](int) { ++got; });
    w.setPendingResultsLimit(5);
    w.setFuture(p.future());
    p.start();
    CHECK(!iface.isThrottled());
    for (int i = 0; i < 10; ++i)
        p.addResult(i);
    CHECK(iface.isThrottled()); // nobody has taken the results yet
    CHECK(waitUntil([&] { return got == 10; }));
    CHECK(!iface.isThrottled());
    p.finish();
}

static void testSynchronizer() {
    QThreadPool pool;
    std::atomic<int> done{0};
    {
        QFutureSynchronizer<int> sync;
        for (int i = 0; i < 8; ++i)
            sync.addFuture(runIn<int>(pool, [&done, i](QPromise<int> &p) {
                p.addResult(i);
                ++done;
            }));
        CHECK(sync.futures().size() == 8);
        sync.waitForFinished();
        CHECK(done == 8);
        sync.clearFutures();
        CHECK(sync.futures().isEmpty());
        sync.addFuture(runIn<int>(pool, [&done](QPromise<int> &) { ++done; }));
    } // the destructor waits
    CHECK(done == 9);

    // cancelOnWait asks the computations to stop.
    std::atomic<int> stopped{0};
    std::vector<std::thread> workers;
    {
        QFutureSynchronizer<void> sync;
        CHECK(!sync.cancelOnWait());
        sync.setCancelOnWait(true);
        CHECK(sync.cancelOnWait());
        std::atomic<int> started{0};
        for (int i = 0; i < 3; ++i) {
            auto promise = std::make_shared<QPromise<void>>();
            sync.addFuture(promise->future());
            workers.emplace_back([promise, &started, &stopped] {
                promise->start();
                ++started;
                while (!promise->isCanceled())
                    std::this_thread::yield();
                ++stopped;
                promise->finish();
            });
        }
        CHECK(spinUntil([&] { return started == 3; }));
    }
    for (auto &t : workers)
        t.join();
    CHECK(stopped == 3);

    QFutureSynchronizer<int> single(QtFuture::makeReadyValueFuture(1));
    single.setFuture(QtFuture::makeReadyValueFuture(2));
    CHECK(single.futures().size() == 1 && single.futures().at(0).result() == 2);
    QFutureSynchronizer<int> moved(std::move(single));
    CHECK(moved.futures().size() == 1);

    // An exception shows from waitForFinished(), not from the destructor.
    QFutureSynchronizer<int> failing(QtFuture::makeExceptionalFuture<int>(MyError(1)));
    bool caught = false;
    try {
        failing.waitForFinished();
    } catch (const MyError &) {
        caught = true;
    }
    CHECK(caught);
}

static void testQtFutureHelpers() {
    auto v = QtFuture::makeReadyValueFuture(std::string("a"));
    CHECK(v.isFinished() && v.result() == "a");
    auto r = QtFuture::makeReadyRangeFuture(std::vector<int>{1, 2, 3});
    CHECK((r.results() == QList<int>{1, 2, 3}));
    auto i = QtFuture::makeReadyRangeFuture({4, 5});
    CHECK(i.resultCount() == 2);
    CHECK(QtFuture::makeReadyVoidFuture().isFinished());

    class Sender : public QObject {
    public:
        void fire(int a, const QString &b) { emit two(a, b); }
        void fireOne(int a) { emit one(a); }
        void fireNone() { emit none(); }

    signals:
        void two(int a, const QString &b) { emitSignal(&Sender::two, a, b); }
        void one(int a) { emitSignal(&Sender::one, a); }
        void none() { emitSignal(&Sender::none); }
    };
    Sender sender;
    auto f2 = QtFuture::connect(&sender, &Sender::two);
    auto f1 = QtFuture::connect(&sender, &Sender::one);
    auto f0 = QtFuture::connect(&sender, &Sender::none);
    CHECK(!f1.isFinished());
    sender.fireOne(4);
    sender.fireOne(5); // only the first emission counts
    sender.fire(1, QStringLiteral("x"));
    sender.fireNone();
    CHECK(f1.isFinished() && f1.result() == 4);
    CHECK(f2.isFinished() && std::get<0>(f2.result()) == 1 && std::get<1>(f2.result()) == QStringLiteral("x"));
    CHECK(f0.isFinished() && !f0.isCanceled());

    // The future is canceled if the sender goes away.
    QFuture<int> gone;
    {
        Sender s2;
        gone = QtFuture::connect(&s2, &Sender::one);
    }
    CHECK(gone.isFinished() && gone.isCanceled());

    // From another thread.
    LoopThread lt;
    auto *s3 = new Sender;
    s3->moveToThread(lt.thread());
    auto f3 = QtFuture::connect(s3, &Sender::one);
    QMetaObject::invokeMethod(s3, [s3] { s3->fireOne(9); });
    CHECK(f3.result() == 9);
    s3->deleteLater();
}

static void testWhenAllAny() {
    QThreadPool pool;
    auto a = runIn<int>(pool, [](QPromise<int> &p) { p.addResult(1); });
    auto b = runIn<std::string>(pool, [](QPromise<std::string> &p) { p.addResult("two"); });
    auto c = runIn<void>(pool, [](QPromise<void> &) {});
    auto all = QtFuture::whenAll(a, b, c);
    auto [fa, fb, fc] = all.result();
    CHECK(fa.result() == 1 && fb.result() == "two" && fc.isFinished());
    CHECK(all.isFinished());

    QPromise<int> slow;
    auto fast = QtFuture::makeReadyValueFuture(7);
    auto slowFuture = slow.future();
    auto any = QtFuture::whenAny(slowFuture, fast);
    auto first = any.result();
    CHECK(first.index() == 1);
    CHECK(std::get<1>(first).result() == 7);
    slow.start();
    slow.finish();

    std::vector<QFuture<int>> range;
    std::vector<QFuture<int>> range2;
    std::vector<QPromise<int>> promises(5);
    std::vector<QPromise<int>> promises2(5);
    for (std::size_t i = 0; i < promises.size(); ++i) {
        range.push_back(promises[i].future());
        range2.push_back(promises2[i].future());
    }
    auto rangeAll = QtFuture::whenAll(range.begin(), range.end());
    auto rangeAny = QtFuture::whenAny(range2.begin(), range2.end());
    CHECK(!rangeAll.isFinished() && !rangeAny.isFinished());
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < promises.size(); ++i)
        threads.emplace_back([&promises, &promises2, i] {
            for (auto *p : {&promises[i], &promises2[i]}) {
                p->start();
                p->addResult(static_cast<int>(i));
                p->finish();
            }
        });
    for (auto &t : threads)
        t.join();
    CHECK(rangeAll.result().size() == 5);
    const auto firstOfRange = rangeAny.result();
    CHECK(firstOfRange.index >= 0 && firstOfRange.index < 5);
    CHECK(firstOfRange.future.result() == firstOfRange.index);

    std::vector<QFuture<int>> none;
    CHECK(QtFuture::whenAll(none.begin(), none.end()).result().isEmpty());
    auto taken = QtFuture::makeReadyValueFuture(1);
    auto cont = taken.then([](int v) { return v; });
    CHECK(QtFuture::whenAll(taken).isCanceled()); // it has a continuation already
    pool.waitForDone();
}

static void testEventLoopLocker() {
    QEventLoop loop;
    std::atomic<int> code{-1};
    auto first = std::make_unique<QEventLoopLocker>(&loop);
    auto second = std::make_unique<QEventLoopLocker>(&loop);
    QTimer::singleShot(0, &loop, [&] { first.reset(); });
    QTimer::singleShot(10, &loop, [&] {
        CHECK(loop.isRunning()); // one locker is left
        second.reset();
    });
    code = loop.exec();
    CHECK(code == 0 && !first && !second);

    // A thread quits when its last locker goes.
    QThread t;
    t.start();
    {
        QEventLoopLocker locker(&t);
        QEventLoopLocker other(&t);
    }
    CHECK(t.wait(60000));

    // The application quits when the last locker is destroyed, if the lock is enabled.
    CHECK(QCoreApplication::isQuitLockEnabled());
    QCoreApplication::setQuitLockEnabled(false);
    CHECK(!QCoreApplication::isQuitLockEnabled());
    QEventLoop appLoop;
    QTimer::singleShot(0, &appLoop, [&] { QEventLoopLocker l; });
    QTimer::singleShot(30, &appLoop, [&] { appLoop.quit(); });
    appLoop.exec();
    QCoreApplication::setQuitLockEnabled(true);

    QEventLoop quitLoop;
    QTimer::singleShot(0, &quitLoop, [&] {
        QEventLoopLocker l1;
        QEventLoopLocker l2;
    });
    // The application-wide quit stops every loop of the application's thread.
    CHECK(quitLoop.exec() == 0);

    // A locker outlives its loop.
    {
        auto *l = new QEventLoop;
        QEventLoopLocker locker(l);
        delete l;
    }
}

static void testAbstractEventDispatcher() {
    QAbstractEventDispatcher *d = QAbstractEventDispatcher::instance();
    CHECK(d != nullptr && d == QAbstractEventDispatcher::instance());
    CHECK(d == QAbstractEventDispatcher::instance(QThread::currentThread()));
    CHECK(d->thread() == QThread::currentThread());
    // QThread and QCoreApplication hand out the same object.
    CHECK(QThread::currentThread()->eventDispatcher() == d);
    CHECK(QCoreApplication::eventDispatcher() == d);
    {
        // A thread has its dispatcher before it is started.
        QThread idle;
        QAbstractEventDispatcher *theirs = idle.eventDispatcher();
        CHECK(theirs != nullptr && theirs != d && theirs == QAbstractEventDispatcher::instance(&idle));
        CHECK(theirs->thread() == &idle);
    }

    TimerCounter obj;
    const int id = d->registerTimer(5, Qt::PreciseTimer, &obj);
    CHECK(id > 0);
    const auto timers = d->registeredTimers(&obj);
    CHECK(timers.size() == 1 && timers[0].timerId == id && timers[0].interval == 5);
    const int left = d->remainingTime(id);
    CHECK(left >= 0 && left <= 5);
    CHECK(d->remainingTime(id + 100000) == -1);
    CHECK(waitUntil([&] { return obj.ids.size() >= 3; }));
    CHECK(obj.ids[0] == id);
    CHECK(d->unregisterTimer(id));
    CHECK(!d->unregisterTimer(id));
    CHECK(d->registeredTimers(&obj).isEmpty());

    const int own = QAbstractEventDispatcher::allocateTimerId();
    d->registerTimer(own, 1000, Qt::CoarseTimer, &obj);
    const int other = d->registerTimer(1000, Qt::CoarseTimer, &obj);
    CHECK(own != other && d->registeredTimers(&obj).size() == 2);
    CHECK(d->unregisterTimers(&obj));
    CHECK(!d->unregisterTimers(&obj));
    QAbstractEventDispatcher::releaseTimerId(own);

    // The signals around a wait.
    std::atomic<int> blocks{0};
    std::atomic<int> awakes{0};
    const auto c1 = QObject::connect(d, &QAbstractEventDispatcher::aboutToBlock, [&] { ++blocks; });
    const auto c2 = QObject::connect(d, &QAbstractEventDispatcher::awake, [&] { ++awakes; });
    QTimer::singleShot(10, [] {});
    CHECK(d->processEvents(QEventLoop::WaitForMoreEvents));
    CHECK(blocks >= 1 && awakes >= 1);
    QObject::disconnect(c1);
    QObject::disconnect(c2);
    d->flush();

    // Another thread wakes and interrupts a dispatcher that waits.
    std::atomic<bool> waiting{false};
    std::atomic<bool> returned{false};
    std::atomic<QAbstractEventDispatcher *> theirs{nullptr};
    QThread *t = QThread::create([&] {
        theirs = QAbstractEventDispatcher::instance();
        waiting = true;
        theirs.load()->processEvents(QEventLoop::WaitForMoreEvents);
        returned = true;
    });
    t->start();
    CHECK(spinUntil([&] { return waiting.load(); }));
    QAbstractEventDispatcher *remote = QAbstractEventDispatcher::instance(t);
    CHECK(remote == theirs.load());
    CHECK(t->eventDispatcher() == remote);
    // The wait may not have started yet; interrupting makes the next one return too.
    remote->interrupt();
    CHECK(spinUntil([&] { return returned.load(); }));
    CHECK(t->wait());
    delete t;

    // Socket notifiers can be switched through the dispatcher.
    int fds[2];
    CHECK(::pipe(fds) == 0);
    {
        QSocketNotifier notifier(fds[0], QSocketNotifier::Read);
        int activations = 0;
        QObject::connect(&notifier, &QSocketNotifier::activated, [&](QSocketDescriptor) {
            ++activations;
            char c = 0;
            const ssize_t n = ::read(fds[0], &c, 1);
            (void)n;
        });
        // Unregistering stops the notifier without changing isEnabled(), as in Qt.
        d->unregisterSocketNotifier(&notifier);
        CHECK(notifier.isEnabled());
        const char byte = 1;
        const ssize_t n = ::write(fds[1], &byte, 1);
        (void)n;
        QCoreApplication::processEvents();
        CHECK(activations == 0);
        d->registerSocketNotifier(&notifier);
        CHECK(waitUntil([&] { return activations == 1; }));
    }
    ::close(fds[0]);
    ::close(fds[1]);
}

// Set NT_TEST_VERBOSE=1 to see which test is running, for example when one hangs.
static void testFutureIterator() {
    {
        // A finished future: forward, backward, peeking and searching.
        QFuture<int> f = QtFuture::makeReadyRangeFuture(QList<int>{1, 2, 3, 2});
        QFutureIterator<int> it(f);
        CHECK(!it.hasPrevious() && it.hasNext());
        CHECK(it.peekNext() == 1);
        CHECK(it.next() == 1 && it.next() == 2);
        CHECK(it.hasPrevious() && it.peekPrevious() == 2);
        CHECK(it.findNext(2) && it.peekPrevious() == 2 && !it.hasNext());
        CHECK(!it.findNext(9) && !it.hasNext());
        it.toFront();
        CHECK(!it.hasPrevious() && it.peekNext() == 1);
        it.toBack();
        CHECK(!it.hasNext() && it.hasPrevious());
        CHECK(it.previous() == 2 && it.previous() == 3);
        CHECK(it.findPrevious(1) && !it.hasPrevious() && it.peekNext() == 1);
        CHECK(!it.findPrevious(7));
        it = QtFuture::makeReadyRangeFuture(QList<int>{8});
        CHECK(it.hasNext() && it.next() == 8 && !it.hasNext());
    }
    {
        // An empty finished future has nothing on either side.
        QPromise<QString> p;
        QFuture<QString> f = p.future();
        p.start();
        p.finish();
        QFutureIterator<QString> it(f);
        CHECK(!it.hasNext() && !it.hasPrevious());
        it.toBack();
        CHECK(!it.hasNext() && !it.hasPrevious());
    }
    {
        // hasNext() waits for results that another thread reports, and for the end.
        QPromise<QString> p;
        QFuture<QString> f = p.future();
        p.start();
        QThread *producer = QThread::create([&p] {
            for (int i = 0; i < 50; ++i)
                p.addResult(QString::number(i));
            p.finish();
        });
        producer->start();
        QFutureIterator<QString> it(f);
        int n = 0;
        bool inOrder = true;
        while (it.hasNext()) {
            inOrder = inOrder && it.next() == QString::number(n);
            ++n;
        }
        producer->wait();
        delete producer;
        CHECK(n == 50 && inOrder);
        CHECK(it.hasPrevious() && it.peekPrevious() == QStringLiteral("49"));
    }
}

namespace {

struct RecordingFilter : QAbstractNativeEventFilter {
    RecordingFilter(std::vector<int> *calls, int filterId) : log(calls), id(filterId) {}
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override {
        log->push_back(id);
        lastType = eventType;
        lastMessage = message;
        if (result)
            *result = id;
        if (onFilter)
            onFilter();
        return stop;
    }
    std::vector<int> *log;
    int id;
    bool stop = false;
    QByteArray lastType;
    void *lastMessage = nullptr;
    std::function<void()> onFilter;
};

} // namespace

static void testNativeEventFilters() {
    QAbstractEventDispatcher *d = QAbstractEventDispatcher::instance();
    std::vector<int> log;
    int message = 0;
    qintptr result = 0;
    CHECK(!d->filterNativeEvent("test_event", &message, &result));

    RecordingFilter first(&log, 1), second(&log, 2);
    d->installNativeEventFilter(&first);
    d->installNativeEventFilter(&second);
    d->installNativeEventFilter(nullptr);
    // The filter installed last is called first; nothing stops the event.
    CHECK(!d->filterNativeEvent("test_event", &message, &result));
    CHECK((log == std::vector<int>{2, 1}));
    CHECK(first.lastType == "test_event" && first.lastMessage == &message && result == 1);

    // Installing again moves a filter to the front; a filter that returns true stops the event.
    log.clear();
    d->installNativeEventFilter(&first);
    first.stop = true;
    CHECK(d->filterNativeEvent("test_event", &message, &result));
    CHECK((log == std::vector<int>{1}));
    first.stop = false;

    // Removed filters are not called; removing one that is not installed does nothing.
    log.clear();
    d->removeNativeEventFilter(&first);
    d->removeNativeEventFilter(&first);
    CHECK(!d->filterNativeEvent("test_event", &message, nullptr));
    CHECK((log == std::vector<int>{2}));

    // A filter can remove a later one while it filters, and the removed one is skipped.
    log.clear();
    d->installNativeEventFilter(&first);
    first.onFilter = [&] { d->removeNativeEventFilter(&second); };
    CHECK(!d->filterNativeEvent("test_event", &message, nullptr));
    CHECK((log == std::vector<int>{1}));
    first.onFilter = nullptr;

    // A destroyed filter removes itself, and QCoreApplication installs on the main thread's dispatcher.
    log.clear();
    {
        RecordingFilter temporary(&log, 3);
        qApp->installNativeEventFilter(&temporary);
        CHECK(!d->filterNativeEvent("test_event", &message, nullptr));
        CHECK((log == std::vector<int>{3, 1}));
    }
    log.clear();
    CHECK(!d->filterNativeEvent("test_event", &message, nullptr));
    CHECK((log == std::vector<int>{1}));
    qApp->removeNativeEventFilter(&first);
    log.clear();
    CHECK(!d->filterNativeEvent("test_event", &message, nullptr));
    CHECK(log.empty());

    // Filters belong to the thread of the dispatcher: another thread's filters do not see this thread's events,
    // and a filter can be installed from another thread.
    std::vector<int> theirLog;
    RecordingFilter theirs(&theirLog, 4);
    QThread worker;
    QAbstractEventDispatcher *wd = worker.eventDispatcher();
    QThread *installer = QThread::create([&] { wd->installNativeEventFilter(&theirs); });
    installer->start();
    installer->wait();
    delete installer;
    CHECK(!d->filterNativeEvent("test_event", &message, nullptr));
    CHECK(theirLog.empty());
    CHECK(!wd->filterNativeEvent("test_event", &message, nullptr));
    CHECK((theirLog == std::vector<int>{4}));
    d->installNativeEventFilter(&theirs);
    // Destroying the filter removes it from both threads.
    {
        RecordingFilter both(&theirLog, 5);
        d->installNativeEventFilter(&both);
        wd->installNativeEventFilter(&both);
    }
    theirLog.clear();
    CHECK(!d->filterNativeEvent("test_event", &message, nullptr) && !wd->filterNativeEvent("x", nullptr, nullptr));
    CHECK((theirLog == std::vector<int>{4, 4}));
    d->removeNativeEventFilter(&theirs);
    wd->removeNativeEventFilter(&theirs);
}

#define RUN(fn)                                                                                                        \
    do {                                                                                                               \
        if (std::getenv("NT_TEST_VERBOSE"))                                                                            \
            std::fprintf(stderr, "%s\n", #fn);                                                                         \
        fn();                                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    RUN(testBasics);
    RUN(testResultOrder);
    RUN(testMoveOnlyResults);
    RUN(testProducerConsumer);
    RUN(testLargeResultCount);
    RUN(testPendingWait);
    RUN(testCancel);
    RUN(testCancelRace);
    RUN(testExceptions);
    RUN(testSuspend);
    RUN(testProgress);
    RUN(testThenSync);
    RUN(testThenFailures);
    RUN(testOnFailedOnCanceled);
    RUN(testThenLaunch);
    RUN(testThenContext);
    RUN(testCancelChain);
    RUN(testContinuationChainAcrossThreads);
    RUN(testWatcherSignals);
    RUN(testWatcherCancel);
    RUN(testWatcherSuspend);
    RUN(testWatcherThreads);
    RUN(testWatcherDestruction);
    RUN(testWatcherThrottle);
    RUN(testSynchronizer);
    RUN(testQtFutureHelpers);
    RUN(testWhenAllAny);
    RUN(testEventLoopLocker);
    RUN(testAbstractEventDispatcher);
    RUN(testFutureIterator);
    RUN(testNativeEventFilters);

    if (g_failures == 0)
        std::puts("all future tests passed");
    return g_failures == 0 ? 0 : 1;
}
