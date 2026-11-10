// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/QAbstractAnimation>
#include <QtCore/QParallelAnimationGroup>
#include <QtCore/QPauseAnimation>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QSequentialAnimationGroup>
#include <QtCore/QVariantAnimation>

#include <cstdio>
#include <string>
#include <vector>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using A = QAbstractAnimation;

namespace {

// Runs the event loop until `condition` holds or `ms` milliseconds have passed.
template <typename Condition> bool waitFor(Condition condition, int ms = 3000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
        if (timer.elapsed() > ms)
            return false;
        QEventLoop loop;
        QTimer::singleShot(2, &loop, [&loop] { loop.quit(); });
        loop.exec();
    }
    return true;
}

void spin(int ms) {
    QElapsedTimer timer;
    timer.start();
    waitFor([&] { return timer.elapsed() >= ms; }, ms + 1000);
}

// An animation that writes down what it is told.
class Probe : public QAbstractAnimation {
public:
    explicit Probe(int duration = 100, QObject *parent = nullptr) : QAbstractAnimation(parent), m_duration(duration) {}
    int duration() const override { return m_duration; }
    void setDuration(int duration) { m_duration = duration; }

    std::vector<int> times;
    std::vector<std::pair<State, State>> states;
    std::vector<Direction> directions;

protected:
    void updateCurrentTime(int currentTime) override { times.push_back(currentTime); }
    void updateState(State newState, State oldState) override { states.emplace_back(newState, oldState); }
    void updateDirection(Direction direction) override { directions.push_back(direction); }

private:
    int m_duration;
};

// Counts the signals of an animation.
struct Signals {
    explicit Signals(QAbstractAnimation *animation) {
        QObject::connect(animation, &QAbstractAnimation::finished, [this] { ++finished; });
        QObject::connect(animation, &QAbstractAnimation::stateChanged,
                         [this](A::State newState, A::State oldState) { states.emplace_back(newState, oldState); });
        QObject::connect(animation, &QAbstractAnimation::currentLoopChanged,
                         [this](int loop) { loops.push_back(loop); });
        QObject::connect(animation, &QAbstractAnimation::directionChanged,
                         [this](A::Direction d) { directions.push_back(d); });
    }
    int finished = 0;
    std::vector<std::pair<A::State, A::State>> states;
    std::vector<int> loops;
    std::vector<A::Direction> directions;
};

QPropertyAnimation *animate(QObject *target, const char *property, int from, int to, int duration) {
    auto *animation = new QPropertyAnimation(target, property);
    animation->setStartValue(from);
    animation->setEndValue(to);
    animation->setDuration(duration);
    return animation;
}

} // namespace

