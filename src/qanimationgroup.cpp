// MIT License - see LICENSE
#include <QtCore/qanimationgroup.h>

#include <QtCore/qlogging.h>

/*!
    \class QAnimationGroup
    \brief The QAnimationGroup class is an abstract base class for groups of animations.
    \since 4.6

    A group is an animation that holds other animations and runs them: one after another in a
    QSequentialAnimationGroup, all together in a QParallelAnimationGroup. The program starts,
    pauses and stops the group, and the group does the same with what it holds. As a group is an
    animation, groups can hold groups.

    \code
    auto *group = new QSequentialAnimationGroup;
    group->addAnimation(new QPropertyAnimation(panel, "pos"));
    group->addPause(200);
    group->addAnimation(new QPropertyAnimation(panel, "size"));
    group->start(QAbstractAnimation::DeleteWhenStopped);
    \endcode

    The group owns its animations: it becomes their parent and deletes them when it is deleted.

    \differences
    None.
    \enddifferences

    \sa QSequentialAnimationGroup, QParallelAnimationGroup
*/

/*!
    Constructs a group with the given \a parent.
*/
QAnimationGroup::QAnimationGroup(QObject *parent) : QAbstractAnimation(parent) {}

/*!
    Destroys the group and the animations in it.
*/
QAnimationGroup::~QAnimationGroup() {
    // The children must not come back to a group that is half destroyed.
    const QList<QAbstractAnimation *> animations = m_animations;
    m_animations.clear();
    for (QAbstractAnimation *animation : animations)
        animation->m_group = nullptr;
}

/*!
    Returns the animation at \a index, which must be at least 0 and less than animationCount().

    \sa animationCount(), indexOfAnimation()
*/
QAbstractAnimation *QAnimationGroup::animationAt(int index) const {
    if (index < 0 || index >= m_animations.size()) {
        qWarning("QAnimationGroup::animationAt: index is out of bounds");
        return nullptr;
    }
    return m_animations.at(index);
}

/*!
    Returns the number of animations in the group.

    \sa animationAt()
*/
int QAnimationGroup::animationCount() const { return static_cast<int>(m_animations.size()); }

/*!
    Returns the index of \a animation in the group, or -1 if it is not in it.

    \sa animationAt()
*/
int QAnimationGroup::indexOfAnimation(QAbstractAnimation *animation) const {
    return static_cast<int>(m_animations.indexOf(animation));
}

/*!
    Adds \a animation at the end of the group, which takes ownership of it. An animation that is
    in another group is taken out of that one.

    \sa insertAnimation(), removeAnimation()
*/
void QAnimationGroup::addAnimation(QAbstractAnimation *animation) { insertAnimation(animationCount(), animation); }

/*!
    Inserts \a animation into the group at \a index, which is 0 for the front and
    animationCount() for the end. The group takes ownership of the animation.

    \sa addAnimation(), takeAnimation()
*/
void QAnimationGroup::insertAnimation(int index, QAbstractAnimation *animation) {
    if (!animation || animation == this)
        return;
    if (index < 0 || index > m_animations.size()) {
        qWarning("QAnimationGroup::insertAnimation: index is out of bounds");
        return;
    }
    if (QAnimationGroup *old = animation->group()) {
        const int oldIndex = old->indexOfAnimation(animation);
        old->removeAnimation(animation);
        // Within this group the animation moves, and its going has shifted what came after it.
        if (old == this && oldIndex < index)
            --index;
    }
    // An animation that ran on its own is the group's to run from now on.
    animation->stop();
    m_animations.insert(index, animation);
    animation->m_group = this;
    animation->setParent(this);
    animationsChanged(index, true);
}

/*!
    Takes \a animation out of the group. The caller owns it afterwards: it has no parent.

    \sa takeAnimation(), addAnimation()
*/
void QAnimationGroup::removeAnimation(QAbstractAnimation *animation) {
    if (!animation) {
        qWarning("QAnimationGroup::remove: cannot remove null animation");
        return;
    }
    const int index = indexOfAnimation(animation);
    if (index < 0) {
        qWarning("QAnimationGroup::remove: animation is not part of this group");
        return;
    }
    takeAnimation(index);
}

/*!
    Takes the animation at \a index out of the group and returns it. The caller owns it
    afterwards: it has no parent. A group that runs and has no animation left stops.

    \sa removeAnimation(), clear()
*/
QAbstractAnimation *QAnimationGroup::takeAnimation(int index) {
    if (index < 0 || index >= m_animations.size()) {
        qWarning("QAnimationGroup::takeAnimation: no animation at index %d", index);
        return nullptr;
    }
    QAbstractAnimation *animation = m_animations.at(index);
    animation->stop();
    m_animations.removeAt(index);
    animation->m_group = nullptr;
    animation->setParent(nullptr);
    animationsChanged(index, false);
    return animation;
}

/*!
    Takes every animation out of the group and deletes it.

    \sa takeAnimation()
*/
void QAnimationGroup::clear() {
    while (!m_animations.isEmpty())
        delete m_animations.last(); // the animation takes itself out
}

/*!
    \reimp
*/
bool QAnimationGroup::event(QEvent *event) { return QAbstractAnimation::event(event); }

void QAnimationGroup::animationsChanged(int index, bool inserted) {
    Q_UNUSED(index)
    if (!inserted && m_animations.isEmpty() && state() != Stopped)
        stop();
}

void QAnimationGroup::animationStopped(QAbstractAnimation *animation) { Q_UNUSED(animation) }

#include "moc_qanimationgroup.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
