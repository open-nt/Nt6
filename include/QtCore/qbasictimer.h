// MIT License - see LICENSE
#pragma once

#include <utility>

#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>

/*!
    \class QBasicTimer
    \brief The QBasicTimer class provides timer events for objects.

    A QBasicTimer is a lightweight wrapper around QObject::startTimer() and
    QObject::killTimer(). Unlike QTimer it emits no signal; the object passed
    to start() receives a QTimerEvent in QObject::timerEvent(). The timer is
    stopped when the QBasicTimer is destroyed, or if the object is deleted.

    \differences
    The \c Qt::TimerType argument is accepted but ignored. There is no
    \c remainingTime().
    \enddifferences

    \sa QTimer, QTimerEvent, QObject::startTimer()
*/
class QBasicTimer {
public:
    /*!
        Constructs a timer that is not running.
    */
    QBasicTimer() noexcept = default;
    /*!
        Destroys the timer, stopping it if it is running.
    */
    ~QBasicTimer() { stop(); }
    /*!
        \since 5.14
        Exchanges the timer of this object with that of \a other.
    */
    void swap(QBasicTimer &other) noexcept {
        m_object.swap(other.m_object);
        std::swap(m_id, other.m_id);
    }
    /*!
        Move-constructs a timer from \a o, taking over its running timer and
        leaving \a o stopped.
    */
    QBasicTimer(QBasicTimer &&o) noexcept : m_object(o.m_object), m_id(o.m_id) {
        o.m_object = nullptr;
        o.m_id = 0;
    }
    /*!
        Move-assigns \a o to this timer. This timer is stopped first, then it
        takes over the running timer of \a o, which is left stopped.
    */
    QBasicTimer &operator=(QBasicTimer &&o) noexcept {
        if (this != &o) {
            stop();
            m_object = o.m_object;
            m_id = o.m_id;
            o.m_object = nullptr;
            o.m_id = 0;
        }
        return *this;
    }

    /*!
        Returns \c true if the timer is running; otherwise returns \c false.

        \sa timerId()
    */
    bool isActive() const noexcept { return m_id != 0; }
    /*!
        Returns the identifier of the timer, or 0 if it is not running. The
        identifier is the one delivered in QTimerEvent::timerId().
    */
    int timerId() const noexcept { return m_id; }
    /*!
        \since 6.8
        Returns the identifier of the timer as a Qt::TimerId, or
        \c Qt::TimerId::Invalid if it is not running.

        \sa timerId()
    */
    Qt::TimerId id() const noexcept { return static_cast<Qt::TimerId>(m_id); }
    /*!
        Starts the timer with a timeout of \a msec milliseconds. \a object
        receives a QTimerEvent each time the timer fires. A running timer is
        stopped first. If \a msec is negative or \a object is \c nullptr the
        timer does not start.

        \sa stop()
    */
    void start(int msec, QObject *object) {
        stop();
        m_object = object;
        m_id = object ? object->startTimer(msec) : 0;
    }
    /*!
        \overload
        Starts the timer with a timeout of \a msec milliseconds. The timer
        type is ignored.
    */
    void start(int msec, Qt::TimerType, QObject *object) { start(msec, object); }
    /*!
        \overload
        Starts the timer with the timeout \a interval, sending the timer
        events to \a object.
    */
    void start(std::chrono::milliseconds interval, QObject *object) {
        start(static_cast<int>(interval.count()), object);
    }
    /*!
        \overload
        Starts the timer with the timeout \a interval. The timer type is
        ignored.
    */
    void start(std::chrono::milliseconds interval, Qt::TimerType, QObject *object) { start(interval, object); }
    /*!
        Stops the timer. Does nothing if it is not running.

        \sa start()
    */
    void stop() {
        if (m_id != 0 && m_object)
            m_object->killTimer(m_id);
        m_id = 0;
        m_object = nullptr;
    }

private:
    QPointer<QObject> m_object;
    int m_id = 0;
};
