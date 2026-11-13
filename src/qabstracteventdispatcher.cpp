// MIT License - see LICENSE
#include <QtCore/qabstracteventdispatcher.h>

#include "dispatcher.h"
#include "qabstracteventdispatcher_p.h"
#include "qobject_p.h"
#include "threaddata_p.h"

#include <QtCore/qabstractnativeeventfilter.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qthread.h>

#include <algorithm>
#include <chrono>
#include <climits>
#include <mutex>
#include <vector>

namespace {

std::mutex &viewLock() {
    static std::mutex *m = new std::mutex;
    return *m;
}

// The native event filters of all threads, the most recently installed first. One list for all threads lets a
// filter remove itself from every dispatcher when it is destroyed.
struct NativeFilter {
    QtPrivate::ThreadData *thread;
    QAbstractNativeEventFilter *filter;
};

std::mutex &filterLock() {
    static std::mutex *m = new std::mutex;
    return *m;
}

std::vector<NativeFilter> &nativeFilters() {
    static auto *list = new std::vector<NativeFilter>;
    return *list;
}

// Removes the entries that match; a null argument matches everything. Must be called with filterLock() held.
void eraseNativeFilters(const QtPrivate::ThreadData *td, const QAbstractNativeEventFilter *filter) {
    std::vector<NativeFilter> &list = nativeFilters();
    list.erase(std::remove_if(
                   list.begin(), list.end(),
                   [&](const NativeFilter &f) { return (!td || f.thread == td) && (!filter || f.filter == filter); }),
               list.end());
}

bool isNativeFilterInstalled(const QtPrivate::ThreadData *td, const QAbstractNativeEventFilter *filter) {
    const std::vector<NativeFilter> &list = nativeFilters();
    return std::any_of(list.begin(), list.end(),
                       [&](const NativeFilter &f) { return f.thread == td && f.filter == filter; });
}

} // namespace

namespace QtPrivate {

void destroyEventDispatcher(ThreadData *td) {
    td->dispatcher.setCustom(nullptr);
    QAbstractEventDispatcher *view = nullptr;
    {
        std::lock_guard<std::mutex> lk(viewLock());
        if (td->eventDispatcher && td->eventDispatcherOwned) {
            view = td->eventDispatcher;
            td->eventDispatcher = nullptr;
            td->dispatcher.setObserver(nullptr);
        }
    }
    delete view;
    std::lock_guard<std::mutex> lk(filterLock());
    eraseNativeFilters(td, nullptr);
}

} // namespace QtPrivate

/*!
    \class QAbstractNativeEventFilter
    \brief The QAbstractNativeEventFilter class provides an interface for receiving native events, such as
    the events of the window system.

    Reimplement nativeEventFilter() and install the filter with QCoreApplication::installNativeEventFilter()
    or QAbstractEventDispatcher::installNativeEventFilter(). The filter then sees each native event before Nt
    handles it, and can stop it:

    \code
    class XcbFilter : public QAbstractNativeEventFilter {
    public:
        bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) override {
            if (eventType == "xcb_generic_event_t") {
                auto *event = static_cast<xcb_generic_event_t *>(message);
                // ...
            }
            return false;
        }
    };
    \endcode

    The \c xcb platform of Nt GUI passes every event that it reads from the X server, with the type
    \c "xcb_generic_event_t" and an \c{xcb_generic_event_t *} as the message.

    \differences
    Destroying a filter removes it from the dispatchers of all threads, not only of the current one.
    \enddifferences

    \sa QAbstractEventDispatcher::filterNativeEvent()
*/

/*!
    Constructs a native event filter. It does nothing until it is installed.
*/
QAbstractNativeEventFilter::QAbstractNativeEventFilter() = default;

/*!
    Destroys the native event filter and removes it from every event dispatcher that it is installed in.
*/
QAbstractNativeEventFilter::~QAbstractNativeEventFilter() {
    std::lock_guard<std::mutex> lk(filterLock());
    eraseNativeFilters(nullptr, this);
}

/*!
    \fn bool QAbstractNativeEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)

    This function is called for each native event, with the type of the event in \a eventType and the event
    itself in \a message. Return \c true to stop the event, so that Nt does not handle it, or \c false to
    let it through. A platform that returns a value for its events reads it from \a result.

    What \a message points to depends on \a eventType: the \c xcb platform passes an
    \c{xcb_generic_event_t *} with the type \c "xcb_generic_event_t".

    \sa QAbstractEventDispatcher::filterNativeEvent()
*/

