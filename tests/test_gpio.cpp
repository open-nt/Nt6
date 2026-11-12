// MIT License - see LICENSE
//
// QGpioLine is tested against a fake backend: a pipe stands in for the line's event descriptor,
// so the edges, the debounce timer, polling and the errors go through the real event loop. The
// Linux backend is checked for how it parses names and reports chips that are missing; driving a
// real line needs the gpio-sim kernel module and root, which a test run does not have.
#include <QtCore>
#include <QtGpio>

#include "qgpiobackend_p.h"

#include <cerrno>
#include <cstdio>
#include <deque>
#include <fcntl.h>
#include <unistd.h>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static void spinEvents(int msecs) {
    QEventLoop loop;
    QTimer::singleShot(msecs, [&] { loop.quit(); });
    loop.exec();
}

// Spins the event loop until cond holds or the timeout passes; returns cond.
template <typename F>
static bool waitFor(F cond, int timeoutMs = 2000) {
    QElapsedTimer timer;
    timer.start();
    while (!cond() && timer.elapsed() < timeoutMs)
        spinEvents(2);
    return cond();
}

namespace {

// What the fake line looks like; shared by the test and the backend it creates.
struct FakeLine {
    QtPrivate::GpioConfig config; // as passed to the last open()
    bool level = false;           // what readValue() returns
    bool edges = true;            // whether eventDescriptor() is a descriptor
    bool failOpen = false;
    bool failRead = false;
    bool failWrite = false;
    int opens = 0;
    int writes = 0;
    std::deque<bool> pending; // levels the next readEvents() returns
    int pipe[2] = {-1, -1};

    void reset() {
        closePipe();
        *this = FakeLine();
        CHECK(::pipe2(pipe, O_NONBLOCK | O_CLOEXEC) == 0);
    }
    void closePipe() {
        for (int &fd : pipe) {
            if (fd >= 0)
                ::close(fd);
            fd = -1;
        }
    }
    // Reports the edges to levels, as the hardware would.
    void edge(std::initializer_list<bool> levels) {
        for (const bool l : levels) {
            pending.push_back(l);
            level = l;
        }
        const char c = 'e';
        CHECK(::write(pipe[1], &c, 1) == 1);
    }
};

FakeLine g_line;

class FakeBackend final : public QtPrivate::GpioBackend {
public:
    bool open(const QtPrivate::GpioConfig &config, QtPrivate::GpioFailure &failure) override {
        ++g_line.opens;
        g_line.config = config;
        if (g_line.failOpen) {
            QtPrivate::setGpioFailure(failure, EBUSY, QStringLiteral("Cannot request the line"));
            return false;
        }
        if (config.direction == QGpioLine::Output)
            g_line.level = config.initialValue;
        return true;
    }
    int eventDescriptor() const override { return g_line.edges ? g_line.pipe[0] : -1; }
    bool readEvents(std::vector<bool> &levels, QtPrivate::GpioFailure &failure) override {
        char buffer[64];
        while (::read(g_line.pipe[0], buffer, sizeof buffer) > 0) {
        }
        if (g_line.failRead) {
            QtPrivate::setGpioFailure(failure, ENODEV, QStringLiteral("Cannot read the line"));
            return false;
        }
        levels.assign(g_line.pending.begin(), g_line.pending.end());
        g_line.pending.clear();
        return true;
    }
    bool readValue(bool &value, QtPrivate::GpioFailure &failure) override {
        if (g_line.failRead) {
            QtPrivate::setGpioFailure(failure, ENODEV, QStringLiteral("Cannot read the line"));
            return false;
        }
        value = g_line.level;
        return true;
    }
    bool writeValue(bool value, QtPrivate::GpioFailure &failure) override {
        if (g_line.failWrite) {
            QtPrivate::setGpioFailure(failure, EIO, QStringLiteral("Cannot set the line"));
            return false;
        }
        ++g_line.writes;
        g_line.level = value;
        return true;
    }
};

std::unique_ptr<QtPrivate::GpioBackend> makeFake() { return std::make_unique<FakeBackend>(); }

} // namespace

