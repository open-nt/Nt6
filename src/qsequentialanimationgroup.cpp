// MIT License - see LICENSE
#include <QtCore/qsequentialanimationgroup.h>

#include <QtCore/qlogging.h>

#include <algorithm>

/*!
    \class QSequentialAnimationGroup
    \brief The QSequentialAnimationGroup class provides a sequential group of animations.
    \since 4.6

    A sequential group runs its animations one after another, in the order they were added: when
    one has finished, the next starts. The group is finished when the last one is.

    \code
    auto *group = new QSequentialAnimationGroup;
    group->addAnimation(fadeOut);
    group->addPause(100);
    group->addAnimation(move);
    group->addAnimation(fadeIn);
    group->start();
    \endcode

    currentAnimation() is the animation that runs now, and currentAnimationChanged() tells when
    the group moves on.

    \differences
    The current animation is not a bindable property.
    \enddifferences

    \sa QParallelAnimationGroup, QPauseAnimation
*/

/*!
    Constructs a sequential group with the given \a parent.
*/
QSequentialAnimationGroup::QSequentialAnimationGroup(QObject *parent) : QAnimationGroup(parent) {}

/*!
    Destroys the group and the animations in it.
*/
QSequentialAnimationGroup::~QSequentialAnimationGroup() = default;

/*!
    Adds a pause of \a msecs milliseconds at the end of the group and returns it.

    \sa insertPause(), QAnimationGroup::addAnimation()
*/
QPauseAnimation *QSequentialAnimationGroup::addPause(int msecs) {
    auto *pause = new QPauseAnimation(msecs);
    addAnimation(pause);
    return pause;
}

/*!
    Inserts a pause of \a msecs milliseconds into the group at \a index and returns it, or
    \c nullptr if the index is out of range.

    \sa addPause(), QAnimationGroup::insertAnimation()
*/
QPauseAnimation *QSequentialAnimationGroup::insertPause(int index, int msecs) {
    if (index < 0 || index > animationCount()) {
        qWarning("QSequentialAnimationGroup::insertPause: index is out of bounds");
        return nullptr;
    }
    auto *pause = new QPauseAnimation(msecs);
    insertAnimation(index, pause);
    return pause;
}

/*!
    Returns the animation that the group is at: the one that runs, or would if the group did.
    It is \c nullptr for a group without animations.

    \sa currentAnimationChanged()
*/
QAbstractAnimation *QSequentialAnimationGroup::currentAnimation() const {
    return m_current >= 0 && m_current < animationCount() ? animationAt(m_current) : nullptr;
}

// The duration of the animation at index with its loops. That of an animation with an undefined duration is known
// once it has run to its end.
int QSequentialAnimationGroup::durationOf(int index) const {
    const int total = animationAt(index)->totalDuration();
    if (total != -1)
        return total;
    return index < m_actualDurations.size() ? m_actualDurations.at(index) : -1;
}

/*!
    \reimp
    Returns the sum of the durations of the animations, with their loops, or -1 while the
    duration of one of them is undefined.
*/
int QSequentialAnimationGroup::duration() const {
    int sum = 0;
    for (int i = 0; i < animationCount(); ++i) {
        const int total = durationOf(i);
        if (total == -1)
            return -1;
        sum += total;
    }
    return sum;
}

void QSequentialAnimationGroup::setCurrent(int index) {
    if (m_current == index)
        return;
    m_current = index;
    currentAnimationChanged(currentAnimation());
}

// Lets the animation at index run from one end to the other at once, so that it leaves behind what it would have
// left had it run in time: its end when the group passes it forwards, its start backwards.
void QSequentialAnimationGroup::runThrough(int index, bool toEnd) {
    QAbstractAnimation *animation = animationAt(index);
    const int total = animation->totalDuration();
    if (total == -1) {
        animation->stop();
        return;
    }
    animation->setDirection(toEnd ? Forward : Backward);
    if (animation->state() == Stopped && state() != Stopped)
        animation->start();
    animation->setCurrentTime(toEnd ? total : 0);
    animation->stop();
}

// Makes the animation at index the one that runs, at localTime.
void QSequentialAnimationGroup::activate(int index, int localTime) {
    setCurrent(index);
    QAbstractAnimation *animation = animationAt(index);
    if (animation->direction() != direction())
        animation->setDirection(direction());
    if (state() != Stopped && animation->state() == Stopped) {
        animation->start();
        if (state() == Paused)
            animation->pause();
    }
    if (m_current == index && (animation->state() != Stopped || state() == Stopped))
        animation->setCurrentTime(localTime);
}