namespace QtPrivate {

int startTimer(ThreadData *td, QObject *object, std::chrono::milliseconds interval) {
    if (QAbstractEventDispatcher *custom = td->dispatcher.custom()) {
        const int id = QAbstractEventDispatcher::allocateTimerId();
        custom->registerTimer(id, static_cast<qint64>(interval.count()), Qt::CoarseTimer, object);
        return id;
    }
    return td->dispatcher.startTimer(object, interval);
}

bool killTimer(ThreadData *td, int id) {
    if (QAbstractEventDispatcher *custom = td->dispatcher.custom())
        return custom->unregisterTimer(id);
    return td->dispatcher.killTimer(id);
}

void killTimers(ThreadData *td, QObject *object) {
    if (QAbstractEventDispatcher *custom = td->dispatcher.custom())
        custom->unregisterTimers(object);
    else
        td->dispatcher.killTimers(object);
}

bool processEvents(ThreadData *td, QEventLoop::ProcessEventsFlags flags, const std::atomic<bool> *stop) {
    if (QAbstractEventDispatcher *custom = td->dispatcher.custom())
        return custom->processEvents(flags);
    return td->dispatcher.process(flags.testFlag(QEventLoop::WaitForMoreEvents), stop);
}

void updateNotifier(ThreadData *td, QSocketNotifier *notifier, bool active) {
    if (QAbstractEventDispatcher *custom = td->dispatcher.custom()) {
        if (active)
            custom->registerSocketNotifier(notifier);
        else
            custom->unregisterSocketNotifier(notifier);
    } else if (active) {
        td->dispatcher.registerNotifier(notifier, static_cast<int>(notifier->socket()),
                                        static_cast<int>(notifier->type()));
    } else {
        td->dispatcher.unregisterNotifier(notifier);
    }
}

void removeNotifier(ThreadData *td, QSocketNotifier *notifier) { td->dispatcher.removeNotifier(notifier); }

namespace {

// Registers a timer under an identifier that it already has, in the thread that `object` lives in.
void restoreTimer(QObject *object, int id, qint64 interval, Qt::TimerType type) {
    ThreadData *td = objectThreadData(object);
    if (QAbstractEventDispatcher *custom = td->dispatcher.custom())
        custom->registerTimer(id, interval, type, object);
    else
        td->dispatcher.startTimerWithId(id, object, std::chrono::milliseconds(interval));
}

} // namespace

void migrateEvents(ThreadData *from, ThreadData *to, const std::vector<QObject *> &objects) {
    QAbstractEventDispatcher *source = from->dispatcher.custom();
    QAbstractEventDispatcher *target = to->dispatcher.custom();
    if (!source && !target) {
        from->dispatcher.migrate(to->dispatcher, objects);
        return;
    }
    // An installed dispatcher is only used from its own thread, so the timers and notifiers leave it here and join
    // the target dispatcher from the target thread, with their identifiers kept.
    struct Moved {
        QObject *object;
        QList<QAbstractEventDispatcher::TimerInfo> timers;
        bool notifier;
    };
    std::vector<Moved> moved;
    for (QObject *o : objects) {
        Moved m{o, {}, false};
        if (source) {
            m.timers = source->registeredTimers(o);
            if (!m.timers.isEmpty())
                source->unregisterTimers(o);
        } else {
            for (const auto &t : from->dispatcher.timersOf(o))
                m.timers.append(
                    {t.id, static_cast<int>(std::min<long long>(t.interval.count(), INT_MAX)), Qt::CoarseTimer});
            from->dispatcher.killTimers(o);
        }
        auto *n = qobject_cast<QSocketNotifier *>(o);
        if (n && n->isEnabled() && n->isValid()) {
            m.notifier = true;
            if (source)
                source->unregisterSocketNotifier(n);
        }
        if (n)
            from->dispatcher.removeNotifier(n);
        if (!m.timers.isEmpty() || m.notifier)
            moved.push_back(std::move(m));
    }
    // What is left are the posted events.
    from->dispatcher.migrate(to->dispatcher, objects);
    for (Moved &m : moved) {
        auto restore = [object = m.object, timers = m.timers, notifier = m.notifier] {
            for (const auto &t : timers)
                restoreTimer(object, t.timerId, t.interval, t.timerType);
            auto *n = static_cast<QSocketNotifier *>(object);
            if (notifier && n->isEnabled() && n->isValid())
                updateNotifier(objectThreadData(object), n, true);
        };
        if (target)
            QMetaObject::invokeMethod(m.object, std::move(restore), Qt::QueuedConnection);
        else
            restore();
    }
}

bool installEventDispatcher(QThread *thread, ThreadData *td, QAbstractEventDispatcher *dispatcher, const char *who) {
    if (!dispatcher)
        return false;
    ThreadData *home = objectThreadData(dispatcher);
    {
        std::lock_guard<std::mutex> lk(viewLock());
        if (td->eventDispatcher && td->eventDispatcher != dispatcher) {
            qWarning("%s: An event dispatcher has already been created for this thread", who);
            return false;
        }
        // The constructor made it the handle of the thread that created it, if that thread had none.
        if (home != td && home->eventDispatcher == dispatcher) {
            home->eventDispatcher = nullptr;
            home->dispatcher.setObserver(nullptr);
        }
    }
    if (home != td) {
        if (thread)
            dispatcher->moveToThread(thread);
        if (objectThreadData(dispatcher) != td) {
            qWarning("%s: Could not move event dispatcher to target thread", who);
            return false;
        }
    }
    {
        std::lock_guard<std::mutex> lk(viewLock());
        td->eventDispatcher = dispatcher;
        td->eventDispatcherOwned = true;
        td->dispatcher.setObserver(dispatcher);
    }
    td->dispatcher.setCustom(dispatcher);
    // Like the handle that instance() makes, it belongs to the thread data and does not keep it alive.
    auto &d = Access::d(dispatcher);
    if (d.ownsThreadRef) {
        d.ownsThreadRef = false;
        td->release();
    }
    return true;
}

void eventDispatcherStarting(ThreadData *td) {
    if (QAbstractEventDispatcher *custom = td->dispatcher.custom())
        custom->startingUp();
}

void eventDispatcherFinished(ThreadData *td) {
    QAbstractEventDispatcher *custom = td->dispatcher.custom();
    if (!custom)
        return;
    custom->closingDown();
    td->dispatcher.setCustom(nullptr);
    {
        std::lock_guard<std::mutex> lk(viewLock());
        if (td->eventDispatcher == custom) {
            td->eventDispatcher = nullptr;
            td->eventDispatcherOwned = false;
            td->dispatcher.setObserver(nullptr);
        }
    }
    delete custom;
}

} // namespace QtPrivate

