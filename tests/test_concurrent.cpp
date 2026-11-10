// MIT License - see LICENSE
#include <QCoreApplication>
#include <QException>
#include <QFuture>
#include <QFutureWatcher>
#include <QList>
#include <QMap>
#include <QPromise>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QThreadPool>
#include <QtConcurrent>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <numeric>
#include <set>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
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
template <typename Pred> static bool spinUntil(Pred pred, int timeoutMs = 60000) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!pred()) {
        if (std::chrono::steady_clock::now() > end)
            return false;
        std::this_thread::yield();
    }
    return true;
}

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

namespace {

struct MyError : QException {
    explicit MyError(int c = 0) : code(c) {}
    void raise() const override { throw *this; }
    MyError *clone() const override { return new MyError(*this); }
    int code;
};

int twice(int v) { return v * 2; }
void triple(int &v) { v *= 3; }
bool isEven(int v) { return v % 2 == 0; }
void addTo(int &sum, int v) { sum += v; }
void sumWithPromise(QPromise<int> &promise, int from, int to) {
    for (int i = from; i < to; ++i)
        promise.addResult(i);
}

struct Number {
    int value = 0;
    void negate() { value = -value; }
    int squared() const { return value * value; }
    bool positive() const { return value > 0; }
    int plus(int other) const { return value + other; }
    void collect(QPromise<int> &promise, int count) const {
        for (int i = 0; i < count; ++i)
            promise.addResult(value + i);
    }
};

struct Accumulator {
    QList<int> values;
    void add(const int &v) { values.append(v); }
};

struct MoveOnly {
    explicit MoveOnly(int v) : value(std::make_unique<int>(v)) {}
    std::unique_ptr<int> value;
};

QList<int> iota(int n) {
    QList<int> list;
    for (int i = 0; i < n; ++i)
        list.append(i);
    return list;
}

// Blocks the threads of a pool until it is told to let go.
class Gate {
public:
    void wait() {
        ++waiting;
        std::unique_lock<std::mutex> lk(m_mutex);
        m_cond.wait(lk, [this] { return m_open; });
    }
    void open() {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_open = true;
        }
        m_cond.notify_all();
    }
    std::atomic<int> waiting{0};

private:
    std::mutex m_mutex;
    std::condition_variable m_cond;
    bool m_open = false;
};

} // namespace

static void testRun() {
    QThreadPool pool;
    const QThread *const here = QThread::currentThread();

    // A function, its arguments, and the type of the future.
    QFuture<int> a = QtConcurrent::run(&pool, twice, 21);
    static_assert(std::is_same_v<decltype(QtConcurrent::run(twice, 1)), QFuture<int>>);
    CHECK(a.result() == 42);
    CHECK(a.isFinished() && !a.isCanceled());
    CHECK(QtConcurrent::run(twice, 4).result() == 8);

    // The task runs in the pool that was named.
    std::atomic<bool> inPool{false};
    QFuture<void> v = QtConcurrent::run(&pool, [&] { inPool = pool.contains(QThread::currentThread()); });
    static_assert(std::is_same_v<decltype(v), QFuture<void>>);
    v.waitForFinished();
    CHECK(inPool);
    std::atomic<QThread *> where{nullptr};
    QtConcurrent::run([&] { where = QThread::currentThread(); }).waitForFinished();
    CHECK(where != nullptr && where != here);

    // Arguments are copied; std::ref shares.
    int shared = 1;
    QString text = QStringLiteral("abc");
    QFuture<QString> s = QtConcurrent::run(
        &pool,
        [](QString t, int &n) {
            n = 7;
            return t + t;
        },
        text, std::ref(shared));
    CHECK(s.result() == QStringLiteral("abcabc"));
    CHECK(shared == 7 && text == QStringLiteral("abc"));

    // Member functions, const and not, through a pointer or a copy.
    Number n{5};
    CHECK(QtConcurrent::run(&pool, &Number::squared, n).result() == 25);
    CHECK(QtConcurrent::run(&pool, &Number::plus, &n, 3).result() == 8);
    QtConcurrent::run(&pool, &Number::negate, &n).waitForFinished();
    CHECK(n.value == -5);

    // Move-only functions, arguments and results.
    auto owned = std::make_unique<int>(9);
    QFuture<MoveOnly> m =
        QtConcurrent::run(&pool, [o = std::move(owned)](std::unique_ptr<int> p) { return MoveOnly(*o + *p); },
                          std::make_unique<int>(1));
    CHECK(*m.takeResult().value == 10);

    // A function that returns a reference gives a future of the value.
    static int g = 3;
    QFuture<int> r = QtConcurrent::run(&pool, []() -> int & { return g; });
    CHECK(r.result() == 3);

    // Exceptions travel to the waiting thread.
    QFuture<int> e = QtConcurrent::run(&pool, []() -> int { throw MyError(4); });
    bool caught = false;
    try {
        e.waitForFinished();
    } catch (const MyError &err) {
        caught = err.code == 4;
    }
    CHECK(caught);
    QFuture<void> e2 = QtConcurrent::run(&pool, [] { throw std::runtime_error("plain"); });
    caught = false;
    try {
        e2.waitForFinished();
    } catch (const std::runtime_error &err) {
        caught = std::string(err.what()) == "plain";
    } catch (const QUnhandledException &) {
        caught = true;
    }
    CHECK(caught);

    // A future that is canceled before its task starts does not run the function.
    {
        QThreadPool one;
        one.setMaxThreadCount(1);
        Gate gate;
        QFuture<void> blocker = QtConcurrent::run(&one, [&] { gate.wait(); });
        CHECK(spinUntil([&] { return gate.waiting == 1; }));
        std::atomic<bool> ran{false};
        QFuture<int> queued = QtConcurrent::run(&one, [&] {
            ran = true;
            return 1;
        });
        CHECK(queued.isStarted() && queued.isRunning());
        queued.cancel();
        gate.open();
        blocker.waitForFinished();
        queued.waitForFinished();
        CHECK(!ran && queued.isCanceled() && queued.isFinished() && queued.resultCount() == 0);
    }

    // then() with Launch::Inherit continues in the pool of the task.
    std::atomic<bool> thenInPool{false};
    QFuture<int> chained = QtConcurrent::run(&pool, twice, 2).then(QtFuture::Launch::Inherit, [&](int x) {
        thenInPool = pool.contains(QThread::currentThread());
        return x + 1;
    });
    CHECK(chained.result() == 5);
    CHECK(thenInPool);
}

