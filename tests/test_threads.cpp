// MIT License - see LICENSE
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QRunnable>
#include <QSocketNotifier>
#include <QString>
#include <QThread>
#include <QThreadPool>
#include <QThreadStorage>
#include <QTimer>

#include <unistd.h>

#include <atomic>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>
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

class Gate {
public:
    void wait() {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [this] { return open_; });
    }
    void open() {
        {
            std::lock_guard<std::mutex> lk(m_);
            open_ = true;
        }
        cv_.notify_all();
    }

private:
    std::mutex m_;
    std::condition_variable cv_;
    bool open_ = false;
};

class Emitter : public QObject {
    Q_OBJECT
public:
    explicit Emitter(QObject *parent = nullptr) : QObject(parent) {}
    void fire(int v) { emit valueChanged(v); }
    void fireText(const QString &s) { emit textChanged(s); }
    void firePing() { emit ping(); }

signals:
    void valueChanged(int value) { emitSignal(&Emitter::valueChanged, value); }
    void textChanged(const QString &text) { emitSignal(&Emitter::textChanged, text); }
    void ping() { emitSignal(&Emitter::ping); }
};

class Recorder : public QObject {
    Q_OBJECT
public:
    explicit Recorder(QObject *parent = nullptr) : QObject(parent) {}
    ~Recorder() override {
        if (deletedIn)
            deletedIn->store(QThread::currentThread());
    }

    std::atomic<int> calls{0};
    std::atomic<QThread *> lastThread{nullptr};
    std::atomic<QObject *> lastSender{nullptr};
    std::atomic<QThread *> *deletedIn = nullptr;
    QMutex mutex;
    std::vector<int> values;
    QString text;

public slots:
    void onValue(int v) {
        lastThread = QThread::currentThread();
        lastSender = sender();
        {
            QMutexLocker lk(&mutex);
            values.push_back(v);
        }
        ++calls;
    }
    void onText(const QString &s) {
        {
            QMutexLocker lk(&mutex);
            text = s;
        }
        lastThread = QThread::currentThread();
        ++calls;
    }
    void onPing() {
        lastThread = QThread::currentThread();
        ++calls;
    }
};

class CountedEvent : public QEvent {
public:
    explicit CountedEvent(int n) : QEvent(Type(QEvent::User + 1)), number(n) { ++alive; }
    ~CountedEvent() override { --alive; }
    int number;
    static std::atomic<int> alive;
};
std::atomic<int> CountedEvent::alive{0};

class EventSink : public QObject {
    Q_OBJECT
public:
    std::atomic<int> count{0};
    std::atomic<int> threadChanges{0};
    std::atomic<QThread *> lastThread{nullptr};
    QMutex mutex;
    std::vector<int> order;

    bool event(QEvent *e) override {
        if (e->type() == QEvent::ThreadChange)
            ++threadChanges;
        return QObject::event(e);
    }

protected:
    void customEvent(QEvent *e) override {
        lastThread = QThread::currentThread();
        {
            QMutexLocker lk(&mutex);
            order.push_back(static_cast<CountedEvent *>(e)->number);
        }
        ++count;
    }
};

class TimerSink : public QObject {
    Q_OBJECT
public:
    std::atomic<int> ticks{0};
    std::atomic<QThread *> lastThread{nullptr};
    std::atomic<int> timerId{0};

protected:
    void timerEvent(QTimerEvent *e) override {
        if (e->timerId() != timerId)
            return;
        lastThread = QThread::currentThread();
        ++ticks;
    }
};

class ExecThread : public QThread {
    Q_OBJECT
public:
    std::atomic<int> code{-1};
    std::atomic<QThread *> seenCurrent{nullptr};
    std::atomic<int> levelInside{-1};

protected:
    void run() override {
        seenCurrent = QThread::currentThread();
        levelInside = loopLevel();
        code = exec();
    }
};

class GatedThread : public QThread {
    Q_OBJECT
public:
    Gate gate;
    std::atomic<bool> inside{false};

protected:
    void run() override {
        inside = true;
        gate.wait();
    }
};

class LoopThread : public QThread {
    Q_OBJECT
public:
    std::atomic<int> nestedLevel{-1};
    std::atomic<int> processed{0};
    std::atomic<bool> done{false};

protected:
    void run() override {
        // A nested loop inside the thread's own loop, and posted events handled by hand.
        EventSink events;
        QCoreApplication::postEvent(&events, new CountedEvent(1));
        QCoreApplication::postEvent(&events, new CountedEvent(2));
        QCoreApplication::processEvents();
        processed = events.count.load();

        QEventLoop outer;
        QTimer::singleShot(1, &outer, [this, &outer] {
            QEventLoop inner;
            QTimer::singleShot(1, &inner, [this, &inner] {
                nestedLevel = loopLevel();
                inner.quit();
            });
            inner.exec();
            outer.quit();
        });
        outer.exec();
        done = true;
    }
};

class TaskRunnable : public QRunnable {
public:
    explicit TaskRunnable(std::atomic<int> *destroyed = nullptr, std::atomic<int> *ran = nullptr)
        : m_destroyed(destroyed), m_ran(ran) {}
    ~TaskRunnable() override {
        if (m_destroyed)
            ++*m_destroyed;
    }
    void run() override {
        if (m_ran)
            ++*m_ran;
    }

private:
    std::atomic<int> *m_destroyed;
    std::atomic<int> *m_ran;
};

static void testThreadBasics() {
    QThread t;
    CHECK(!t.isRunning());
    CHECK(!t.isFinished());
    CHECK(t.wait());
    CHECK(t.priority() == QThread::InheritPriority);
    CHECK(t.loopLevel() == 0);

    std::atomic<int> started{0};
    std::atomic<int> finished{0};
    std::atomic<QThread *> startedIn{nullptr};
    QObject::connect(&t, &QThread::started, [&] {
        startedIn = QThread::currentThread();
        ++started;
    });
    QObject::connect(&t, &QThread::finished, [&] { ++finished; });

    t.start();
    CHECK(waitUntil([&] { return started == 1; }));
    CHECK(t.isRunning());
    CHECK(!t.isFinished());
    CHECK(startedIn == &t);
    CHECK(waitUntil([&] { return t.loopLevel() == 1; }));
    t.start(); // already running: no effect
    t.quit();
    CHECK(t.wait());
    CHECK(!t.isRunning());
    CHECK(t.isFinished());
    CHECK(finished == 1);
    CHECK(started == 1);
    CHECK(t.loopLevel() == 0);

    // A finished thread can start again.
    t.start();
    CHECK(waitUntil([&] { return started == 2; }));
    CHECK(!t.isFinished());
    t.quit();
    CHECK(t.wait(QDeadlineTimer(QDeadlineTimer::Forever)));
    CHECK(finished == 2);

    // wait() with a time limit, on a thread that is held up.
    GatedThread g;
    g.start();
    CHECK(waitUntil([&] { return g.inside.load(); }));
    CHECK(!g.wait(QDeadlineTimer(30)));
    CHECK(!g.wait(20UL));
    CHECK(g.isRunning());
    g.gate.open();
    CHECK(g.wait(60000UL));
    CHECK(g.isFinished());
}