static void testAbstractAnimation() {
    Probe probe(100);
    Signals seen(&probe);
    CHECK(probe.state() == A::Stopped && probe.direction() == A::Forward && probe.loopCount() == 1);
    CHECK(probe.currentTime() == 0 && probe.currentLoopTime() == 0 && probe.currentLoop() == 0 &&
          probe.group() == nullptr);
    CHECK(probe.duration() == 100 && probe.totalDuration() == 100);

    // The time of a stopped animation is set by hand, and kept inside the animation
    probe.setCurrentTime(40);
    CHECK(probe.currentTime() == 40 && probe.currentLoopTime() == 40 && probe.times.back() == 40);
    probe.setCurrentTime(-5);
    CHECK(probe.currentTime() == 0 && probe.times.back() == 0);
    probe.setCurrentTime(1000);
    CHECK(probe.currentTime() == 100 && probe.currentLoopTime() == 100 && probe.state() == A::Stopped &&
          seen.finished == 0);

    // Loops
    probe.setLoopCount(3);
    CHECK(probe.loopCount() == 3 && probe.totalDuration() == 300);
    probe.setCurrentTime(250);
    CHECK(probe.currentTime() == 250 && probe.currentLoop() == 2 && probe.currentLoopTime() == 50);
    CHECK(seen.loops == std::vector<int>({2}));
    probe.setCurrentTime(100); // a boundary is the start of the later loop
    CHECK(probe.currentLoop() == 1 && probe.currentLoopTime() == 0 && seen.loops == std::vector<int>({2, 1}));
    probe.setCurrentTime(300); // the end is the end of the last loop
    CHECK(probe.currentLoop() == 2 && probe.currentLoopTime() == 100 && probe.currentTime() == 300);
    probe.setCurrentTime(99);
    CHECK(probe.currentLoop() == 0 && probe.currentLoopTime() == 99);
    probe.setLoopCount(-1);
    CHECK(probe.totalDuration() == -1);
    probe.setCurrentTime(100 * 1000 + 7);
    CHECK(probe.currentLoop() == 1000 && probe.currentLoopTime() == 7);
    probe.setLoopCount(1);
    probe.setDuration(0);
    CHECK(probe.totalDuration() == 0);
    probe.setDuration(-1);
    CHECK(probe.totalDuration() == -1);
    probe.setCurrentTime(123456);
    CHECK(probe.currentTime() == 123456 && probe.currentLoopTime() == 123456 && probe.currentLoop() == 0);
    probe.setDuration(100);
    probe.setCurrentTime(0);

    // Start, pause, resume and stop
    probe.times.clear();
    probe.states.clear();
    seen.states.clear();
    probe.start();
    CHECK(probe.state() == A::Running && probe.currentTime() == 0);
    CHECK(probe.states.size() == 1 && probe.states[0] == std::make_pair(A::Running, A::Stopped));
    CHECK(seen.states.size() == 1 && seen.states[0] == std::make_pair(A::Running, A::Stopped));
    CHECK(probe.times == std::vector<int>({0})); // shown at its first moment
    probe.start();                               // already running
    CHECK(seen.states.size() == 1);
    probe.setCurrentTime(30);
    probe.pause();
    CHECK(probe.state() == A::Paused && seen.states.back() == std::make_pair(A::Paused, A::Running));
    probe.resume();
    CHECK(probe.state() == A::Running && seen.states.back() == std::make_pair(A::Running, A::Paused) &&
          probe.currentTime() == 30);
    probe.setPaused(true);
    CHECK(probe.state() == A::Paused);
    probe.setPaused(false);
    CHECK(probe.state() == A::Running);
    probe.stop();
    CHECK(probe.state() == A::Stopped && seen.states.back() == std::make_pair(A::Stopped, A::Running));
    CHECK(seen.finished == 0 && probe.currentTime() == 30); // stopped early: not finished, and left where it was
    probe.stop();
    probe.pause();  // a stopped animation cannot be paused
    probe.resume(); // nor resumed
    CHECK(probe.state() == A::Stopped && seen.states.size() == 6);

    // The end stops the animation
    probe.start();
    CHECK(probe.currentTime() == 0); // a start is from the beginning
    probe.setCurrentTime(100);
    CHECK(probe.state() == A::Stopped && seen.finished == 1 && probe.currentTime() == 100 && probe.times.back() == 100);
    // A paused animation starts over only after a stop
    probe.start();
    probe.setCurrentTime(60);
    probe.pause();
    probe.start();
    CHECK(probe.state() == A::Running && probe.currentTime() == 60);
    probe.stop();

    // Without loops there is nothing to run
    probe.setLoopCount(0);
    probe.start();
    CHECK(probe.state() == A::Stopped);
    probe.setLoopCount(1);

    // Backwards: from the end to the start
    probe.setDirection(A::Backward);
    CHECK(probe.direction() == A::Backward && seen.directions == std::vector<A::Direction>({A::Backward}));
    CHECK(probe.directions == std::vector<A::Direction>({A::Backward}));
    probe.setDirection(A::Backward);
    CHECK(seen.directions.size() == 1);
    seen.finished = 0;
    probe.start();
    CHECK(probe.state() == A::Running && probe.currentTime() == 100 && probe.times.back() == 100);
    probe.setCurrentTime(100);
    CHECK(probe.state() == A::Running); // the end is where it starts
    probe.setCurrentTime(40);
    CHECK(probe.currentLoopTime() == 40 && probe.state() == A::Running);
    probe.setCurrentTime(0);
    CHECK(probe.state() == A::Stopped && seen.finished == 1);
    // Backwards a boundary is the end of the earlier loop
    probe.setLoopCount(3);
    probe.setCurrentTime(200);
    CHECK(probe.currentLoop() == 1 && probe.currentLoopTime() == 100);
    probe.setCurrentTime(201);
    CHECK(probe.currentLoop() == 2 && probe.currentLoopTime() == 1);
    probe.start();
    CHECK(probe.currentTime() == 300 && probe.currentLoop() == 2);
    probe.stop();
    probe.setDirection(A::Forward);
    probe.setLoopCount(1);

    // An animation without an end is finished when it is stopped
    seen.finished = 0;
    probe.setLoopCount(-1);
    probe.start();
    probe.setCurrentTime(450);
    CHECK(probe.state() == A::Running && probe.currentLoop() == 4);
    probe.stop();
    CHECK(seen.finished == 1);
    probe.setLoopCount(1);
    probe.setDuration(-1);
    probe.start();
    probe.setCurrentTime(100000);
    CHECK(probe.state() == A::Running);
    probe.stop();
    CHECK(seen.finished == 2);
}

static void testRealTime() {
    {
        // The event loop moves the time
        Probe probe(80);
        Signals seen(&probe);
        QElapsedTimer timer;
        timer.start();
        probe.start();
        CHECK(waitFor([&] { return probe.state() == A::Stopped; }));
        CHECK(seen.finished == 1 && timer.elapsed() >= 70 && probe.currentTime() == 80);
        CHECK(probe.times.size() >= 3 && probe.times.front() == 0 && probe.times.back() == 80);
        bool ordered = true;
        for (std::size_t i = 1; i < probe.times.size(); ++i)
            ordered = ordered && probe.times[i] >= probe.times[i - 1];
        CHECK(ordered);
        // And again, from the start
        probe.times.clear();
        probe.start();
        CHECK(probe.currentTime() == 0);
        CHECK(waitFor([&] { return probe.state() == A::Stopped; }) && seen.finished == 2);
    }
    {
        // Backwards, and several loops
        Probe probe(40);
        Signals seen(&probe);
        probe.setDirection(A::Backward);
        probe.setLoopCount(3);
        probe.start();
        CHECK(probe.currentTime() == 120);
        CHECK(waitFor([&] { return probe.state() == A::Stopped; }));
        CHECK(seen.finished == 1 && probe.currentTime() == 0 && probe.times.back() == 0);
        // The loops are counted down; the first signal is for the loop that the start is in
        bool down = !seen.loops.empty() && seen.loops.front() == 2 && seen.loops.back() == 0;
        for (std::size_t i = 1; i < seen.loops.size(); ++i)
            down = down && seen.loops[i] < seen.loops[i - 1];
        CHECK(down);
    }
    {
        // A paused animation stands still
        Probe probe(150);
        probe.start();
        spin(40);
        probe.pause();
        const int held = probe.currentTime();
        CHECK(held > 0 && held < 150);
        spin(60);
        CHECK(probe.currentTime() == held && probe.state() == A::Paused);
        probe.resume();
        spin(30);
        CHECK(probe.currentTime() > held);
        CHECK(waitFor([&] { return probe.state() == A::Stopped; }) && probe.currentTime() == 150);
    }
    {
        // The direction can change on the way
        Probe probe(400);
        Signals seen(&probe);
        probe.start();
        spin(60);
        probe.setDirection(A::Backward);
        const int turned = probe.currentTime();
        CHECK(turned >= 40 && turned < 400);
        CHECK(waitFor([&] { return probe.state() == A::Stopped; }));
        CHECK(probe.currentTime() == 0 && seen.finished == 1);
    }
    {
        // Deleted when stopped, if asked
        QPointer<Probe> owned = new Probe(30);
        owned->start(A::DeleteWhenStopped);
        CHECK(waitFor([&] { return owned.isNull(); }));
        QPointer<Probe> kept = new Probe(30);
        kept->start(A::KeepWhenStopped);
        CHECK(waitFor([&] { return kept->state() == A::Stopped; }));
        spin(20);
        CHECK(!kept.isNull());
        delete kept.data();
    }
    {
        // An animation can be destroyed while it runs, also from inside an update
        auto *doomed = new Probe(1000);
        Signals seen(doomed);
        doomed->start();
        spin(20);
        delete doomed;
        CHECK(seen.states.back() == std::make_pair(A::Stopped, A::Running));
        spin(40); // the timer finds nothing to move

        QPointer<QVariantAnimation> self = new QVariantAnimation;
        self->setStartValue(0);
        self->setEndValue(100);
        self->setDuration(1000);
        QObject::connect(self.data(), &QVariantAnimation::valueChanged, [&](const QVariant &value) {
            if (value.toInt() > 2)
                delete self.data();
        });
        self->start();
        CHECK(waitFor([&] { return self.isNull(); }));
        spin(40);
    }
    {
        // Many at once
        std::vector<Probe *> probes;
        for (int i = 0; i < 20; ++i) {
            probes.push_back(new Probe(20 + 5 * i));
            probes.back()->start();
        }
        CHECK(waitFor([&] {
            for (Probe *probe : probes) {
                if (probe->state() != A::Stopped)
                    return false;
            }
            return true;
        }));
        for (Probe *probe : probes) {
            CHECK(probe->currentTime() == probe->duration());
            delete probe;
        }
    }
}

