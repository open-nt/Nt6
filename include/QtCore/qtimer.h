// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>

#include <chrono>

class QTimer;

namespace QtPrivate {
// Makes `timer`, which was just created in the calling thread, live where `context` lives.
void placeSingleShotTimer(QTimer *timer, QObject *context);
} // namespace QtPrivate

class QTimer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool singleShot READ isSingleShot WRITE setSingleShot)
    Q_PROPERTY(int interval READ interval WRITE setInterval)
    Q_PROPERTY(bool active READ isActive)
    Q_PROPERTY(int remainingTime READ remainingTime)
    Q_PROPERTY(Qt::TimerType timerType READ timerType WRITE setTimerType)
public:
    explicit QTimer(QObject *parent = nullptr);
    ~QTimer() override;

    bool isActive() const;
    int timerId() const;

    void setInterval(int msec);
    void setInterval(std::chrono::milliseconds interval);
    int interval() const;
    std::chrono::milliseconds intervalAsDuration() const;

    int remainingTime() const;
    std::chrono::milliseconds remainingTimeAsDuration() const;

    void setSingleShot(bool singleShot);
    bool isSingleShot() const;

    void setTimerType(Qt::TimerType type);
    Qt::TimerType timerType() const;

    /*!
        \since 5.12
        Connects timeout() to \a functor, which is called each time the timer
        fires, with the connection \a type. Returns the connection.

        \sa timeout()
    */
    template <typename Func>
    QMetaObject::Connection callOnTimeout(Func &&functor, Qt::ConnectionType type = Qt::AutoConnection) {
        return QObject::connect(this, &QTimer::timeout, std::forward<Func>(functor), type);
    }

    /*!
        \since 5.12
        \overload

        Connects timeout() to \a slot, a member function of \a receiver or a
        functor run in the thread of \a receiver, with the connection \a type.
        The connection is removed when \a receiver is destroyed.
    */
    template <typename RecvT, typename Func>
    QMetaObject::Connection callOnTimeout(const RecvT *receiver, Func &&slot,
                                          Qt::ConnectionType type = Qt::AutoConnection) {
        return QObject::connect(this, &QTimer::timeout, receiver, std::forward<Func>(slot), type);
    }

    /*!
        \overload
        Calls \a functor once, after \a interval.
    */
    template <typename Func> static void singleShot(std::chrono::milliseconds interval, Func &&functor) {
        singleShot(static_cast<int>(interval.count()), std::forward<Func>(functor));
    }

    /*!
        \overload
        Calls \a functor once, after \a interval, unless \a context is destroyed
        first.
    */
    template <typename RecvT, typename Func>
    static void singleShot(std::chrono::milliseconds interval, const RecvT *context, Func &&functor) {
        singleShot(static_cast<int>(interval.count()), context, std::forward<Func>(functor));
    }

    /*!
        Calls \a functor once, \a msec milliseconds from now.

        The call is made from the event loop. It cannot be cancelled; use the
        context overload if the target may be destroyed first.
    */
    template <typename Func>
    static void singleShot(int msec, Func &&functor) {
        auto *t = new QTimer;
        t->setSingleShot(true);
        QObject::connect(t, &QTimer::timeout, t,
                         [t, f = std::decay_t<Func>(std::forward<Func>(functor))]() mutable {
                             t->deleteLater();
                             f();
                         });
        t->start(msec);
    }

    /*!
        \overload

        Calls \a functor once, \a msec milliseconds from now, unless \a context
        is destroyed first. The functor runs in the thread that \a context lives
        in, so this also runs it in another thread, as long as that thread has an
        event loop.
    */
    template <typename RecvT, typename Func>
    static void singleShot(int msec, const RecvT *context, Func &&functor) {
        auto *t = new QTimer;
        t->setSingleShot(true);
        QObject::connect(t, &QTimer::timeout, context, std::forward<Func>(functor));
        QObject::connect(t, &QTimer::timeout, t, &QObject::deleteLater);
        t->start(msec);
        QtPrivate::placeSingleShotTimer(t, const_cast<RecvT *>(context));
    }

public slots:
    void start();
    void start(int msec);
    void start(std::chrono::milliseconds interval);
    void stop();

signals:
    /*!
        This signal is emitted each time the timer's interval elapses. A
        single-shot timer is already stopped when the signal is emitted.

        \sa start(), setInterval()
    */
    void timeout() { emitSignal(&QTimer::timeout); }

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    int m_id = 0;
    int m_interval = 0;
    bool m_single = false;
    Qt::TimerType m_type = Qt::CoarseTimer;
    std::chrono::steady_clock::time_point m_deadline;
};