/*!
    \class QAbstractEventDispatcher
    \brief The QAbstractEventDispatcher class provides an interface to manage Nt's event queue.

    Every thread has an event dispatcher that handles its posted events, timers and socket notifiers.
    QAbstractEventDispatcher gives access to it. Get the one of a thread with instance(), then
    register timers, wake it up when it waits, or connect to aboutToBlock() and awake():

    \code
    QAbstractEventDispatcher *dispatcher = QAbstractEventDispatcher::instance();
    QObject::connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, [] { flushCaches(); });
    \endcode

    \section1 Custom event dispatchers

    To run a thread's events from another event loop, such as the one of a toolkit that Nt is embedded in,
    subclass QAbstractEventDispatcher (or QAbstractEventDispatcherV2) and install the subclass with
    QThread::setEventDispatcher() before the thread starts, or with QCoreApplication::setEventDispatcher() before
    the application object is made. From then on, QEventLoop and QCoreApplication::processEvents() call its
    processEvents(), QObject::startTimer() and QTimer call its registerTimer(), and QSocketNotifier calls its
    registerSocketNotifier() and unregisterSocketNotifier(). Posting an event calls wakeUp(), from the thread that
    posts it.

    A subclass delivers the events itself: it sends a QTimerEvent for each timer that is due, an event of the type
    QEvent::SockAct to each notifier whose socket is ready, and calls QCoreApplication::sendPostedEvents() for the
    posted events. A function that it does not reimplement uses Nt's own dispatcher, so a subclass that only wants
    to watch the timers can reimplement registerTimer() and call the base class:

    \code
    class CountingDispatcher : public QAbstractEventDispatcher {
    public:
        void registerTimer(int id, qint64 interval, Qt::TimerType type, QObject *object) override {
            ++timersStarted;
            QAbstractEventDispatcher::registerTimer(id, interval, type, object);
        }
        int timersStarted = 0;
    };

    QThread worker;
    worker.setEventDispatcher(new CountingDispatcher);
    worker.start();
    \endcode

    The thread owns the installed dispatcher: it calls startingUp() when it starts and closingDown() when it
    finishes, and then deletes the dispatcher. A thread that is started again uses Nt's dispatcher.

    \differences
    The class is not abstract: each virtual function has an implementation that uses Nt's own dispatcher for
    POSIX systems, which a subclass may call. Without an installed subclass a thread has one handle onto that
    dispatcher, which instance() creates on first use and which is deleted with the thread; an object that the
    constructor makes is another handle, and is the one that reports aboutToBlock() and awake() if it was the first
    for its thread. The functions work on the dispatcher of the thread that the object lives in. Nt's dispatcher
    accepts timer types and ignores them, as it does all timers of millisecond precision, and its registeredTimers()
    reports \c Qt::CoarseTimer. flush() does nothing.
    \enddifferences

    \sa QEventLoop, QTimer, QSocketNotifier, QThread::setEventDispatcher()
*/