namespace {

class CountingAnimation : public QVariantAnimation {
public:
    int updates = 0;
    QVariant last;

protected:
    void updateCurrentValue(const QVariant &value) override {
        ++updates;
        last = value;
    }
};

class SteppedAnimation : public QVariantAnimation {
protected:
    QVariant interpolated(const QVariant &from, const QVariant &to, qreal progress) const override {
        return progress < 0.5 ? from : to;
    }
};

QVariant spellOut(const QString &from, const QString &to, qreal progress) {
    return from + to.left(qRound(progress * static_cast<qreal>(to.size())));
}

QVariant at(const QVariant &from, const QVariant &to, int percent) {
    QVariantAnimation animation;
    animation.setDuration(100);
    animation.setStartValue(from);
    animation.setEndValue(to);
    animation.setCurrentTime(percent);
    return animation.currentValue();
}

} // namespace

static void testVariantAnimation() {
    QVariantAnimation animation;
    CHECK(animation.duration() == 250 && !animation.startValue().isValid() && !animation.endValue().isValid());
    CHECK(!animation.currentValue().isValid() && animation.easingCurve() == QEasingCurve(QEasingCurve::Linear));
    CHECK(animation.keyValues().isEmpty());
    std::vector<int> values;
    QObject::connect(&animation, &QVariantAnimation::valueChanged,
                     [&](const QVariant &value) { values.push_back(value.toInt()); });

    animation.setDuration(100);
    animation.setCurrentTime(50);
    CHECK(!animation.currentValue().isValid() && values.empty()); // no values to go between
    animation.setStartValue(0);
    CHECK(animation.startValue() == QVariant(0) && !animation.currentValue().isValid());
    animation.setEndValue(100);
    CHECK(animation.endValue() == QVariant(100) && animation.keyValues().size() == 2);
    // The value follows the time, and the signal tells of changes only
    CHECK(animation.currentValue() == QVariant(50) && values == std::vector<int>({50}));
    animation.setCurrentTime(25);
    CHECK(animation.currentValue() == QVariant(25) && animation.currentValue().typeId() == QMetaType::Int);
    animation.setCurrentTime(25);
    animation.setCurrentTime(100);
    animation.setCurrentTime(0);
    CHECK(values == std::vector<int>({50, 25, 100, 0}));

    // Key values in between
    animation.setKeyValueAt(0.5, 200);
    CHECK(animation.keyValueAt(0.5) == QVariant(200) && !animation.keyValueAt(0.4).isValid());
    CHECK(animation.keyValues().size() == 3 && animation.keyValues().at(1).first == 0.5);
    animation.setCurrentTime(25);
    CHECK(animation.currentValue() == QVariant(100));
    animation.setCurrentTime(50);
    CHECK(animation.currentValue() == QVariant(200));
    animation.setCurrentTime(75);
    CHECK(animation.currentValue() == QVariant(150));
    animation.setKeyValueAt(0.5, 300); // replaced
    CHECK(animation.keyValues().size() == 3 && animation.currentValue() == QVariant(200));
    animation.setKeyValueAt(0.5, QVariant()); // removed
    CHECK(animation.keyValues().size() == 2 && animation.currentValue() == QVariant(75));
    animation.setKeyValueAt(1.5, 7); // not a step
    animation.setKeyValueAt(-0.1, 7);
    CHECK(animation.keyValues().size() == 2);
    animation.setKeyValues({{1.0, QVariant(10)}, {0.25, QVariant(40)}, {0.0, QVariant(0)}});
    const QVariantAnimation::KeyValues keys = animation.keyValues();
    CHECK(keys.size() == 3 && keys.at(0).first == 0.0 && keys.at(1).first == 0.25 && keys.at(2).second == QVariant(10));
    animation.setCurrentTime(25);
    CHECK(animation.currentValue() == QVariant(40));
    animation.setCurrentTime(100);
    CHECK(animation.currentValue() == QVariant(10) && animation.endValue() == QVariant(10));

    // The duration
    animation.setDuration(-5);
    CHECK(animation.duration() == 100);
    animation.setDuration(0);
    animation.setCurrentTime(0);
    CHECK(animation.duration() == 0 && animation.currentValue() == QVariant(10)); // no time: at the end at once

    // The types that are interpolated
    CHECK(at(0.0, 1.0, 25) == QVariant(0.25) && at(1.5f, 2.5f, 50) == QVariant(2.0f));
    CHECK(at(QVariant::fromValue<uint>(10), QVariant::fromValue<uint>(20), 50) == QVariant::fromValue<uint>(15));
    CHECK(at(QVariant::fromValue<qlonglong>(-100), QVariant::fromValue<qlonglong>(100), 75) ==
          QVariant::fromValue<qlonglong>(50));
    CHECK(at(QVariant::fromValue<qulonglong>(0), QVariant::fromValue<qulonglong>(1000), 10) ==
          QVariant::fromValue<qulonglong>(100));
    CHECK(at(10, -10, 25) == QVariant(5) && at(0, 3, 50).toInt() == 2); // rounded
    CHECK(at(QPoint(0, 100), QPoint(100, 0), 25) == QVariant(QPoint(25, 75)));
    CHECK(at(QPointF(0, 1), QPointF(1, 0), 25) == QVariant(QPointF(0.25, 0.75)));
    CHECK(at(QSize(10, 20), QSize(30, 60), 50) == QVariant(QSize(20, 40)));
    CHECK(at(QSizeF(1, 2), QSizeF(2, 4), 50) == QVariant(QSizeF(1.5, 3)));
    CHECK(at(QRect(0, 0, 10, 10), QRect(100, 200, 30, 50), 50) == QVariant(QRect(50, 100, 20, 30)));
    CHECK(at(QRectF(0, 0, 1, 1), QRectF(1, 2, 3, 5), 50) == QVariant(QRectF(0.5, 1, 2, 3)));
    CHECK(at(QLine(0, 0, 10, 10), QLine(10, 20, 30, 40), 50) == QVariant(QLine(5, 10, 20, 25)));
    CHECK(at(QLineF(0, 0, 1, 1), QLineF(1, 2, 3, 4), 50) == QVariant(QLineF(0.5, 1, 2, 2.5)));
    CHECK(at(QPoint(1, 2), QPoint(3, 4), 50).typeId() == QMetaType::QPoint);
    // Numbers of two types meet as doubles
    CHECK(at(0, 1.0, 25) == QVariant(0.25) && at(0, 1.0, 25).typeId() == QMetaType::Double);
    // What cannot be interpolated changes at the end
    CHECK(at(QString("from"), QString("to"), 0) == QVariant(QString("from")));
    CHECK(at(QString("from"), QString("to"), 99) == QVariant(QString("from")));
    CHECK(at(QString("from"), QString("to"), 100) == QVariant(QString("to")));
    CHECK(at(true, false, 50) == QVariant(true) && at(QString("x"), 5, 100) == QVariant(5));

    // A function for a type of the program's choice, and none again
    qRegisterAnimationInterpolator<QString>(spellOut);
    CHECK(at(QString(">"), QString("abcdefghij"), 30) == QVariant(QString(">abc")));
    CHECK(at(QString(">"), QString("abcdefghij"), 100) == QVariant(QString(">abcdefghij")));
    qRegisterAnimationInterpolator<QString>(nullptr);
    CHECK(at(QString(">"), QString("abcdefghij"), 30) == QVariant(QString(">")));

    // The easing curve shapes the progress, and may leave the range
    QVariantAnimation eased;
    eased.setDuration(100);
    eased.setStartValue(0.0);
    eased.setEndValue(100.0);
    eased.setEasingCurve(QEasingCurve::InQuad);
    CHECK(eased.easingCurve().type() == QEasingCurve::InQuad);
    eased.setCurrentTime(50);
    CHECK(qAbs(eased.currentValue().toDouble() - 25.0) < 1e-9);
    eased.setEasingCurve(QEasingCurve::OutBack);
    double highest = 0;
    for (int time = 0; time <= 100; ++time) {
        eased.setCurrentTime(time);
        highest = qMax(highest, eased.currentValue().toDouble());
    }
    CHECK(highest > 100.5 && eased.currentValue().toDouble() == 100.0);
    // With key values a curve that overshoots goes on along the last pair
    eased.setKeyValueAt(0.5, 0.0);
    QEasingCurve beyond;
    beyond.setCustomType([](qreal) -> qreal { return 1.5; });
    eased.setEasingCurve(beyond);
    eased.setCurrentTime(10);
    CHECK(qAbs(eased.currentValue().toDouble() - 200.0) < 1e-9);

    // The hooks of a subclass
    CountingAnimation counting;
    int changes = 0;
    QObject::connect(&counting, &QVariantAnimation::valueChanged, [&] { ++changes; });
    counting.setDuration(100);
    counting.setStartValue(0);
    counting.setEndValue(10);
    counting.setCurrentTime(50);
    counting.setCurrentTime(51); // the same value: told to the subclass, not signalled
    counting.setCurrentTime(52);
    CHECK(counting.last == QVariant(5) && counting.updates >= 3 && changes == counting.updates - 2);
    SteppedAnimation stepped;
    stepped.setDuration(100);
    stepped.setStartValue(1);
    stepped.setEndValue(2);
    stepped.setCurrentTime(49);
    CHECK(stepped.currentValue() == QVariant(1));
    stepped.setCurrentTime(50);
    CHECK(stepped.currentValue() == QVariant(2));

    // In time
    QVariantAnimation timed;
    timed.setDuration(60);
    timed.setStartValue(0);
    timed.setEndValue(1000);
    std::vector<int> seen;
    QObject::connect(&timed, &QVariantAnimation::valueChanged,
                     [&](const QVariant &value) { seen.push_back(value.toInt()); });
    timed.start();
    CHECK(waitFor([&] { return timed.state() == A::Stopped; }));
    CHECK(seen.size() >= 2 && seen.front() > 0 && seen.back() == 1000 && timed.currentValue() == QVariant(1000));
    bool rising = true;
    for (std::size_t i = 1; i < seen.size(); ++i)
        rising = rising && seen[i] > seen[i - 1];
    CHECK(rising);
    CHECK(std::string(timed.metaObject()->className()) == "QVariantAnimation");
    CHECK(timed.property("duration").toInt() == 60 && timed.setProperty("duration", 70) && timed.duration() == 70);
}