static void testExecAndExit() {
    // exit() right after start() must not be lost, even if the thread is not in exec() yet.
    for (int i = 0; i < 20; ++i) {
        ExecThread t;
        t.start();
        t.exit(40 + i);
        CHECK(t.wait());
        CHECK(t.code == 40 + i);
        CHECK(t.seenCurrent == &t);
    }
    ExecThread t;
    t.start();
    CHECK(waitUntil([&] { return t.loopLevel() == 1; }));
    t.quit();
    CHECK(t.wait());
    CHECK(t.code == 0);

    // exit() on a thread that does not run is ignored and does not affect the next start.
    ExecThread u;
    u.exit(5);
    u.start();
    CHECK(waitUntil([&] { return u.loopLevel() == 1; }));
    u.quit();
    CHECK(u.wait());
    CHECK(u.code == 0);
}

static void testThreadStatics() {
    CHECK(QThread::idealThreadCount() >= 1);
    CHECK(QThread::currentThread() != nullptr);
    CHECK(QThread::currentThread() == QCoreApplication::instance()->thread());
    CHECK(QThread::isMainThread());
    CHECK(QThread::currentThreadId() != nullptr);

    const Qt::HANDLE mainId = QThread::currentThreadId();
    std::atomic<Qt::HANDLE> otherId{nullptr};
    std::atomic<QThread *> otherThread{nullptr};
    std::atomic<bool> otherIsMain{true};
    QThread *t = QThread::create([&] {
        otherId = QThread::currentThreadId();
        otherThread = QThread::currentThread();
        otherIsMain = QThread::isMainThread();
        QThread::yieldCurrentThread();
    });
    t->start();
    CHECK(t->wait());
    CHECK(otherId != mainId);
    CHECK(otherThread == t);
    CHECK(!otherIsMain);
    delete t;

    QElapsedTimer timer;
    timer.start();
    QThread::msleep(50);
    CHECK(timer.elapsed() >= 45);
    timer.restart();
    QThread::usleep(20000);
    CHECK(timer.elapsed() >= 18);
    timer.restart();
    QThread::sleep(std::chrono::milliseconds(30));
    CHECK(timer.elapsed() >= 28);
    QThread::sleep(0);
    QThread::msleep(0);
}

static void testThreadCreate() {
    std::atomic<int> sum{0};
    QThread *t = QThread::create([&sum](int a, int b) { sum = a + b; }, 3, 4);
    CHECK(!t->isRunning());
    t->start();
    CHECK(t->wait());
    CHECK(sum == 7);
    CHECK(t->isFinished());
    delete t;

    // The thread deletes itself through finished() -> deleteLater().
    QPointer<QThread> guard;
    std::atomic<bool> ran{false};
    QThread *u = QThread::create([&ran] { ran = true; });
    guard = u;
    QObject::connect(u, &QThread::finished, u, &QObject::deleteLater);
    u->start();
    CHECK(waitUntil([&] { return guard.isNull(); }));
    CHECK(ran);
}

static void testThreadParameters() {
    GatedThread t;
    t.setStackSize(2 * 1024 * 1024);
    CHECK(t.stackSize() == 2U * 1024 * 1024);
    t.start(QThread::LowPriority);
    CHECK(waitUntil([&] { return t.inside.load(); }));
    CHECK(t.priority() == QThread::LowPriority);
    t.setPriority(QThread::HighPriority);
    CHECK(t.priority() == QThread::HighPriority);
    t.gate.open();
    CHECK(t.wait());

    GatedThread idle;
    idle.start(QThread::IdlePriority);
    CHECK(waitUntil([&] { return idle.inside.load(); }));
    CHECK(idle.priority() == QThread::IdlePriority);
    idle.gate.open();
    CHECK(idle.wait());
}

static void testInterruption() {
    std::atomic<bool> inside{false};
    std::atomic<bool> sawIt{false};
    QThread *t = nullptr;
    std::atomic<int> runs{0};
    t = QThread::create([&] {
        if (runs++ > 0)
            return;
        inside = true;
        while (!QThread::currentThread()->isInterruptionRequested())
            QThread::msleep(1);
        sawIt = true;
    });
    CHECK(!t->isInterruptionRequested());
    t->start();
    CHECK(waitUntil([&] { return inside.load(); }));
    CHECK(!t->isInterruptionRequested());
    t->requestInterruption();
    CHECK(t->isInterruptionRequested());
    CHECK(t->wait());
    CHECK(sawIt);
    // The request is cleared by the next start.
    t->start();
    CHECK(t->wait());
    CHECK(!t->isInterruptionRequested());
    delete t;
}