/*!
    \class QAbstractEventDispatcher::TimerInfo
    \brief The TimerInfo struct describes a timer that is registered with an event dispatcher.

    \sa registeredTimers()
*/

/*!
    \variable QAbstractEventDispatcher::TimerInfo::timerId

    The identifier of the timer.
*/

/*!
    \variable QAbstractEventDispatcher::TimerInfo::interval

    The interval of the timer, in milliseconds.
*/

/*!
    \variable QAbstractEventDispatcher::TimerInfo::timerType

    The type of the timer.
*/

/*!
    \fn QAbstractEventDispatcher::TimerInfo::TimerInfo(int id, int i, Qt::TimerType type)

    Constructs a timer description from the identifier \a id, the interval \a i and the type \a type.
*/

/*!
    \class QAbstractEventDispatcher::TimerInfoV2
    \since 6.8
    \brief The TimerInfoV2 struct describes a timer that is registered with an event dispatcher, with its interval
    as a duration.

    \sa timersForObject()
*/

/*!
    \variable QAbstractEventDispatcher::TimerInfoV2::interval

    The interval of the timer.
*/

/*!
    \variable QAbstractEventDispatcher::TimerInfoV2::timerId

    The identifier of the timer.
*/

/*!
    \variable QAbstractEventDispatcher::TimerInfoV2::timerType

    The type of the timer.
*/

/*!
    Constructs an event dispatcher with the given \a parent. Install it with QThread::setEventDispatcher() or
    QCoreApplication::setEventDispatcher(), or use it as a handle onto the dispatcher of the calling thread; for
    that, instance() is the usual way.

    \sa instance()
*/
QAbstractEventDispatcher::QAbstractEventDispatcher(QObject *parent) : QAbstractEventDispatcher(parent, false) {}

QAbstractEventDispatcher::QAbstractEventDispatcher(QObject *parent, bool v2) : QObject(parent), m_isV2(v2) {
    QtPrivate::ThreadData *td = QtPrivate::objectThreadData(this);
    std::lock_guard<std::mutex> lk(viewLock());
    if (!td->eventDispatcher) {
        td->eventDispatcher = this;
        td->dispatcher.setObserver(this);
    }
}

// Makes the handle of `owner`, which lives in it and is deleted with it.
QAbstractEventDispatcher::QAbstractEventDispatcher(QtPrivate::ThreadData *owner, int) : QObject(nullptr), m_owned(true) {
    auto &d = QtPrivate::Access::d(this);
    QtPrivate::ThreadData *created = d.threadData.load();
    d.threadData.store(owner);
    d.ownsThreadRef = false;
    created->release();
}

/*!
    Destroys the event dispatcher. A thread deletes the dispatcher that was installed in it when it finishes; do
    not delete one that is installed.
*/
QAbstractEventDispatcher::~QAbstractEventDispatcher() {
    QtPrivate::ThreadData *td = QtPrivate::objectThreadData(this);
    if (td->dispatcher.custom() == this)
        td->dispatcher.setCustom(nullptr);
    std::lock_guard<std::mutex> lk(viewLock());
    if (td->eventDispatcher == this) {
        td->eventDispatcher = nullptr;
        td->dispatcher.setObserver(nullptr);
    }
}

/*!
    Returns the event dispatcher of \a thread, or the one of the calling thread if \a thread is \c nullptr: the
    one that was installed with QThread::setEventDispatcher(), or else a handle onto Nt's dispatcher, which is
    created on first use and valid as long as the thread lives. \a thread does not have to be running.

    \sa QThread::currentThread(), QThread::eventDispatcher()
*/
QAbstractEventDispatcher *QAbstractEventDispatcher::instance(QThread *thread) {
    QtPrivate::ThreadData *td = thread ? QtPrivate::ThreadAccess::data(thread) : QtPrivate::ThreadData::current();
    std::lock_guard<std::mutex> lk(viewLock());
    if (!td->eventDispatcher) {
        td->eventDispatcher = new QAbstractEventDispatcher(td, 0);
        td->eventDispatcherOwned = true;
        td->dispatcher.setObserver(td->eventDispatcher);
    }
    return td->eventDispatcher;
}