static void testRunWithPromise() {
    QThreadPool pool;

    // A function pointer, a lambda and a member function that take a promise first.
    QFuture<int> f = QtConcurrent::run(&pool, sumWithPromise, 3, 6);
    static_assert(std::is_same_v<decltype(f), QFuture<int>>);
    CHECK(f.results() == QList<int>({3, 4, 5}));
    QFuture<QString> l = QtConcurrent::run([](QPromise<QString> &p, const QString &s) {
        p.addResult(s);
        p.addResult(s + s);
    }, QStringLiteral("x"));
    CHECK(l.results() == QList<QString>({QStringLiteral("x"), QStringLiteral("xx")}));
    const Number n{10};
    CHECK(QtConcurrent::run(&pool, &Number::collect, &n, 2).results() == QList<int>({10, 11}));
    QFuture<void> v = QtConcurrent::run(&pool, [](QPromise<void> &) {});
    v.waitForFinished();
    CHECK(v.isFinished() && !v.isCanceled());

    // Progress, cancellation and suspension reach the function.
    std::atomic<int> steps{0};
    QFuture<int> c = QtConcurrent::run(&pool, [&](QPromise<int> &p) {
        p.setProgressRange(0, 1000000);
        while (!p.isCanceled()) {
            p.suspendIfRequested();
            p.setProgressValue(++steps);
            QThread::msleep(1);
        }
    });
    CHECK(spinUntil([&] { return c.progressValue() > 2; }));
    CHECK(c.progressMaximum() == 1000000);
    c.suspend();
    CHECK(spinUntil([&] { return c.isSuspended(); }));
    const int frozen = steps;
    QThread::msleep(20);
    CHECK(steps == frozen);
    c.resume();
    CHECK(spinUntil([&] { return steps > frozen; }));
    c.cancel();
    c.waitForFinished();
    CHECK(c.isCanceled() && c.isFinished());

    // An exception ends the future; the results before it stay.
    QFuture<int> e = QtConcurrent::run(&pool, [](QPromise<int> &p) {
        p.addResult(1);
        throw MyError(2);
    });
    bool caught = false;
    try {
        e.waitForFinished();
    } catch (const MyError &err) {
        caught = err.code == 2;
    }
    CHECK(caught && e.isFinished());
}

