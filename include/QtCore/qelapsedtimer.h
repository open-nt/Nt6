// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qnamespace.h>

#include <chrono>
#include <limits>
#include <utility>

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
    returns \c true. hasExpired() treats an invalid timer as expired.
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
        \since 6.6
        Returns a \c std::chrono::nanoseconds duration with the time since the
        timer was started, or zero if the timer is invalid.

        \sa nsecsElapsed()
    */
    std::chrono::nanoseconds durationElapsed() const noexcept { return std::chrono::nanoseconds(nsecsElapsed()); }
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
    /*!
        Returns the number of seconds from this timer's start time to the
        start time of \a o. The result is negative if \a o was started
        earlier.

        \sa msecsTo()
    */
    qint64 secsTo(const QElapsedTimer &o) const noexcept { return msecsTo(o) / 1000; }
    /*!
        \since 6.6
        Returns the time from this timer's start time to the start time of
        \a o as a \c std::chrono::nanoseconds duration.

        \sa msecsTo()
    */
    std::chrono::nanoseconds durationTo(const QElapsedTimer &o) const noexcept {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(o.t0 - t0);
    }
    friend bool operator==(const QElapsedTimer &a, const QElapsedTimer &b) noexcept { return a.t0 == b.t0 && a.valid == b.valid; }
    friend bool operator!=(const QElapsedTimer &a, const QElapsedTimer &b) noexcept { return !(a == b); }
    friend bool operator<(const QElapsedTimer &a, const QElapsedTimer &b) noexcept { return a.t0 < b.t0; }

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

    Deadlines can be compared and moved with \c{+} and \c{-}; a deadline that
    never expires compares greater than any other and is not changed by
    arithmetic.

    \differences
    The timer is based on \c std::chrono::steady_clock. The Qt::TimerType
    argument of the constructors and setters is stored but does not change the
    precision. Constructors and setters taking a \c std::chrono::time_point
    accept only the steady clock.
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
        Constructs a deadline that is already expired, with the timer type
        \a type.

        \sa setRemainingTime()
    */
    QDeadlineTimer(Qt::TimerType type = Qt::CoarseTimer) noexcept : m_forever(false), tp(Clock::now()), m_type(type) {}
    /*!
        Constructs a deadline that never expires, with the timer type \a type.

        \sa isForever()
    */
    QDeadlineTimer(ForeverConstant, Qt::TimerType type = Qt::CoarseTimer) noexcept : m_forever(true), m_type(type) {}
    /*!
        Constructs a deadline that expires in \a msecs milliseconds, with the
        timer type \a type. A negative value means the deadline never expires.

        \sa setRemainingTime()
    */
    QDeadlineTimer(qint64 msecs, Qt::TimerType type = Qt::CoarseTimer) noexcept { setRemainingTime(msecs, type); }
    /*!
        Constructs a deadline that expires after the duration \a remaining,
        with the timer type \a type. A zero or negative duration gives a
        deadline that has already expired, and \c{duration::max()} one that
        never expires.

        \sa setRemainingTime()
    */
    template <class Rep, class Period>
    QDeadlineTimer(std::chrono::duration<Rep, Period> remaining, Qt::TimerType type = Qt::CoarseTimer) noexcept {
        setRemainingTime(remaining, type);
    }
    /*!
        Constructs a deadline at the steady-clock time point \a deadline, with
        the timer type \a type.

        \sa setDeadline()
    */
    QDeadlineTimer(Clock::time_point deadline, Qt::TimerType type = Qt::CoarseTimer) noexcept
        : m_forever(false), tp(deadline), m_type(type) {}

    /*!
        Returns a deadline that expired at the current time, with the timer
        type \a type.

        \sa hasExpired()
    */
    static QDeadlineTimer current(Qt::TimerType type = Qt::CoarseTimer) noexcept {
        return QDeadlineTimer(Clock::now(), type);
    }
    /*!
        Returns a deadline \a nsecs nanoseconds later than \a dt. A deadline
        that never expires is returned unchanged.

        \sa deadlineNSecs()
    */
    static QDeadlineTimer addNSecs(QDeadlineTimer dt, qint64 nsecs) noexcept {
        if (!dt.m_forever)
            dt.tp += std::chrono::nanoseconds(nsecs);
        return dt;
    }

    /*!
        Swaps this deadline with \a other.
    */
    void swap(QDeadlineTimer &other) noexcept {
        std::swap(m_forever, other.m_forever);
        std::swap(tp, other.tp);
        std::swap(m_type, other.m_type);
    }

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
        Returns the timer type of this deadline.

        \sa setTimerType()
    */
    Qt::TimerType timerType() const noexcept { return m_type; }
    /*!
        Sets the timer type of this deadline to \a type. Nt records the type
        but always uses the precision of the steady clock.

        \sa timerType()
    */
    void setTimerType(Qt::TimerType type) noexcept { m_type = type; }
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
        Returns the time left until the deadline as a
        \c std::chrono::nanoseconds duration. The result is zero once the
        deadline has passed and \c{std::chrono::nanoseconds::max()} if it never
        expires.

        \sa remainingTimeNSecs()
    */
    std::chrono::nanoseconds remainingTimeAsDuration() const noexcept {
        if (m_forever)
            return std::chrono::nanoseconds::max();
        return std::chrono::nanoseconds(remainingTimeNSecs());
    }
    /*!
        Sets the deadline to \a msecs milliseconds from now and the timer type
        to \a type. A negative value means the deadline never expires.

        \sa setPreciseRemainingTime()
    */
    void setRemainingTime(qint64 msecs, Qt::TimerType type = Qt::CoarseTimer) noexcept {
        m_type = type;
        if (msecs < 0) {
            m_forever = true;
        } else {
            m_forever = false;
            tp = Clock::now() + std::chrono::milliseconds(msecs);
        }
    }
    /*!
        \overload
        Sets the deadline to the duration \a remaining from now and the timer
        type to \a type. A zero or negative duration makes the deadline expire
        now, and \c{duration::max()} makes it never expire.
    */
    template <class Rep, class Period>
    void setRemainingTime(std::chrono::duration<Rep, Period> remaining, Qt::TimerType type = Qt::CoarseTimer) noexcept {
        m_type = type;
        m_forever = remaining == remaining.max();
        if (!m_forever) {
            tp = Clock::now();
            if (remaining > remaining.zero())
                tp += std::chrono::duration_cast<Clock::duration>(remaining);
        }
    }
    /*!
        Sets the deadline to \a secs seconds and \a nsecs nanoseconds from
        now and the timer type to \a type. A negative \a secs means the
        deadline never expires.

        \sa setRemainingTime()
    */
    void setPreciseRemainingTime(qint64 secs, qint64 nsecs = 0, Qt::TimerType type = Qt::CoarseTimer) noexcept {
        m_type = type;
        if (secs < 0) {
            m_forever = true;
            return;
        }
        m_forever = false;
        tp = Clock::now() + std::chrono::seconds(secs) + std::chrono::nanoseconds(nsecs);
    }
    /*!
        Returns the deadline in milliseconds since the reference point of the
        steady clock, the same reference that QElapsedTimer::msecsSinceReference()
        uses. A deadline that never expires returns the largest \c qint64.

        \sa deadlineNSecs(), setDeadline()
    */
    qint64 deadline() const noexcept {
        if (m_forever)
            return std::numeric_limits<qint64>::max();
        return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
    }
    /*!
        Returns the deadline in nanoseconds since the reference point of the
        steady clock. A deadline that never expires returns the largest
        \c qint64.

        \sa deadline(), setPreciseDeadline()
    */
    qint64 deadlineNSecs() const noexcept {
        if (m_forever)
            return std::numeric_limits<qint64>::max();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count();
    }
    /*!
        Sets the deadline to \a msecs milliseconds since the reference point of
        the steady clock and the timer type to \a type. The largest \c qint64
        means the deadline never expires.

        \sa deadline(), setPreciseDeadline()
    */
    void setDeadline(qint64 msecs, Qt::TimerType type = Qt::CoarseTimer) noexcept {
        m_type = type;
        m_forever = msecs == std::numeric_limits<qint64>::max();
        if (!m_forever)
            tp = Clock::time_point(std::chrono::duration_cast<Clock::duration>(std::chrono::milliseconds(msecs)));
    }
    /*!
        \overload
        Sets the deadline to the steady-clock time point \a deadline and the
        timer type to \a type.
    */
    void setDeadline(Clock::time_point deadline, Qt::TimerType type = Qt::CoarseTimer) noexcept {
        m_type = type;
        m_forever = false;
        tp = deadline;
    }
    /*!
        Sets the deadline to \a secs seconds and \a nsecs nanoseconds since the
        reference point of the steady clock and the timer type to \a type. The
        largest \c qint64 for \a secs means the deadline never expires.

        \sa setDeadline(), deadlineNSecs()
    */
    void setPreciseDeadline(qint64 secs, qint64 nsecs = 0, Qt::TimerType type = Qt::CoarseTimer) noexcept {
        m_type = type;
        m_forever = secs == std::numeric_limits<qint64>::max();
        if (!m_forever)
            tp = Clock::time_point(std::chrono::duration_cast<Clock::duration>(std::chrono::seconds(secs) +
                                                                               std::chrono::nanoseconds(nsecs)));
    }
    /*!
        Returns the deadline as a time point of the timer's clock. A deadline
        that never expires returns \c{Clock::time_point::max()}.

        \sa isForever()
    */
    Clock::time_point timePoint() const noexcept { return m_forever ? Clock::time_point::max() : tp; }
    /*!
        Moves the deadline \a msecs milliseconds later and returns a reference
        to it. A deadline that never expires is not changed.

        \sa addNSecs()
    */
    QDeadlineTimer &operator+=(qint64 msecs) noexcept {
        *this = addNSecs(*this, msecs * 1000000);
        return *this;
    }
    /*!
        Moves the deadline \a msecs milliseconds earlier and returns a
        reference to it. A deadline that never expires is not changed.

        \sa addNSecs()
    */
    QDeadlineTimer &operator-=(qint64 msecs) noexcept { return *this += -msecs; }

    friend QDeadlineTimer operator+(QDeadlineTimer dt, qint64 msecs) noexcept { return dt += msecs; }
    friend QDeadlineTimer operator+(qint64 msecs, QDeadlineTimer dt) noexcept { return dt += msecs; }
    friend QDeadlineTimer operator-(QDeadlineTimer dt, qint64 msecs) noexcept { return dt -= msecs; }
    friend qint64 operator-(QDeadlineTimer a, QDeadlineTimer b) noexcept {
        return (a.deadlineNSecs() - b.deadlineNSecs()) / 1000000;
    }
    friend bool operator==(QDeadlineTimer a, QDeadlineTimer b) noexcept {
        return a.deadlineNSecs() == b.deadlineNSecs();
    }
    friend bool operator!=(QDeadlineTimer a, QDeadlineTimer b) noexcept { return !(a == b); }
    friend bool operator<(QDeadlineTimer a, QDeadlineTimer b) noexcept { return a.deadlineNSecs() < b.deadlineNSecs(); }
    friend bool operator<=(QDeadlineTimer a, QDeadlineTimer b) noexcept { return !(b < a); }
    friend bool operator>(QDeadlineTimer a, QDeadlineTimer b) noexcept { return b < a; }
    friend bool operator>=(QDeadlineTimer a, QDeadlineTimer b) noexcept { return !(a < b); }

private:
    bool m_forever = false;
    Clock::time_point tp{};
    Qt::TimerType m_type = Qt::CoarseTimer;
};