/*!
    Processes pending events once, according to \a flags, and returns \c true if an event, a timer or a notifier
    was handled. With \c QEventLoop::WaitForMoreEvents it blocks until there is something to handle, or wakeUp()
    or interrupt() is called. It has to be called from the thread that the dispatcher belongs to.

    QEventLoop::exec() calls it with \c QEventLoop::WaitForMoreEvents and \c QEventLoop::EventLoopExec until the
    loop is told to exit. A reimplementation delivers the posted events with QCoreApplication::sendPostedEvents().

    \sa QEventLoop::processEvents(), interrupt()
*/
bool QAbstractEventDispatcher::processEvents(QEventLoop::ProcessEventsFlags flags) {
    QtPrivate::ThreadData *td = QtPrivate::objectThreadData(this);
    if (td != QtPrivate::ThreadData::current()) {
        qWarning("QAbstractEventDispatcher::processEvents: Must be called from the thread of the dispatcher");
        return false;
    }
    return td->dispatcher.process(flags.testFlag(QEventLoop::WaitForMoreEvents), nullptr);
}

/*!
    Starts watching the socket of \a notifier for the notifier's type of activity. QSocketNotifier calls this when
    it is enabled and has a socket; a reimplementation then sends \a notifier an event of the type QEvent::SockAct
    whenever the socket is ready.

    \sa unregisterSocketNotifier(), QSocketNotifier::setEnabled()
*/
void QAbstractEventDispatcher::registerSocketNotifier(QSocketNotifier *notifier) {
    if (notifier && notifier->isValid())
        QtPrivate::objectThreadData(this)->dispatcher.registerNotifier(notifier, static_cast<int>(notifier->socket()),
                                                                       static_cast<int>(notifier->type()));
}

/*!
    Stops watching the socket of \a notifier. QSocketNotifier calls this when it is disabled or destroyed, or gets
    another socket. It does not change QSocketNotifier::isEnabled().

    \sa registerSocketNotifier(), QSocketNotifier::setEnabled()
*/
void QAbstractEventDispatcher::unregisterSocketNotifier(QSocketNotifier *notifier) {
    if (notifier)
        QtPrivate::objectThreadData(this)->dispatcher.unregisterNotifier(notifier);
}

/*!
    Registers a timer with the interval \a interval milliseconds for \a object, which has to live in the
    thread of this dispatcher, and returns its identifier. \a object gets a \c QTimerEvent with the
    identifier each time the timer fires. \a timerType is passed on to the other overload.

    \sa unregisterTimer(), unregisterTimers(), registeredTimers()
*/
int QAbstractEventDispatcher::registerTimer(qint64 interval, Qt::TimerType timerType, QObject *object) {
    const int id = allocateTimerId();
    registerTimer(id, interval, timerType, object);
    return id;
}

/*!
    \since 6.8
    \overload
    Registers a timer with the interval \a interval and the type \a timerType for \a object, and returns its
    identifier.
*/
Qt::TimerId QAbstractEventDispatcher::registerTimer(Duration interval, Qt::TimerType timerType, QObject *object) {
    const auto id = static_cast<Qt::TimerId>(allocateTimerId());
    registerTimer(id, interval, timerType, object);
    return id;
}

/*!
    \overload
    Registers a timer with the identifier \a timerId, which comes from allocateTimerId(), the
    interval \a interval milliseconds and the type \a timerType, for \a object. A reimplementation sends
    \a object a QTimerEvent with \a timerId each time the interval elapses, until the timer is unregistered.
*/
void QAbstractEventDispatcher::registerTimer(int timerId, qint64 interval, Qt::TimerType timerType, QObject *object) {
    Q_UNUSED(timerType)
    QtPrivate::ThreadData *td = QtPrivate::objectThreadData(this);
    if (!object || QtPrivate::objectThreadData(object) != td) {
        qWarning("QAbstractEventDispatcher::registerTimer: The object must live in the thread of the dispatcher");
        return;
    }
    td->dispatcher.startTimerWithId(timerId, object, std::chrono::milliseconds(std::max<qint64>(0, interval)));
}

namespace {

qint64 toMilliseconds(QAbstractEventDispatcher::Duration interval) {
    return std::max<qint64>(0, static_cast<qint64>(std::chrono::ceil<std::chrono::milliseconds>(interval).count()));
}

} // namespace

/*!
    \since 6.8
    \overload
    Registers a timer with the identifier \a timerId, the interval \a interval and the type \a timerType for
    \a object. It calls the reimplementation of QAbstractEventDispatcherV2 if this is one, and otherwise the
    millisecond overload, rounding \a interval up.
*/
void QAbstractEventDispatcher::registerTimer(Qt::TimerId timerId, Duration interval, Qt::TimerType timerType,
                                             QObject *object) {
    if (m_isV2)
        static_cast<QAbstractEventDispatcherV2 *>(this)->registerTimer(timerId, interval, timerType, object);
    else
        registerTimer(static_cast<int>(timerId), toMilliseconds(interval), timerType, object);
}