static void testTask() {
    QThreadPool pool;
    CHECK(QtConcurrent::task(twice).withArguments(8).spawn().result() == 16);
    CHECK(QtConcurrent::task([] { return 5; }).spawn().result() == 5);

    std::atomic<bool> inPool{false};
    QFuture<QString> f = QtConcurrent::task([&](const QString &a, int b) {
                             inPool = pool.contains(QThread::currentThread());
                             return a + QString::number(b);
                         })
                             .withArguments(QStringLiteral("n"), 3)
                             .onThreadPool(pool)
                             .withPriority(2)
                             .spawn();
    CHECK(f.result() == QStringLiteral("n3"));
    CHECK(inPool);

    // The order of the settings does not matter, and a promise works as in run().
    QFuture<int> p = QtConcurrent::task(sumWithPromise).onThreadPool(pool).withArguments(0, 3).spawn();
    CHECK(p.results() == QList<int>({0, 1, 2}));

    // Ignoring the future still runs the task.
    std::atomic<bool> ran{false};
    QtConcurrent::task([&] { ran = true; }).onThreadPool(pool).spawn(QtConcurrent::FutureResult::Ignore);
    CHECK(spinUntil([&] { return ran.load(); }));

    // Tasks with a higher priority leave the queue first.
    QThreadPool one;
    one.setMaxThreadCount(1);
    Gate gate;
    QFuture<void> blocker = QtConcurrent::run(&one, [&] { gate.wait(); });
    CHECK(spinUntil([&] { return gate.waiting == 1; }));
    std::mutex m;
    std::vector<int> order;
    QList<QFuture<void>> futures;
    for (int priority : {0, 5, 2})
        futures.append(QtConcurrent::task([&m, &order](int value) {
                           std::lock_guard<std::mutex> lk(m);
                           order.push_back(value);
                       })
                           .withArguments(priority)
                           .onThreadPool(one)
                           .withPriority(priority)
                           .spawn());
    gate.open();
    for (QFuture<void> &future : futures)
        future.waitForFinished();
    blocker.waitForFinished();
    CHECK(order == std::vector<int>({5, 2, 0}));
}

static void testMap() {
    QThreadPool pool;
    pool.setMaxThreadCount(4);

    // In place, on a sequence and on a range, with every kind of function.
    QList<int> list = iota(1000);
    QFuture<void> f = QtConcurrent::map(&pool, list, triple);
    f.waitForFinished();
    CHECK(f.isFinished() && !f.isCanceled());
    CHECK(f.progressMinimum() == 0 && f.progressMaximum() == 1000 && f.progressValue() == 1000);
    bool ok = true;
    for (int i = 0; i < 1000; ++i)
        ok = ok && list.at(i) == i * 3;
    CHECK(ok);

    std::vector<int> vec(500, 1);
    QtConcurrent::map(vec, [](int &v) { ++v; }).waitForFinished();
    CHECK(std::accumulate(vec.begin(), vec.end(), 0) == 1000);
    QtConcurrent::map(&pool, vec.begin() + 100, vec.end(), [](int &v) { v = 0; }).waitForFinished();
    CHECK(std::accumulate(vec.begin(), vec.end(), 0) == 200);
    QtConcurrent::map(vec.begin(), vec.begin() + 100, std::function<void(int &)>(triple)).waitForFinished();
    CHECK(std::accumulate(vec.begin(), vec.end(), 0) == 600);

    QList<Number> numbers;
    for (int i = 1; i <= 50; ++i)
        numbers.append(Number{i});
    QtConcurrent::map(&pool, numbers, &Number::negate).waitForFinished();
    CHECK(numbers.first().value == -1 && numbers.last().value == -50);

    // Iterators that are not random access.
    std::list<int> linked(300, 2);
    QtConcurrent::map(&pool, linked, triple).waitForFinished();
    CHECK(std::accumulate(linked.begin(), linked.end(), 0) == 1800);
    QMap<int, int> byKey;
    for (int i = 0; i < 100; ++i)
        byKey.insert(i, i);
    QtConcurrent::map(&pool, byKey.begin(), byKey.end(), [](int &v) { v += 1; }).waitForFinished();
    CHECK(byKey.value(0) == 1 && byKey.value(99) == 100);

    // The work is shared between the threads of the pool.
    std::mutex m;
    std::set<QThread *> threads;
    QList<int> slow = iota(64);
    QtConcurrent::map(&pool, slow, [&](int &) {
        {
            std::lock_guard<std::mutex> lk(m);
            threads.insert(QThread::currentThread());
        }
        QThread::msleep(2);
    }).waitForFinished();
    CHECK(threads.size() > 1 && threads.size() <= 4);
    CHECK(threads.count(QThread::currentThread()) == 0);

    // Nothing to do.
    QList<int> empty;
    QFuture<void> e = QtConcurrent::map(&pool, empty, triple);
    CHECK(e.isStarted() && e.isFinished() && !e.isCanceled());

    // The blocking form works in the calling thread as well, and returns when all is done.
    QList<int> b = iota(2000);
    QtConcurrent::blockingMap(&pool, b, triple);
    CHECK(b.at(1999) == 5997 && b.at(1) == 3);
    QtConcurrent::blockingMap(b, [](int &v) { v /= 3; });
    CHECK(b == iota(2000));
    QtConcurrent::blockingMap(&pool, b.begin(), b.begin() + 10, triple);
    QtConcurrent::blockingMap(b.begin() + 10, b.begin() + 20, triple);
    CHECK(b.at(9) == 27 && b.at(19) == 57 && b.at(20) == 20);
}

