// MIT License - see LICENSE
#include <QtCore/qtimeline.h>

#include <QtCore/qcoreevent.h>
#include <QtCore/qlogging.h>

#include <algorithm>
#include <cmath>
#include <limits>

/*!
    Constructs a time line that runs for \a duration milliseconds, with the
    given \a parent. The frame range is 0 to 0 and the easing curve
    QEasingCurve::InOutSine.
*/
QTimeLine::QTimeLine(int duration, QObject *parent) : QObject(parent), m_duration(duration) {}

/*!
    Destroys the time line, stopping it first.
*/
QTimeLine::~QTimeLine() {
    if (m_timerId)
        killTimer(m_timerId);
}

/*!
    Returns the state of the time line.
*/
QTimeLine::State QTimeLine::state() const { return m_state; }

/*!
    Returns the number of times the time line runs before it finishes, 0
    meaning forever. The default is 1.

    \sa setLoopCount()
*/
int QTimeLine::loopCount() const { return m_loops; }

/*!
    Sets the number of loops to \a count; 0 makes the time line loop forever.
*/
void QTimeLine::setLoopCount(int count) { m_loops = count; }

/*!
    Returns the direction of the time line.

    \sa setDirection(), toggleDirection()
*/
QTimeLine::Direction QTimeLine::direction() const { return m_direction; }

/*!
    Sets the direction of the time line to \a direction. A running time line
    continues from its current time in the new direction.
*/
void QTimeLine::setDirection(Direction direction) {
    m_direction = direction;
    m_startTime = m_currentTime;
    m_clock.start();
}

/*!
    Returns the duration of one loop in milliseconds.
*/
int QTimeLine::duration() const { return m_duration; }

/*!
    Sets the duration of one loop to \a duration milliseconds, which must be
    positive.
*/
void QTimeLine::setDuration(int duration) {
    if (duration <= 0) {
        qWarning("QTimeLine::setDuration: cannot set duration <= 0");
        return;
    }
    m_duration = duration;
}

/*!
    Returns the first frame of the frame range.
*/
int QTimeLine::startFrame() const { return m_startFrame; }

/*!
    Sets the first frame of the frame range to \a frame.
*/
void QTimeLine::setStartFrame(int frame) { m_startFrame = frame; }

/*!
    Returns the last frame of the frame range.
*/
int QTimeLine::endFrame() const { return m_endFrame; }

/*!
    Sets the last frame of the frame range to \a frame.
*/
void QTimeLine::setEndFrame(int frame) { m_endFrame = frame; }

/*!
    Sets the frame range to \a startFrame to \a endFrame.

    \sa frameForTime(), frameChanged()
*/
void QTimeLine::setFrameRange(int startFrame, int endFrame) {
    m_startFrame = startFrame;
    m_endFrame = endFrame;
}

/*!
    Returns the interval in milliseconds between updates while the time line
    runs. The default is 40, 25 updates a second.
*/
int QTimeLine::updateInterval() const { return m_updateInterval; }

/*!
    Sets the update interval to \a interval milliseconds. A running time line
    uses it from the next start or resume.
*/
void QTimeLine::setUpdateInterval(int interval) { m_updateInterval = interval; }

/*!
    \since 4.6
    Returns the easing curve that shapes the value of the time line.
*/
QEasingCurve QTimeLine::easingCurve() const { return m_curve; }

/*!
    \since 4.6
    Sets the easing curve to \a curve.
*/
void QTimeLine::setEasingCurve(const QEasingCurve &curve) { m_curve = curve; }

/*!
    Returns the current time of the time line in milliseconds, from 0 to
    duration().
*/
int QTimeLine::currentTime() const { return m_currentTime; }

/*!
    Returns the frame for the current time.
*/
int QTimeLine::currentFrame() const { return frameForTime(m_currentTime); }

/*!
    Returns the value for the current time.
*/
qreal QTimeLine::currentValue() const { return valueForTime(m_currentTime); }

/*!
    Returns the frame for the time \a msec: the frame range scaled by
    valueForTime(), rounded toward the start frame when running forward and
    away from it when running backward.
*/
int QTimeLine::frameForTime(int msec) const {
    const qreal span = static_cast<qreal>(m_endFrame - m_startFrame) * valueForTime(msec);
    return m_startFrame + static_cast<int>(m_direction == Forward ? span : std::ceil(span));
}