static void testAffinity() {
    CHECK(QThread::currentThread() == QCoreApplication::instance()->thread());
    QObject plain;
    CHECK(plain.thread() == QThread::currentThread());

    QThread t;
    t.start();
    auto *parent = new EventSink;
    auto *child = new QObject(parent);
    CHECK(parent->thread() == QThread::currentThread());
    parent->moveToThread(&t);
    CHECK(parent->thread() == &t);
    CHECK(child->thread() == &t);
    CHECK(parent->threadChanges == 1);

    // A child cannot be moved on its own and a parent from another thread cannot be set.
    child->moveToThread(QThread::currentThread());
    CHECK(child->thread() == &t);
    QObject stranger;
    stranger.setParent(parent);
    CHECK(stranger.parent() == nullptr);
    auto *adopted = new QObject(parent);
    CHECK(adopted->parent() == nullptr);
    CHECK(adopted->thread() == QThread::currentThread());
    delete adopted;

    // The thread must be the object's own to move it.
    plain.moveToThread(&t);
    CHECK(plain.thread() == &t);
    plain.moveToThread(QThread::currentThread()); // not allowed from here: plain lives in t now
    CHECK(plain.thread() == &t);
    t.quit();
    CHECK(t.wait());
    delete parent;

    // An object can be moved to no thread and then pulled into one.
    QObject orphan;
    orphan.moveToThread(nullptr);
    CHECK(orphan.thread() == nullptr);
    orphan.moveToThread(QThread::currentThread());
    CHECK(orphan.thread() == QThread::currentThread());

    // An object whose thread object is gone lives in no thread.
    auto *gone = new QThread;
    auto *obj = new QObject;
    obj->moveToThread(gone);
    delete gone;
    CHECK(obj->thread() == nullptr);
    delete obj;

    // Objects created in a thread live there; a thread can push its objects to another one.
    std::atomic<QThread *> madeIn{nullptr};
    auto *made = new std::atomic<QObject *>{nullptr};
    QThread *maker = QThread::create([&] {
        auto *o = new QObject;
        madeIn = o->thread();
        o->moveToThread(QCoreApplication::instance()->thread());
        *made = o;
    });
    maker->start();
    CHECK(maker->wait());
    CHECK(madeIn == maker);
    CHECK(made->load() != nullptr);
    CHECK(made->load()->thread() == QThread::currentThread());
    delete made->load();
    delete made;
    delete maker;
}

static void testQueuedToThread() {
    QThread t;
    t.start();
    auto *r = new Recorder;
    r->moveToThread(&t);
    Emitter e;
    QObject::connect(&e, &Emitter::valueChanged, r, &Recorder::onValue);
    QObject::connect(&e, &Emitter::textChanged, r, &Recorder::onText);

    for (int i = 0; i < 100; ++i)
        e.fire(i);
    CHECK(waitUntil([&] { return r->calls == 100; }));
    CHECK(r->lastThread == &t);
    CHECK(r->lastSender == &e);
    {
        QMutexLocker lk(&r->mutex);
        bool ordered = r->values.size() == 100;
        for (int i = 0; ordered && i < 100; ++i)
            ordered = r->values[static_cast<std::size_t>(i)] == i;
        CHECK(ordered);
    }
    e.fireText(QString("hello across threads"));
    CHECK(waitUntil([&] { return r->calls == 101; }));
    {
        QMutexLocker lk(&r->mutex);
        CHECK(r->text == QString("hello across threads"));
    }

    // An explicit direct connection runs in the emitting thread.
    Recorder direct;
    direct.moveToThread(&t);
    QObject::connect(&e, &Emitter::ping, &direct, &Recorder::onPing, Qt::DirectConnection);
    e.firePing();
    CHECK(direct.calls == 1);
    CHECK(direct.lastThread == QThread::currentThread());
    QThread::msleep(1);
    direct.moveToThread(QThread::currentThread()); // not possible: lives in t
    CHECK(direct.thread() == &t);

    // Blocking queued: the call has finished when the emit returns.
    Emitter blocker;
    QObject::connect(&blocker, &Emitter::valueChanged, r, &Recorder::onValue, Qt::BlockingQueuedConnection);
    const int before = r->calls;
    blocker.fire(7);
    CHECK(r->calls == before + 1);
    CHECK(r->lastThread == &t);

    // ... and it does not deadlock when the receiver lives in the emitting thread.
    Recorder here;
    QObject::connect(&blocker, &Emitter::valueChanged, &here, &Recorder::onValue, Qt::BlockingQueuedConnection);
    blocker.fire(8);
    CHECK(here.calls == 1);

    std::atomic<QThread *> deletedIn{nullptr};
    r->deletedIn = &deletedIn;
    r->deleteLater();
    CHECK(waitUntil([&] { return deletedIn.load() != nullptr; }));
    CHECK(deletedIn == &t);
    t.quit();
    CHECK(t.wait());
}

static void testQueuedToMain() {
    Recorder r;
    Emitter e;
    QObject::connect(&e, &Emitter::valueChanged, &r, &Recorder::onValue);

    QThread *t = QThread::create([&e] {
        for (int i = 0; i < 100; ++i)
            e.fire(i);
    });
    t->start();
    CHECK(t->wait());
    delete t;
    CHECK(r.calls == 0); // queued: nothing runs until the main thread processes events
    CHECK(waitUntil([&] { return r.calls == 100; }));
    CHECK(r.lastThread == QThread::currentThread());
    CHECK(r.lastSender == &e);
    {
        QMutexLocker lk(&r.mutex);
        bool ordered = true;
        for (int i = 0; i < 100; ++i)
            ordered = ordered && r.values[static_cast<std::size_t>(i)] == i;
        CHECK(ordered);
    }

    // In one thread a queued connection still waits for the event loop.
    Emitter e2;
    Recorder r2;
    QObject::connect(&e2, &Emitter::ping, &r2, &Recorder::onPing, Qt::QueuedConnection);
    e2.firePing();
    CHECK(r2.calls == 0);
    QCoreApplication::processEvents();
    CHECK(r2.calls == 1);
}

static void testFunctorConnections() {
    QThread t;
    t.start();
    auto *ctx = new Recorder;
    ctx->moveToThread(&t);
    Emitter e;
    std::atomic<QThread *> inContext{nullptr};
    std::atomic<QThread *> noContext{nullptr};
    QObject::connect(&e, &Emitter::ping, ctx, [&] { inContext = QThread::currentThread(); });
    QObject::connect(&e, &Emitter::ping, [&] { noContext = QThread::currentThread(); });
    e.firePing();
    CHECK(noContext == QThread::currentThread()); // no context object: direct
    CHECK(waitUntil([&] { return inContext.load() != nullptr; }));
    CHECK(inContext == &t);

    // The connection goes away with the context object.
    t.quit();
    CHECK(t.wait());
    delete ctx;
    inContext = nullptr;
    e.firePing();
    QCoreApplication::processEvents();
    CHECK(inContext == nullptr);
}