static void testMapped() {
    QThreadPool pool;
    pool.setMaxThreadCount(4);
    const QList<int> list = iota(1000);

    QFuture<int> f = QtConcurrent::mapped(&pool, list, twice);
    QList<int> expected;
    for (int v : list)
        expected.append(v * 2);
    CHECK(f.results() == expected);
    CHECK(f.resultCount() == 1000 && f.progressValue() == 1000);

    // The type of the future comes from the function; a temporary sequence is kept alive.
    QFuture<QString> s = QtConcurrent::mapped(iota(100), [](int v) { return QString::number(v); });
    CHECK(s.resultCount() == 100 || (s.waitForFinished(), s.resultCount() == 100));
    CHECK(s.resultAt(42) == QStringLiteral("42") && s.resultAt(99) == QStringLiteral("99"));
    QStringList words{QStringLiteral("a"), QStringLiteral("bcd"), QStringLiteral("ef")};
    QFuture<qsizetype> lengths = QtConcurrent::mapped(&pool, words, &QString::size);
    CHECK(lengths.results() == QList<qsizetype>({1, 3, 2}));

    // Ranges, random access or not.
    CHECK(QtConcurrent::mapped(&pool, list.begin() + 10, list.begin() + 13, twice).results() ==
          QList<int>({20, 22, 24}));
    const std::set<int> ordered{5, 1, 3};
    CHECK(QtConcurrent::mapped(ordered.begin(), ordered.end(), twice).results() == QList<int>({2, 6, 10}));
    QFuture<int> fromSet = QtConcurrent::mapped(&pool, ordered, twice);
    CHECK(fromSet.results() == QList<int>({2, 6, 10}));

    // Move-only results.
    QFuture<MoveOnly> owned = QtConcurrent::mapped(&pool, iota(10), [](int v) { return MoveOnly(v); });
    owned.waitForFinished();
    CHECK(owned.resultCount() == 10);

    // A watcher sees every result.
    QFutureWatcher<int> watcher;
    int ready = 0;
    bool finished = false;
    QObject::connect(&watcher, &QFutureWatcherBase::resultsReadyAt, [&](int begin, int end) { ready += end - begin; });
    QObject::connect(&watcher, &QFutureWatcherBase::finished, [&] { finished = true; });
    watcher.setFuture(QtConcurrent::mapped(&pool, list, twice));
    CHECK(waitUntil([&] { return finished; }));
    CHECK(ready == 1000);

    // Blocking, into the container that is named or into one like the input.
    const std::vector<int> named = QtConcurrent::blockingMapped<std::vector<int>>(&pool, list, twice);
    CHECK(named.size() == 1000 && named[999] == 1998);
    const QSet<int> unique = QtConcurrent::blockingMapped<QSet<int>>(list, [](int v) { return v % 10; });
    CHECK(unique.size() == 10);
    auto same = QtConcurrent::blockingMapped(&pool, list, [](int v) { return QString::number(v); });
    static_assert(std::is_same_v<decltype(same), QList<QString>>);
    CHECK(same.size() == 1000 && same.at(17) == QStringLiteral("17"));
    auto fromVector = QtConcurrent::blockingMapped(std::vector<int>{1, 2, 3}, [](int v) { return v * 0.5; });
    static_assert(std::is_same_v<decltype(fromVector), std::vector<double>>);
    CHECK(fromVector == std::vector<double>({0.5, 1.0, 1.5}));
    auto fromWords = QtConcurrent::blockingMapped(words, &QString::size);
    static_assert(std::is_same_v<decltype(fromWords), QList<qsizetype>>);
    CHECK(fromWords == QList<qsizetype>({1, 3, 2}));
    auto fromRange = QtConcurrent::blockingMapped(&pool, list.begin(), list.begin() + 3, twice);
    static_assert(std::is_same_v<decltype(fromRange), QList<int>>);
    CHECK(fromRange == QList<int>({0, 2, 4}));
    CHECK(QtConcurrent::blockingMapped(list.begin(), list.begin() + 2, twice) == QList<int>({0, 2}));
    CHECK(QtConcurrent::blockingMapped<std::list<int>>(&pool, list.begin(), list.begin() + 2, twice) ==
          std::list<int>({0, 2}));
    CHECK(QtConcurrent::blockingMapped<std::list<int>>(list.begin(), list.begin() + 2, twice) ==
          std::list<int>({0, 2}));
}

