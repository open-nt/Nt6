// MIT License - see LICENSE
#pragma once

#include <QtCore/qanimationgroup.h>
#include <QtCore/qpauseanimation.h>

class QSequentialAnimationGroup : public QAnimationGroup {
    Q_OBJECT
    Q_PROPERTY(QAbstractAnimation *currentAnimation READ currentAnimation NOTIFY currentAnimationChanged)
public:
    explicit QSequentialAnimationGroup(QObject *parent = nullptr);
    ~QSequentialAnimationGroup() override;

    QPauseAnimation *addPause(int msecs);
    QPauseAnimation *insertPause(int index, int msecs);

    QAbstractAnimation *currentAnimation() const;
    int duration() const override;

signals:
    /*!
        This signal is emitted when the group moves on to another animation, with the animation
        that now runs in \a current. It is \c nullptr when the group has none.

        \sa currentAnimation()
    */
    void currentAnimationChanged(QAbstractAnimation *current) {
        emitSignal(&QSequentialAnimationGroup::currentAnimationChanged, current);
    }

protected:
    bool event(QEvent *event) override;

    void updateCurrentTime(int currentTime) override;
    void updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState) override;
    void updateDirection(QAbstractAnimation::Direction direction) override;
    /*! \internal */
    void animationsChanged(int index, bool inserted) override;
    /*! \internal */
    void animationStopped(QAbstractAnimation *animation) override;

private:
    int durationOf(int index) const;
    void setCurrent(int index);
    void activate(int index, int localTime);
    void runThrough(int index, bool toEnd);

    int m_current = -1;
    int m_lastLoop = 0;
    QList<int> m_actualDurations; // of the animations whose duration is undefined: -1 until they have ended
};