static void testDefaults() {
    QGpioLine line;
    CHECK(line.lineName().isEmpty());
    CHECK(line.direction() == QGpioLine::Input);
    CHECK(line.bias() == QGpioLine::AsIs);
    CHECK(!line.isActiveLow());
    CHECK(line.debounceInterval() == 0);
    CHECK(line.pollInterval() == 10);
    CHECK(!line.isOpen());
    CHECK(!line.isEventDriven());
    CHECK(!line.value());
    CHECK(line.error() == QGpioLine::NoError);
    line.setDebounceInterval(-5);
    CHECK(line.debounceInterval() == 0);
    line.setPollInterval(0);
    CHECK(line.pollInterval() == 1);

    QList<QGpioLine::GpioError> errors;
    QObject::connect(&line, &QGpioLine::errorOccurred, [&](QGpioLine::GpioError e) { errors.append(e); });
    CHECK(!line.open());
    CHECK(line.error() == QGpioLine::DeviceNotFoundError);
    CHECK(!line.errorString().isEmpty());
    CHECK(errors == QList<QGpioLine::GpioError>{QGpioLine::DeviceNotFoundError});
    line.clearError();
    CHECK(line.error() == QGpioLine::NoError);
    CHECK(line.errorString().isEmpty());

    const QMetaObject *mo = &QGpioLine::staticMetaObject;
    CHECK(mo->indexOfProperty("value") >= 0);
    CHECK(mo->indexOfProperty("debounceInterval") >= 0);
    CHECK(mo->indexOfEnumerator("GpioError") >= 0);
}

static void testConfigPassed() {
    g_line.reset();
    QGpioLine line(QStringLiteral("gpiochip3:7"));
    line.setActiveLow(true);
    line.setBias(QGpioLine::PullUp);
    g_line.level = true;
    QList<bool> changes;
    QObject::connect(&line, &QGpioLine::valueChanged, [&](bool v) { changes.append(v); });
    CHECK(line.open());
    CHECK(line.isOpen());
    CHECK(line.isEventDriven());
    CHECK(g_line.config.name == QStringLiteral("gpiochip3:7"));
    CHECK(g_line.config.activeLow);
    CHECK(g_line.config.bias == QGpioLine::PullUp);
    CHECK(g_line.config.direction == QGpioLine::Input);
    // The value is read by open() and reported as a change from the default of false.
    CHECK(line.value());
    CHECK(changes == QList<bool>{true});

    CHECK(!line.open());
    CHECK(line.error() == QGpioLine::OpenError);
    CHECK(line.isOpen());
    line.close();
    CHECK(!line.isOpen());
    line.close(); // harmless
}

static void testEdges() {
    g_line.reset();
    QGpioLine line(QStringLiteral("PTT"));
    QList<bool> changes;
    QObject::connect(&line, &QGpioLine::valueChanged, [&](bool v) { changes.append(v); });
    CHECK(line.open());
    CHECK(changes.isEmpty());

    g_line.edge({true});
    CHECK(waitFor([&] { return changes.size() == 1; }));
    CHECK(line.value());

    // Without debouncing, every edge is reported in order.
    changes.clear();
    g_line.edge({false, true, false});
    CHECK(waitFor([&] { return changes.size() == 3; }));
    CHECK((changes == QList<bool>{false, true, false}));
    CHECK(!line.value());

    // An edge to the value the line already has is not a change.
    changes.clear();
    g_line.edge({false});
    spinEvents(20);
    CHECK(changes.isEmpty());
}

static void testDebounce() {
    g_line.reset();
    QGpioLine line(QStringLiteral("PTT"));
    line.setDebounceInterval(40);
    QList<bool> changes;
    QObject::connect(&line, &QGpioLine::valueChanged, [&](bool v) { changes.append(v); });
    CHECK(line.open());

    // Contact bounce: the line settles high, which is reported once, after the interval.
    QElapsedTimer timer;
    timer.start();
    g_line.edge({true, false, true, false, true});
    spinEvents(15);
    CHECK(changes.isEmpty());
    CHECK(waitFor([&] { return !changes.isEmpty(); }));
    CHECK(timer.elapsed() >= 35);
    CHECK(changes == QList<bool>{true});

    // A glitch that goes back before the interval ends is not reported.
    changes.clear();
    g_line.edge({false, true});
    spinEvents(100);
    CHECK(changes.isEmpty());
    CHECK(line.value());
}

static void testPolling() {
    g_line.reset();
    g_line.edges = false;
    QGpioLine line(QStringLiteral("PTT"));
    line.setPollInterval(5);
    QList<bool> changes;
    QObject::connect(&line, &QGpioLine::valueChanged, [&](bool v) { changes.append(v); });
    CHECK(line.open());
    CHECK(line.isOpen());
    CHECK(!line.isEventDriven());

    g_line.level = true;
    CHECK(waitFor([&] { return changes.size() == 1; }));
    g_line.level = false;
    CHECK(waitFor([&] { return changes.size() == 2; }));
    CHECK((changes == QList<bool>{true, false}));

    // Polling with a debounce interval.
    changes.clear();
    line.setDebounceInterval(30);
    g_line.level = true;
    spinEvents(15);
    CHECK(changes.isEmpty());
    CHECK(waitFor([&] { return changes.size() == 1; }));
    CHECK(line.value());

    line.close();
    g_line.level = false;
    spinEvents(30);
    CHECK(changes.size() == 1);
}