static void testMappedReduced() {
    QThreadPool pool;
    pool.setMaxThreadCount(4);
    const QList<int> list = iota(1000);
    const int sum = 999 * 1000; // of the doubled values
    using namespace QtConcurrent;

    // The result type is named or taken from the reduce function.
    QFuture<int> a = mappedReduced<int>(&pool, list, twice, [](auto &s, int v) { s += v; });
    CHECK(a.result() == sum && a.resultCount() == 1);
    QFuture<int> b = mappedReduced(&pool, list, twice, addTo);
    CHECK(b.result() == sum);
    CHECK(mappedReduced(list, twice, [](int &s, int v) { s += v; }).result() == sum);
    CHECK(mappedReduced<int>(list, twice, addTo).result() == sum);
    QFuture<long long> wide = mappedReduced(&pool, list, twice, [](long long &s, int v) { s += v; });
    static_assert(std::is_same_v<decltype(wide), QFuture<long long>>);
    CHECK(wide.result() == sum);

    // An initial value.
    CHECK(mappedReduced<int>(&pool, list, twice, addTo, 10).result() == sum + 10);
    CHECK(mappedReduced(&pool, list, twice, addTo, 10).result() == sum + 10);
    CHECK(mappedReduced<int>(list, twice, addTo, 10).result() == sum + 10);
    CHECK(mappedReduced(list, twice, addTo, 10).result() == sum + 10);

    // Ranges.
    const auto first = list.begin(), last = list.begin() + 10;
    CHECK(mappedReduced<int>(&pool, first, last, twice, addTo).result() == 90);
    CHECK(mappedReduced(&pool, first, last, twice, addTo).result() == 90);
    CHECK(mappedReduced<int>(first, last, twice, addTo).result() == 90);
    CHECK(mappedReduced(first, last, twice, addTo).result() == 90);
    CHECK(mappedReduced<int>(&pool, first, last, twice, addTo, 1).result() == 91);
    CHECK(mappedReduced(&pool, first, last, twice, addTo, 1).result() == 91);
    CHECK(mappedReduced<int>(first, last, twice, addTo, 1).result() == 91);
    CHECK(mappedReduced(first, last, twice, addTo, 1).result() == 91);

    // A reduce option is not taken for an initial value.
    CHECK(mappedReduced(&pool, list, twice, addTo, OrderedReduce).result() == sum);
    CHECK(mappedReduced(list, twice, addTo, UnorderedReduce | SequentialReduce).result() == sum);
    CHECK(mappedReduced<int>(first, last, twice, addTo, OrderedReduce).result() == 90);
    CHECK(mappedReduced(first, last, twice, addTo, 5, OrderedReduce).result() == 95);

    // Ordered reduction sees the results in the order of the items.
    QFuture<QList<int>> ordered = mappedReduced<QList<int>>(
        &pool, list, twice, [](QList<int> &all, int v) { all.append(v); }, OrderedReduce | SequentialReduce);
    QList<int> doubled;
    for (int v : list)
        doubled.append(v * 2);
    CHECK(ordered.result() == doubled);
    QFuture<QList<int>> unordered = mappedReduced<QList<int>>(&pool, list, twice, [](QList<int> &all, int v) { all.append(v); });
    QList<int> sorted = unordered.result();
    std::sort(sorted.begin(), sorted.end());
    CHECK(sorted == doubled);

    // Member functions for both steps; the class of the reduce function is the result type.
    QList<Number> numbers;
    for (int i = 1; i <= 100; ++i)
        numbers.append(Number{i});
    QFuture<Accumulator> acc = mappedReduced(&pool, numbers, &Number::squared, &Accumulator::add, OrderedReduce);
    CHECK(acc.result().values.size() == 100 && acc.result().values.at(9) == 100);
    QFuture<QSet<int>> set = mappedReduced<QSet<int>>(
        numbers, [](const Number &n) { return n.value % 7; }, [](QSet<int> &s, int v) { s.insert(v); });
    CHECK(set.result().size() == 7);

    // The reduce function is entered by one thread at a time.
    std::atomic<int> inside{0};
    std::atomic<bool> overlapped{false};
    mappedReduced<int>(&pool, list, twice, [&](int &s, int v) {
        if (++inside != 1)
            overlapped = true;
        s += v;
        --inside;
    }).waitForFinished();
    CHECK(!overlapped);

    // Nothing to reduce gives the initial value.
    CHECK(mappedReduced(&pool, QList<int>(), twice, addTo).result() == 0);
    CHECK(mappedReduced(&pool, QList<int>(), twice, addTo, 3).result() == 3);

    // Blocking.
    CHECK(blockingMappedReduced<int>(&pool, list, twice, addTo) == sum);
    CHECK(blockingMappedReduced(&pool, list, twice, addTo) == sum);
    CHECK(blockingMappedReduced<int>(list, twice, addTo) == sum);
    CHECK(blockingMappedReduced(list, twice, addTo) == sum);
    CHECK(blockingMappedReduced<int>(&pool, list, twice, addTo, 2) == sum + 2);
    CHECK(blockingMappedReduced(&pool, list, twice, addTo, 2) == sum + 2);
    CHECK(blockingMappedReduced<int>(list, twice, addTo, 2) == sum + 2);
    CHECK(blockingMappedReduced(list, twice, addTo, 2) == sum + 2);
    CHECK(blockingMappedReduced<int>(&pool, first, last, twice, addTo) == 90);
    CHECK(blockingMappedReduced(&pool, first, last, twice, addTo) == 90);
    CHECK(blockingMappedReduced<int>(first, last, twice, addTo) == 90);
    CHECK(blockingMappedReduced(first, last, twice, addTo) == 90);
    CHECK(blockingMappedReduced<int>(&pool, first, last, twice, addTo, 2) == 92);
    CHECK(blockingMappedReduced(&pool, first, last, twice, addTo, 2) == 92);
    CHECK(blockingMappedReduced<int>(first, last, twice, addTo, 2) == 92);
    CHECK(blockingMappedReduced(first, last, twice, addTo, 2, OrderedReduce) == 92);
    CHECK(blockingMappedReduced<QList<int>>(
              &pool, list, twice, [](QList<int> &all, int v) { all.append(v); }, OrderedReduce) == doubled);
}

