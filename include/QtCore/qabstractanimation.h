// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>

class QAnimationGroup;

class QAbstractAnimation : public QObject {
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(int loopCount READ loopCount WRITE setLoopCount)
    Q_PROPERTY(int currentTime READ currentTime WRITE setCurrentTime)
    Q_PROPERTY(int currentLoop READ currentLoop NOTIFY currentLoopChanged)
    Q_PROPERTY(Direction direction READ direction WRITE setDirection NOTIFY directionChanged)
    Q_PROPERTY(int duration READ duration)
public:
    enum Direction { Forward, Backward };
    Q_ENUM(Direction)
    enum State { Stopped, Paused, Running };
    Q_ENUM(State)
    enum DeletionPolicy { KeepWhenStopped = 0, DeleteWhenStopped };

    explicit QAbstractAnimation(QObject *parent = nullptr);
    ~QAbstractAnimation() override;

    State state() const;
    QAnimationGroup *group() const;

    Direction direction() const;
    void setDirection(Direction direction);

    int currentTime() const;
    int currentLoopTime() const;

    int loopCount() const;
    void setLoopCount(int loopCount);
    int currentLoop() const;

    virtual int duration() const = 0;
    int totalDuration() const;

public slots:
    void start(QAbstractAnimation::DeletionPolicy policy = KeepWhenStopped);
    void pause();
    void resume();
    void setPaused(bool paused);
    void stop();
    void setCurrentTime(int msecs);

signals:
    /*!
        This signal is emitted when the animation has run to its end and stopped by itself. It is
        not emitted when stop() ends an animation early, unless the animation has no end of its
        own: an undefined duration or loops without end.

        \sa stateChanged(), stop()
    */
    void finished() { emitSignal(&QAbstractAnimation::finished); }
    /*!
        This signal is emitted when the state of the animation has changed from \a oldState to
        \a newState.

        \sa updateState()
    */
    void stateChanged(QAbstractAnimation::State newState, QAbstractAnimation::State oldState) {
        emitSignal(&QAbstractAnimation::stateChanged, newState, oldState);
    }
    /*!
        This signal is emitted when the animation starts another loop, with the number of the
        loop in \a currentLoop.

        \sa currentLoop(), setLoopCount()
    */
    void currentLoopChanged(int currentLoop) { emitSignal(&QAbstractAnimation::currentLoopChanged, currentLoop); }
    /*!
        This signal is emitted when the direction has changed, with the new direction in
        \a newDirection.

        \sa setDirection()
    */
    void directionChanged(QAbstractAnimation::Direction newDirection) {
        emitSignal(&QAbstractAnimation::directionChanged, newDirection);
    }

protected:
    bool event(QEvent *event) override;

    virtual void updateCurrentTime(int currentTime) = 0;
    virtual void updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState);
    virtual void updateDirection(QAbstractAnimation::Direction direction);

private:
    friend class QAnimationGroup;
    friend class QtAnimationDriver;
    void setState(State newState);

    State m_state = Stopped;
    Direction m_direction = Forward;
    int m_totalCurrentTime = 0;
    int m_currentTime = 0;
    int m_loopCount = 1;
    int m_currentLoop = 0;
    bool m_deleteWhenStopped = false;
    bool m_registered = false;
    QAnimationGroup *m_group = nullptr;
};