/*!
    Returns the value for the time \a msec: the easing curve applied to the
    progress \a msec / duration(), clamped to the range 0 to 1. Reimplement
    it for a custom shape.
*/
qreal QTimeLine::valueForTime(int msec) const {
    const qreal progress = m_duration > 0 ? std::clamp<qreal>(static_cast<qreal>(msec) / m_duration, 0, 1) : 0;
    return m_curve.valueForProgress(progress);
}

void QTimeLine::setState(State state) {
    if (state == m_state)
        return;
    m_state = state;
    stateChanged(state);
}

/*!
    Starts the time line from its beginning (from duration() when running
    backward). Does nothing, with a warning, if it is already running.

    \sa resume(), stop()
*/
void QTimeLine::start() {
    if (m_timerId) {
        qWarning("QTimeLine::start: already running");
        return;
    }
    const int begin = m_direction == Backward ? m_duration : 0;
    m_currentLoop = 0;
    m_startTime = begin;
    m_timerId = startTimer(m_updateInterval);
    m_clock.start();
    setState(Running);
    setCurrentTime(begin);
}

/*!
    Resumes the time line from its current time. Does nothing, with a
    warning, if it is already running.
*/
void QTimeLine::resume() {
    if (m_timerId) {
        qWarning("QTimeLine::resume: already running");
        return;
    }
    m_startTime = m_currentTime;
    m_timerId = startTimer(m_updateInterval);
    m_clock.start();
    setState(Running);
}

/*!
    Stops the time line, keeping its current time. finished() is not
    emitted.
*/
void QTimeLine::stop() {
    if (m_timerId) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    setState(NotRunning);
}

/*!
    Pauses the time line if \a paused is \c true, or resumes it if \a paused
    is \c false and it is paused.
*/
void QTimeLine::setPaused(bool paused) {
    if (m_state == NotRunning) {
        qWarning("QTimeLine::setPaused: Not running");
        return;
    }
    if (paused && m_state != Paused) {
        m_startTime = m_currentTime;
        if (m_timerId) {
            killTimer(m_timerId);
            m_timerId = 0;
        }
        setState(Paused);
    } else if (!paused && m_state == Paused) {
        resume();
    }
}

/*!
    Sets the current time to \a msec, measured from the start of the
    current loop in the forward direction, and emits valueChanged() and
    frameChanged() as needed. A time past the end of the loop moves on to the
    next loops; after the last loop the time line stops and emits finished().
*/
void QTimeLine::setCurrentTime(int msec) {
    if (m_duration <= 0) {
        stop();
        return;
    }
    advanceTo(msec);
}

void QTimeLine::advanceTo(int msec) {
    const int lastFrame = currentFrame();
    const qreal lastValue = currentValue();
    // Time elapsed in the running direction, possibly over several loops.
    const qint64 elapsed = m_direction == Backward ? qint64(m_duration) - msec : msec;
    const qint64 loop = elapsed >= 0 ? elapsed / m_duration : -1;
    bool done = false;
    if (elapsed < 0) {
        m_currentTime = m_direction == Backward ? m_duration : 0;
    } else if (m_loops > 0 && loop >= m_loops) {
        done = true;
        m_currentLoop = m_loops - 1;
        m_currentTime = m_direction == Backward ? 0 : m_duration;
    } else {
        m_currentLoop = static_cast<int>(loop);
        const int inLoop = static_cast<int>(elapsed % m_duration);
        m_currentTime = m_direction == Backward ? m_duration - inLoop : inLoop;
    }
    const qreal value = currentValue();
    if (value != lastValue || m_state == Running)
        valueChanged(value);
    const int frame = currentFrame();
    if (frame != lastFrame)
        frameChanged(frame);
    if (done && m_state == Running) {
        stop();
        finished();
    }
}

/*!
    Reverses the direction of the time line.

    \sa setDirection()
*/
void QTimeLine::toggleDirection() { setDirection(m_direction == Forward ? Backward : Forward); }

/*!
    \reimp
*/
void QTimeLine::timerEvent(QTimerEvent *event) {
    if (event->timerId() != m_timerId) {
        QObject::timerEvent(event);
        return;
    }
    const auto elapsed = static_cast<int>(std::min<qint64>(m_clock.elapsed(), std::numeric_limits<int>::max() / 2));
    // Loops are counted from where the time line was started or resumed.
    if (m_direction == Forward)
        advanceTo(m_startTime + elapsed);
    else
        advanceTo(m_startTime - elapsed);
}

#include "moc_qtimeline.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