static void testFilter() {
    QThreadPool pool;
    pool.setMaxThreadCount(4);
    QList<int> evens;
    for (int i = 0; i < 1000; i += 2)
        evens.append(i);

    // In place: the sequence is assigned at the end, in order.
    QList<int> list = iota(1000);
    QFuture<void> f = QtConcurrent::filter(&pool, list, isEven);
    f.waitForFinished();
    CHECK(list == evens && f.progressValue() == 1000);
    std::vector<int> vec(100);
    std::iota(vec.begin(), vec.end(), 0);
    QtConcurrent::filter(vec, [](const int &v) { return v >= 90; }).waitForFinished();
    CHECK(vec == std::vector<int>({90, 91, 92, 93, 94, 95, 96, 97, 98, 99}));
    QList<Number> numbers{Number{1}, Number{-2}, Number{3}};
    QtConcurrent::filter(&pool, numbers, &Number::positive).waitForFinished();
    CHECK(numbers.size() == 2 && numbers.at(1).value == 3);
    std::set<int> tree{1, 2, 3, 4};
    QtConcurrent::blockingFilter(&pool, tree, isEven);
    CHECK(tree == std::set<int>({2, 4}));
    QStringList words{QStringLiteral("a"), QString(), QStringLiteral("b")};
    QtConcurrent::blockingFilter(words, [](const QString &w) { return !w.isEmpty(); });
    CHECK(words == QStringList({QStringLiteral("a"), QStringLiteral("b")}));

    // Copies of the items that are kept, in order.
    const QList<int> all = iota(1000);
    QFuture<int> kept = QtConcurrent::filtered(&pool, all, isEven);
    CHECK(kept.results() == evens);
    CHECK(QtConcurrent::filtered(iota(10), [](int v) { return v > 7; }).results() == QList<int>({8, 9}));
    CHECK(QtConcurrent::filtered(&pool, all.begin(), all.begin() + 5, isEven).results() == QList<int>({0, 2, 4}));
    CHECK(QtConcurrent::filtered(all.begin(), all.begin() + 5, isEven).results() == QList<int>({0, 2, 4}));
    QFuture<int> none = QtConcurrent::filtered(&pool, all, [](int) { return false; });
    none.waitForFinished();
    CHECK(none.resultCount() == 0 && none.isFinished());
    QFuture<int> fromSet = QtConcurrent::filtered(&pool, std::set<int>{3, 4, 5, 6}, isEven);
    CHECK(fromSet.results() == QList<int>({4, 6}));

    // Blocking: the same type as the sequence, or the one that is named for a range.
    auto same = QtConcurrent::blockingFiltered(&pool, all, isEven);
    static_assert(std::is_same_v<decltype(same), QList<int>>);
    CHECK(same == evens);
    const std::vector<int> v2 = QtConcurrent::blockingFiltered(std::vector<int>{1, 2, 3, 4}, isEven);
    CHECK(v2 == std::vector<int>({2, 4}));
    CHECK(QtConcurrent::blockingFiltered<std::vector<int>>(&pool, all.begin(), all.begin() + 5, isEven) ==
          std::vector<int>({0, 2, 4}));
    CHECK(QtConcurrent::blockingFiltered<QList<int>>(all.begin(), all.begin() + 5, isEven) == QList<int>({0, 2, 4}));
}