/*!
    \reimp
*/
bool QSequentialAnimationGroup::event(QEvent *event) { return QAnimationGroup::event(event); }

/*!
    \reimp
*/
void QSequentialAnimationGroup::updateCurrentTime(int currentTime) {
    const int count = animationCount();
    if (count == 0)
        return;
    const bool forward = direction() == Forward;

    // The animation that the time falls into, and the time inside it. A boundary between two animations is the
    // start of the later one forwards and the end of the earlier one backwards.
    int index = 0, start = 0;
    for (; index < count - 1; ++index) {
        const int total = durationOf(index);
        if (total == -1 || currentTime < start + total || (!forward && currentTime == start + total))
            break;
        start += total;
    }
    const int local = currentTime - start;

    const int old = m_current;
    if (currentLoop() != m_lastLoop && old >= 0 && old < count) {
        // The loop that has ended is played out, then the next begins at the other end.
        if (currentLoop() > m_lastLoop) {
            for (int i = old; i < count; ++i)
                runThrough(i, true);
            for (int i = 0; i < index; ++i)
                runThrough(i, true);
        } else {
            for (int i = old; i >= 0; --i)
                runThrough(i, false);
            for (int i = count - 1; i > index; --i)
                runThrough(i, false);
        }
    } else if (old >= 0 && old < count && old != index) {
        // The animations that the time has passed are played out on the way.
        if (index > old) {
            for (int i = old; i < index; ++i)
                runThrough(i, true);
        } else {
            for (int i = old; i > index; --i)
                runThrough(i, false);
        }
    }
    m_lastLoop = currentLoop();
    activate(index, local);
}

/*!
    \reimp
*/
void QSequentialAnimationGroup::updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState) {
    QAnimationGroup::updateState(newState, oldState);
    QAbstractAnimation *current = currentAnimation();
    switch (newState) {
    case Stopped:
        if (current)
            current->stop();
        break;
    case Paused:
        if (current && current->state() == Running)
            current->pause();
        break;
    case Running:
        if (oldState == Paused) {
            if (current && current->state() == Paused)
                current->resume();
            break;
        }
        // A new run: nothing is known yet of the animations whose duration is undefined.
        m_actualDurations.clear();
        for (int i = 0; i < animationCount(); ++i)
            m_actualDurations.append(-1);
        m_lastLoop = direction() == Forward ? 0 : std::max(loopCount() - 1, 0);
        if (animationCount() > 0) {
            const int first = direction() == Forward ? 0 : animationCount() - 1;
            setCurrent(first);
            QAbstractAnimation *animation = animationAt(first);
            animation->stop();
            animation->setDirection(direction());
            animation->start();
        }
        break;
    }
}

/*!
    \reimp
*/
void QSequentialAnimationGroup::updateDirection(QAbstractAnimation::Direction direction) {
    if (state() != Stopped) {
        if (QAbstractAnimation *current = currentAnimation())
            current->setDirection(direction);
    }
}

void QSequentialAnimationGroup::animationsChanged(int index, bool inserted) {
    if (index < m_actualDurations.size()) {
        if (inserted)
            m_actualDurations.insert(index, -1);
        else
            m_actualDurations.removeAt(index);
    }
    if (inserted) {
        if (m_current < 0)
            setCurrent(0);
        else if (index <= m_current)
            ++m_current; // the same animation, one place on
    } else if (animationCount() == 0) {
        setCurrent(-1);
    } else if (index < m_current) {
        --m_current;
    } else if (index == m_current) {
        // The current animation is gone: the one that took its place is next, or the last one.
        m_current = -1;
        setCurrent(std::min(index, animationCount() - 1));
        if (state() != Stopped)
            setCurrentTime(currentTime());
    }
    QAnimationGroup::animationsChanged(index, inserted);
}

// An animation of undefined duration has ended by itself: now the group knows how long it took, and goes on.
void QSequentialAnimationGroup::animationStopped(QAbstractAnimation *animation) {
    const int index = indexOfAnimation(animation);
    if (state() == Stopped || index != m_current || animation->totalDuration() != -1)
        return;
    while (m_actualDurations.size() <= index)
        m_actualDurations.append(-1);
    if (m_actualDurations.at(index) != -1)
        return;
    int before = 0;
    for (int i = 0; i < index; ++i)
        before += std::max(durationOf(i), 0);
    m_actualDurations[index] = std::max(currentTime() - before, 0);
    setCurrentTime(currentTime());
}

#include "moc_qsequentialanimationgroup.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
