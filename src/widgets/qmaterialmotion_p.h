// MIT License - see LICENSE
#pragma once

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qhash.h>
#include <QtCore/qpointer.h>
#include <QtCore/qtimer.h>
#include <QtWidgets/qwidget.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace QtPrivate {

// The stiffness and damping ratio of a spring that moves a mass of one, with time in seconds.
struct SpringSpec {
    double damping;
    double stiffness;
};

// The springs of the expressive motion scheme of Material 3: spatial ones move things and may pass their mark, effects
// ones change colours and the like and do not.
constexpr SpringSpec fastSpatialSpring{0.6, 800};
constexpr SpringSpec defaultSpatialSpring{0.8, 380};
constexpr SpringSpec slowSpatialSpring{0.8, 200};
constexpr SpringSpec fastEffectsSpring{1.0, 3800};
constexpr SpringSpec defaultEffectsSpring{1.0, 1600};
constexpr SpringSpec slowEffectsSpring{1.0, 800};

// A damped harmonic oscillator on its way from a position and a velocity to a target, worked out in closed form.
class Spring {
public:
    struct State {
        double position;
        double velocity;
    };

    Spring() = default;
    Spring(double position, double velocity, double target, SpringSpec spec)
        : m_start(position - target), m_velocity(velocity), m_target(target), m_spec(spec) {}

    double target() const { return m_target; }

    State at(double seconds) const {
        const double omega = std::sqrt(m_spec.stiffness);
        const double zeta = std::min(m_spec.damping, 1.0);
        const double decay = std::exp(-zeta * omega * seconds);
        if (zeta >= 1.0) { // critically damped: it does not pass the target from rest
            const double b = m_velocity + omega * m_start;
            const double x = (m_start + b * seconds) * decay;
            const double v = (b - omega * (m_start + b * seconds)) * decay;
            return {m_target + x, v};
        }
        const double damped = omega * std::sqrt(1.0 - zeta * zeta);
        const double b = (m_velocity + zeta * omega * m_start) / damped;
        const double cosine = std::cos(damped * seconds), sine = std::sin(damped * seconds);
        const double x = decay * (m_start * cosine + b * sine);
        const double v =
            decay * ((b * damped - zeta * omega * m_start) * cosine - (m_start * damped + zeta * omega * b) * sine);
        return {m_target + x, v};
    }

    // Whether the spring is at its target and still, to within the precision.
    bool restsAt(double seconds, double precision) const {
        const State state = at(seconds);
        return std::abs(state.position - m_target) < precision && std::abs(state.velocity) < precision * 20;
    }

private:
    double m_start = 0; // the distance from the target at the start
    double m_velocity = 0;
    double m_target = 0;
    SpringSpec m_spec = defaultSpatialSpring;
};

// The springs of the widgets that a style draws. A style asks, as it paints, where a value of a widget is on its way
// to a target; the widget is repainted until every one of its springs rests.
class Animator : public QObject {
public:
    Animator() {
        m_clock.start();
        m_timer.setInterval(16);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this] { tick(); });
    }

    // The value of `channel` of `widget` now. The first time a value is asked for it is at the target; when the target
    // changes, the spring sets off from where the value is, with the speed that it has.
    double value(const QWidget *widget, int channel, double target, SpringSpec spec, double precision) {
        const Key key{widget, channel};
        const double now = static_cast<double>(m_clock.nsecsElapsed()) * 1e-9;
        auto it = m_motions.find(key);
        if (it == m_motions.end() || it->widget.isNull()) {
            // A widget that took the place of a deleted one at the same address starts afresh.
            Motion motion;
            motion.widget = const_cast<QWidget *>(widget); // NOLINT(cppcoreguidelines-pro-type-const-cast)
            motion.spring = Spring(target, 0, target, spec);
            motion.precision = precision;
            motion.start = now;
            m_motions.insert(key, motion);
            return target;
        }
        if (it->spring.target() != target) {
            const Spring::State state =
                it->moving ? it->spring.at(now - it->start) : Spring::State{it->spring.target(), 0};
            it->spring = Spring(state.position, state.velocity, target, spec);
            it->start = now;
            it->moving = true;
            if (!m_timer.isActive())
                m_timer.start();
        }
        return it->moving ? it->spring.at(now - it->start).position : target;
    }

    // Marks a value that sizes its widget: each frame of its spring asks for the widget's new size as well as a
    // repaint, which a widget with no height left (a toolbar that scrolled away) would never get.
    void resizes(const QWidget *widget, int channel) {
        if (auto it = m_motions.find(Key{widget, channel}); it != m_motions.end())
            it->sizes = true;
    }

    // The target of `channel` of `widget`, or `fallback` if nothing has been asked of it yet.
    double target(const QWidget *widget, int channel, double fallback) const {
        const auto it = m_motions.constFind(Key{widget, channel});
        return it == m_motions.cend() || it->widget.isNull() ? fallback : it->spring.target();
    }

    // Puts a value somewhere, at rest, for the next spring to set off from.
    void place(const QWidget *widget, int channel, double position) {
        Motion motion;
        motion.widget = const_cast<QWidget *>(widget); // NOLINT(cppcoreguidelines-pro-type-const-cast)
        motion.spring = Spring(position, 0, position, defaultEffectsSpring);
        motion.start = static_cast<double>(m_clock.nsecsElapsed()) * 1e-9;
        m_motions.insert(Key{widget, channel}, motion);
    }

    // The time that things which never come to rest are drawn at, in seconds.
    double seconds() const { return static_cast<double>(m_clock.nsecsElapsed()) * 1e-9; }

    // Repaints `widget` once more, soon. What never rests asks each time it is painted, so that the repainting ends
    // with the first time that it is not: when the widget is hidden, or has nothing left to show moving.
    void keepPainting(const QWidget *widget) {
        auto *asked = const_cast<QWidget *>(widget); // NOLINT(cppcoreguidelines-pro-type-const-cast)
        for (const QPointer<QWidget> &other : m_painting) {
            if (other.data() == asked)
                return;
        }
        m_painting.push_back(asked);
        if (!m_timer.isActive())
            m_timer.start();
    }

    bool isActive() const { return m_timer.isActive(); }
    void clear() {
        m_motions.clear();
        m_painting.clear();
        m_timer.stop();
    }