static void testFilteredReduced() {
    QThreadPool pool;
    pool.setMaxThreadCount(4);
    const QList<int> list = iota(1000);
    const int sum = 499 * 500; // of the even values
    using namespace QtConcurrent;
    const auto first = list.begin(), last = list.begin() + 10;

    CHECK(filteredReduced<int>(&pool, list, isEven, addTo).result() == sum);
    CHECK(filteredReduced(&pool, list, isEven, addTo).result() == sum);
    CHECK(filteredReduced<int>(list, isEven, addTo).result() == sum);
    CHECK(filteredReduced(list, isEven, addTo).result() == sum);
    CHECK(filteredReduced<int>(&pool, list, isEven, addTo, 1).result() == sum + 1);
    CHECK(filteredReduced(&pool, list, isEven, addTo, 1).result() == sum + 1);
    CHECK(filteredReduced<int>(list, isEven, addTo, 1).result() == sum + 1);
    CHECK(filteredReduced(list, isEven, addTo, 1).result() == sum + 1);
    CHECK(filteredReduced<int>(&pool, first, last, isEven, addTo).result() == 20);
    CHECK(filteredReduced(&pool, first, last, isEven, addTo).result() == 20);
    CHECK(filteredReduced<int>(first, last, isEven, addTo).result() == 20);
    CHECK(filteredReduced(first, last, isEven, addTo).result() == 20);
    CHECK(filteredReduced<int>(&pool, first, last, isEven, addTo, 1).result() == 21);
    CHECK(filteredReduced(&pool, first, last, isEven, addTo, 1).result() == 21);
    CHECK(filteredReduced<int>(first, last, isEven, addTo, 1).result() == 21);
    CHECK(filteredReduced(first, last, isEven, addTo, 1).result() == 21);
    CHECK(filteredReduced(list, isEven, addTo, OrderedReduce).result() == sum);
    CHECK(filteredReduced(first, last, isEven, addTo, 1, OrderedReduce).result() == 21);

    QList<int> evens;
    for (int i = 0; i < 1000; i += 2)
        evens.append(i);
    QFuture<QList<int>> ordered = filteredReduced<QList<int>>(
        &pool, list, isEven, [](QList<int> &all, int v) { all.append(v); }, OrderedReduce | SequentialReduce);
    CHECK(ordered.result() == evens);
    QFuture<Accumulator> acc = filteredReduced(&pool, list, [](int v) { return v < 3; }, &Accumulator::add, OrderedReduce);
    CHECK(acc.result().values == QList<int>({0, 1, 2}));
    QFuture<QString> text = filteredReduced<QString>(
        QStringList{QStringLiteral("a"), QString(), QStringLiteral("c")}, [](const QString &s) { return !s.isEmpty(); },
        [](QString &all, const QString &s) { all += s; }, QStringLiteral(">"), OrderedReduce);
    CHECK(text.result() == QStringLiteral(">ac"));

    CHECK(blockingFilteredReduced<int>(&pool, list, isEven, addTo) == sum);
    CHECK(blockingFilteredReduced(&pool, list, isEven, addTo) == sum);
    CHECK(blockingFilteredReduced<int>(list, isEven, addTo) == sum);
    CHECK(blockingFilteredReduced(list, isEven, addTo) == sum);
    CHECK(blockingFilteredReduced<int>(&pool, list, isEven, addTo, 1) == sum + 1);
    CHECK(blockingFilteredReduced(&pool, list, isEven, addTo, 1) == sum + 1);
    CHECK(blockingFilteredReduced<int>(list, isEven, addTo, 1) == sum + 1);
    CHECK(blockingFilteredReduced(list, isEven, addTo, 1) == sum + 1);
    CHECK(blockingFilteredReduced<int>(&pool, first, last, isEven, addTo) == 20);
    CHECK(blockingFilteredReduced(&pool, first, last, isEven, addTo) == 20);
    CHECK(blockingFilteredReduced<int>(first, last, isEven, addTo) == 20);
    CHECK(blockingFilteredReduced(first, last, isEven, addTo) == 20);
    CHECK(blockingFilteredReduced<int>(&pool, first, last, isEven, addTo, 1) == 21);
    CHECK(blockingFilteredReduced(&pool, first, last, isEven, addTo, 1) == 21);
    CHECK(blockingFilteredReduced<int>(first, last, isEven, addTo, 1) == 21);
    CHECK(blockingFilteredReduced(first, last, isEven, addTo, 1, OrderedReduce) == 21);
}

static void testCancelAndSuspend() {
    QThreadPool pool;
    pool.setMaxThreadCount(2);

    // Canceling stops the threads at the next block; the future finishes as canceled.
    {
        Gate gate;
        std::atomic<int> calls{0};
        QList<int> list = iota(10000);
        QFuture<void> f = QtConcurrent::map(&pool, list, [&](int &) {
            ++calls;
            gate.wait();
        });
        CHECK(spinUntil([&] { return gate.waiting == 2; }));
        CHECK(f.isRunning() && !f.isFinished());
        f.cancel();
        gate.open();
        f.waitForFinished();
        CHECK(f.isCanceled() && f.isFinished());
        CHECK(calls < 10000);
    }

    // A canceled filter leaves its sequence alone, and a canceled reduction has no result.
    {
        Gate gate;
        QList<int> list = iota(5000);
        QFuture<void> f = QtConcurrent::filter(&pool, list, [&](int) {
            gate.wait();
            return false;
        });
        CHECK(spinUntil([&] { return gate.waiting == 2; }));
        f.cancel();
        gate.open();
        f.waitForFinished();
        CHECK(list.size() == 5000);

        Gate gate2;
        QFuture<int> r = QtConcurrent::mappedReduced(&pool, list, [&](int v) {
            gate2.wait();
            return v;
        }, addTo);
        CHECK(spinUntil([&] { return gate2.waiting == 2; }));
        r.cancel();
        gate2.open();
        r.waitForFinished();
        CHECK(r.isCanceled() && r.resultCount() == 0);
    }

    // Suspending holds the threads between blocks until the future is resumed.
    {
        std::atomic<int> calls{0};
        QList<int> list = iota(4000);
        QFuture<void> f = QtConcurrent::map(&pool, list, [&](int &v) {
            ++calls;
            v = -1;
            QThread::usleep(200);
        });
        CHECK(spinUntil([&] { return calls > 0; }));
        f.suspend();
        CHECK(spinUntil([&] { return f.isSuspended(); }));
        // Blocks that were under way end; then nothing moves.
        int seen = -1;
        CHECK(spinUntil([&] {
            const int now = calls;
            const bool still = now == seen;
            seen = now;
            if (!still)
                QThread::msleep(30);
            return still;
        }));
        CHECK(seen < 4000 && !f.isFinished());
        f.resume();
        f.waitForFinished();
        CHECK(calls == 4000 && !f.isCanceled());
        CHECK(list.count(-1) == 4000);
    }
}