/*!
    Unregisters the timer \a timerId and returns \c true if it was registered; otherwise returns \c false.

    \sa registerTimer()
*/
bool QAbstractEventDispatcher::unregisterTimer(int timerId) {
    return QtPrivate::objectThreadData(this)->dispatcher.killTimer(timerId);
}

/*!
    \since 6.8
    \overload
    Unregisters the timer \a timerId and returns \c true if it was registered; otherwise returns \c false.
*/
bool QAbstractEventDispatcher::unregisterTimer(Qt::TimerId timerId) {
    if (m_isV2)
        return static_cast<QAbstractEventDispatcherV2 *>(this)->unregisterTimer(timerId);
    return unregisterTimer(static_cast<int>(timerId));
}

/*!
    Unregisters all timers of \a object and returns \c true if there were any; otherwise returns \c false. Every
    QObject calls this as it is destroyed.

    \sa unregisterTimer()
*/
bool QAbstractEventDispatcher::unregisterTimers(QObject *object) {
    return object && QtPrivate::objectThreadData(this)->dispatcher.killTimers(object);
}

/*!
    Returns the timers that are registered for \a object.

    \sa registerTimer(), timersForObject()
*/
QList<QAbstractEventDispatcher::TimerInfo> QAbstractEventDispatcher::registeredTimers(QObject *object) const {
    QList<TimerInfo> out;
    if (!object)
        return out;
    for (const auto &t : QtPrivate::objectThreadData(this)->dispatcher.timersOf(object))
        out.append(TimerInfo(t.id, static_cast<int>(std::min<long long>(t.interval.count(), INT_MAX)), Qt::CoarseTimer));
    return out;
}

/*!
    \since 6.8
    Returns the timers that are registered for \a object, with their intervals as durations.

    \sa registeredTimers()
*/
QList<QAbstractEventDispatcher::TimerInfoV2> QAbstractEventDispatcher::timersForObject(QObject *object) const {
    if (m_isV2)
        return static_cast<const QAbstractEventDispatcherV2 *>(this)->timersForObject(object);
    QList<TimerInfoV2> out;
    for (const TimerInfo &t : registeredTimers(object))
        out.append({std::chrono::milliseconds(t.interval), static_cast<Qt::TimerId>(t.timerId), t.timerType});
    return out;
}

/*!
    Returns the number of milliseconds until the timer \a timerId fires, 0 if it is overdue, or -1 if
    there is no such timer.

    \sa registerTimer()
*/
int QAbstractEventDispatcher::remainingTime(int timerId) {
    const long long ms = QtPrivate::objectThreadData(this)->dispatcher.remainingTime(timerId);
    return static_cast<int>(std::min<long long>(ms, INT_MAX));
}

/*!
    \since 6.8
    \overload
    Returns the time until the timer \a timerId fires, zero if it is overdue, or a negative duration if there is
    no such timer.
*/
QAbstractEventDispatcher::Duration QAbstractEventDispatcher::remainingTime(Qt::TimerId timerId) const {
    if (m_isV2)
        return static_cast<const QAbstractEventDispatcherV2 *>(this)->remainingTime(timerId);
    const int ms = const_cast<QAbstractEventDispatcher *>(this)->remainingTime(static_cast<int>(timerId));
    return ms < 0 ? Duration(-1) : Duration(std::chrono::milliseconds(ms));
}

/*!
    Wakes the dispatcher if it is blocked waiting for events, so that it looks at them again. It can be called
    from any thread, and is called whenever an event is posted to the dispatcher's thread.

    \sa interrupt()
*/
void QAbstractEventDispatcher::wakeUp() { QtPrivate::objectThreadData(this)->dispatcher.wakeBuiltin(); }

/*!
    Makes the processEvents() call that waits for events, or the next one that would, return without waiting.
    It can be called from any thread. QEventLoop::exit() calls it, so that the loop sees that it should exit.

    \sa wakeUp()
*/
void QAbstractEventDispatcher::interrupt() { QtPrivate::objectThreadData(this)->dispatcher.interrupt(); }

/*!
    Does nothing. It exists because other dispatchers flush buffered output here.
*/
void QAbstractEventDispatcher::flush() {}

/*!
    \since 4.4
    Called by the thread that the dispatcher was installed in as it starts, or for the main thread when the
    application object is made, before any event is handled. The implementation does nothing.

    \sa closingDown(), QThread::setEventDispatcher()
*/
void QAbstractEventDispatcher::startingUp() {}

