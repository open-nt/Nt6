#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>
#include <QTimer>

#include <cstdio>
#include <string>
#include <vector>

static int g_failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

class Source : public QObject {
    Q_OBJECT
public:
    explicit Source(QObject *parent = nullptr) : QObject(parent) {}
    void fire(int v) { emit valueChanged(v); }
    void firePing() { emit ping(); }
    void fireText(const std::string &s) { emit textChanged(s); }

signals:
    void valueChanged(int value) { emitSignal(&Source::valueChanged, value); }
    void ping() { emitSignal(&Source::ping); }
    void textChanged(const std::string &text) { emitSignal(&Source::textChanged, text); }
};

class Sink : public QObject {
    Q_OBJECT
public:
    explicit Sink(QObject *parent = nullptr) : QObject(parent) {}
    int last = 0;
    int calls = 0;
    int pings = 0;
    std::string text;
    QObject *lastSender = nullptr;
    std::vector<int> order;

public slots:
    void onValue(int v) { last = v; ++calls; lastSender = sender(); }
    void onValueConst(int v) const { Q_UNUSED(v) }
    void onPing() { ++pings; }
    void onText(const std::string &s) { text = s; }
};

class EventSink : public QObject {
public:
    std::vector<int> seen;
    int timerHits = 0;
    int timerIdSeen = 0;

protected:
    void customEvent(QEvent *e) override { seen.push_back(e->type() - QEvent::User); }
    void timerEvent(QTimerEvent *e) override {
        ++timerHits;
        timerIdSeen = e->timerId();
    }
};

class UserEvent : public QEvent {
public:
    explicit UserEvent(int n) : QEvent(Type(QEvent::User + n)) {}
};

static int g_freeCalls = 0;
static void freeSlot(int v) { g_freeCalls += v; }

static void testSignalSlot() {
    Source src;
    Sink sink;

    auto c = QObject::connect(&src, &Source::valueChanged, &sink, &Sink::onValue);
    CHECK(static_cast<bool>(c));
    src.fire(5);
    CHECK(sink.last == 5 && sink.calls == 1);
    CHECK(sink.lastSender == &src);

    // slot with fewer arguments
    QObject::connect(&src, &Source::valueChanged, &sink, &Sink::onPing);
    src.fire(1);
    CHECK(sink.pings == 1);

    // const slot, free function, lambda, functor with context
    QObject::connect(&src, &Source::valueChanged, &sink, &Sink::onValueConst);
    QObject::connect(&src, &Source::valueChanged, &freeSlot);
    int lam = 0;
    QObject::connect(&src, &Source::valueChanged, [&lam](int v) { lam += v; });
    QObject::connect(&src, &Source::valueChanged, &sink, [&lam](int v) { lam += 100 * v; });
    src.fire(2);
    CHECK(g_freeCalls == 2);
    CHECK(lam == 202);

    // zero-arg signal, overload helper
    QObject::connect(&src, &Source::ping, &sink, &Sink::onPing);
    src.firePing();
    CHECK(sink.pings == 3);

    // disconnect via handle
    CHECK(QObject::disconnect(c));
    CHECK(!c);
    const int before = sink.calls;
    src.fire(9);
    CHECK(sink.calls == before);

    // disconnect by signal + receiver
    CHECK(QObject::disconnect(&src, &Source::valueChanged, &sink));
    const int pingsBefore = sink.pings;
    src.fire(10);
    CHECK(sink.pings == pingsBefore);

    // blockSignals
    src.disconnect();
    QObject::connect(&src, &Source::ping, &sink, &Sink::onPing);
    src.blockSignals(true);
    src.firePing();
    CHECK(sink.pings == pingsBefore);
    src.blockSignals(false);
    src.firePing();
    CHECK(sink.pings == pingsBefore + 1);
}

static void testLifetime() {
    Sink sink;
    {
        auto *src = new Source;
        auto c = QObject::connect(src, &Source::valueChanged, &sink, &Sink::onValue);
        delete src;
        CHECK(!c);
    }
    {
        auto *rcv = new Sink;
        Source src;
        QObject::connect(&src, &Source::valueChanged, rcv, &Sink::onValue);
        delete rcv;
        src.fire(1); // must not touch the deleted receiver
    }
    {
        auto *parent = new QObject;
        auto *child = new Source(parent);
        bool destroyed = false;
        QObject::connect(child, &QObject::destroyed, [&destroyed] { destroyed = true; });
        CHECK(parent->children().size() == 1);
        CHECK(child->parent() == parent);
        delete parent;
        CHECK(destroyed);
    }
    {
        // slot deletes the sender mid-emission
        auto *src = new Source;
        Sink sink2;
        QObject::connect(src, &Source::ping, [src] { delete src; });
        QObject::connect(src, &Source::ping, &sink2, &Sink::onPing);
        src->firePing();
        CHECK(sink2.pings == 0);
    }
    {
        // slot disconnects itself mid-emission
        Source src;
        int n = 0;
        QMetaObject::Connection c;
        c = QObject::connect(&src, &Source::ping, [&] { ++n; QObject::disconnect(c); });
        src.firePing();
        src.firePing();
        CHECK(n == 1);
    }
}

