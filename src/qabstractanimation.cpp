// MIT License - see LICENSE
#include <QtCore/qabstractanimation.h>

#include <QtCore/qanimationgroup.h>
#include <QtCore/qcoreevent.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qlogging.h>
#include <QtCore/qpointer.h>

#include <algorithm>
#include <vector>

/*!
    \class QAbstractAnimation
    \brief The QAbstractAnimation class is the base of all animations.
    \since 4.6

    An animation is something that changes over a time: it has a duration(), a current time
    between 0 and the duration, and can be started, paused, resumed and stopped. While it runs,
    the event loop of its thread moves its current time forward about sixty times a second, and
    each time the animation is told with updateCurrentTime().

    The classes that are used most are QPropertyAnimation, which changes a property of an object,
    and the groups QSequentialAnimationGroup and QParallelAnimationGroup, which run animations
    one after another or together. To make an animation of another kind, subclass
    QAbstractAnimation and implement duration() and updateCurrentTime():

    \code
    class Blink : public QAbstractAnimation {
    public:
        int duration() const override { return 1000; }
    protected:
        void updateCurrentTime(int time) override { setLamp(time < 500); }
    };
    \endcode

    An animation can run several times, see setLoopCount(), and backwards, see setDirection().
    When it reaches its end it stops and emits finished().

    \differences
    The properties are not bindable. Time advances with a timer of the event loop of the thread
    that the animation is started in, and there is no \c QAnimationDriver to replace that timer.
    \enddifferences

    \sa QVariantAnimation, QPropertyAnimation, QAnimationGroup
*/

/*!
    \enum QAbstractAnimation::Direction

    The direction in which the current time moves while the animation runs.

    \value Forward The current time increases, from 0 to the end.
    \value Backward The current time decreases, from the end to 0.
*/

/*!
    \enum QAbstractAnimation::State

    The states of an animation.

    \value Stopped The animation does not run. Starting it begins at the start, or for the direction \c Backward at the end.
    \value Paused The animation is held where it is. resume() lets it go on from there.
    \value Running The animation runs: the event loop moves its current time.
*/

/*!
    \enum QAbstractAnimation::DeletionPolicy

    What start() arranges for the time the animation stops.

    \value KeepWhenStopped The animation stays, to be started again.
    \value DeleteWhenStopped The animation is deleted when it stops, with deleteLater().
*/

// Moves the current time of the animations of a thread that run and are in no group. It exists only while there
// are such animations.
class QtAnimationDriver : public QObject {
public:
    static QtAnimationDriver *&instance() {
        static thread_local QtAnimationDriver *driver = nullptr;
        return driver;
    }

    static void add(QAbstractAnimation *animation) {
        QtAnimationDriver *&driver = instance();
        if (!driver)
            driver = new QtAnimationDriver;
        else
            driver->advance(); // the time up to now is not the new animation's
        driver->m_animations.push_back(animation);
    }

    static void remove(QAbstractAnimation *animation) {
        QtAnimationDriver *&driver = instance();
        if (!driver)
            return;
        auto &list = driver->m_animations;
        // advance() walks a copy of the list and looks here for what is still in it.
        list.erase(std::remove(list.begin(), list.end(), animation), list.end());
        if (list.empty() && !driver->m_advancing) {
            delete driver;
            driver = nullptr;
        }
    }

    // Brings a running animation up to date before its time is read or its direction changes.
    static void update() {
        if (QtAnimationDriver *driver = instance())
            driver->advance();
    }

protected:
    void timerEvent(QTimerEvent *) override {
        advance();
        QtAnimationDriver *&driver = instance();
        if (driver == this && m_animations.empty()) {
            driver = nullptr;
            delete this;
        }
    }

private:
    QtAnimationDriver() {
        m_clock.start();
        m_timer = startTimer(16);
    }
    ~QtAnimationDriver() override { killTimer(m_timer); }

    void advance() {
        if (m_advancing)
            return;
        const qint64 now = m_clock.elapsed();
        const int delta = static_cast<int>(std::min<qint64>(now - m_last, 1000 * 1000));
        m_last = now;
        if (delta <= 0)
            return;
        m_advancing = true;
        const std::vector<QAbstractAnimation *> running = m_animations;
        for (QAbstractAnimation *animation : running) {
            // An animation may stop or delete another one as it is updated.
            if (std::find(m_animations.begin(), m_animations.end(), animation) == m_animations.end())
                continue;
            const int time = animation->m_totalCurrentTime;
            animation->setCurrentTime(animation->m_direction == QAbstractAnimation::Forward ? time + delta
                                                                                            : time - delta);
        }
        m_advancing = false;
    }

    std::vector<QAbstractAnimation *> m_animations;
    QElapsedTimer m_clock;
    qint64 m_last = 0;
    int m_timer = 0;
    bool m_advancing = false;
};

/*!
    Constructs an animation with the given \a parent. If the parent is a QAnimationGroup, add the
    animation to it with QAnimationGroup::addAnimation().
*/
QAbstractAnimation::QAbstractAnimation(QObject *parent) : QObject(parent) {}