static void testTimersInThreads() {
    QThread t;
    t.start();

    // A timer started in the object's own thread.
    auto *a = new TimerSink;
    a->moveToThread(&t);
    QTimer::singleShot(0, a, [a] { a->timerId = a->startTimer(2); });
    CHECK(waitUntil([&] { return a->ticks >= 3; }));
    CHECK(a->lastThread == &t);

    // Timers cannot be started or stopped from another thread.
    CHECK(a->startTimer(5) == 0);
    a->killTimer(a->timerId); // ignored
    const int seen = a->ticks;
    CHECK(waitUntil([&] { return a->ticks > seen; }));

    // A timer that runs when the object moves follows it.
    auto *b = new TimerSink;
    b->timerId = b->startTimer(2);
    CHECK(b->timerId != 0);
    b->moveToThread(&t);
    CHECK(waitUntil([&] { return b->ticks >= 3; }));
    CHECK(b->lastThread == &t);

    // QTimer on a moved object.
    auto *timer = new QTimer;
    std::atomic<int> hits{0};
    std::atomic<QThread *> hitThread{nullptr};
    timer->setInterval(2);
    QObject::connect(timer, &QTimer::timeout, timer, [&] {
        hitThread = QThread::currentThread();
        ++hits;
    });
    timer->moveToThread(&t);
    QTimer::singleShot(0, timer, [timer] { timer->start(); });
    CHECK(waitUntil([&] { return hits >= 3; }));
    CHECK(hitThread == &t);
    std::atomic<bool> stopped{false};
    QTimer::singleShot(0, timer, [timer, &stopped] {
        timer->stop();
        stopped = true;
    });
    CHECK(waitUntil([&] { return stopped.load(); }));

    // A single shot with a context in another thread runs there.
    std::atomic<QThread *> shotThread{nullptr};
    QTimer::singleShot(1, a, [&] { shotThread = QThread::currentThread(); });
    CHECK(waitUntil([&] { return shotThread.load() != nullptr; }));
    CHECK(shotThread == &t);

    t.quit();
    CHECK(t.wait());
    delete a;
    delete b;
    delete timer;
}

static void testSocketNotifierInThread() {
    int fds[2];
    CHECK(::pipe(fds) == 0);
    QThread t;
    t.start();

    auto *notifier = new QSocketNotifier(fds[0], QSocketNotifier::Read);
    std::atomic<int> hits{0};
    std::atomic<QThread *> hitThread{nullptr};
    QObject::connect(notifier, &QSocketNotifier::activated, [&] {
        char c;
        if (::read(fds[0], &c, 1) == 1) {
            hitThread = QThread::currentThread();
            ++hits;
        }
    });
    notifier->moveToThread(&t);
    for (int i = 0; i < 3; ++i) {
        CHECK(::write(fds[1], "x", 1) == 1);
        CHECK(waitUntil([&] { return hits == i + 1; }));
    }
    CHECK(hitThread == &t);

    // Disabling from another thread is refused; the notifier keeps working.
    notifier->setEnabled(false);
    CHECK(notifier->isEnabled());
    CHECK(::write(fds[1], "y", 1) == 1);
    CHECK(waitUntil([&] { return hits == 4; }));

    t.quit();
    CHECK(t.wait());
    delete notifier;
    ::close(fds[0]);
    ::close(fds[1]);
}

static void testDeleteLater() {
    // In a running thread.
    {
        QThread t;
        t.start();
        auto *r = new Recorder;
        std::atomic<QThread *> deletedIn{nullptr};
        r->deletedIn = &deletedIn;
        r->moveToThread(&t);
        r->deleteLater();
        CHECK(waitUntil([&] { return deletedIn.load() != nullptr; }));
        CHECK(deletedIn == &t);
        t.quit();
        CHECK(t.wait());
    }
    // Posted before the thread starts: deleted once its loop runs.
    {
        QThread t;
        auto *r = new Recorder;
        QPointer<Recorder> guard(r);
        r->moveToThread(&t);
        r->deleteLater();
        CHECK(!guard.isNull());
        t.start();
        CHECK(waitUntil([&] { return guard.isNull(); }));
        t.quit();
        CHECK(t.wait());
    }
    // A thread without an event loop deletes such objects when it finishes.
    {
        GatedThread t;
        auto *r = new Recorder;
        std::atomic<QThread *> deletedIn{nullptr};
        r->deletedIn = &deletedIn;
        QPointer<Recorder> guard(r);
        r->moveToThread(&t);
        t.start();
        CHECK(waitUntil([&] { return t.inside.load(); }));
        r->deleteLater();
        QCoreApplication::processEvents();
        CHECK(!guard.isNull());
        t.gate.open();
        CHECK(t.wait());
        CHECK(guard.isNull());
        CHECK(deletedIn == &t);
    }
    // From another thread, for an object in the main thread.
    {
        auto *r = new Recorder;
        QPointer<Recorder> guard(r);
        QThread *t = QThread::create([r] { r->deleteLater(); });
        t->start();
        CHECK(t->wait());
        delete t;
        CHECK(!guard.isNull());
        CHECK(waitUntil([&] { return guard.isNull(); }));
    }
}

static void testPostedEvents() {
    QThread t;
    t.start();
    auto *sink = new EventSink;
    sink->moveToThread(&t);
    for (int i = 0; i < 50; ++i)
        QCoreApplication::postEvent(sink, new CountedEvent(i));
    CHECK(waitUntil([&] { return sink->count == 50; }));
    CHECK(sink->lastThread == &t);
    {
        QMutexLocker lk(&sink->mutex);
        bool ordered = true;
        for (int i = 0; i < 50; ++i)
            ordered = ordered && sink->order[static_cast<std::size_t>(i)] == i;
        CHECK(ordered);
    }

    // sendEvent() calls the receiver in the calling thread.
    {
        CountedEvent sent(99);
        QCoreApplication::sendEvent(sink, &sent);
        CHECK(sink->lastThread == QThread::currentThread());
    }

    // Events follow an object that moves.
    auto *mover = new EventSink;
    for (int i = 0; i < 5; ++i)
        QCoreApplication::postEvent(mover, new CountedEvent(i));
    mover->moveToThread(&t);
    CHECK(waitUntil([&] { return mover->count == 5; }));
    CHECK(mover->lastThread == &t);

    // Events posted from many threads to an object in the main thread.
    EventSink fromThreads;
    std::vector<std::thread> posters;
    for (int p = 0; p < 4; ++p) {
        posters.emplace_back([&fromThreads] {
            for (int i = 0; i < 100; ++i)
                QCoreApplication::postEvent(&fromThreads, new CountedEvent(i));
        });
    }
    for (auto &th : posters)
        th.join();
    CHECK(waitUntil([&] { return fromThreads.count == 400; }));
    CHECK(fromThreads.lastThread == QThread::currentThread());
    CHECK(waitUntil([] { return CountedEvent::alive == 0; }));

    // removePostedEvents() and destruction discard events, which are freed.
    GatedThread idle;
    auto *parked = new EventSink;
    parked->moveToThread(&idle);
    for (int i = 0; i < 5; ++i)
        QCoreApplication::postEvent(parked, new CountedEvent(i));
    CHECK(CountedEvent::alive == 5);
    QCoreApplication::removePostedEvents(parked);
    CHECK(CountedEvent::alive == 0);
    for (int i = 0; i < 5; ++i)
        QCoreApplication::postEvent(parked, new CountedEvent(i));
    delete parked;
    CHECK(CountedEvent::alive == 0);

    t.quit();
    CHECK(t.wait());
    delete sink;
    delete mover;
    CHECK(CountedEvent::alive == 0);

    // sendPostedEvents() delivers the calling thread's events at once.
    EventSink local;
    QCoreApplication::postEvent(&local, new CountedEvent(1));
    CHECK(local.count == 0);
    QCoreApplication::sendPostedEvents(&local);
    CHECK(local.count == 1);
    QCoreApplication::postEvent(&local, new CountedEvent(2));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::User + 1);
    CHECK(local.count == 2);
    CHECK(waitUntil([] { return CountedEvent::alive == 0; }));
}

