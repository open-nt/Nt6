// MIT License - see LICENSE
#include <QtCore/qparallelanimationgroup.h>

#include <algorithm>

/*!
    \class QParallelAnimationGroup
    \brief The QParallelAnimationGroup class provides a parallel group of animations.
    \since 4.6

    A parallel group starts all its animations when it starts and runs them together. The group
    is finished when the animation that takes longest is.

    \code
    auto *group = new QParallelAnimationGroup;
    group->addAnimation(new QPropertyAnimation(window, "geometry"));
    group->addAnimation(new QPropertyAnimation(window, "windowOpacity"));
    group->start();
    \endcode

    \differences
    None.
    \enddifferences

    \sa QSequentialAnimationGroup, QAnimationGroup
*/

/*!
    Constructs a parallel group with the given \a parent.
*/
QParallelAnimationGroup::QParallelAnimationGroup(QObject *parent) : QAnimationGroup(parent) {}

/*!
    Destroys the group and the animations in it.
*/
QParallelAnimationGroup::~QParallelAnimationGroup() = default;

/*!
    \reimp
    Returns the duration of the animation that takes longest, with its loops, or -1 if the
    duration of one of the animations is undefined.
*/
int QParallelAnimationGroup::duration() const {
    int longest = 0;
    for (int i = 0; i < animationCount(); ++i) {
        const int total = animationAt(i)->totalDuration();
        if (total == -1)
            return -1;
        longest = std::max(longest, total);
    }
    return longest;
}

/*!
    \reimp
*/
bool QParallelAnimationGroup::event(QEvent *event) { return QAnimationGroup::event(event); }

/*!
    \reimp
*/
void QParallelAnimationGroup::updateCurrentTime(int currentTime) {
    const bool forward = direction() == Forward;
    const bool looped = currentLoop() != m_lastLoop;
    m_lastLoop = currentLoop();
    for (int i = 0; i < animationCount(); ++i) {
        QAbstractAnimation *animation = animationAt(i);
        const int total = animation->totalDuration();
        if (looped && animation->state() != Stopped) {
            // The loop that has ended is played out before the next one begins.
            if (total != -1)
                animation->setCurrentTime(forward ? total : 0);
            animation->stop();
        }
        if (state() == Stopped) {
            // The group is moved by hand: its animations show the same time.
            animation->setCurrentTime(total == -1 ? currentTime : std::min(currentTime, total));
            continue;
        }
        if (animation->direction() != direction())
            animation->setDirection(direction());
        // An animation runs while the time of the group is inside it; a shorter one ends before the group does,
        // and starts again when the time comes back to it. One without an end of its own starts with each loop.
        const bool inside = total == -1 ? (looped && forward)
                                        : (forward ? currentTime < total : currentTime > 0 && currentTime <= total);
        if (animation->state() == Stopped && inside) {
            animation->start();
            if (state() == Paused)
                animation->pause();
        }
        if (animation->state() != Stopped)
            animation->setCurrentTime(total == -1 ? currentTime : std::min(currentTime, total));
    }
}

/*!
    \reimp
*/
void QParallelAnimationGroup::updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState) {
    QAnimationGroup::updateState(newState, oldState);
    const int count = animationCount();
    switch (newState) {
    case Stopped:
        for (int i = 0; i < count; ++i)
            animationAt(i)->stop();
        break;
    case Paused:
        for (int i = 0; i < count; ++i) {
            if (animationAt(i)->state() == Running)
                animationAt(i)->pause();
        }
        break;
    case Running:
        for (int i = 0; i < count; ++i) {
            QAbstractAnimation *animation = animationAt(i);
            if (oldState == Stopped) {
                animation->stop();
                m_lastLoop = direction() == Forward ? 0 : std::max(loopCount() - 1, 0);
            }
            animation->setDirection(direction());
            if (oldState == Paused) {
                if (animation->state() == Paused)
                    animation->resume();
            } else if (animation->totalDuration() != 0 || direction() == Forward) {
                animation->start();
            }
        }
        break;
    }
}

/*!
    \reimp
*/
void QParallelAnimationGroup::updateDirection(QAbstractAnimation::Direction direction) {
    if (state() != Stopped) {
        for (int i = 0; i < animationCount(); ++i)
            animationAt(i)->setDirection(direction);
    } else {
        m_lastLoop = direction == Forward ? 0 : std::max(loopCount() - 1, 0);
    }
}

// A group with an animation of undefined duration has no end of its own: it ends when its animations have.
void QParallelAnimationGroup::animationStopped(QAbstractAnimation *animation) {
    Q_UNUSED(animation)
    if (state() == Stopped || duration() != -1)
        return;
    for (int i = 0; i < animationCount(); ++i) {
        if (animationAt(i)->state() != Stopped)
            return;
    }
    stop();
}

#include "moc_qparallelanimationgroup.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