static void testPropertyAnimation() {
    QObject target;
    target.setProperty("level", 10);
    QPropertyAnimation animation(&target, "level");
    CHECK(animation.targetObject() == &target && animation.propertyName() == "level");
    animation.setDuration(100);
    animation.setEndValue(110);

    // Without a start value the animation starts from what the property is
    animation.start();
    CHECK(target.property("level") == QVariant(10));
    animation.setCurrentTime(50);
    CHECK(target.property("level") == QVariant(60));
    // The target and the property of a running animation stay
    QObject other;
    animation.setTargetObject(&other);
    animation.setPropertyName("other");
    CHECK(animation.targetObject() == &target && animation.propertyName() == "level");
    animation.stop();
    CHECK(target.property("level") == QVariant(60));
    animation.setCurrentTime(80); // stopped: the property is left alone
    CHECK(target.property("level") == QVariant(60) && animation.currentValue() == QVariant(90));
    // The next run starts from the new value of the property
    animation.start();
    animation.setCurrentTime(50);
    CHECK(target.property("level") == QVariant(85));
    animation.setCurrentTime(100);
    CHECK(target.property("level") == QVariant(110) && animation.state() == A::Stopped);

    // With a start value the property jumps to it
    animation.setStartValue(0);
    animation.start();
    CHECK(target.property("level") == QVariant(0));
    animation.setCurrentTime(30);
    CHECK(target.property("level") == QVariant(33));
    animation.stop();

    // Another object and another property
    animation.setTargetObject(&other);
    animation.setPropertyName("height");
    CHECK(animation.targetObject() == &other && animation.propertyName() == "height");
    animation.start();
    animation.setCurrentTime(70);
    CHECK(other.property("height") == QVariant(77) && target.property("level") == QVariant(33));
    animation.stop();

    // One animation for a property at a time
    QPropertyAnimation first(&target, "level"), second(&target, "level"), elsewhere(&target, "depth");
    for (QPropertyAnimation *each : {&first, &second, &elsewhere}) {
        each->setDuration(100);
        each->setStartValue(0);
        each->setEndValue(100);
    }
    first.start();
    elsewhere.start();
    CHECK(first.state() == A::Running);
    second.start();
    CHECK(first.state() == A::Stopped && second.state() == A::Running && elsewhere.state() == A::Running);
    first.start();
    CHECK(first.state() == A::Running && second.state() == A::Stopped);
    first.stop();
    elsewhere.stop();
    second.start(); // nothing to stop
    CHECK(second.state() == A::Running);
    second.stop();

    {
        // The target goes away: the animation stops at its next update
        auto *gone = new QObject;
        QPropertyAnimation orphan(gone, "x");
        orphan.setDuration(100);
        orphan.setStartValue(0);
        orphan.setEndValue(10);
        orphan.start();
        delete gone;
        CHECK(orphan.targetObject() == nullptr && orphan.state() == A::Running);
        orphan.setCurrentTime(50);
        CHECK(orphan.state() == A::Stopped);
        // And an animation that never had one does not run
        QPropertyAnimation none;
        CHECK(none.targetObject() == nullptr && none.propertyName().isEmpty());
        none.setStartValue(0);
        none.setEndValue(1);
        none.start();
        CHECK(none.state() == A::Stopped);
    }
    {
        // In time, with a declared property and a rectangle
        QTimer timer;
        QPropertyAnimation interval(&timer, "interval");
        interval.setDuration(50);
        interval.setStartValue(100);
        interval.setEndValue(900);
        interval.start();
        CHECK(timer.interval() == 100);
        CHECK(waitFor([&] { return interval.state() == A::Stopped; }) && timer.interval() == 900);

        target.setProperty("box", QRect(0, 0, 10, 10));
        auto *box = new QPropertyAnimation(&target, "box", &target);
        CHECK(box->parent() == &target);
        box->setDuration(40);
        box->setEndValue(QRect(100, 100, 50, 50));
        const QPointer<QPropertyAnimation> watch(box);
        box->start(A::DeleteWhenStopped);
        CHECK(waitFor([&] { return watch.isNull(); }) && target.property("box") == QVariant(QRect(100, 100, 50, 50)));
    }
    CHECK(std::string(animation.metaObject()->className()) == "QPropertyAnimation");
    CHECK(animation.property("propertyName").toByteArray() == "height");
}