static void testEventLoopsInThreads() {
    LoopThread t;
    t.start();
    CHECK(t.wait());
    CHECK(t.processed == 2);
    CHECK(t.nestedLevel == 2);
    CHECK(t.done);

    // quit() on the application stops the loops of its own thread only.
    ExecThread worker;
    worker.start();
    CHECK(waitUntil([&] { return worker.loopLevel() == 1; }));
    QCoreApplication::exit(3);
    QThread::msleep(20);
    CHECK(worker.loopLevel() == 1);
    CHECK(worker.isRunning());

    // quit() from another thread stops the main loop.
    QThread *quitter = QThread::create([] { QCoreApplication::quit(); });
    QTimer::singleShot(0, [quitter] { quitter->start(); });
    CHECK(QCoreApplication::exec() == 0);
    CHECK(quitter->wait());
    delete quitter;

    worker.quit();
    CHECK(worker.wait());
}

static void testThreadStorage() {
    static std::atomic<int> destroyed{0};
    struct Tracked {
        explicit Tracked(int v = 0) : value(v) {}
        ~Tracked() { ++destroyed; }
        int value;
    };

    QThreadStorage<Tracked *> pointers;
    QThreadStorage<int> ints;
    CHECK(!pointers.hasLocalData());
    pointers.setLocalData(new Tracked(1));
    CHECK(pointers.hasLocalData());
    CHECK(pointers.localData()->value == 1);
    pointers.setLocalData(new Tracked(2)); // replaces and deletes the first
    CHECK(destroyed == 1);
    CHECK(pointers.localData()->value == 2);

    CHECK(ints.localData() == 0);
    ints.localData() = 5;
    CHECK(ints.localData() == 5);
    const QThreadStorage<int> &constInts = ints;
    CHECK(constInts.localData() == 5);

    std::atomic<int> otherSaw{-1};
    std::atomic<bool> otherHad{true};
    QThread *t = QThread::create([&] {
        otherHad = pointers.hasLocalData() || ints.hasLocalData();
        pointers.setLocalData(new Tracked(10));
        ints.setLocalData(77);
        otherSaw = pointers.localData()->value + ints.localData();
    });
    t->start();
    CHECK(t->wait());
    delete t;
    CHECK(!otherHad);
    CHECK(otherSaw == 87);
    CHECK(destroyed == 2); // the thread's value dies with the thread
    CHECK(pointers.localData()->value == 2);
    CHECK(ints.localData() == 5);

    pointers.setLocalData(nullptr);
    CHECK(!pointers.hasLocalData());
    CHECK(destroyed == 3);

    {
        QThreadStorage<Tracked *> scoped;
        scoped.setLocalData(new Tracked(3));
    }
    CHECK(destroyed == 4); // destroying the storage drops the calling thread's value
}

static void testRunnable() {
    std::atomic<int> destroyed{0};
    std::atomic<int> ran{0};
    QThreadPool pool;
    pool.start(new TaskRunnable(&destroyed, &ran));
    CHECK(pool.waitForDone());
    CHECK(ran == 1);
    CHECK(destroyed == 1);

    TaskRunnable keep(&destroyed, &ran);
    CHECK(keep.autoDelete());
    keep.setAutoDelete(false);
    CHECK(!keep.autoDelete());
    pool.start(&keep);
    CHECK(pool.waitForDone());
    CHECK(ran == 2);
    CHECK(destroyed == 1);

    QRunnable *r = QRunnable::create([&ran] { ++ran; });
    CHECK(r->autoDelete());
    pool.start(r);
    CHECK(pool.waitForDone());
    CHECK(ran == 3);
}

static void testPoolMoveOnly() {
    QThreadPool pool;
    std::atomic<int> sum{0};
    auto owned = std::make_unique<int>(5);
    pool.start([&sum, p = std::move(owned)] { sum += *p; });
    auto second = std::make_unique<int>(7);
    while (!pool.tryStart([&sum, p = std::move(second)] { sum += *p; })) {
        // a moved-from callable is not retried: tryStart() destroyed it
        second = std::make_unique<int>(7);
    }
    CHECK(pool.waitForDone());
    CHECK(sum == 12);
    std::unique_ptr<QRunnable> r(QRunnable::create([p = std::make_unique<int>(1), &sum] { sum += *p; }));
    r->run();
    CHECK(sum == 13 && r->autoDelete());
    pool.reserveThread();
    pool.startOnReservedThread([p = std::make_unique<int>(2), &sum] { sum += *p; });
    CHECK(pool.waitForDone());
    CHECK(sum == 15);
    std::function<void()> copyable = [&sum] { ++sum; };
    pool.start(copyable); // still the std::function overload
    CHECK(pool.waitForDone());
    CHECK(sum == 16);
}

