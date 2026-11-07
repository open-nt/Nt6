// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <chrono>

/*!
    \class QElapsedTimer
    \brief The QElapsedTimer class provides a fast way to calculate elapsed times.

    Call start() or restart() to begin timing, then elapsed() or
    nsecsElapsed() to read the time since then. The timer uses a monotonic
    clock, so it is not affected by changes to the system time.

    \code
    QElapsedTimer timer;
    timer.start();
    slowOperation();
    qDebug() << "took" << timer.elapsed() << "ms";
    \endcode

    \differences
    The timer is always based on \c std::chrono::steady_clock, so
    clockType() always returns MonotonicClock and isMonotonic() always
    returns \c true. There is no \c secsTo(), \c operator<() or
    \c durationElapsed(). hasExpired() treats an invalid timer as expired.
    \enddifferences

    \sa QDeadlineTimer
*/
class QElapsedTimer {
public:
    /*!
        The clock that the timer reads, \c std::chrono::steady_clock.
    */
    using Clock = std::chrono::steady_clock;

    /*!
        \enum QElapsedTimer::ClockType

        Describes the clock that a timer uses.

        \value SystemTime The system time. Not used by Nt.
        \value MonotonicClock A monotonic clock. This is the clock that Nt uses.
        \value TickCounter A tick counter. Not used by Nt.
        \value MachAbsoluteTime The Mach absolute time. Not used by Nt.
        \value PerformanceCounter A high-resolution performance counter. Not used by Nt.

        \sa clockType()
    */
    enum ClockType { SystemTime, MonotonicClock, TickCounter, MachAbsoluteTime, PerformanceCounter };
    /*!
        Returns the clock type that the timer uses. This is always
        MonotonicClock.

        \sa isMonotonic()
    */
    static ClockType clockType() noexcept { return MonotonicClock; }
    /*!
        Returns \c true if the clock is monotonic, which is always the case.

        \sa clockType()
    */
    static bool isMonotonic() noexcept { return true; }

    /*!
        Starts the timer and makes it valid.

        \sa restart(), elapsed()
    */
    void start() noexcept { t0 = Clock::now(); valid = true; }
    /*!
        Restarts the timer and returns the number of milliseconds that had
        elapsed since it was started.

        \sa start(), elapsed()
    */
    qint64 restart() noexcept {
        const qint64 e = elapsed();
        start();
        return e;
    }
    /*!
        Marks the timer as invalid.

        \sa isValid()
    */
    void invalidate() noexcept { valid = false; }
    /*!
        Returns \c true if start() or restart() has been called and the timer
        has not been invalidated; otherwise returns \c false.

        \sa invalidate()
    */
    bool isValid() const noexcept { return valid; }
    /*!
        Returns the number of nanoseconds since the timer was started, or 0
        if the timer is invalid.

        \sa elapsed()
    */
    qint64 nsecsElapsed() const noexcept {
        if (!valid)
            return 0;
        return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count();
    }
    /*!
        Returns the number of milliseconds since the timer was started, or 0
        if the timer is invalid.

        \sa nsecsElapsed(), hasExpired()
    */
    qint64 elapsed() const noexcept { return nsecsElapsed() / 1000000; }
    /*!
        Returns \c true if more than \a timeoutMs milliseconds have elapsed
        since the timer was started, or if the timer is invalid; otherwise
        returns \c false.

        \sa elapsed()
    */
    bool hasExpired(qint64 timeoutMs) const noexcept { return !valid || elapsed() > timeoutMs; }
    /*!
        Returns the start time of the timer as milliseconds since the clock's
        reference point.

        \sa msecsTo()
    */
    qint64 msecsSinceReference() const noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(t0.time_since_epoch()).count();
    }
    /*!
        Returns the number of milliseconds from this timer's start time to
        the start time of \a o. The result is negative if \a o was started
        earlier.

        \sa msecsSinceReference()
    */
    qint64 msecsTo(const QElapsedTimer &o) const noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(o.t0 - t0).count();
    }
    friend bool operator==(const QElapsedTimer &a, const QElapsedTimer &b) noexcept { return a.t0 == b.t0 && a.valid == b.valid; }
    friend bool operator!=(const QElapsedTimer &a, const QElapsedTimer &b) noexcept { return !(a == b); }

private:
    Clock::time_point t0{};
    bool valid = false;
};

