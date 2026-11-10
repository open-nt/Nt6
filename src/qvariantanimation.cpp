// MIT License - see LICENSE
#include <QtCore/qvariantanimation.h>

#include <QtCore/qline.h>
#include <QtCore/qlogging.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>

/*!
    \class QVariantAnimation
    \brief The QVariantAnimation class provides a base class for animations.
    \since 4.6

    A QVariantAnimation changes a value over its duration: from a start value to an end value,
    by way of any key values in between. As the current time moves, it works out the current
    value, emits valueChanged() and calls updateCurrentValue().

    \code
    QVariantAnimation animation;
    animation.setDuration(500);
    animation.setStartValue(0);
    animation.setKeyValueAt(0.8, 110);
    animation.setEndValue(100);
    animation.setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(&animation, &QVariantAnimation::valueChanged, [&](const QVariant &value) {
        bar.setValue(value.toInt());
    });
    animation.start();
    \endcode

    The values between two key values are interpolated. That works for \c int, \c{unsigned int},
    \c{long long}, \c{unsigned long long}, \c double, \c float, QPoint, QPointF, QSize, QSizeF,
    QLine, QLineF, QRect and QRectF; register a function for another type with
    \c qRegisterAnimationInterpolator(). A value of a type without one changes from the start
    value to the end value at the end. The easing curve shapes how the progress follows the time.

    \differences
    The properties are not bindable. A key value is a \c std::pair, as Nt has no \c QPair.
    \enddifferences

    \sa QPropertyAnimation, QEasingCurve
*/

namespace {

std::mutex &interpolatorMutex() {
    static std::mutex mutex;
    return mutex;
}
std::map<int, QVariantAnimation::Interpolator> &interpolators() {
    static std::map<int, QVariantAnimation::Interpolator> registered;
    return registered;
}

template <typename T> T mix(T from, T to, qreal progress) {
    const auto a = static_cast<qreal>(from), b = static_cast<qreal>(to);
    return static_cast<T>(a + (b - a) * progress);
}
int mixInt(int from, int to, qreal progress) { return from + qRound((to - from) * progress); }

bool builtInInterpolation(const QVariant &from, const QVariant &to, qreal progress, QVariant *out) {
    switch (from.typeId()) {
    case QMetaType::Int:
        *out = mixInt(from.toInt(), to.toInt(), progress);
        return true;
    case QMetaType::UInt:
        *out = QVariant::fromValue(static_cast<uint>(qRound64(mix<double>(from.toUInt(), to.toUInt(), progress))));
        return true;
    case QMetaType::LongLong:
        *out = QVariant::fromValue(static_cast<qlonglong>(std::llround(
            mix<double>(static_cast<double>(from.toLongLong()), static_cast<double>(to.toLongLong()), progress))));
        return true;
    case QMetaType::ULongLong:
        *out = QVariant::fromValue(static_cast<qulonglong>(std::llround(
            mix<double>(static_cast<double>(from.toULongLong()), static_cast<double>(to.toULongLong()), progress))));
        return true;
    case QMetaType::Double:
        *out = mix<double>(from.toDouble(), to.toDouble(), progress);
        return true;
    case QMetaType::Float:
        *out = mix<float>(from.toFloat(), to.toFloat(), progress);
        return true;
    case QMetaType::QPoint: {
        const QPoint a = from.toPoint(), b = to.toPoint();
        *out = QPoint(mixInt(a.x(), b.x(), progress), mixInt(a.y(), b.y(), progress));
        return true;
    }
    case QMetaType::QPointF: {
        const QPointF a = from.toPointF(), b = to.toPointF();
        *out = QPointF(mix(a.x(), b.x(), progress), mix(a.y(), b.y(), progress));
        return true;
    }
    case QMetaType::QSize: {
        const QSize a = from.toSize(), b = to.toSize();
        *out = QSize(mixInt(a.width(), b.width(), progress), mixInt(a.height(), b.height(), progress));
        return true;
    }
    case QMetaType::QSizeF: {
        const QSizeF a = from.toSizeF(), b = to.toSizeF();
        *out = QSizeF(mix(a.width(), b.width(), progress), mix(a.height(), b.height(), progress));
        return true;
    }
    case QMetaType::QLine: {
        const QLine a = from.toLine(), b = to.toLine();
        *out = QLine(mixInt(a.x1(), b.x1(), progress), mixInt(a.y1(), b.y1(), progress),
                     mixInt(a.x2(), b.x2(), progress), mixInt(a.y2(), b.y2(), progress));
        return true;
    }
    case QMetaType::QLineF: {
        const QLineF a = from.toLineF(), b = to.toLineF();
        *out = QLineF(mix(a.x1(), b.x1(), progress), mix(a.y1(), b.y1(), progress), mix(a.x2(), b.x2(), progress),
                      mix(a.y2(), b.y2(), progress));
        return true;
    }
    case QMetaType::QRect: {
        const QRect a = from.toRect(), b = to.toRect();
        *out = QRect(mixInt(a.x(), b.x(), progress), mixInt(a.y(), b.y(), progress),
                     mixInt(a.width(), b.width(), progress), mixInt(a.height(), b.height(), progress));
        return true;
    }
    case QMetaType::QRectF: {
        const QRectF a = from.toRectF(), b = to.toRectF();
        *out = QRectF(mix(a.x(), b.x(), progress), mix(a.y(), b.y(), progress), mix(a.width(), b.width(), progress),
                      mix(a.height(), b.height(), progress));
        return true;
    }
    default:
        return false;
    }
}

} // namespace