static void testPoolBasics() {
    QThreadPool pool;
    CHECK(pool.maxThreadCount() == QThread::idealThreadCount());
    CHECK(pool.expiryTimeout() == 30000);
    CHECK(pool.activeThreadCount() == 0);
    pool.setMaxThreadCount(4);
    CHECK(pool.maxThreadCount() == 4);

    std::atomic<int> done{0};
    std::atomic<int> running{0};
    std::atomic<int> peak{0};
    for (int i = 0; i < 100; ++i) {
        pool.start([&] {
            const int now = ++running;
            int p = peak.load();
            while (now > p && !peak.compare_exchange_weak(p, now)) {
            }
            QThread::usleep(200);
            --running;
            ++done;
        });
    }
    CHECK(pool.waitForDone());
    CHECK(done == 100);
    CHECK(peak <= 4);
    CHECK(peak >= 1);
    CHECK(pool.activeThreadCount() == 0);

    // The pool can be reused after waitForDone() ended its threads.
    pool.start([&] { ++done; });
    CHECK(pool.waitForDone());
    CHECK(done == 101);

    pool.setExpiryTimeout(-1);
    CHECK(pool.expiryTimeout() == -1);
    pool.setExpiryTimeout(10);
    CHECK(pool.expiryTimeout() == 10);
    pool.start([&] { ++done; });
    QThread::msleep(100); // the idle thread expires; the next task gets a new one
    pool.start([&] { ++done; });
    CHECK(pool.waitForDone());
    CHECK(done == 103);

    pool.setStackSize(1024 * 1024);
    CHECK(pool.stackSize() == 1024U * 1024);
    pool.setThreadPriority(QThread::LowPriority);
    CHECK(pool.threadPriority() == QThread::LowPriority);
    std::atomic<QThread::Priority> seen{QThread::NormalPriority};
    pool.start([&] { seen = QThread::currentThread()->priority(); });
    CHECK(pool.waitForDone());
    CHECK(seen == QThread::LowPriority);

    // Tasks run in pool threads, which the pool recognises.
    std::atomic<bool> inPool{false};
    std::atomic<bool> inOther{true};
    QThreadPool other;
    pool.start([&] {
        inPool = pool.contains(QThread::currentThread());
        inOther = other.contains(QThread::currentThread());
    });
    CHECK(pool.waitForDone());
    CHECK(inPool);
    CHECK(!inOther);
    CHECK(!pool.contains(QThread::currentThread()));
}

static void testPoolLimits() {
    QThreadPool pool;
    pool.setMaxThreadCount(1);
    Gate gate;
    std::atomic<bool> started{false};
    pool.start([&] {
        started = true;
        gate.wait();
    });
    CHECK(waitUntil([&] { return started.load(); }));
    CHECK(pool.activeThreadCount() == 1);

    // Nothing is free: tryStart() refuses and does not queue.
    std::atomic<int> ran{0};
    CHECK(!pool.tryStart([&] { ++ran; }));
    auto *refused = new TaskRunnable(nullptr, &ran);
    CHECK(!pool.tryStart(refused));
    delete refused;

    // tryTake() and clear() remove queued runnables only.
    std::atomic<int> destroyed{0};
    auto *taken = new TaskRunnable(&destroyed, &ran);
    pool.start(taken, 0);
    CHECK(pool.tryTake(taken));
    CHECK(!pool.tryTake(taken));
    CHECK(destroyed == 0);
    delete taken;
    CHECK(destroyed == 1);
    auto *c1 = new TaskRunnable(&destroyed, &ran);
    auto *c2 = new TaskRunnable(&destroyed, &ran);
    pool.start(c1);
    pool.start(c2);
    pool.clear();
    CHECK(destroyed == 3);

    // Queued tasks run by priority, then by arrival.
    std::vector<int> order;
    std::mutex orderMutex;
    auto record = [&](int id) {
        return [&, id] {
            std::lock_guard<std::mutex> lk(orderMutex);
            order.push_back(id);
        };
    };
    pool.start(record(1), 1);
    pool.start(record(2), 5);
    pool.start(record(3), 3);
    pool.start(record(4), 5);
    pool.start(record(5), -2);

    // waitForDone() with a limit gives up while the gate is closed.
    CHECK(!pool.waitForDone(30));
    CHECK(!pool.waitForDone(QDeadlineTimer(20)));
    gate.open();
    CHECK(pool.waitForDone());
    CHECK(ran == 0);
    CHECK((order == std::vector<int>{2, 4, 3, 1, 5}));
    CHECK(pool.tryStart([&] { ++ran; }));
    CHECK(pool.waitForDone());
    CHECK(ran == 1);
}

static void testPoolReservation() {
    QThreadPool pool;
    pool.setMaxThreadCount(1);
    CHECK(pool.activeThreadCount() == 0);
    pool.reserveThread();
    CHECK(pool.activeThreadCount() == 1);
    pool.reserveThread();
    CHECK(pool.activeThreadCount() == 2); // may exceed maxThreadCount()
    pool.releaseThread();
    pool.releaseThread();
    CHECK(pool.activeThreadCount() == 0);

    // A reserved thread keeps the pool's capacity, until it is used or released.
    pool.reserveThread();
    Gate gate;
    std::atomic<bool> started{false};
    pool.startOnReservedThread([&] {
        started = true;
        gate.wait();
    });
    CHECK(waitUntil([&] { return started.load(); }));
    CHECK(pool.activeThreadCount() == 1);
    std::atomic<int> ran{0};
    pool.start([&] { ++ran; }); // queued behind the running task
    QThread::msleep(20);
    CHECK(ran == 0);
    gate.open();
    CHECK(pool.waitForDone());
    CHECK(ran == 1);
    CHECK(pool.activeThreadCount() == 0);

    // Raising the limit starts queued work.
    QThreadPool second;
    second.setMaxThreadCount(1);
    Gate gate2;
    std::atomic<bool> started2{false};
    second.start([&] {
        started2 = true;
        gate2.wait();
    });
    CHECK(waitUntil([&] { return started2.load(); }));
    second.setMaxThreadCount(2);
    std::atomic<int> ran2{0};
    second.start([&] { ++ran2; });
    CHECK(waitUntil([&] { return ran2 == 1; })); // raised limit started a second thread
    gate2.open();
    CHECK(second.waitForDone());
}

