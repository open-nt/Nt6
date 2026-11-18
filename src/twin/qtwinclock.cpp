// MIT License - see LICENSE
#include "qtwinclock.h"

#include <QtCore/qtimer.h>

#include <cmath>

namespace QtTwin {

/*!
    \class QtTwin::SimulationClock
    \inmodule NtTwin
    \brief The SimulationClock class is the time of a digital twin, separate from wall-clock time.

    A twin runs live, replays history or simulates what-if cases, faster or slower than real
    time. Everything in it that depends on time reads this clock instead of the system clock.
    While running, the clock advances by the elapsed wall-clock time times rate(); advance()
    moves it by a given amount, which is how a headless simulation steps deterministically.

    \code
    QtTwin::SimulationClock clock;
    clock.setTime(QDateTime(QDate(2026, 11, 19), QTime(12, 0), Qt::UTC));
    clock.setRate(10.0);   // ten times real time
    clock.start();
    \endcode

    Times are in UTC. toGpsTime() gives GPS time, which runs ahead of UTC by the leap seconds
    inserted since 1980.

    \differences
    Qt has no simulation clock; this class is part of Nt Twin.
    \enddifferences

    \sa QtTwin::GnssReceiver
*/

/*!
    Constructs a paused clock at the current UTC time, with the given \a parent.
*/
SimulationClock::SimulationClock(QObject *parent)
    : QObject(parent), m_time(QDateTime::currentDateTimeUtc()), m_timer(new QTimer(this)) {
    connect(m_timer, &QTimer::timeout, this, &SimulationClock::tick);
}

/*!
    Destroys the clock.
*/
SimulationClock::~SimulationClock() = default;

/*!
    Returns the simulation time in UTC.
*/
QDateTime SimulationClock::time() const { return m_time; }

/*!
    Sets the simulation time to \a time, converted to UTC.
*/
void SimulationClock::setTime(const QDateTime &time) {
    const QDateTime utc = time.toUTC();
    if (utc == m_time || !utc.isValid())
        return;
    m_time = utc;
    m_carry = 0.0;
    emit timeChanged(m_time);
}

/*!
    \fn double QtTwin::SimulationClock::rate() const
    Returns how many seconds of simulation time pass per second of wall-clock time while the clock
    runs.
*/

/*!
    Sets the rate to \a rate seconds of simulation time per second of wall-clock time. Negative
    rates are taken as 0.
*/
void SimulationClock::setRate(double rate) {
    const double r = std::isfinite(rate) ? std::max(0.0, rate) : 0.0;
    if (r == m_rate)
        return;
    tick(); // account for the time at the old rate
    m_rate = r;
    emit rateChanged(m_rate);
}

/*!
    \fn bool QtTwin::SimulationClock::isRunning() const
    Returns \c true if the clock advances with wall-clock time; otherwise returns \c false.
*/

/*!
    Sets the interval at which a running clock updates, and emits timeChanged(), to \a msec
    milliseconds of wall-clock time. The default is 10.
*/
void SimulationClock::setTickInterval(int msec) {
    m_tickInterval = std::max(1, msec);
    if (m_running)
        m_timer->start(m_tickInterval);
}

/*!
    \fn int QtTwin::SimulationClock::tickInterval() const
    Returns the update interval of a running clock in milliseconds.
*/

/*!
    Returns the number of seconds by which GPS time is ahead of UTC at \a utc: the leap seconds
    inserted since the GPS epoch, 6 January 1980. It is 18 since 1 January 2017, and 0 before the
    epoch.
*/
int SimulationClock::leapSeconds(const QDateTime &utc) {
    static const QDate insertions[] = {
        QDate(1981, 7, 1), QDate(1982, 7, 1), QDate(1983, 7, 1), QDate(1985, 7, 1), QDate(1988, 1, 1),
        QDate(1990, 1, 1), QDate(1991, 1, 1), QDate(1992, 7, 1), QDate(1993, 7, 1), QDate(1994, 7, 1),
        QDate(1996, 1, 1), QDate(1997, 7, 1), QDate(1999, 1, 1), QDate(2006, 1, 1), QDate(2009, 1, 1),
        QDate(2012, 7, 1), QDate(2015, 7, 1), QDate(2017, 1, 1),
    };
    const QDate day = utc.toUTC().date();
    int count = 0;
    for (const QDate &insertion : insertions) {
        if (day >= insertion)
            ++count;
    }
    return count;
}

/*!
    Returns GPS time at \a utc: \a utc plus leapSeconds(). The result is labelled UTC, since
    QDateTime has no GPS time scale.
*/
QDateTime SimulationClock::toGpsTime(const QDateTime &utc) { return utc.toUTC().addSecs(leapSeconds(utc)); }

/*!
    Starts advancing the clock with wall-clock time.
*/
void SimulationClock::start() {
    if (m_running)
        return;
    m_running = true;
    m_wall.start();
    m_wallAtLastTick = 0;
    m_timer->start(m_tickInterval);
    emit runningChanged(true);
}

/*!
    Stops advancing the clock with wall-clock time. advance() and setTime() still move it.
*/
void SimulationClock::pause() {
    if (!m_running)
        return;
    tick();
    m_running = false;
    m_timer->stop();
    emit runningChanged(false);
}

/*!
    Moves the clock forward by \a msec milliseconds of simulation time, or back for a negative
    value, and emits timeChanged().
*/
void SimulationClock::advance(qint64 msec) {
    if (msec == 0)
        return;
    m_time = m_time.addMSecs(msec);
    emit timeChanged(m_time);
}

/*!
    \fn void QtTwin::SimulationClock::timeChanged(const QDateTime &time)
    This signal is emitted when the simulation time changes to \a time.
*/

/*!
    \fn void QtTwin::SimulationClock::rateChanged(double rate)
    This signal is emitted when the rate changes to \a rate.
*/

/*!
    \fn void QtTwin::SimulationClock::runningChanged(bool running)
    This signal is emitted when the clock starts or pauses; \a running is the new state.
*/

void SimulationClock::tick() {
    if (!m_running)
        return;
    const qint64 now = m_wall.elapsed();
    const double simulated = static_cast<double>(now - m_wallAtLastTick) * m_rate + m_carry;
    m_wallAtLastTick = now;
    const double whole = std::floor(simulated);
    m_carry = simulated - whole;
    advance(static_cast<qint64>(whole));
}

} // namespace QtTwin

#include "moc_qtwinclock.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
