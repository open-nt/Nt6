// MIT License - see LICENSE
#include <QtCore/qpropertyanimation.h>

#include <QtCore/qanimationgroup.h>
#include <QtCore/qlogging.h>

#include <map>
#include <mutex>
#include <utility>

/*!
    \class QPropertyAnimation
    \brief The QPropertyAnimation class animates Qt properties.
    \since 4.6

    A QPropertyAnimation changes a property of an object over time: at each moment it sets the
    property to the value that QVariantAnimation works out between the start and the end value.

    \code
    auto *animation = new QPropertyAnimation(button, "geometry");
    animation->setDuration(400);
    animation->setEndValue(QRect(200, 50, 120, 30));
    animation->setEasingCurve(QEasingCurve::InOutQuad);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
    \endcode

    Without a start value the animation starts from the value that the property has when the
    animation starts. The property must have a setter, declared with \c Q_PROPERTY; dynamic
    properties work as well.

    Only one animation changes a property of an object at a time: starting a second one stops
    the first.

    \differences
    The properties are not bindable.
    \enddifferences

    \sa QVariantAnimation, QAnimationGroup
*/

namespace {

using Key = std::pair<const QObject *, QByteArray>;

std::mutex &runningMutex() {
    static std::mutex mutex;
    return mutex;
}
// The animation that is changing each property of each object.
std::map<Key, QPropertyAnimation *> &runningAnimations() {
    static std::map<Key, QPropertyAnimation *> running;
    return running;
}

} // namespace

/*!
    Constructs a property animation with the given \a parent and no target.

    \sa setTargetObject(), setPropertyName()
*/
QPropertyAnimation::QPropertyAnimation(QObject *parent) : QVariantAnimation(parent) {}

/*!
    Constructs a property animation with the given \a parent that changes the property
    \a propertyName of \a target.
*/
QPropertyAnimation::QPropertyAnimation(QObject *target, const QByteArray &propertyName, QObject *parent)
    : QVariantAnimation(parent), m_target(target), m_propertyName(propertyName) {}

/*!
    Destroys the animation, after stopping it.
*/
QPropertyAnimation::~QPropertyAnimation() {
    stop();
    // stop() does not reach the subclass any more, so the bookkeeping is undone here.
    const std::lock_guard<std::mutex> lock(runningMutex());
    auto &running = runningAnimations();
    for (auto it = running.begin(); it != running.end();) {
        if (it->second == this)
            it = running.erase(it);
        else
            ++it;
    }
}

/*!
    Returns the object whose property the animation changes, or \c nullptr if there is none or
    it has been deleted.

    \sa setTargetObject()
*/
QObject *QPropertyAnimation::targetObject() const { return m_target.data(); }

/*!
    Sets the object whose property the animation changes to \a target. The target of an
    animation that runs cannot be changed.

    \sa targetObject()
*/
void QPropertyAnimation::setTargetObject(QObject *target) {
    if (state() != Stopped) {
        qWarning("QPropertyAnimation::setTargetObject: you can't change the target of a running animation");
        return;
    }
    m_target = target;
}

/*!
    Returns the name of the property that the animation changes.

    \sa setPropertyName()
*/
QByteArray QPropertyAnimation::propertyName() const { return m_propertyName; }

/*!
    Sets the name of the property that the animation changes to \a propertyName. The property
    of an animation that runs cannot be changed.

    \sa propertyName()
*/
void QPropertyAnimation::setPropertyName(const QByteArray &propertyName) {
    if (state() != Stopped) {
        qWarning("QPropertyAnimation::setPropertyName: you can't change the property name of a running animation");
        return;
    }
    m_propertyName = propertyName;
}

/*!
    \reimp
*/
bool QPropertyAnimation::event(QEvent *event) { return QVariantAnimation::event(event); }

/*!
    \reimp
    Sets the property of the target to \a value. A stopped animation changes nothing, and an
    animation whose target has been deleted stops.
*/
void QPropertyAnimation::updateCurrentValue(const QVariant &value) {
    if (state() == Stopped)
        return;
    if (!m_target) {
        stop();
        return;
    }
    m_target->setProperty(m_propertyName.constData(), value);
}

/*!
    \reimp
    When the animation starts, takes the current value of the property as the start value if
    none was set, and stops another animation of the same property.
*/
void QPropertyAnimation::updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState) {
    if (!m_target && oldState == Stopped) {
        qWarning("QPropertyAnimation::updateState (%s): Changing state of an animation without target",
                 m_propertyName.constData());
        return;
    }
    QVariantAnimation::updateState(newState, oldState);

    QPropertyAnimation *toStop = nullptr;
    {
        const std::lock_guard<std::mutex> lock(runningMutex());
        auto &running = runningAnimations();
        const Key key(m_target.data(), m_propertyName);
        if (newState == Running) {
            QPropertyAnimation *&current = running[key];
            if (current && current != this)
                toStop = current;
            current = this;
        } else if (newState == Stopped) {
            const auto it = running.find(key);
            if (it != running.end() && it->second == this)
                running.erase(it);
        }
    }
    if (oldState == Stopped) {
        m_defaultStart = QVariant();
        if (!startValue().isValid() && m_target)
            m_defaultStart = m_target->property(m_propertyName.constData());
        if (!startValue().isValid() && !m_defaultStart.isValid())
            qWarning("QPropertyAnimation: you're trying to animate a non-existing property %s of your QObject",
                     m_propertyName.constData());
    }
    if (toStop) {
        // In a group the group is what runs; stopping the animation alone would leave the group out of step.
        QAbstractAnimation *top = toStop;
        while (top->group())
            top = top->group();
        top->stop();
    }
}

#include "moc_qpropertyanimation.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
