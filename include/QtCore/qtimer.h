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
public:
    explicit QTimer(QObject *parent = nullptr);
    ~QTimer() override;

    bool isActive() const;
    int timerId() const;

    void setInterval(int msec);
    void setInterval(std::chrono::milliseconds interval);
    int interval() const;
    std::chrono::milliseconds intervalAsDuration() const;

    void setSingleShot(bool singleShot);
    bool isSingleShot() const;

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
};
