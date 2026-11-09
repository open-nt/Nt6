// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>

#include <chrono>

/*!
    \class QChronoTimer
    \since 6.8
    \brief The QChronoTimer class provides repetitive and single-shot timers with intervals given as std::chrono durations.

    A QChronoTimer is like QTimer, but its interval is a
    \c std::chrono::nanoseconds duration:

    \code
    using namespace std::chrono_literals;
    QChronoTimer timer(250ms);
    QObject::connect(&timer, &QChronoTimer::timeout, [] { poll(); });
    timer.start();
    \endcode

    \differences
    The event dispatcher of Nt works in milliseconds, so the interval is
    rounded up to whole milliseconds when the timer runs, and the timer type
    is recorded but does not change the precision. There is no \c singleShot()
    static function and no \c bindable properties.
    \enddifferences

    \sa QTimer
*/
class QChronoTimer : public QObject {
    Q_OBJECT
public:
    explicit QChronoTimer(QObject *parent = nullptr);
    explicit QChronoTimer(std::chrono::nanoseconds nsec, QObject *parent = nullptr);
    ~QChronoTimer() override;

    void setInterval(std::chrono::nanoseconds nsec);
    std::chrono::nanoseconds interval() const;
    std::chrono::nanoseconds remainingTime() const;
    void setSingleShot(bool singleShot);
    bool isSingleShot() const;
    void setTimerType(Qt::TimerType atype);
    Qt::TimerType timerType() const;
    bool isActive() const;
    int id() const;

public slots:
    void start();
    void stop();

signals:
    /*!
        This signal is emitted when the timer times out.

        \sa interval(), start()
    */
    void timeout() { emitSignal(&QChronoTimer::timeout); }

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    int intervalMs() const;
    std::chrono::nanoseconds m_interval{0};
    std::chrono::steady_clock::time_point m_started;
    int m_id = 0;
    bool m_singleShot = false;
    Qt::TimerType m_type = Qt::CoarseTimer;
};