void QVariantAnimation::registerInterpolator(int typeId, Interpolator interpolator) {
    const std::lock_guard<std::mutex> lock(interpolatorMutex());
    if (interpolator)
        interpolators()[typeId] = std::move(interpolator);
    else
        interpolators().erase(typeId);
}

/*!
    Constructs an animation with the given \a parent, a duration of 250 milliseconds and a linear
    easing curve.
*/
QVariantAnimation::QVariantAnimation(QObject *parent) : QAbstractAnimation(parent) {}

/*!
    Destroys the animation.
*/
QVariantAnimation::~QVariantAnimation() = default;

/*!
    Returns the value that the animation starts with, which is the key value at step 0. It is
    invalid if none was set.

    \sa setStartValue(), endValue()
*/
QVariant QVariantAnimation::startValue() const { return keyValueAt(0); }

/*!
    Sets the value that the animation starts with to \a value.

    \sa startValue(), setEndValue()
*/
void QVariantAnimation::setStartValue(const QVariant &value) { setKeyValueAt(0, value); }

/*!
    Returns the value that the animation ends with, which is the key value at step 1. It is
    invalid if none was set.

    \sa setEndValue(), startValue()
*/
QVariant QVariantAnimation::endValue() const { return keyValueAt(1); }

/*!
    Sets the value that the animation ends with to \a value.

    \sa endValue(), setStartValue()
*/
void QVariantAnimation::setEndValue(const QVariant &value) { setKeyValueAt(1, value); }

/*!
    Returns the key value at \a step, or an invalid variant if there is none at exactly that
    step.

    \sa setKeyValueAt(), keyValues()
*/
QVariant QVariantAnimation::keyValueAt(qreal step) const {
    for (const KeyValue &key : m_keyValues) {
        if (key.first == step)
            return key.second;
    }
    return QVariant();
}

/*!
    Makes \a value the value of the animation at \a step, which is between 0 for the start and 1
    for the end. An invalid \a value removes the key value at that step.

    \sa keyValueAt(), setKeyValues()
*/
void QVariantAnimation::setKeyValueAt(qreal step, const QVariant &value) {
    if (step < 0 || step > 1) {
        qWarning("QVariantAnimation::setKeyValueAt: invalid step = %f", step);
        return;
    }
    auto it = std::lower_bound(m_keyValues.begin(), m_keyValues.end(), step,
                               [](const KeyValue &key, qreal wanted) { return key.first < wanted; });
    if (it != m_keyValues.end() && it->first == step) {
        if (value.isValid())
            it->second = value;
        else
            m_keyValues.erase(it);
    } else if (value.isValid()) {
        m_keyValues.insert(it, KeyValue(step, value));
    }
    recalculate(false);
}

/*!
    Returns the key values of the animation, ordered by their steps.

    \sa setKeyValues(), keyValueAt()
*/
QVariantAnimation::KeyValues QVariantAnimation::keyValues() const { return m_keyValues; }

/*!
    Replaces the key values of the animation by \a values, which need not be ordered.

    \sa keyValues(), setKeyValueAt()
*/
void QVariantAnimation::setKeyValues(const KeyValues &values) {
    m_keyValues = values;
    std::stable_sort(m_keyValues.begin(), m_keyValues.end(),
                     [](const KeyValue &a, const KeyValue &b) { return a.first < b.first; });
    recalculate(false);
}

/*!
    Returns the value that the animation has at its current time. It is invalid before the
    animation has a start and an end value and was updated for the first time.

    \sa valueChanged(), updateCurrentValue()
*/
QVariant QVariantAnimation::currentValue() const { return m_currentValue; }