static void testPauseAnimation() {
    QPauseAnimation pause;
    CHECK(pause.duration() == 250 && pause.totalDuration() == 250);
    pause.setDuration(40);
    pause.setDuration(-1); // ignored
    CHECK(pause.duration() == 40);
    QObject parent;
    auto *owned = new QPauseAnimation(70, &parent);
    CHECK(owned->duration() == 70 && owned->parent() == &parent && (new QPauseAnimation(&parent))->parent() == &parent);
    QElapsedTimer timer;
    timer.start();
    Signals seen(&pause);
    pause.start();
    CHECK(waitFor([&] { return pause.state() == A::Stopped; }) && seen.finished == 1 && timer.elapsed() >= 35);
    CHECK(std::string(pause.metaObject()->className()) == "QPauseAnimation" &&
          pause.property("duration").toInt() == 40);
}

static void testGroupMembers() {
    auto *group = new QParallelAnimationGroup;
    CHECK(group->animationCount() == 0 && group->duration() == 0 && group->animationAt(0) == nullptr);
    auto *a = new Probe(100), *b = new Probe(200), *c = new Probe(300);
    group->addAnimation(a);
    group->addAnimation(c);
    group->insertAnimation(1, b);
    CHECK(group->animationCount() == 3 && group->animationAt(0) == a && group->animationAt(1) == b &&
          group->animationAt(2) == c);
    CHECK(group->indexOfAnimation(b) == 1 && a->group() == group && a->parent() == group && group->duration() == 300);
    group->insertAnimation(7, new Probe(1, group)); // out of range
    group->insertAnimation(-1, a);
    group->addAnimation(nullptr);
    group->addAnimation(group);
    CHECK(group->animationCount() == 3);
    // An animation moves inside its group
    group->insertAnimation(3, a);
    CHECK(group->animationCount() == 3 && group->animationAt(0) == b && group->animationAt(2) == a);
    group->insertAnimation(0, a);
    CHECK(group->animationAt(0) == a && group->animationAt(1) == b);

    // Taken out, an animation is the caller's
    group->removeAnimation(b);
    CHECK(group->animationCount() == 2 && b->group() == nullptr && b->parent() == nullptr &&
          group->indexOfAnimation(b) == -1);
    group->removeAnimation(b); // not in the group
    group->removeAnimation(nullptr);
    CHECK(group->takeAnimation(5) == nullptr && group->takeAnimation(-1) == nullptr);
    CHECK(group->takeAnimation(1) == c && c->group() == nullptr && group->animationCount() == 1);

    // From one group to another
    QSequentialAnimationGroup sequence;
    sequence.addAnimation(b);
    sequence.addAnimation(a);
    CHECK(group->animationCount() == 0 && a->group() == &sequence && a->parent() == &sequence &&
          sequence.animationCount() == 2);
    // An animation that runs on its own is stopped when a group takes it
    c->start();
    sequence.addAnimation(c);
    CHECK(c->state() == A::Stopped && sequence.duration() == 600);
    // A deleted animation leaves its group
    delete b;
    CHECK(sequence.animationCount() == 2 && sequence.animationAt(0) == a && sequence.duration() == 400);
    // A group in a group
    sequence.addAnimation(group);
    CHECK(group->group() == &sequence && sequence.animationCount() == 3);
    group->addAnimation(new Probe(50));
    CHECK(sequence.duration() == 450);

    // clear() deletes; so does the group when it goes
    const QPointer<Probe> watchA(a);
    const QPointer<QParallelAnimationGroup> watchGroup(group);
    sequence.clear();
    CHECK(sequence.animationCount() == 0 && watchA.isNull() && watchGroup.isNull() && sequence.duration() == 0);
    auto *outer = new QSequentialAnimationGroup;
    const QPointer<Probe> inner(new Probe(10));
    outer->addAnimation(inner.data());
    delete outer;
    CHECK(inner.isNull());
    CHECK(std::string(sequence.metaObject()->className()) == "QSequentialAnimationGroup");
    CHECK(sequence.metaObject()->superClass() &&
          std::string(sequence.metaObject()->superClass()->className()) == "QAnimationGroup");
}