/*!
    \since 4.4
    Called by the thread that the dispatcher was installed in as it finishes, or for the main thread when the
    application object is destroyed, just before the dispatcher is deleted. The implementation does nothing.

    \sa startingUp()
*/
void QAbstractEventDispatcher::closingDown() {}

/*!
    \class QAbstractEventDispatcherV2
    \since 6.8
    \brief The QAbstractEventDispatcherV2 class is the base of event dispatchers that take timer intervals as
    durations.

    A subclass reimplements the overloads that take a Qt::TimerId and a QAbstractEventDispatcher::Duration; the
    overloads of QAbstractEventDispatcher that take an \c int identifier and milliseconds are final here and call
    them. processEventsWithDeadline() lets the dispatcher wait for events up to a deadline.

    \differences
    Like QAbstractEventDispatcher, the class is not abstract: the functions that Qt leaves pure virtual use Nt's
    own dispatcher, which has millisecond precision and so rounds intervals up to whole milliseconds.
    \enddifferences

    \sa QAbstractEventDispatcher, QThread::setEventDispatcher()
*/

/*!
    Constructs an event dispatcher with the given \a parent.
*/
QAbstractEventDispatcherV2::QAbstractEventDispatcherV2(QObject *parent) : QAbstractEventDispatcher(parent, true) {}

/*!
    Destroys the event dispatcher.
*/
QAbstractEventDispatcherV2::~QAbstractEventDispatcherV2() = default;

/*!
    \reimp
    Calls the overload that takes a Qt::TimerId and a duration with \a timerId, \a interval, \a timerType and
    \a object.
*/
void QAbstractEventDispatcherV2::registerTimer(int timerId, qint64 interval, Qt::TimerType timerType, QObject *object) {
    registerTimer(static_cast<Qt::TimerId>(timerId), Duration(std::chrono::milliseconds(interval)), timerType, object);
}

/*!
    Registers a timer with the identifier \a timerId, the interval \a interval and the type \a timerType for
    \a object. A reimplementation sends \a object a QTimerEvent each time the interval elapses. The
    implementation uses Nt's dispatcher, rounding \a interval up to whole milliseconds.
*/
void QAbstractEventDispatcherV2::registerTimer(Qt::TimerId timerId, Duration interval, Qt::TimerType timerType,
                                               QObject *object) {
    QAbstractEventDispatcher::registerTimer(static_cast<int>(timerId), toMilliseconds(interval), timerType, object);
}

/*!
    \reimp
    Calls the overload that takes a Qt::TimerId with \a timerId.
*/
bool QAbstractEventDispatcherV2::unregisterTimer(int timerId) {
    return unregisterTimer(static_cast<Qt::TimerId>(timerId));
}

/*!
    Unregisters the timer \a timerId and returns \c true if it was registered; otherwise returns \c false.
*/
bool QAbstractEventDispatcherV2::unregisterTimer(Qt::TimerId timerId) {
    return QAbstractEventDispatcher::unregisterTimer(static_cast<int>(timerId));
}

/*!
    \reimp
    Returns the timers of \a object that timersForObject() reports, with their intervals in milliseconds.
*/
QList<QAbstractEventDispatcher::TimerInfo> QAbstractEventDispatcherV2::registeredTimers(QObject *object) const {
    QList<TimerInfo> out;
    for (const TimerInfoV2 &t : timersForObject(object)) {
        const auto ms = std::chrono::ceil<std::chrono::milliseconds>(t.interval).count();
        out.append(
            TimerInfo(static_cast<int>(t.timerId), static_cast<int>(std::min<long long>(ms, INT_MAX)), t.timerType));
    }
    return out;
}

/*!
    Returns the timers that are registered for \a object.
*/
QList<QAbstractEventDispatcher::TimerInfoV2> QAbstractEventDispatcherV2::timersForObject(QObject *object) const {
    QList<TimerInfoV2> out;
    for (const TimerInfo &t : QAbstractEventDispatcher::registeredTimers(object))
        out.append({std::chrono::milliseconds(t.interval), static_cast<Qt::TimerId>(t.timerId), t.timerType});
    return out;
}

/*!
    \reimp
    Returns the time that the overload taking a Qt::TimerId reports for \a timerId, in whole milliseconds rounded
    up, or -1.
*/
int QAbstractEventDispatcherV2::remainingTime(int timerId) {
    const Duration left = remainingTime(static_cast<Qt::TimerId>(timerId));
    if (left < Duration::zero())
        return -1;
    return static_cast<int>(std::min<long long>(std::chrono::ceil<std::chrono::milliseconds>(left).count(), INT_MAX));
}