static void testExceptions() {
    QThreadPool pool;
    pool.setMaxThreadCount(4);
    const QList<int> list = iota(1000);

    QFuture<int> f = QtConcurrent::mapped(&pool, list, [](int v) {
        if (v == 500)
            throw MyError(v);
        return v;
    });
    bool caught = false;
    try {
        f.waitForFinished();
    } catch (const MyError &e) {
        caught = e.code == 500;
    }
    CHECK(caught && f.isFinished() && f.isCanceled());

    // The blocking forms throw in the caller, whichever thread ran into it.
    caught = false;
    QList<int> copy = list;
    try {
        QtConcurrent::blockingMap(&pool, copy, [](int &v) {
            if (v == 999)
                throw MyError(1);
        });
    } catch (const MyError &) {
        caught = true;
    }
    CHECK(caught);
    caught = false;
    try {
        (void)QtConcurrent::blockingMappedReduced<int>(&pool, list, twice, [](int &, int v) {
            if (v == 20)
                throw std::runtime_error("reduce");
        });
    } catch (const std::exception &) {
        caught = true;
    }
    CHECK(caught);
    caught = false;
    QList<int> kept = list;
    try {
        QtConcurrent::blockingFilter(&pool, kept, [](int v) -> bool {
            if (v == 3)
                throw MyError(3);
            return false;
        });
    } catch (const MyError &) {
        caught = true;
    }
    CHECK(caught && kept == list);
}

// A job that is started from a thread of its own pool, which has no thread to spare.
static void testNestedBlocking() {
    QThreadPool pool;
    pool.setMaxThreadCount(1);
    QFuture<int> outer = QtConcurrent::run(&pool, [&pool] {
        const QList<int> inner = iota(200);
        return QtConcurrent::blockingMappedReduced<int>(&pool, inner, twice, addTo);
    });
    CHECK(outer.result() == 199 * 200);

    QThreadPool wide;
    wide.setMaxThreadCount(4);
    QList<QList<int>> lists;
    for (int i = 0; i < 16; ++i)
        lists.append(iota(100));
    const int total = QtConcurrent::blockingMappedReduced<int>(
        &wide, lists, [&wide](const QList<int> &l) { return QtConcurrent::blockingMappedReduced<int>(&wide, l, twice, addTo); },
        addTo);
    CHECK(total == 16 * 99 * 100);
}

static void testManyJobs() {
    QThreadPool pool;
    pool.setMaxThreadCount(4);
    QList<QFuture<int>> futures;
    for (int i = 0; i < 200; ++i)
        futures.append(QtConcurrent::mappedReduced<int>(&pool, iota(i), twice, addTo, QtConcurrent::OrderedReduce));
    bool ok = true;
    for (int i = 0; i < 200; ++i)
        ok = ok && futures[i].result() == i * (i - 1);
    CHECK(ok);
    // Futures that nobody waits for finish and let go of their state.
    auto alive = std::make_shared<int>(1);
    for (int i = 0; i < 50; ++i)
        (void)QtConcurrent::mapped(&pool, iota(50), [alive](int v) { return v + *alive; });
    CHECK(pool.waitForDone());
    CHECK(alive.use_count() == 1);
}

// Set NT_TEST_VERBOSE=1 to see which test is running, for example when one hangs.
#define RUN(fn)                                                                                                        \
    do {                                                                                                               \
        if (std::getenv("NT_TEST_VERBOSE"))                                                                            \
            std::fprintf(stderr, "%s\n", #fn);                                                                         \
        fn();                                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    RUN(testRun);
    RUN(testRunWithPromise);
    RUN(testTask);
    RUN(testMap);
    RUN(testMapped);
    RUN(testMappedReduced);
    RUN(testFilter);
    RUN(testFilteredReduced);
    RUN(testCancelAndSuspend);
    RUN(testExceptions);
    RUN(testNestedBlocking);
    RUN(testManyJobs);

    if (g_failures == 0)
        std::puts("all concurrent tests passed");
    return g_failures == 0 ? 0 : 1;
}