static void testPoolShutdown() {
    std::atomic<int> ran{0};
    {
        QThreadPool pool;
        pool.setMaxThreadCount(2);
        for (int i = 0; i < 20; ++i)
            pool.start([&] {
                QThread::usleep(100);
                ++ran;
            });
    } // the destructor waits for queued tasks
    CHECK(ran == 20);

    QThreadPool *global = QThreadPool::globalInstance();
    CHECK(global == QThreadPool::globalInstance());
    CHECK(global->maxThreadCount() == QThread::idealThreadCount());
    std::atomic<int> g{0};
    for (int i = 0; i < 10; ++i)
        global->start([&] { ++g; });
    CHECK(global->waitForDone());
    CHECK(g == 10);

    // Tasks can start tasks, and many threads can start tasks at once.
    QThreadPool pool;
    std::atomic<int> nested{0};
    pool.start([&] {
        for (int i = 0; i < 5; ++i)
            pool.start([&] { ++nested; });
    });
    std::vector<std::thread> starters;
    for (int s = 0; s < 4; ++s)
        starters.emplace_back([&] {
            for (int i = 0; i < 50; ++i)
                pool.start([&] { ++nested; });
        });
    for (auto &th : starters)
        th.join();
    CHECK(pool.waitForDone());
    CHECK(nested == 205);
}

static void testConcurrentSignals() {
    // Threads emit, connect and disconnect on the same sender at the same time.
    Emitter e;
    std::atomic<int> direct{0};
    QObject::connect(&e, &Emitter::valueChanged, [&](int) { ++direct; });
    std::atomic<bool> stop{false};

    std::vector<std::thread> threads;
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&] {
            for (int n = 0; n < 2000; ++n)
                e.fire(n);
        });
    }
    // The counter outlives the loop: an emission that began before disconnect() may still call the slot afterwards.
    std::atomic<int> extra{0};
    threads.emplace_back([&] {
        while (!stop) {
            Recorder r;
            auto c = QObject::connect(&e, &Emitter::valueChanged, [&extra](int) { ++extra; });
            QObject::disconnect(c);
            QObject::connect(&e, &Emitter::valueChanged, &r, &Recorder::onValue, Qt::QueuedConnection);
            // r is destroyed here with its connection in place and events possibly queued
        }
    });
    for (std::size_t i = 0; i < 3; ++i)
        threads[i].join();
    stop = true;
    threads.back().join();
    CHECK(direct == 6000);
}

static void testDeleteReceiverWhileEmitting() {
    // The receiver lives in the main thread and dies here while another thread emits to it.
    Emitter e;
    std::atomic<bool> stop{false};
    std::atomic<int> emitted{0};
    QThread *t = QThread::create([&] {
        while (!stop) {
            e.fire(1);
            ++emitted;
        }
    });
    t->start();
    for (int round = 0; round < 50; ++round) {
        auto *r = new Recorder;
        QObject::connect(&e, &Emitter::valueChanged, r, &Recorder::onValue);
        const int base = emitted;
        CHECK(waitUntil([&] { return emitted > base + 5; }));
        QCoreApplication::processEvents();
        delete r;
    }
    stop = true;
    CHECK(t->wait());
    delete t;
    QCoreApplication::processEvents();
}

static void testManyThreads() {
    // Create, use and destroy many threads, each with objects, timers and queued signals.
    Recorder sink;
    std::atomic<int> total{0};
    for (int round = 0; round < 20; ++round) {
        QThread t;
        auto *worker = new Emitter;
        auto *rec = new Recorder;
        worker->moveToThread(&t);
        rec->moveToThread(&t);
        QObject::connect(worker, &Emitter::valueChanged, rec, &Recorder::onValue);
        QObject::connect(worker, &Emitter::valueChanged, &sink, [&total](int) { ++total; });
        QObject::connect(&t, &QThread::finished, worker, &QObject::deleteLater);
        QObject::connect(&t, &QThread::finished, rec, &QObject::deleteLater);
        QObject::connect(&t, &QThread::started, worker, [worker] {
            for (int i = 0; i < 10; ++i)
                worker->fire(i);
        });
        t.start();
        CHECK(waitUntil([&] { return total == (round + 1) * 10; }));
        t.quit();
        CHECK(t.wait());
    }
    CHECK(total == 200);
}

static void testSelfWait() {
    std::atomic<bool> result{true};
    QThread *t = nullptr;
    t = QThread::create([&] { result = t->wait(); });
    t->start();
    CHECK(t->wait());
    CHECK(!result); // a thread cannot wait for itself
    delete t;
}