/*!
    Returns the duration of one loop in milliseconds, which is 250 by default.

    \sa setDuration()
*/
int QVariantAnimation::duration() const { return m_duration; }

/*!
    Sets the duration of one loop to \a msecs milliseconds, which must not be negative.

    \sa duration()
*/
void QVariantAnimation::setDuration(int msecs) {
    if (msecs < 0) {
        qWarning("QVariantAnimation::setDuration: cannot set a negative duration");
        return;
    }
    if (m_duration == msecs)
        return;
    m_duration = msecs;
    recalculate(false);
}

/*!
    Returns the easing curve of the animation, which is linear by default.

    \sa setEasingCurve()
*/
QEasingCurve QVariantAnimation::easingCurve() const { return m_easing; }

/*!
    Sets the easing curve of the animation to \a easing. The curve maps the part of the duration
    that has passed to the progress between the start and the end value: an animation can start
    slowly, overshoot its end, or bounce.

    \sa easingCurve()
*/
void QVariantAnimation::setEasingCurve(const QEasingCurve &easing) {
    m_easing = easing;
    recalculate(false);
}

// Works out the value at the current time. `force` also reports a value that has not changed.
void QVariantAnimation::recalculate(bool force) {
    // With one of the two ends missing, what was there when the animation started stands in for the start.
    KeyValues keys = m_keyValues;
    if ((keys.isEmpty() || keys.first().first != 0) && m_defaultStart.isValid())
        keys.prepend(KeyValue(0, m_defaultStart));
    if (keys.size() < 2)
        return;

    const int single = duration();
    const qreal time = single == 0 ? 1.0 : static_cast<qreal>(currentLoopTime()) / single;
    const qreal progress = m_easing.valueForProgress(time);
    // The pair of key values around the progress; a curve that leaves 0..1 goes on along the outermost pair.
    qsizetype upper = 1;
    while (upper + 1 < keys.size() && keys.at(upper).first < progress)
        ++upper;
    const KeyValue &from = keys.at(upper - 1), &to = keys.at(upper);
    const qreal span = to.first - from.first;
    const qreal local = span > 0 ? (progress - from.first) / span : 1.0;

    const QVariant value = interpolated(from.second, to.second, local);
    const bool changed = m_currentValue != value;
    if (!changed && !force)
        return;
    m_currentValue = value;
    updateCurrentValue(m_currentValue);
    if (changed)
        valueChanged(m_currentValue);
}

/*!
    \reimp
*/
bool QVariantAnimation::event(QEvent *event) { return QAbstractAnimation::event(event); }

/*!
    \reimp
*/
void QVariantAnimation::updateCurrentTime(int currentTime) {
    Q_UNUSED(currentTime)
    recalculate(true);
}

/*!
    \reimp
*/
void QVariantAnimation::updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState) {
    Q_UNUSED(newState)
    Q_UNUSED(oldState)
}

/*!
    Called each time the animation has been updated, with its current value in \a value, also
    when the value is the same as before. The default implementation does nothing; connect to
    valueChanged() to be told of changes only.

    \sa currentValue()
*/
void QVariantAnimation::updateCurrentValue(const QVariant &value) { Q_UNUSED(value) }

/*!
    Returns the value that lies between \a from and \a to at \a progress, which is 0 at \a from
    and 1 at \a to and outside that range for an easing curve that overshoots. The default
    implementation uses the function that is registered for the type of the values, and for a
    type without one returns \a from until \a progress reaches 1 and \a to from then on.
*/
QVariant QVariantAnimation::interpolated(const QVariant &from, const QVariant &to, qreal progress) const {
    Interpolator custom;
    {
        const std::lock_guard<std::mutex> lock(interpolatorMutex());
        const auto it = interpolators().find(from.typeId());
        if (it != interpolators().end())
            custom = it->second;
    }
    if (custom)
        return custom(from, to, progress);
    QVariant result;
    if (from.typeId() == to.typeId() && builtInInterpolation(from, to, progress, &result))
        return result;
    // Numbers of different types meet as doubles.
    if (from.typeId() != to.typeId() && from.canConvert<double>() && to.canConvert<double>()) {
        bool fromOk = false, toOk = false;
        const double a = from.toDouble(&fromOk), b = to.toDouble(&toOk);
        if (fromOk && toOk)
            return mix<double>(a, b, progress);
    }
    return progress < 1.0 ? from : to;
}

#include "moc_qvariantanimation.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