/*!
    Returns the time until the timer \a timerId fires, zero if it is overdue, or a negative duration if there is
    no such timer.
*/
QAbstractEventDispatcher::Duration QAbstractEventDispatcherV2::remainingTime(Qt::TimerId timerId) const {
    const long long ms = QtPrivate::objectThreadData(this)->dispatcher.remainingTime(static_cast<int>(timerId));
    return ms < 0 ? Duration(-1) : Duration(std::chrono::milliseconds(ms));
}

/*!
    Processes pending events once, according to \a flags, and returns \c true if something was handled. With
    \c QEventLoop::WaitForMoreEvents it waits for events until \a deadline expires at the latest. The
    implementation uses Nt's dispatcher.

    \sa processEvents()
*/
bool QAbstractEventDispatcherV2::processEventsWithDeadline(QEventLoop::ProcessEventsFlags flags,
                                                           QDeadlineTimer deadline) {
    QtPrivate::ThreadData *td = QtPrivate::objectThreadData(this);
    if (td != QtPrivate::ThreadData::current()) {
        qWarning("QAbstractEventDispatcherV2::processEventsWithDeadline: Must be called from the thread of the "
                 "dispatcher");
        return false;
    }
    auto until = QtPrivate::Dispatcher::Clock::time_point::max();
    if (!deadline.isForever()) {
        const qint64 left = deadline.remainingTime();
        until = QtPrivate::Dispatcher::Clock::now() + std::chrono::milliseconds(std::max<qint64>(0, left));
    }
    return td->dispatcher.process(flags.testFlag(QEventLoop::WaitForMoreEvents), nullptr, until);
}

/*!
    \since 5.0
    Installs \a filterObj, so that filterNativeEvent() passes it the native events of this dispatcher's thread.
    The filter that was installed last is called first. Installing a filter again moves it to the front.

    A filter can be installed and removed from any thread, and can remove itself while it filters.

    \sa removeNativeEventFilter(), QCoreApplication::installNativeEventFilter()
*/
void QAbstractEventDispatcher::installNativeEventFilter(QAbstractNativeEventFilter *filterObj) {
    if (!filterObj)
        return;
    QtPrivate::ThreadData *td = QtPrivate::objectThreadData(this);
    std::lock_guard<std::mutex> lk(filterLock());
    eraseNativeFilters(td, filterObj);
    nativeFilters().insert(nativeFilters().begin(), NativeFilter{td, filterObj});
}

/*!
    \since 5.0
    Removes \a filterObj from this dispatcher's thread. Nothing happens if it is not installed there.

    \sa installNativeEventFilter()
*/
void QAbstractEventDispatcher::removeNativeEventFilter(QAbstractNativeEventFilter *filterObj) {
    if (!filterObj)
        return;
    QtPrivate::ThreadData *td = QtPrivate::objectThreadData(this);
    std::lock_guard<std::mutex> lk(filterLock());
    eraseNativeFilters(td, filterObj);
}

/*!
    \since 5.0
    Passes the native event \a message, of the type \a eventType, to the native event filters of this
    dispatcher's thread, the most recently installed first, until one of them returns \c true. Returns
    \c true if a filter stopped the event; otherwise returns \c false. \a result is handed to each filter.

    Platform code calls this for each native event before it handles it.

    \sa installNativeEventFilter(), QAbstractNativeEventFilter::nativeEventFilter()
*/
bool QAbstractEventDispatcher::filterNativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
    QtPrivate::ThreadData *td = QtPrivate::objectThreadData(this);
    std::vector<QAbstractNativeEventFilter *> filters;
    {
        std::lock_guard<std::mutex> lk(filterLock());
        for (const NativeFilter &f : nativeFilters()) {
            if (f.thread == td)
                filters.push_back(f.filter);
        }
    }
    for (QAbstractNativeEventFilter *filter : filters) {
        {
            // An earlier filter may have removed or destroyed this one.
            std::lock_guard<std::mutex> lk(filterLock());
            if (!isNativeFilterInstalled(td, filter))
                continue;
        }
        if (filter->nativeEventFilter(eventType, message, result))
            return true;
    }
    return false;
}

/*!
    Returns a timer identifier that is not in use. The identifiers are unique across all threads.

    \sa registerTimer(), releaseTimerId()
*/
int QAbstractEventDispatcher::allocateTimerId() { return QtPrivate::Dispatcher::allocateTimerId(); }

/*!
    Gives back \a timerId, which was allocated with allocateTimerId(). Identifiers are never reused, so this does
    nothing.

    \sa allocateTimerId()
*/
void QAbstractEventDispatcher::releaseTimerId(int) {}

#include "moc_qabstracteventdispatcher.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