// Set NT_TEST_VERBOSE=1 to see which test is running, for example when one hangs.
#define RUN(fn)                                                                                                        \
    do {                                                                                                               \
        if (std::getenv("NT_TEST_VERBOSE"))                                                                            \
            std::fprintf(stderr, "%s\n", #fn);                                                                         \
        fn();                                                                                                          \
    } while (0)

// Deadline overloads of the locking classes, and waiting on a QReadWriteLock.
static void testDeadlineLocking() {
    {
        QMutex m;
        CHECK(m.tryLock(QDeadlineTimer(QDeadlineTimer::Forever)));
        std::atomic<int> result{-1};
        std::thread t([&] {
            QElapsedTimer clock;
            clock.start();
            const bool got = m.tryLock(QDeadlineTimer(20));
            result = got ? 1 : (clock.elapsed() >= 15 ? 0 : 2);
        });
        t.join();
        CHECK(result == 0);
        std::thread waiter([&] {
            result = m.tryLock(-1) ? 3 : 4;
            m.unlock();
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        m.unlock();
        waiter.join();
        CHECK(result == 3);
        std::unique_lock<QMutex> lk(m, std::chrono::milliseconds(5));
        CHECK(lk.owns_lock());
        lk.unlock();
        CHECK(m.try_lock_until(std::chrono::steady_clock::now()));
        m.unlock();
    }
    {
        QRecursiveMutex m;
        CHECK(m.try_lock() && m.tryLock(QDeadlineTimer(0)) && m.try_lock_for(std::chrono::milliseconds(1)));
        m.unlock();
        m.unlock();
        m.unlock();
        CHECK(m.tryLock(-1));
        m.unlock();
    }
    {
        QMutex a, b;
        QMutexLocker la(&a);
        QMutexLocker lb(&b);
        lb = std::move(la);
        CHECK(lb.mutex() == &a && lb.isLocked() && !la.mutex());
        CHECK(b.tryLock()); // the old mutex of lb was unlocked
        b.unlock();
        QMutexLocker<QMutex> lc(nullptr);
        lc.swap(lb);
        CHECK(lc.mutex() == &a && !lb.mutex());
    }
    {
        QReadWriteLock rw;
        rw.lockForWrite();
        std::atomic<int> result{-1};
        std::thread t([&] {
            result = (rw.tryLockForRead(QDeadlineTimer(10)) ? 1 : 0) + (rw.tryLockForWrite(QDeadlineTimer(0)) ? 2 : 0);
        });
        t.join();
        CHECK(result == 0);
        std::thread waiter([&] {
            result = rw.tryLockForRead(-1) ? 5 : 6;
            rw.unlock();
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        rw.unlock();
        waiter.join();
        CHECK(result == 5);
        CHECK(rw.tryLockForWrite(QDeadlineTimer(QDeadlineTimer::Forever)));
        rw.unlock();
    }
    {
        // A recursive lock is locked several times by the thread that holds it.
        QReadWriteLock rw(QReadWriteLock::Recursive);
        // Whether another thread can lock rw for reading right now.
        const auto readable = [&rw] {
            bool ok = false;
            std::thread t([&] {
                ok = rw.tryLockForRead();
                if (ok)
                    rw.unlock();
            });
            t.join();
            return ok;
        };
        rw.lockForWrite();
        CHECK(rw.tryLockForWrite());
        rw.lockForRead(); // the writer may read as well
        CHECK(!readable());
        rw.unlock();
        CHECK(!readable());
        rw.unlock();
        CHECK(!readable());
        rw.unlock();
        CHECK(readable());

        // A reader gets the lock again although a writer waits, and the writer runs after the
        // last unlock().
        rw.lockForRead();
        std::atomic<bool> wrote{false};
        std::thread writer([&] {
            rw.lockForWrite();
            wrote = true;
            rw.unlock();
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        CHECK(rw.tryLockForRead());
        CHECK(!rw.tryLockForWrite(QDeadlineTimer(5))); // a read lock is not upgraded
        rw.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        CHECK(!wrote);
        rw.unlock();
        writer.join();
        CHECK(wrote && readable());

        // Two threads read at the same time, each with its own count.
        rw.lockForRead();
        rw.lockForRead();
        CHECK(readable());
        rw.unlock();
        rw.unlock();
        CHECK(rw.tryLockForWrite());
        rw.unlock();

        // A wait condition releases a lock that is held once, and refuses one held twice.
        QWaitCondition cond;
        rw.lockForWrite();
        CHECK(!cond.wait(&rw, 5)); // timed out, and locked for writing again
        CHECK(!readable());
        rw.lockForWrite();
        CHECK(!cond.wait(&rw, 60000)); // returns at once
        rw.unlock();
        rw.unlock();
        CHECK(readable());

        // A non-recursive lock is unchanged.
        QReadWriteLock plain(QReadWriteLock::NonRecursive);
        plain.lockForWrite();
        CHECK(!plain.tryLockForWrite() && !plain.tryLockForRead());
        plain.unlock();
        CHECK(plain.tryLockForRead() && plain.tryLockForRead());
        plain.unlock();
        plain.unlock();
        CHECK(plain.tryLockForWrite());
        plain.unlock();
    }
    {
        QSemaphore sem;
        CHECK(!sem.tryAcquire() && !sem.try_acquire() && !sem.tryAcquire(1, QDeadlineTimer(5)));
        CHECK(!sem.try_acquire_for(std::chrono::milliseconds(1)));
        CHECK(!sem.try_acquire_until(std::chrono::steady_clock::now() - std::chrono::seconds(1)));
        std::thread t([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            sem.release(3);
        });
        CHECK(sem.tryAcquire(2, -1));
        t.join();
        CHECK(sem.tryAcquire(1, QDeadlineTimer(QDeadlineTimer::Forever)) && sem.available() == 0);
        sem.release(2);
        CHECK(sem.try_acquire() && sem.try_acquire_until(std::chrono::system_clock::now() + std::chrono::seconds(1)));
    }
    {
        QSemaphore sem;
        {
            QSemaphoreReleaser r1(&sem, 2);
            QSemaphoreReleaser r2(std::move(r1));
            CHECK(r2.semaphore() == &sem && r1.semaphore() == nullptr);
            QSemaphoreReleaser r3;
            r3 = std::move(r2);
            QSemaphoreReleaser r4(static_cast<QSemaphore *>(nullptr));
            r4.swap(r3);
            CHECK(r4.semaphore() == &sem && !r3.semaphore());
        }
        CHECK(sem.available() == 2);
    }
    {
        // wait() on a QReadWriteLock relocks it in the mode it was held.
        QReadWriteLock rw;
        QWaitCondition cond;
        bool ready = false;
        std::atomic<int> modes{0};
        std::thread writer([&] {
            rw.lockForWrite();
            while (!ready)
                cond.wait(&rw);
            modes += rw.tryLockForRead(0) ? 100 : 1; // still held for writing
            rw.unlock();
        });
        std::thread reader([&] {
            rw.lockForRead();
            while (!ready)
                cond.wait(&rw, QDeadlineTimer(QDeadlineTimer::Forever));
            modes += rw.tryLockForWrite(0) ? 100 : 10; // still held for reading
            rw.unlock();
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        rw.lockForWrite();
        ready = true;
        rw.unlock();
        cond.wakeAll();
        writer.join();
        reader.join();
        CHECK(modes == 11);
        rw.lockForRead();
        CHECK(!cond.wait(&rw, 5) && !cond.wait(&rw, QDeadlineTimer(5)));
        CHECK(!rw.tryLockForWrite(0));
        rw.unlock();
    }
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    RUN(testThreadBasics);
    RUN(testPoolMoveOnly);
    RUN(testExecAndExit);
    RUN(testThreadStatics);
    RUN(testThreadCreate);
    RUN(testThreadParameters);
    RUN(testInterruption);
    RUN(testAffinity);
    RUN(testQueuedToThread);
    RUN(testQueuedToMain);
    RUN(testFunctorConnections);
    RUN(testTimersInThreads);
    RUN(testSocketNotifierInThread);
    RUN(testDeleteLater);
    RUN(testPostedEvents);
    RUN(testEventLoopsInThreads);
    RUN(testThreadStorage);
    RUN(testRunnable);
    RUN(testPoolBasics);
    RUN(testPoolLimits);
    RUN(testPoolReservation);
    RUN(testPoolShutdown);
    RUN(testConcurrentSignals);
    RUN(testDeleteReceiverWhileEmitting);
    RUN(testManyThreads);
    RUN(testSelfWait);
    RUN(testDeadlineLocking);

    if (g_failures == 0)
        std::puts("all thread tests passed");
    return g_failures == 0 ? 0 : 1;
}