static void testOutput() {
    g_line.reset();
    QGpioLine led(QStringLiteral("gpiochip0:18"));
    led.setDirection(QGpioLine::Output);
    QList<bool> changes;
    QObject::connect(&led, &QGpioLine::valueChanged, [&](bool v) { changes.append(v); });

    // Set while closed: kept for open().
    CHECK(led.setValue(true));
    CHECK(led.value());
    CHECK(g_line.writes == 0);
    CHECK(led.open());
    CHECK(g_line.config.direction == QGpioLine::Output);
    CHECK(g_line.config.initialValue);
    CHECK(g_line.level);
    CHECK(!led.isEventDriven());

    CHECK(led.setValue(false));
    CHECK(g_line.writes == 1);
    CHECK(!g_line.level);
    CHECK(!led.value());
    CHECK(led.setValue(false));
    CHECK((changes == QList<bool>{true, false}));

    g_line.failWrite = true;
    CHECK(!led.setValue(true));
    CHECK(led.error() == QGpioLine::ResourceError);
    CHECK(!led.isOpen());

    QGpioLine input(QStringLiteral("PTT"));
    CHECK(!input.setValue(true));
    CHECK(input.error() == QGpioLine::UnsupportedOperationError);
}

static void testErrors() {
    g_line.reset();
    g_line.failOpen = true;
    QGpioLine line(QStringLiteral("PTT"));
    CHECK(!line.open());
    CHECK(line.error() == QGpioLine::BusyError);
    CHECK(line.errorString().startsWith(QStringLiteral("Cannot request the line")));
    CHECK(!line.isOpen());

    // The line goes away while it is open.
    g_line.failOpen = false;
    QList<QGpioLine::GpioError> errors;
    QObject::connect(&line, &QGpioLine::errorOccurred, [&](QGpioLine::GpioError e) { errors.append(e); });
    CHECK(line.open());
    CHECK(line.error() == QGpioLine::NoError);
    g_line.failRead = true;
    g_line.edge({true});
    CHECK(waitFor([&] { return !errors.isEmpty(); }));
    CHECK(errors == QList<QGpioLine::GpioError>{QGpioLine::ResourceError});
    CHECK(!line.isOpen());

    // Open again once it is back.
    g_line.failRead = false;
    CHECK(line.open());
    CHECK(line.isOpen());
}

static void testCloseFromSlot() {
    g_line.reset();
    QGpioLine line(QStringLiteral("PTT"));
    int changes = 0;
    QObject::connect(&line, &QGpioLine::valueChanged, [&](bool) {
        ++changes;
        line.close();
    });
    CHECK(line.open());
    g_line.edge({true, false, true});
    CHECK(waitFor([&] { return changes > 0; }));
    spinEvents(20);
    CHECK(changes == 1);
    CHECK(!line.isOpen());

    // Destroying the line from its own signal is safe too.
    g_line.reset();
    auto *owned = new QGpioLine(QStringLiteral("PTT"));
    bool destroyed = false;
    QObject::connect(owned, &QObject::destroyed, [&] { destroyed = true; });
    QObject::connect(owned, &QGpioLine::valueChanged, owned, &QObject::deleteLater);
    CHECK(owned->open());
    g_line.edge({true});
    CHECK(waitFor([&] { return destroyed; }));
}

static void testLinuxBackend() {
    QtPrivate::setGpioBackendFactory(nullptr);
    {
        QGpioLine line(QStringLiteral("nt-no-such-chip:3"));
        CHECK(!line.open());
        CHECK(line.error() == QGpioLine::DeviceNotFoundError);
        CHECK(line.errorString().contains(QStringLiteral("/dev/nt-no-such-chip")));
    }
    {
        QGpioLine line(QStringLiteral("/nt/no/such/chip:3"));
        CHECK(!line.open());
        CHECK(line.error() == QGpioLine::DeviceNotFoundError);
    }
    {
        QGpioLine line(QStringLiteral("gpiochip0:x"));
        CHECK(!line.open());
        CHECK(line.error() == QGpioLine::DeviceNotFoundError);
        CHECK(line.errorString().contains(QStringLiteral("chip:offset")));
    }
    {
        // Not found anywhere; if some chip may not be opened, that is what is reported.
        QGpioLine line(QStringLiteral("nt-no-such-line-name"));
        CHECK(!line.open());
        CHECK(line.error() == QGpioLine::DeviceNotFoundError || line.error() == QGpioLine::PermissionError);
        CHECK(line.errorString().contains(QStringLiteral("nt-no-such-line-name")));
    }
    QtPrivate::setGpioBackendFactory(makeFake);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QtPrivate::setGpioBackendFactory(makeFake);
    testDefaults();
    testConfigPassed();
    testEdges();
    testDebounce();
    testPolling();
    testOutput();
    testErrors();
    testCloseFromSlot();
    testLinuxBackend();
    g_line.closePipe();
    std::printf("%s: %d failure(s)\n", g_failures ? "FAILED" : "PASSED", g_failures);
    return g_failures ? 1 : 0;
}
