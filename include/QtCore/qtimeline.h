// MIT License - see LICENSE
#pragma once

#include <QtCore/qeasingcurve.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qobject.h>

/*!
    \class QTimeLine
    \since 4.2
    \brief The QTimeLine class provides a timeline for controlling animations.

    A QTimeLine runs for duration() milliseconds and emits valueChanged()
    with the progress, shaped by easingCurve(), every updateInterval()
    milliseconds. With a frame range it also emits frameChanged():

    \code
    QTimeLine timeLine(1000);
    timeLine.setFrameRange(0, 100);
    QObject::connect(&timeLine, &QTimeLine::frameChanged, progressBar, &QProgressBar::setValue);
    timeLine.start();
    \endcode

    \differences
    The time line advances with a timer of the event loop and the elapsed
    time of a QElapsedTimer. There are no bindable properties.
    \enddifferences

    \sa QEasingCurve, QTimer
*/
class QTimeLine : public QObject {
    Q_OBJECT
public:
    /*!
        \enum QTimeLine::State

        \value NotRunning The time line is not running.
        \value Paused The time line is paused; resume() or setPaused(false) continues it.
        \value Running The time line is running.
    */
    enum State { NotRunning, Paused, Running };
    /*!
        \enum QTimeLine::Direction

        \value Forward The current time increases with time.
        \value Backward The current time decreases with time.
    */
    enum Direction { Forward, Backward };

    explicit QTimeLine(int duration = 1000, QObject *parent = nullptr);
    ~QTimeLine() override;

    State state() const;
    int loopCount() const;
    void setLoopCount(int count);
    Direction direction() const;
    void setDirection(Direction direction);
    int duration() const;
    void setDuration(int duration);
    int startFrame() const;
    void setStartFrame(int frame);
    int endFrame() const;
    void setEndFrame(int frame);
    void setFrameRange(int startFrame, int endFrame);
    int updateInterval() const;
    void setUpdateInterval(int interval);
    QEasingCurve easingCurve() const;
    void setEasingCurve(const QEasingCurve &curve);
    int currentTime() const;
    int currentFrame() const;
    qreal currentValue() const;
    int frameForTime(int msec) const;
    virtual qreal valueForTime(int msec) const;

public slots:
    void start();
    void resume();
    void stop();
    void setPaused(bool paused);
    void setCurrentTime(int msec);
    void toggleDirection();

signals:
    /*!
        This signal is emitted at regular intervals while the time line runs,
        and when the current time changes, with the new \a value.

        \sa valueForTime(), updateInterval()
    */
    void valueChanged(qreal value) { emitSignal(&QTimeLine::valueChanged, value); }
    /*!
        This signal is emitted when the current frame changes, with the new
        \a frame.

        \sa setFrameRange()
    */
    void frameChanged(int frame) { emitSignal(&QTimeLine::frameChanged, frame); }
    /*!
        This signal is emitted when the state of the time line changes, with
        the new state \a newState.
    */
    void stateChanged(QTimeLine::State newState) { emitSignal(&QTimeLine::stateChanged, newState); }
    /*!
        This signal is emitted when the time line finishes, that is, when it
        reaches its end after the last loop. It is not emitted by stop().
    */
    void finished() { emitSignal(&QTimeLine::finished); }

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    void setState(State state);
    void advanceTo(int msec);
    int m_duration;
    int m_startFrame = 0;
    int m_endFrame = 0;
    int m_updateInterval = 1000 / 25;
    int m_loops = 1;
    int m_currentLoop = 0;
    int m_currentTime = 0;
    int m_startTime = 0;
    int m_timerId = 0;
    Direction m_direction = Forward;
    State m_state = NotRunning;
    QEasingCurve m_curve{QEasingCurve::InOutSine};
    QElapsedTimer m_clock;
};