private:
    struct Key {
        const QWidget *widget;
        int channel;
        friend bool operator==(const Key &a, const Key &b) { return a.widget == b.widget && a.channel == b.channel; }
        friend size_t qHash(const Key &key, size_t seed = 0) {
            return ::qHash(reinterpret_cast<quintptr>(key.widget), seed) ^ static_cast<size_t>(key.channel);
        }
    };
    struct Motion {
        QPointer<QWidget> widget;
        Spring spring;
        double precision = 0.01;
        double start = 0;
        bool moving = false;
        bool sizes = false; // a frame asks for a new size too
    };

    void tick() {
        const double now = static_cast<double>(m_clock.nsecsElapsed()) * 1e-9;
        std::vector<QPointer<QWidget>> painting;
        painting.swap(m_painting); // each asks again as it is painted
        for (const QPointer<QWidget> &widget : painting) {
            if (widget)
                widget->update();
        }
        bool any = !painting.empty();
        for (auto it = m_motions.begin(); it != m_motions.end();) {
            if (it->widget.isNull()) {
                it = m_motions.erase(it);
                continue;
            }
            if (it->moving) {
                // One more repaint when it comes to rest, to show the target itself.
                if (it->spring.restsAt(now - it->start, it->precision))
                    it->moving = false;
                else
                    any = true;
                it->widget->update();
                if (it->sizes)
                    it->widget->updateGeometry();
            }
            ++it;
        }
        if (!any)
            m_timer.stop();
    }

    QHash<Key, Motion> m_motions;
    std::vector<QPointer<QWidget>> m_painting;
    QElapsedTimer m_clock;
    QTimer m_timer;
};

// Moves something along a spring from 0 to 1, a frame at a time: `step` is given where it is, and 1 when it rests, and
// returns false to stop it before then. It belongs to `owner`, and goes when it ends or with the owner; a new one of the
// same name takes the place of one that is still going.
class Transition : public QObject {
public:
    Transition(QObject *owner, const QString &name, SpringSpec spec, std::function<bool(double)> step)
        : QObject(owner), m_spring(0, 0, 1, spec), m_step(std::move(step)) {
        stop(owner, name);
        setObjectName(name);
        m_clock.start();
        m_timer.setInterval(16);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this] { tick(); });
        m_timer.start();
    }

    // Stops the transition of `owner` called `name` where it is, if there is one.
    static void stop(QObject *owner, const QString &name) {
        const QList<Transition *> going = owner->findChildren<Transition *>(name, Qt::FindDirectChildrenOnly);
        for (Transition *transition : going)
            transition->finish();
    }

private:
    void tick() {
        if (m_done)
            return;
        const double seconds = static_cast<double>(m_clock.nsecsElapsed()) * 1e-9;
        const bool rests = m_spring.restsAt(seconds, 0.002);
        if (!m_step(rests ? 1.0 : m_spring.at(seconds).position) || rests)
            finish();
    }
    void finish() {
        if (m_done)
            return;
        m_done = true;
        m_timer.stop();
        setObjectName(QString());
        deleteLater();
    }

    Spring m_spring;
    std::function<bool(double)> m_step;
    QElapsedTimer m_clock;
    QTimer m_timer;
    bool m_done = false;
};

} // namespace QtPrivate
