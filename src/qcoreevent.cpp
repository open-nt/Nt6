// MIT License - see LICENSE
#include <QtCore/qcoreevent.h>

#include <atomic>
#include <mutex>
#include <set>

/*!
    \class QEvent
    \brief The QEvent class is the base class of all event classes.

    Events are delivered to QObject::event() by QCoreApplication::sendEvent()
    or QCoreApplication::postEvent(). A QEvent carries its type() and an
    accept flag, and subclasses add the data for a specific kind of event.

    \differences
    Only the event types in QEvent::Type exist. spontaneous() always
    returns \c false, and isInputEvent(), isPointerEvent() and
    isSinglePointEvent() are decided by the event type.
    \enddifferences

    \sa QObject::event(), QTimerEvent, QCoreApplication
*/

/*!
    \class QTimerEvent
    \brief The QTimerEvent class contains parameters that describe a timer event.

    A timer event of type \c QEvent::Timer is sent to a QObject when one of
    its timers, started with QObject::startTimer(), fires. Handle it by
    reimplementing QObject::timerEvent().

    \differences
    None beyond QEvent: the event has no \c{TimerType}.
    \enddifferences

    \sa QObject::timerEvent(), QObject::startTimer(), QEvent
*/

/*!
    \class QDynamicPropertyChangeEvent
    \brief The QDynamicPropertyChangeEvent class contains event parameters for dynamic property changes.

    QObject::setProperty() sends this event of type \c QEvent::DynamicPropertyChange to the object
    when a dynamic property is added, changed or removed. Handle it by reimplementing
    QObject::event():

    \code
    bool Widget::event(QEvent *e) {
        if (e->type() == QEvent::DynamicPropertyChange)
            qDebug() << static_cast<QDynamicPropertyChangeEvent *>(e)->propertyName();
        return QObject::event(e);
    }
    \endcode

    \differences
    None.
    \enddifferences

    \sa QObject::setProperty(), QObject::dynamicPropertyNames(), QEvent
*/

/*!
    \class QChildEvent
    \brief The QChildEvent class contains event parameters for child object events.

    An object receives a \c ChildAdded event when it becomes the parent of
    another object, and a \c ChildRemoved event when a child is removed or
    destroyed. QObject::event() passes both to QObject::childEvent().

    \code
    void Tracker::childEvent(QChildEvent *event) {
        if (event->added())
            qDebug() << "new child" << event->child();
    }
    \endcode

    \differences
    \c ChildPolished is defined, but Nt never sends it.
    \enddifferences

    \sa QObject::childEvent(), QObject::setParent()
*/

/*!
    Destroys the event.
*/
QEvent::~QEvent() = default;

/*!
    \since 6.0
    Returns a copy of this event, allocated with \c new; the caller owns it.
    Each event class reimplements clone() to copy itself.
*/
QEvent *QEvent::clone() const { return new QEvent(*this); }

/*!
    \since 6.0
    Returns \c true if the event is a mouse, wheel or key event; otherwise
    returns \c false.

    \sa isPointerEvent()
*/
bool QEvent::isInputEvent() const noexcept {
    switch (t) {
    case KeyPress:
    case KeyRelease:
        return true;
    default:
        return isPointerEvent();
    }
}

/*!
    \since 6.0
    Returns \c true if the event is a mouse or wheel event; otherwise returns
    \c false.

    \sa isSinglePointEvent()
*/
bool QEvent::isPointerEvent() const noexcept { return isSinglePointEvent(); }

/*!
    \since 6.0
    Returns \c true if the event comes from a pointing device with one
    point, as mouse and wheel events do; otherwise returns \c false.
*/
bool QEvent::isSinglePointEvent() const noexcept {
    switch (t) {
    case MouseButtonPress:
    case MouseButtonRelease:
    case MouseButtonDblClick:
    case MouseMove:
    case Wheel:
        return true;
    default:
        return false;
    }
}

/*!
    Destroys the child event.
*/
QChildEvent::~QChildEvent() = default;

/*!
    Destroys the timer event.
*/
QTimerEvent::~QTimerEvent() = default;

/*!
    Destroys the event.
*/
QDynamicPropertyChangeEvent::~QDynamicPropertyChangeEvent() = default;

/*!
    Registers and returns a custom event type. If \a hint lies between
    \c QEvent::User and \c QEvent::MaxUser and has not been handed out
    before, it is returned. Otherwise an unused type is allocated, counting
    down from \c QEvent::MaxUser. Returns \c -1 if no type is available.

    The function is thread-safe.
*/
int QEvent::registerEventType(int hint) {
    static std::mutex m;
    static std::set<int> used;
    static int next = QEvent::MaxUser;

    std::lock_guard<std::mutex> lk(m);
    if (hint >= QEvent::User && hint <= QEvent::MaxUser && used.insert(hint).second)
        return hint;
    while (next >= QEvent::User && used.count(next))
        --next;
    if (next < QEvent::User)
        return -1;
    used.insert(next);
    return next--;
}