static void testParallelGroup() {
    QObject target;
    QParallelAnimationGroup group;
    Signals seen(&group);
    QPropertyAnimation *a = animate(&target, "a", 0, 100, 100), *b = animate(&target, "b", 0, 100, 200);
    group.addAnimation(a);
    group.addAnimation(b);
    Signals seenA(a), seenB(b);
    CHECK(group.duration() == 200 && group.totalDuration() == 200);

    // All start with the group, and each ends in its own time
    group.start();
    CHECK(group.state() == A::Running && a->state() == A::Running && b->state() == A::Running);
    CHECK(target.property("a") == QVariant(0) && target.property("b") == QVariant(0));
    group.setCurrentTime(50);
    CHECK(target.property("a") == QVariant(50) && target.property("b") == QVariant(25));
    group.setCurrentTime(150);
    CHECK(target.property("a") == QVariant(100) && target.property("b") == QVariant(75));
    CHECK(a->state() == A::Stopped && seenA.finished == 1 && b->state() == A::Running && group.state() == A::Running);
    group.setCurrentTime(160);
    CHECK(a->state() == A::Stopped && seenA.finished == 1); // stays ended
    // Back in time, the shorter one runs again
    group.setCurrentTime(40);
    CHECK(a->state() == A::Running && target.property("a") == QVariant(40) && target.property("b") == QVariant(20));
    group.setCurrentTime(200);
    CHECK(group.state() == A::Stopped && seen.finished == 1 && seenB.finished == 1 && seenA.finished == 2);
    CHECK(target.property("a") == QVariant(100) && target.property("b") == QVariant(100));

    // Pause, resume and stop go to the animations
    group.start();
    group.setCurrentTime(50);
    group.pause();
    CHECK(group.state() == A::Paused && a->state() == A::Paused && b->state() == A::Paused);
    group.resume();
    CHECK(a->state() == A::Running && b->state() == A::Running);
    group.setCurrentTime(120);
    group.pause();
    CHECK(a->state() == A::Stopped && b->state() == A::Paused);
    group.resume();
    CHECK(a->state() == A::Stopped && b->state() == A::Running);
    group.stop();
    CHECK(a->state() == A::Stopped && b->state() == A::Stopped && seen.finished == 1);
    // The animations of a group are the group's to run
    CHECK(group.currentTime() == 120);

    // Loops: each loop starts all again
    group.setLoopCount(2);
    CHECK(group.totalDuration() == 400);
    group.start();
    group.setCurrentTime(150);
    CHECK(a->state() == A::Stopped);
    group.setCurrentTime(250);
    CHECK(group.currentLoop() == 1 && seen.loops.back() == 1 && a->state() == A::Running);
    CHECK(target.property("a") == QVariant(50) && target.property("b") == QVariant(25));
    group.setCurrentTime(400);
    CHECK(group.state() == A::Stopped && seen.finished == 2 && target.property("b") == QVariant(100));
    group.setLoopCount(1);

    // Backwards
    group.setDirection(A::Backward);
    group.start();
    CHECK(group.currentTime() == 200 && a->direction() == A::Backward && b->direction() == A::Backward);
    group.setCurrentTime(150);
    CHECK(target.property("a") == QVariant(100) && target.property("b") == QVariant(75));
    group.setCurrentTime(50);
    CHECK(target.property("a") == QVariant(50) && target.property("b") == QVariant(25));
    group.setCurrentTime(0);
    CHECK(group.state() == A::Stopped && target.property("a") == QVariant(0) && target.property("b") == QVariant(0));
    CHECK(a->state() == A::Stopped && b->state() == A::Stopped);
    group.setDirection(A::Forward);

    {
        // In time
        seen.finished = 0;
        QElapsedTimer timer;
        timer.start();
        a->setDuration(30);
        b->setDuration(70);
        group.start();
        CHECK(waitFor([&] { return group.state() == A::Stopped; }) && seen.finished == 1 && timer.elapsed() >= 60);
        CHECK(target.property("a") == QVariant(100) && target.property("b") == QVariant(100));
    }
    {
        // An empty group is over at once
        QParallelAnimationGroup empty;
        Signals none(&empty);
        empty.start();
        CHECK(empty.state() == A::Stopped && none.finished == 1);
    }
    {
        // With an animation that has no end of its own the group ends when all have ended
        QParallelAnimationGroup open;
        Signals openSeen(&open);
        auto *endless = new Probe(-1);
        auto *fixed = new Probe(100);
        open.addAnimation(endless);
        open.addAnimation(fixed);
        CHECK(open.duration() == -1);
        open.start();
        open.setCurrentTime(500);
        CHECK(open.state() == A::Running && fixed->state() == A::Stopped && endless->state() == A::Running);
        CHECK(endless->currentTime() == 500);
        endless->stop();
        CHECK(open.state() == A::Stopped && openSeen.finished == 1);
    }
    {
        // Taking the last animation out of a running group stops it
        QParallelAnimationGroup shrinking;
        auto *only = new Probe(100);
        shrinking.addAnimation(only);
        shrinking.start();
        delete only;
        CHECK(shrinking.state() == A::Stopped && shrinking.animationCount() == 0);
    }
}

