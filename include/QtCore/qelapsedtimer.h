// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <chrono>

class QElapsedTimer {
public:
    using Clock = std::chrono::steady_clock;

    enum ClockType { SystemTime, MonotonicClock, TickCounter, MachAbsoluteTime, PerformanceCounter };
    static ClockType clockType() noexcept { return MonotonicClock; }
    static bool isMonotonic() noexcept { return true; }

    void start() noexcept { t0 = Clock::now(); valid = true; }
    qint64 restart() noexcept {
        const qint64 e = elapsed();
        start();
        return e;
    }
    void invalidate() noexcept { valid = false; }
    bool isValid() const noexcept { return valid; }
    qint64 nsecsElapsed() const noexcept {
        if (!valid)
            return 0;
        return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count();
    }
    qint64 elapsed() const noexcept { return nsecsElapsed() / 1000000; }
    bool hasExpired(qint64 timeoutMs) const noexcept { return !valid || elapsed() > timeoutMs; }
    qint64 msecsSinceReference() const noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(t0.time_since_epoch()).count();
    }
    qint64 msecsTo(const QElapsedTimer &o) const noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(o.t0 - t0).count();
    }
    friend bool operator==(const QElapsedTimer &a, const QElapsedTimer &b) noexcept { return a.t0 == b.t0 && a.valid == b.valid; }
    friend bool operator!=(const QElapsedTimer &a, const QElapsedTimer &b) noexcept { return !(a == b); }

private:
    Clock::time_point t0{};
    bool valid = false;
};

class QDeadlineTimer {
public:
    using Clock = std::chrono::steady_clock;
    enum ForeverConstant { Forever };

    QDeadlineTimer(Qt::TimerType = Qt::CoarseTimer) noexcept : m_forever(false), tp(Clock::now()) {}
    QDeadlineTimer(ForeverConstant, Qt::TimerType = Qt::CoarseTimer) noexcept : m_forever(true) {}
    QDeadlineTimer(qint64 msecs, Qt::TimerType = Qt::CoarseTimer) noexcept { setRemainingTime(msecs); }

    bool isForever() const noexcept { return m_forever; }
    bool hasExpired() const noexcept { return !m_forever && Clock::now() >= tp; }
    qint64 remainingTimeNSecs() const noexcept {
        if (m_forever)
            return -1;
        const auto r = std::chrono::duration_cast<std::chrono::nanoseconds>(tp - Clock::now()).count();
        return r < 0 ? 0 : r;
    }
    qint64 remainingTime() const noexcept {
        const qint64 n = remainingTimeNSecs();
        return n < 0 ? -1 : (n + 999999) / 1000000;
    }
    void setRemainingTime(qint64 msecs, Qt::TimerType = Qt::CoarseTimer) noexcept {
        if (msecs < 0) {
            m_forever = true;
        } else {
            m_forever = false;
            tp = Clock::now() + std::chrono::milliseconds(msecs);
        }
    }
    void setPreciseRemainingTime(qint64 secs, qint64 nsecs = 0, Qt::TimerType = Qt::CoarseTimer) noexcept {
        m_forever = false;
        tp = Clock::now() + std::chrono::seconds(secs) + std::chrono::nanoseconds(nsecs);
    }
    Clock::time_point timePoint() const noexcept { return tp; }

private:
    bool m_forever = false;
    Clock::time_point tp{};
};
