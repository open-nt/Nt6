// MIT License - see LICENSE
#include <QtCore/qtimer.h>

/*!
    \class QTimer
    \brief The QTimer class provides repetitive and single-shot timers.

    A QTimer emits timeout() each time its interval elapses. Connect that
    signal to whatever should run, then call start():

    \code
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [] { poll(); });
    timer.start(250);
    \endcode

    Timers run on the event loop, so timeout() is only emitted while a
    \c QCoreApplication or \c QEventLoop is executing. An interval is a minimum
    delay, not a guarantee: a busy loop delivers the signal late.

    A timer normally repeats until stop() is called. Call
    \l{setSingleShot()}{setSingleShot(true)} to have it fire once, or use the
    static singleShot() functions when no timer object is needed.

    \differences
    Intervals are stored as an \c int number of milliseconds. There is no
    timer type, \c remainingTime() or \c callOnTimeout(), and interval
    properties cannot be bound. A timer must be started and stopped from the
    thread it lives in; the thread needs a running event loop.
    \enddifferences

    \sa {Signals and Slots}
*/

/*!
    Constructs a timer with the given \a parent. The timer is not started.
*/
QTimer::QTimer(QObject *parent) : QObject(parent) {}

/*!
    Destroys the timer. A running timer stops without emitting timeout().
*/
QTimer::~QTimer() = default;

/*!
    Returns \c true if the timer is running; otherwise returns \c false.

    \sa start(), stop()
*/
bool QTimer::isActive() const { return m_id != 0; }

/*!
    Returns the identifier of the running timer, or \c 0 if it is not running.
*/
int QTimer::timerId() const { return m_id; }

/*!
    Sets the timeout interval to \a msec milliseconds.

    If the timer is running it restarts, so the next timeout is a full
    interval from now.

    \sa interval(), start()
*/
void QTimer::setInterval(int msec) {
    m_interval = msec;
    if (isActive())
        start();
}

/*!
    \overload

    Sets the timeout interval to \a interval.
*/
void QTimer::setInterval(std::chrono::milliseconds interval) {
    setInterval(static_cast<int>(interval.count()));
}

/*!
    Returns the timeout interval in milliseconds. The default is \c 0.

    \sa setInterval(), intervalAsDuration()
*/
int QTimer::interval() const { return m_interval; }

/*!
    Returns the timeout interval as a \c{std::chrono::milliseconds} value.

    \sa interval()
*/
std::chrono::milliseconds QTimer::intervalAsDuration() const {
    return std::chrono::milliseconds(m_interval);
}

/*!
    Sets whether the timer stops after its first timeout. It does if
    \a singleShot is \c true; otherwise it repeats.

    \sa isSingleShot()
*/
void QTimer::setSingleShot(bool singleShot) { m_single = singleShot; }

/*!
    Returns \c true if the timer stops after its first timeout. The default is
    \c false.

    \sa setSingleShot()
*/
bool QTimer::isSingleShot() const { return m_single; }

/*!
    Starts the timer with the current interval, or restarts it if it is already
    running. With a negative interval the timer does not start.

    \sa stop(), isActive()
*/
void QTimer::start() {
    stop();
    m_id = startTimer(m_interval);
}

/*!
    \overload

    Sets the interval to \a msec milliseconds and starts the timer.
*/
void QTimer::start(int msec) {
    m_interval = msec;
    start();
}

/*!
    \overload

    Sets the interval to \a interval and starts the timer.
*/
void QTimer::start(std::chrono::milliseconds interval) { start(static_cast<int>(interval.count())); }

/*!
    Stops the timer. Calling this on a stopped timer has no effect.

    \sa start(), isActive()
*/
void QTimer::stop() {
    if (m_id) {
        killTimer(m_id);
        m_id = 0;
    }
}

void QTimer::timerEvent(QTimerEvent *event) {
    if (event->timerId() != m_id)
        return;
    if (m_single)
        stop();
    timeout();
}

namespace QtPrivate {

// Within a thread the timer becomes a child, so it dies with the context; across threads it
// moves, and the finished timer deletes itself.
void placeSingleShotTimer(QTimer *timer, QObject *context) {
    if (context->thread() == timer->thread())
        timer->setParent(context);
    else
        timer->moveToThread(context->thread());
}

} // namespace QtPrivate