static void testQueued() {
    Source src;
    Sink sink;
    QObject::connect(&src, &Source::textChanged, &sink, &Sink::onText, Qt::QueuedConnection);
    QObject::connect(&src, &Source::valueChanged, &sink, &Sink::onValue, Qt::QueuedConnection);
    {
        std::string tmp = "hello";
        src.fireText(tmp);
        tmp = "changed";
    }
    src.fire(42);
    CHECK(sink.text.empty() && sink.calls == 0);
    QCoreApplication::processEvents();
    CHECK(sink.text == "hello");
    CHECK(sink.last == 42 && sink.lastSender == &src);

    // receiver destroyed before delivery
    auto *rcv = new Sink;
    QObject::connect(&src, &Source::valueChanged, rcv, &Sink::onValue, Qt::QueuedConnection);
    src.fire(1);
    delete rcv;
    QCoreApplication::processEvents();
}

static void testEvents() {
    EventSink es;
    QCoreApplication::postEvent(&es, new UserEvent(1));
    QCoreApplication::postEvent(&es, new UserEvent(2));
    QCoreApplication::postEvent(&es, new UserEvent(3));
    CHECK(es.seen.empty());
    QCoreApplication::processEvents();
    CHECK((es.seen == std::vector<int>{1, 2, 3}));

    UserEvent direct(7);
    CHECK(QCoreApplication::sendEvent(&es, &direct));
    CHECK(es.seen.back() == 7);

    QCoreApplication::postEvent(&es, new UserEvent(4));
    QCoreApplication::removePostedEvents(&es);
    QCoreApplication::processEvents();
    CHECK(es.seen.size() == 4);

    const int t = QEvent::registerEventType();
    CHECK(t >= QEvent::User && t <= QEvent::MaxUser);
    CHECK(QEvent::registerEventType() != t);

    // deleteLater
    auto *obj = new Source;
    bool gone = false;
    QObject::connect(obj, &QObject::destroyed, [&gone] { gone = true; });
    obj->deleteLater();
    CHECK(!gone);
    QCoreApplication::processEvents();
    CHECK(gone);
}

static void testTimers() {
    // QTimer repeating, stopped by quit
    {
        QEventLoop loop;
        QTimer timer;
        int hits = 0;
        QObject::connect(&timer, &QTimer::timeout, [&] {
            if (++hits == 3)
                loop.quit();
        });
        timer.start(5);
        CHECK(timer.isActive());
        CHECK(loop.exec() == 0);
        CHECK(hits == 3);
        timer.stop();
        CHECK(!timer.isActive());
    }
    // single shot (lambda, and with context)
    {
        QEventLoop loop;
        int a = 0, b = 0;
        QTimer::singleShot(1, [&a] { ++a; });
        Sink sink;
        QTimer::singleShot(2, &sink, &Sink::onPing);
        QTimer::singleShot(20, &loop, &QEventLoop::quit);
        loop.exec();
        CHECK(a == 1);
        CHECK(sink.pings == 1);
        Q_UNUSED(b)
    }
    // context destroyed before the single shot fires
    {
        auto *ctx = new Sink;
        QTimer::singleShot(1, ctx, &Sink::onPing);
        delete ctx;
        QEventLoop loop;
        QTimer::singleShot(10, &loop, &QEventLoop::quit);
        loop.exec();
    }
    // QObject::startTimer / killTimer
    {
        EventSink es;
        QEventLoop loop;
        const int id = es.startTimer(1);
        CHECK(id != 0);
        QTimer::singleShot(15, &loop, &QEventLoop::quit);
        loop.exec();
        CHECK(es.timerHits >= 1 && es.timerIdSeen == id);
        es.killTimer(id);
        const int hits = es.timerHits;
        QCoreApplication::processEvents();
        CHECK(es.timerHits == hits);
    }
    // nested event loop
    {
        QEventLoop outer;
        bool innerDone = false;
        QTimer::singleShot(1, [&] {
            QEventLoop inner;
            QTimer::singleShot(5, &inner, &QEventLoop::quit);
            inner.exec();
            innerDone = true;
            outer.quit();
        });
        outer.exec();
        CHECK(innerDone);
    }
}

static void testApplication(QCoreApplication &app) {
    CHECK(QCoreApplication::instance() == &app);
    bool aboutToQuit = false;
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&] { aboutToQuit = true; });
    QTimer::singleShot(1, [] { QCoreApplication::exit(7); });
    CHECK(app.exec() == 7);
    CHECK(aboutToQuit);

    QTimer::singleShot(1, &app, &QCoreApplication::quit);
    CHECK(QCoreApplication::exec() == 0);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    testSignalSlot();
    testLifetime();
    testQueued();
    testEvents();
    testTimers();
    testApplication(app);

    if (g_failures == 0)
        std::puts("all tests passed");
    return g_failures == 0 ? 0 : 1;
}