static void testSequentialGroup() {
    QObject target;
    QSequentialAnimationGroup group;
    Signals seen(&group);
    std::vector<QAbstractAnimation *> currents;
    QObject::connect(&group, &QSequentialAnimationGroup::currentAnimationChanged,
                     [&](QAbstractAnimation *current) { currents.push_back(current); });
    CHECK(group.currentAnimation() == nullptr && group.duration() == 0);
    QPropertyAnimation *a = animate(&target, "a", 0, 100, 100);
    group.addAnimation(a);
    CHECK(group.currentAnimation() == a && currents == std::vector<QAbstractAnimation *>({a}));
    QPauseAnimation *pause = group.addPause(50);
    QPropertyAnimation *c = animate(&target, "c", 0, 100, 100);
    group.addAnimation(c);
    CHECK(pause->duration() == 50 && group.animationAt(1) == pause && group.duration() == 250);
    CHECK(group.insertPause(9, 10) == nullptr && group.animationCount() == 3);
    Signals seenA(a), seenC(c);
    target.setProperty("a", -1);
    target.setProperty("c", -1);

    // One after another
    group.start();
    CHECK(group.state() == A::Running && a->state() == A::Running && pause->state() == A::Stopped &&
          c->state() == A::Stopped);
    CHECK(group.currentAnimation() == a && target.property("a") == QVariant(0) && target.property("c") == QVariant(-1));
    group.setCurrentTime(50);
    CHECK(target.property("a") == QVariant(50));
    group.setCurrentTime(100); // the boundary is the start of the next
    CHECK(a->state() == A::Stopped && seenA.finished == 1 && target.property("a") == QVariant(100));
    CHECK(group.currentAnimation() == pause && pause->state() == A::Running && currents.back() == pause);
    group.setCurrentTime(149);
    CHECK(group.currentAnimation() == pause && c->state() == A::Stopped);
    group.setCurrentTime(200);
    CHECK(group.currentAnimation() == c && c->state() == A::Running && pause->state() == A::Stopped);
    CHECK(target.property("c") == QVariant(50) && currents.size() == 3);
    group.setCurrentTime(250);
    CHECK(group.state() == A::Stopped && seen.finished == 1 && seenC.finished == 1 &&
          target.property("c") == QVariant(100));
    CHECK(group.currentAnimation() == c);

    // A step over several animations plays out what it passes
    target.setProperty("a", -1);
    target.setProperty("c", -1);
    group.start();
    CHECK(group.currentAnimation() == a && target.property("a") == QVariant(0));
    group.setCurrentTime(220);
    CHECK(target.property("a") == QVariant(100) && target.property("c") == QVariant(70) &&
          group.currentAnimation() == c);
    CHECK(a->state() == A::Stopped && pause->state() == A::Stopped && c->state() == A::Running);
    // And back
    group.setCurrentTime(30);
    CHECK(group.currentAnimation() == a && a->state() == A::Running && c->state() == A::Stopped);
    CHECK(target.property("a") == QVariant(30) && target.property("c") == QVariant(0));

    // Pause, resume and stop go to the current animation
    group.pause();
    CHECK(group.state() == A::Paused && a->state() == A::Paused);
    group.setCurrentTime(120); // moved by hand while paused: the next one is held too
    CHECK(group.currentAnimation() == pause && pause->state() == A::Paused && a->state() == A::Stopped);
    group.resume();
    CHECK(pause->state() == A::Running);
    group.stop();
    CHECK(pause->state() == A::Stopped && group.state() == A::Stopped && seen.finished == 1);

    // Loops
    group.setLoopCount(2);
    CHECK(group.totalDuration() == 500);
    target.setProperty("c", -1);
    group.start();
    group.setCurrentTime(60);
    group.setCurrentTime(300); // into the second loop: the first is played out
    CHECK(group.currentLoop() == 1 && group.currentAnimation() == a && target.property("a") == QVariant(50));
    CHECK(target.property("c") == QVariant(100) && a->state() == A::Running);
    group.setCurrentTime(500);
    CHECK(group.state() == A::Stopped && seen.finished == 2 && target.property("c") == QVariant(100));
    group.setLoopCount(1);

    // Backwards: from the last to the first
    group.setDirection(A::Backward);
    group.start();
    CHECK(group.currentTime() == 250 && group.currentAnimation() == c && c->state() == A::Running);
    CHECK(c->direction() == A::Backward && target.property("c") == QVariant(100));
    group.setCurrentTime(200);
    CHECK(target.property("c") == QVariant(50));
    group.setCurrentTime(150); // the boundary is the end of the earlier one... which is the pause
    CHECK(group.currentAnimation() == pause && target.property("c") == QVariant(0) && c->state() == A::Stopped);
    group.setCurrentTime(70);
    CHECK(group.currentAnimation() == a && target.property("a") == QVariant(70) && a->direction() == A::Backward);
    group.setCurrentTime(0);
    CHECK(group.state() == A::Stopped && target.property("a") == QVariant(0) && seen.finished == 3);
    group.setDirection(A::Forward);

    {
        // In time
        seen.finished = 0;
        a->setDuration(30);
        pause->setDuration(50); // longer than a step of the timer, so that it is not passed over
        c->setDuration(30);
        QElapsedTimer timer;
        timer.start();
        currents.clear();
        group.start();
        CHECK(waitFor([&] { return group.state() == A::Stopped; }) && seen.finished == 1 && timer.elapsed() >= 100);
        CHECK(target.property("a") == QVariant(100) && target.property("c") == QVariant(100));
        // The first animation was the current one already, from the run backwards
        CHECK(currents == std::vector<QAbstractAnimation *>({pause, c}));
    }
    {
        // The current animation is taken out while the group runs
        a->setDuration(100);
        pause->setDuration(50);
        c->setDuration(100);
        group.start();
        group.setCurrentTime(120);
        CHECK(group.currentAnimation() == pause);
        delete pause;
        CHECK(group.animationCount() == 2 && group.duration() == 200 && group.currentAnimation() == c);
        CHECK(group.state() == A::Running && c->state() == A::Running);
        group.stop();
        // An animation put in front keeps the current one current
        group.setCurrentTime(0);
        CHECK(group.currentAnimation() == a);
        QPauseAnimation *front = group.insertPause(0, 10);
        CHECK(group.animationAt(0) == front && group.currentAnimation() == a && group.duration() == 210);
    }
    {
        // An animation without an end of its own: the group waits for it and then knows how long it took
        QSequentialAnimationGroup open;
        Signals openSeen(&open);
        auto *first = new Probe(100), *endless = new Probe(-1), *last = new Probe(100);
        open.addAnimation(first);
        open.addAnimation(endless);
        open.addAnimation(last);
        CHECK(open.duration() == -1);
        open.start();
        open.setCurrentTime(160);
        CHECK(open.currentAnimation() == endless && endless->state() == A::Running && endless->currentTime() == 60);
        open.setCurrentTime(1000);
        CHECK(open.currentAnimation() == endless && open.state() == A::Running && endless->currentTime() == 900);
        endless->stop();
        CHECK(open.duration() == 1100 && open.currentAnimation() == last && last->state() == A::Running);
        CHECK(open.state() == A::Running && last->currentTime() == 0);
        open.setCurrentTime(1050);
        CHECK(last->currentTime() == 50);
        open.setCurrentTime(1100);
        CHECK(open.state() == A::Stopped && openSeen.finished == 1);
        // The last one has no end: the group ends with it
        QSequentialAnimationGroup tail;
        Signals tailSeen(&tail);
        auto *lead = new Probe(50), *open2 = new Probe(-1);
        tail.addAnimation(lead);
        tail.addAnimation(open2);
        tail.start();
        tail.setCurrentTime(80);
        CHECK(open2->state() == A::Running && tail.state() == A::Running);
        open2->stop();
        CHECK(tail.state() == A::Stopped && tailSeen.finished == 1 && tail.duration() == 80);
    }
    {
        // Groups in groups
        QObject object;
        QSequentialAnimationGroup outer;
        auto *both = new QParallelAnimationGroup;
        both->addAnimation(animate(&object, "x", 0, 10, 100));
        both->addAnimation(animate(&object, "y", 0, 20, 50));
        outer.addAnimation(animate(&object, "w", 0, 5, 50));
        outer.addAnimation(both);
        outer.addAnimation(animate(&object, "z", 100, 0, 50));
        CHECK(outer.duration() == 200);
        outer.start();
        outer.setCurrentTime(100);
        CHECK(object.property("w") == QVariant(5) && object.property("x") == QVariant(5) &&
              object.property("y") == QVariant(20));
        CHECK(both->state() == A::Running && outer.currentAnimation() == both);
        outer.setCurrentTime(175);
        CHECK(both->state() == A::Stopped && object.property("x") == QVariant(10) &&
              object.property("z") == QVariant(50));
        outer.stop();
        Signals outerSeen(&outer);
        outer.start();
        CHECK(waitFor([&] { return outer.state() == A::Stopped; }) && outerSeen.finished == 1);
        CHECK(object.property("z") == QVariant(0) && object.property("x") == QVariant(10) &&
              object.property("y") == QVariant(20));
    }
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testAbstractAnimation();
    testRealTime();
    testVariantAnimation();
    testPropertyAnimation();
    testPauseAnimation();
    testGroupMembers();
    testParallelGroup();
    testSequentialGroup();

    if (g_failures) {
        std::printf("%d animation test check(s) failed\n", g_failures);
        return 1;
    }
    std::puts("all animation tests passed");
    return 0;
}
