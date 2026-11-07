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
    Only the event types in QEvent::Type exist. There is no \c clone(),
    \c spontaneous() or \c QEvent::isInputEvent(), and events cannot be
    copied.
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
    Destroys the event.
*/
QEvent::~QEvent() = default;

/*!
    Destroys the timer event.
*/
QTimerEvent::~QTimerEvent() = default;

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
