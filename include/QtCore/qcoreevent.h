// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

class QEvent {
public:
    /*!
        \enum QEvent::Type

        This enum describes the kinds of event Nt delivers.

        \value None Not an event.
        \value Timer A timer fired; delivered as a QTimerEvent.
        \value MetaCall A queued signal-slot invocation, handled by QObject::event().
        \value SockAct A socket notifier became active; see QSocketNotifier.
        \value DeferredDelete The receiver should be deleted; posted by QObject::deleteLater().
        \value User The first value available for application-defined events.
        \value MaxUser The last value available for application-defined events.
    */
    enum Type {
        None = 0,
        Timer = 1,
        MetaCall = 43,
        SockAct = 50,
        DeferredDelete = 52,
        User = 1000,
        MaxUser = 65535
    };

    /*!
        Constructs an event of the given \a type.
    */
    explicit QEvent(Type type) : t(type) {}
    virtual ~QEvent();
    Q_DISABLE_COPY(QEvent)

    /*!
        Returns the event type.
    */
    Type type() const { return t; }
    /*!
        Returns \c true if the event has been accepted; otherwise returns
        \c false. Events are accepted when constructed.

        \sa setAccepted(), accept(), ignore()
    */
    bool isAccepted() const { return accepted; }
    /*!
        Sets the accept flag to \a a.

        \sa isAccepted()
    */
    void setAccepted(bool a) { accepted = a; }
    /*!
        Marks the event as accepted. This is the same as
        \c{setAccepted(true)}.

        \sa ignore()
    */
    void accept() { accepted = true; }
    /*!
        Marks the event as ignored, so the receiver does not handle it. This
        is the same as \c{setAccepted(false)}.

        \sa accept()
    */
    void ignore() { accepted = false; }

    static int registerEventType(int hint = -1);

private:
    Type t;
    bool accepted = true;
};

class QTimerEvent : public QEvent {
public:
    /*!
        Constructs a timer event for the timer with the identifier
        \a timerId.
    */
    explicit QTimerEvent(int timerId) : QEvent(Timer), id(timerId) {}
    ~QTimerEvent() override;
    /*!
        Returns the identifier of the timer that fired, as returned by
        QObject::startTimer().
    */
    int timerId() const { return id; }

private:
    int id;
};