/*!
    \class QDeadlineTimer
    \brief The QDeadlineTimer class marks a deadline in the future.

    A QDeadlineTimer stores a point in time, or the special value Forever
    that never expires. Use hasExpired() to test whether the deadline has
    passed and remainingTime() to find out how long is left, for example to
    pass a timeout to a wait function.

    \code
    QDeadlineTimer deadline(500);
    while (!deadline.hasExpired())
        poll();
    \endcode

    \differences
    The timer is based on \c std::chrono::steady_clock. The Qt::TimerType
    argument of the constructors and setters is accepted but ignored. There
    is no \c QDeadlineTimer::current(), \c addNSecs(), \c deadline(),
    \c setDeadline(), \c setPreciseDeadline(), \c operator+(), comparison
    operators, or constructor taking a \c std::chrono::duration.
    \enddifferences

    \sa QElapsedTimer
*/
class QDeadlineTimer {
public:
    /*!
        The clock that the timer reads, \c std::chrono::steady_clock.
    */
    using Clock = std::chrono::steady_clock;
    /*!
        \enum QDeadlineTimer::ForeverConstant

        \value Forever A deadline that never expires.
    */
    enum ForeverConstant { Forever };

    /*!
        Constructs a deadline that is already expired. The timer type is
        ignored.

        \sa setRemainingTime()
    */
    QDeadlineTimer(Qt::TimerType = Qt::CoarseTimer) noexcept : m_forever(false), tp(Clock::now()) {}
    /*!
        Constructs a deadline that never expires. The timer type is ignored.

        \sa isForever()
    */
    QDeadlineTimer(ForeverConstant, Qt::TimerType = Qt::CoarseTimer) noexcept : m_forever(true) {}
    /*!
        Constructs a deadline that expires in \a msecs milliseconds. A
        negative value means the deadline never expires. The timer type is
        ignored.

        \sa setRemainingTime()
    */
    QDeadlineTimer(qint64 msecs, Qt::TimerType = Qt::CoarseTimer) noexcept { setRemainingTime(msecs); }

    /*!
        Returns \c true if the deadline never expires; otherwise returns
        \c false.
    */
    bool isForever() const noexcept { return m_forever; }
    /*!
        Returns \c true if the deadline has passed; otherwise returns
        \c false. A deadline that never expires has not expired.

        \sa remainingTime()
    */
    bool hasExpired() const noexcept { return !m_forever && Clock::now() >= tp; }
    /*!
        Returns the time left until the deadline in nanoseconds, or -1 if the
        deadline never expires. The result is 0 once the deadline has passed.

        \sa remainingTime()
    */
    qint64 remainingTimeNSecs() const noexcept {
        if (m_forever)
            return -1;
        const auto r = std::chrono::duration_cast<std::chrono::nanoseconds>(tp - Clock::now()).count();
        return r < 0 ? 0 : r;
    }
    /*!
        Returns the time left until the deadline in milliseconds, rounded up,
        or -1 if the deadline never expires. The result is 0 once the
        deadline has passed.

        \sa remainingTimeNSecs()
    */
    qint64 remainingTime() const noexcept {
        const qint64 n = remainingTimeNSecs();
        return n < 0 ? -1 : (n + 999999) / 1000000;
    }
    /*!
        Sets the deadline to \a msecs milliseconds from now. A negative value
        means the deadline never expires. The timer type is ignored.

        \sa setPreciseRemainingTime()
    */
    void setRemainingTime(qint64 msecs, Qt::TimerType = Qt::CoarseTimer) noexcept {
        if (msecs < 0) {
            m_forever = true;
        } else {
            m_forever = false;
            tp = Clock::now() + std::chrono::milliseconds(msecs);
        }
    }
    /*!
        Sets the deadline to \a secs seconds and \a nsecs nanoseconds from
        now. The timer type is ignored.

        \sa setRemainingTime()
    */
    void setPreciseRemainingTime(qint64 secs, qint64 nsecs = 0, Qt::TimerType = Qt::CoarseTimer) noexcept {
        m_forever = false;
        tp = Clock::now() + std::chrono::seconds(secs) + std::chrono::nanoseconds(nsecs);
    }
    /*!
        Returns the deadline as a time point of the timer's clock. The value
        is meaningless if the deadline never expires.

        \sa isForever()
    */
    Clock::time_point timePoint() const noexcept { return tp; }

private:
    bool m_forever = false;
    Clock::time_point tp{};
};
