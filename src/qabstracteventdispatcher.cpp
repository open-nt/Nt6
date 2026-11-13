// MIT License - see LICENSE
#include <QtCore/qabstracteventdispatcher.h>

#include "dispatcher.h"
#include "qobject_p.h"
#include "threaddata_p.h"

#include <QtCore/qabstractnativeeventfilter.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qdebug.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qthread.h>

#include <algorithm>
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

    \differences
    The class is not abstract and cannot be reimplemented: it is a handle onto the one
    dispatcher that Nt has for POSIX systems, which is not replaceable, and there is no
    \c QThread::setEventDispatcher(). A thread has one handle, which instance() creates on first use and
    which is deleted with the thread; an object that the constructor makes is another handle, and is the one
    that reports aboutToBlock() and awake() if it was the first for its thread. The functions
    work on the dispatcher of the thread that the handle lives in. Timer types are accepted and ignored,
    as are all timers of millisecond precision, and registeredTimers() reports \c Qt::CoarseTimer.
    registerSocketNotifier() and unregisterSocketNotifier() enable and disable a notifier, since a
    QSocketNotifier registers itself, and must be called from the thread of the dispatcher. flush() does
    nothing.
    \enddifferences

    \sa QEventLoop, QTimer, QSocketNotifier, QThread
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
    Constructs a handle onto the event dispatcher of the calling thread, with the given \a parent. Use
    instance() instead, unless the handle has to have a parent.

    \sa instance()
*/
QAbstractEventDispatcher::QAbstractEventDispatcher(QObject *parent) : QObject(parent) {
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
    Destroys the handle. The dispatcher itself belongs to the thread.
*/
QAbstractEventDispatcher::~QAbstractEventDispatcher() {
    QtPrivate::ThreadData *td = QtPrivate::objectThreadData(this);
    std::lock_guard<std::mutex> lk(viewLock());
    if (td->eventDispatcher == this) {
        td->eventDispatcher = nullptr;
        td->dispatcher.setObserver(nullptr);
    }
}

/*!
    Returns the event dispatcher of \a thread, or the one of the calling thread if \a thread is \c nullptr.
    It is created on first use, and valid as long as the thread lives. \a thread does not have
    to be running.

    \sa QThread::currentThread()
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
    was handled. With \c QEventLoop::WaitForMoreEvents it blocks until there is something to handle, or interrupt()
    is called. It has to be called from the thread that the dispatcher belongs to.

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
    Enables \a notifier, which has to live in the thread of this dispatcher, so that it reports activity. A
    QSocketNotifier registers itself when it is created, so this is only needed after
    unregisterSocketNotifier().

    \sa unregisterSocketNotifier(), QSocketNotifier::setEnabled()
*/
void QAbstractEventDispatcher::registerSocketNotifier(QSocketNotifier *notifier) {
    if (notifier)
        notifier->setEnabled(true);
}

/*!
    Disables \a notifier, which has to live in the thread of this dispatcher, so that it stops reporting
    activity.

    \sa registerSocketNotifier(), QSocketNotifier::setEnabled()
*/
void QAbstractEventDispatcher::unregisterSocketNotifier(QSocketNotifier *notifier) {
    if (notifier)
        notifier->setEnabled(false);
}

/*!
    Registers a timer with the interval \a interval milliseconds for \a object, which has to live in the
    thread of this dispatcher, and returns its identifier. \a object gets a \c QTimerEvent with the
    identifier each time the timer fires. \a timerType is ignored.

    \sa unregisterTimer(), unregisterTimers(), registeredTimers()
*/
int QAbstractEventDispatcher::registerTimer(qint64 interval, Qt::TimerType timerType, QObject *object) {
    const int id = allocateTimerId();
    registerTimer(id, interval, timerType, object);
    return id;
}

/*!
    \overload
    Registers a timer with the identifier \a timerId, which comes from allocateTimerId(), the
    interval \a interval milliseconds and the type \a timerType, for \a object.
*/
void QAbstractEventDispatcher::registerTimer(int timerId, qint64 interval, Qt::TimerType, QObject *object) {
    QtPrivate::ThreadData *td = QtPrivate::objectThreadData(this);
    if (!object || QtPrivate::objectThreadData(object) != td) {
        qWarning("QAbstractEventDispatcher::registerTimer: The object must live in the thread of the dispatcher");
        return;
    }
    td->dispatcher.startTimerWithId(timerId, object, std::chrono::milliseconds(std::max<qint64>(0, interval)));
}

/*!
    Unregisters the timer \a timerId and returns \c true if it was registered; otherwise returns \c false.

    \sa registerTimer()
*/
bool QAbstractEventDispatcher::unregisterTimer(int timerId) {
    return QtPrivate::objectThreadData(this)->dispatcher.killTimer(timerId);
}

/*!
    Unregisters all timers of \a object and returns \c true if there were any; otherwise returns \c false.

    \sa unregisterTimer()
*/
bool QAbstractEventDispatcher::unregisterTimers(QObject *object) {
    return object && QtPrivate::objectThreadData(this)->dispatcher.killTimers(object);
}

/*!
    Returns the timers that are registered for \a object.

    \sa registerTimer()
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
    Returns the number of milliseconds until the timer \a timerId fires, 0 if it is overdue, or -1 if
    there is no such timer.

    \sa registerTimer()
*/
int QAbstractEventDispatcher::remainingTime(int timerId) {
    const long long ms = QtPrivate::objectThreadData(this)->dispatcher.remainingTime(timerId);
    return static_cast<int>(std::min<long long>(ms, INT_MAX));
}

/*!
    Wakes the dispatcher if it is blocked waiting for events, so that it looks at them again. It can be called
    from any thread.

    \sa interrupt()
*/
void QAbstractEventDispatcher::wakeUp() { QtPrivate::objectThreadData(this)->dispatcher.wakeUp(); }

/*!
    Makes the processEvents() call that waits for events, or the next one that would, return without waiting.
    It can be called from any thread.

    \sa wakeUp()
*/
void QAbstractEventDispatcher::interrupt() { QtPrivate::objectThreadData(this)->dispatcher.interrupt(); }

/*!
    Does nothing. It exists because other dispatchers flush buffered output here.
*/
void QAbstractEventDispatcher::flush() {}

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