/*!
    Destroys the animation, after stopping it and taking it out of its group.
*/
QAbstractAnimation::~QAbstractAnimation() {
    if (m_registered)
        QtAnimationDriver::remove(this);
    if (m_state != Stopped) {
        // The subclass is gone: only the signal tells of the stop.
        const State old = m_state;
        m_state = Stopped;
        stateChanged(Stopped, old);
    }
    if (m_group) {
        QAnimationGroup *group = m_group;
        m_group = nullptr;
        const qsizetype index = group->m_animations.indexOf(this);
        if (index >= 0) {
            group->m_animations.removeAt(index);
            group->animationsChanged(static_cast<int>(index), false);
        }
    }
}

/*!
    Returns the state of the animation.

    \sa start(), pause(), stop(), stateChanged()
*/
QAbstractAnimation::State QAbstractAnimation::state() const { return m_state; }

/*!
    Returns the group that the animation is in, or \c nullptr if it is in none. An animation in a
    group is started, paused and stopped by its group.

    \sa QAnimationGroup::addAnimation()
*/
QAnimationGroup *QAbstractAnimation::group() const { return m_group; }

/*!
    Returns the direction of the animation, which is \c Forward by default.

    \sa setDirection()
*/
QAbstractAnimation::Direction QAbstractAnimation::direction() const { return m_direction; }

/*!
    Sets the direction of the animation to \a direction. An animation that runs goes on from
    where it is in the new direction. A stopped animation with the direction \c Backward starts
    at its end; if it loops without end, it starts at the end of its first loop.

    \sa direction(), directionChanged()
*/
void QAbstractAnimation::setDirection(Direction direction) {
    if (m_direction == direction)
        return;
    if (m_state == Stopped) {
        if (direction == Backward) {
            m_currentTime = duration();
            m_currentLoop = m_loopCount - 1;
        } else {
            m_currentTime = 0;
            m_currentLoop = 0;
        }
    }
    // The time that has passed was in the old direction.
    if (m_registered)
        QtAnimationDriver::update();
    m_direction = direction;
    updateDirection(direction);
    directionChanged(direction);
}

/*!
    Returns the current time of the animation in milliseconds, counted over all its loops: from 0
    to totalDuration().

    \sa setCurrentTime(), currentLoopTime()
*/
int QAbstractAnimation::currentTime() const { return m_totalCurrentTime; }

/*!
    Returns the current time inside the current loop, from 0 to duration().

    \sa currentTime(), currentLoop()
*/
int QAbstractAnimation::currentLoopTime() const { return m_currentTime; }

/*!
    Returns the number of times the animation runs before it stops, which is 1 by default. -1
    means that it runs until it is stopped.

    \sa setLoopCount()
*/
int QAbstractAnimation::loopCount() const { return m_loopCount; }

/*!
    Sets the number of times the animation runs to \a loopCount. With 0 the animation does not
    run at all, and with -1 it runs until it is stopped.

    \sa loopCount(), currentLoop()
*/
void QAbstractAnimation::setLoopCount(int loopCount) { m_loopCount = loopCount; }

/*!
    Returns the number of the loop that the animation is in, starting with 0.

    \sa loopCount(), currentLoopChanged()
*/
int QAbstractAnimation::currentLoop() const { return m_currentLoop; }

/*!
    \fn int QAbstractAnimation::duration() const

    Returns the duration of one loop of the animation in milliseconds. -1 means that the
    duration is undefined: the animation runs until it stops itself or is stopped.

    \sa totalDuration(), loopCount()
*/

/*!
    Returns the duration of all loops together: duration() times loopCount(). It is -1 if the
    duration is undefined or the animation loops without end.

    \sa duration(), currentTime()
*/
int QAbstractAnimation::totalDuration() const {
    const int single = duration();
    if (single <= 0)
        return single;
    return m_loopCount < 0 ? -1 : single * m_loopCount;
}

/*!
    Starts the animation: the state becomes \c Running, and the event loop moves the current
    time from the start to the end. With the \a policy \c DeleteWhenStopped the animation is
    deleted when it stops. Starting an animation that runs does nothing; a paused one starts
    over from the beginning only after stop(), use resume() to go on.

    The animation needs the event loop of its thread, as every timer does.

    \sa stop(), pause(), finished()
*/
void QAbstractAnimation::start(DeletionPolicy policy) {
    if (m_state == Running)
        return;
    m_deleteWhenStopped = policy == DeleteWhenStopped;
    setState(Running);
}

/*!
    Pauses a running animation: the current time stays where it is until resume() or stop().

    \sa resume(), setPaused()
*/
void QAbstractAnimation::pause() {
    if (m_state == Stopped) {
        qWarning("QAbstractAnimation::pause: Cannot pause a stopped animation");
        return;
    }
    setState(Paused);
}

/*!
    Lets a paused animation go on from where it was paused.

    \sa pause(), setPaused()
*/
void QAbstractAnimation::resume() {
    if (m_state != Paused) {
        qWarning("QAbstractAnimation::resume: Cannot resume an animation that is not paused");
        return;
    }
    setState(Running);
}

/*!
    Pauses the animation if \a paused is \c true, and resumes it otherwise.

    \sa pause(), resume()
*/
void QAbstractAnimation::setPaused(bool paused) {
    if (paused)
        pause();
    else
        resume();
}

/*!
    Stops the animation. The current time stays where it is, and the next start() begins at the
    start again. finished() is not emitted, unless the animation has no end of its own.

    \sa start(), finished()
*/
void QAbstractAnimation::stop() {
    if (m_state == Stopped)
        return;
    setState(Stopped);
}

/*!
    Sets the current time of the animation to \a msecs, a time between 0 and totalDuration(),
    and updates the animation to that time. The event loop calls this for an animation that runs;
    a program can call it to move an animation by hand, also one that is paused or stopped. An
    animation that runs and is put at its end stops.

    \sa currentTime(), updateCurrentTime()
*/
void QAbstractAnimation::setCurrentTime(int msecs) {
    msecs = std::max(msecs, 0);
    const int single = duration();
    const int total = totalDuration();
    if (total != -1)
        msecs = std::min(total, msecs);
    m_totalCurrentTime = msecs;

    const int oldLoop = m_currentLoop;
    m_currentLoop = single <= 0 ? 0 : msecs / single;
    if (m_currentLoop == m_loopCount) {
        // At the very end: the last moment of the last loop, not the first of a loop that is not run.
        m_currentTime = std::max(0, single);
        m_currentLoop = std::max(0, m_loopCount - 1);
    } else if (m_direction == Forward) {
        m_currentTime = single <= 0 ? msecs : msecs % single;
    } else {
        // Backwards a loop is entered at its end, so the boundary belongs to the earlier loop.
        m_currentTime = single <= 0 ? msecs : (msecs - 1) % single + 1;
        if (m_currentTime == single)
            --m_currentLoop;
    }

    // What follows may delete the animation.
    const QPointer<QAbstractAnimation> alive(this);
    updateCurrentTime(m_currentTime);
    if (!alive)
        return;
    if (m_currentLoop != oldLoop)
        currentLoopChanged(m_currentLoop);
    if (!alive)
        return;
    if ((m_direction == Forward && m_totalCurrentTime == total) || (m_direction == Backward && m_totalCurrentTime == 0))
        stop();
}

void QAbstractAnimation::setState(State newState) {
    if (m_state == newState || m_loopCount == 0)
        return;
    const State oldState = m_state;
    const int oldTime = m_totalCurrentTime;
    const Direction oldDirection = m_direction;

    // A start from the stopped state is a start from the beginning, which backwards is the end.
    if (oldState == Stopped) {
        const int end = m_loopCount == -1 ? duration() : totalDuration();
        m_totalCurrentTime = m_currentTime = m_direction == Forward ? 0 : std::max(end, 0);
    }
    m_state = newState;

    // The driver moves the animations that run and are in no group; a group moves its own.
    const bool wanted = newState == Running && !m_group;
    if (wanted && !m_registered) {
        QtAnimationDriver::add(this);
        m_registered = true;
    } else if (!wanted && m_registered) {
        QtAnimationDriver::remove(this);
        m_registered = false;
    }

    const QPointer<QAbstractAnimation> alive(this);
    updateState(newState, oldState);
    if (!alive || m_state != newState)
        return;
    stateChanged(newState, oldState);
    if (!alive || m_state != newState)
        return;

    if (newState == Running && oldState == Stopped) {
        if (!m_group) {
            // Shows the animation at its first moment; a group does this for its animations.
            m_currentLoop = 0;
            setCurrentTime(m_totalCurrentTime);
        }
    } else if (newState == Stopped) {
        const int total = totalDuration();
        const bool atEnd = (oldDirection == Forward && oldTime == total) || (oldDirection == Backward && oldTime == 0);
        const bool endless = duration() == -1 || m_loopCount < 0;
        QAnimationGroup *group = m_group;
        if (m_deleteWhenStopped)
            deleteLater();
        if (atEnd || endless)
            finished();
        if (alive && group && m_group == group)
            group->animationStopped(this);
    }
}

/*!
    \fn void QAbstractAnimation::updateCurrentTime(int currentTime)

    Called when the current time of the animation has changed, with the time inside the current
    loop in \a currentTime. A subclass shows the animation as it is at that time.

    \sa setCurrentTime(), updateState()
*/

/*!
    Called when the state of the animation changes from \a oldState to \a newState, before
    stateChanged() is emitted. The default implementation does nothing.

    \sa start(), stop(), pause()
*/
void QAbstractAnimation::updateState(State newState, State oldState) {
    Q_UNUSED(newState)
    Q_UNUSED(oldState)
}

/*!
    Called when the direction of the animation changes to \a direction, before
    directionChanged() is emitted. The default implementation does nothing.

    \sa setDirection()
*/
void QAbstractAnimation::updateDirection(Direction direction) { Q_UNUSED(direction) }

/*!
    \reimp
*/
bool QAbstractAnimation::event(QEvent *event) { return QObject::event(event); }

#include "moc_qabstractanimation.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
