// MIT License - see LICENSE

/*!
    \page signals-and-slots.html
    \title Signals and Slots
    \brief How Nt objects notify each other without knowing about each other.

    A \e signal is a member function an object emits when something happens. A
    \e slot is any function that runs in response. The sender does not know who
    is listening, which keeps classes independent.

    \section1 Declaring Signals

    Declare signals in a \c signals: section of a class marked \c Q_OBJECT.
    The \c ntmoc tool generates their bodies:

    \code
    class Counter : public QObject {
        Q_OBJECT
    public:
        void setValue(int value) { emit valueChanged(value); }
    signals:
        void valueChanged(int value);
    };
    \endcode

    \section1 Connecting

    Pass a pointer to the signal and a slot to \c{QObject::connect()}. A slot
    may be a member function or any callable, and may take fewer arguments than
    the signal:

    \code
    QObject::connect(&counter, &Counter::valueChanged, &display, &Display::show);
    QObject::connect(&counter, &Counter::valueChanged, [](int v) { log(v); });
    \endcode

    \section1 Delivery

    \table
    \header
        \li Connection type
        \li Behaviour
    \row
        \li \c Qt::DirectConnection
        \li The slot runs immediately, inside the emit.
    \row
        \li \c Qt::QueuedConnection
        \li The arguments are copied and the slot runs from the event loop.
    \row
        \li \c Qt::BlockingQueuedConnection
        \li Like \c Qt::QueuedConnection, but the emitting thread waits until the slot has run.
    \row
        \li \c Qt::AutoConnection
        \li The default. Direct if the receiver lives in the emitting thread, queued otherwise.
    \endtable

    \section1 Threads

    Every object lives in a thread: the one that created it, or the one it was moved to with
    \c{QObject::moveToThread()}. That thread's event loop runs the object's timers and
    delivers its posted events and queued signals. \c Qt::AutoConnection compares the
    receiver's thread with the emitting thread each time the signal is emitted, so a signal
    emitted in a worker thread reaches a slot of an object in the main thread through the
    main thread's event loop:

    \code
    auto *worker = new Worker;
    worker->moveToThread(&thread);
    QObject::connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(this, &Controller::start, worker, &Worker::run);   // queued
    QObject::connect(worker, &Worker::result, this, &Controller::show); // queued
    thread.start();
    \endcode

    Connecting, disconnecting and emitting are safe from any thread. An object must be
    deleted from its own thread, or while its thread is not running events for it; use
    \c deleteLater() to delete an object in another thread.

    \section1 String-based Connections

    The \c SIGNAL() and \c SLOT() macros name a signal or slot by its
    signature, which lets you connect to members that you only know at run
    time. The class must have gone through \c ntmoc, which records its signals
    and slots:

    \code
    QObject::connect(&counter, SIGNAL(valueChanged(int)), &display, SLOT(show(int)));
    \endcode

    A connection that names a member that does not exist, or one whose
    arguments do not match, prints a warning and is not made. Signals that
    a class declares with a body of its own, and classes that \c ntmoc has not
    processed, can only be connected with the pointer-to-member syntax.

    \sa QTimer, QThread
*/
#include <QtCore/qobject.h>

#include "dispatcher.h"
#include "qobject_p.h"
#include "threaddata_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>

#include <algorithm>

/*!
    \class QMetaObject::Connection
    \brief The Connection class represents a handle to a signal-slot connection.

    A Connection is returned by QObject::connect() and can be passed to
    QObject::disconnect() to remove the connection. It does not keep the
    connection alive, and may be copied freely.

    \differences
    A Connection is only a weak handle; it has no \c operator==().
    \enddifferences

    \sa QObject::connect(), QObject::disconnect()
*/

/*!
    \class QObject
    \brief The QObject class is the base class of all Nt objects that use signals and slots.

    QObject provides the object tree (parent and children), signals and slots
    connected with connect(), per-object timers and event delivery. Deleting a
    parent deletes its children, and destroying an object removes every
    connection involving it.

    Classes that declare signals must contain the \c Q_OBJECT macro; the
    \c ntmoc tool generates their bodies. See \l{Signals and Slots} for an
    overview.

    \code
    QObject parent;
    auto *child = new QObject(&parent);
    child->setObjectName("worker");
    Q_ASSERT(parent.findChild<QObject *>("worker") == child);
    \endcode

    \differences
    Translation via tr() is not implemented, and there are no event filters or
    \c Qt::UniqueConnection. A queued
    invocation that has not run yet is discarded if its connection is
    disconnected, where Qt still runs it. A \c Qt::BlockingQueuedConnection to
    a receiver in the emitting thread calls the slot directly, where Qt
    reports a deadlock. QObject does not warn when an object is deleted from
    a thread other than its own. Connection lists are guarded by one
    process-wide lock, which is held while arguments are copied for queued
    delivery but never while a slot or a functor runs. Signal bodies are generated by
    \c ntmoc and emit through emitSignal(). \c qobject_cast() is a wrapper
    around \c dynamic_cast.
    The meta-object of a class exists only if \c ntmoc processed the class;
    metaObject() of any other class returns that of its nearest registered
    base class, which is \c QObject::staticMetaObject at the least. There
    is no \c qt_metacast(), \c connectNotify(), \c receivers() or
    \c isSignalConnected(), and no bindable properties. \c property() and
    \c setProperty() cover the static properties that \c Q_PROPERTY()
    declares and dynamic properties, and send a QDynamicPropertyChangeEvent
    when a dynamic property changes; they are not thread-safe, so call them
    from the thread that the object lives in.
    \enddifferences

    \sa QPointer, QTimer, QCoreApplication, QThread, {Signals and Slots}
*/

namespace {

// One lock for every connection list, guard and `alive` flag. It is a leaf lock: nothing that
// runs user code (slots, functor destructors) may run while it is held. Leaked so that objects
// with static storage can still use it during exit.
std::mutex &signalSlotLock() {
    static std::mutex *m = new std::mutex;
    return *m;
}

} // namespace

namespace QtPrivate {
ObjectData &Access::d(const QObject *o) { return *o->d_ptr; }

std::shared_ptr<PointerData> Access::guard(const QObject *o) {
    std::lock_guard<std::mutex> lk(signalSlotLock());
    auto &d = *o->d_ptr;
    if (!d.guard) {
        d.guard = std::make_shared<PointerData>();
        d.guard->obj = const_cast<QObject *>(o);
    }
    return d.guard;
}
} // namespace QtPrivate

namespace {

using QtPrivate::Access;
using QtPrivate::ConnectionData;
using ConnPtr = std::shared_ptr<ConnectionData>;

thread_local QObject *g_sender = nullptr;

struct SenderScope {
    QObject *prev;
    explicit SenderScope(QObject *s) : prev(g_sender) { g_sender = s; }
    ~SenderScope() { g_sender = prev; }
};

void eraseConn(std::vector<ConnPtr> &v, const ConnPtr &c) {
    v.erase(std::remove(v.begin(), v.end(), c), v.end());
}

// The caller holds the signal-slot lock. `dead` keeps the connection, and with it the functor,
// alive so that it is destroyed after the lock is released.
void unlinkConn(const ConnPtr &c, std::vector<ConnPtr> &dead) {
    if (!c->alive.exchange(false))
        return;
    if (c->sender)
        eraseConn(Access::d(c->sender).outgoing, c);
    if (c->receiver)
        eraseConn(Access::d(c->receiver).incoming, c);
    dead.push_back(c);
}

} // namespace

namespace QtPrivate {
void unlinkConnection(const std::shared_ptr<ConnectionData> &c) { unlinkConn(c); }
} // namespace QtPrivate

/*!
    Returns \c true if the connection is still valid; otherwise returns
    \c false. A connection becomes invalid when it is disconnected or when
    its sender or receiver is destroyed.
*/
QMetaObject::Connection::operator bool() const noexcept {
    auto p = d.lock();
    return p && p->alive;
}

namespace QtPrivate {

void BlockingLatch::release() {
    {
        std::lock_guard<std::mutex> lk(m_);
        done_ = true;
    }
    cv_.notify_all();
}

void BlockingLatch::wait() {
    std::unique_lock<std::mutex> lk(m_);
    cv_.wait(lk, [this] { return done_; });
}

} // namespace QtPrivate

QMetaCallEvent::~QMetaCallEvent() {
    if (m_latch)
        m_latch->release();
}

void QMetaCallEvent::place() {
    if (m_function) {
        m_function();
    } else if (m_conn->alive) {
        SenderScope scope(m_conn->sender);
        m_conn->invoke(m_args.argv.data());
    }
    if (m_latch)
        m_latch->release();
}

/*!
    Constructs an object with the given \a parent. The parent takes ownership
    and deletes the object when it is destroyed.

    \sa setParent()
*/
QObject::QObject(QObject *parent) : d_ptr(new QtPrivate::ObjectData) {
    QtPrivate::initObjectThread(this);
    if (parent && QtPrivate::objectThreadData(parent) != QtPrivate::objectThreadData(this)) {
        qWarning("QObject: Cannot create children for a parent that is in a different thread.");
        parent = nullptr;
    }
    setParent(parent);
}

/*!
    Destroys the object, emitting destroyed() first. Every connection to or
    from the object is removed, its timers are stopped, its pending posted
    events are discarded, and its children are deleted.
*/
QObject::~QObject() {
    destroyed(this);

    auto &d = *d_ptr;
    std::vector<ConnPtr> dead;
    {
        std::lock_guard<std::mutex> lk(signalSlotLock());
        if (d.guard)
            d.guard->obj = nullptr;
        const auto out = d.outgoing;
        for (const ConnPtr &c : out)
            unlinkConn(c, dead);
        const auto in = d.incoming;
        for (const ConnPtr &c : in)
            unlinkConn(c, dead);
    }
    dead.clear();

    auto &disp = QtPrivate::objectThreadData(this)->dispatcher;
    disp.killTimers(this);
    disp.removePosted(this, 0);

    // Each child detaches itself from d.children in its own destructor.
    while (!d.children.empty())
        delete d.children.back();

    if (d.parent) {
        auto &siblings = Access::d(d.parent).children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    }
    QtPrivate::releaseObjectThread(this);
}

/*!
    Returns the parent object, or \c nullptr if the object has none.

    \sa setParent(), children()
*/
QObject *QObject::parent() const { return d_ptr->parent; }

/*!
    Returns the list of child objects, in the order they were added.

    \sa parent(), findChildren()
*/
const QObjectList &QObject::children() const { return d_ptr->children; }

/*!
    Returns the object's name, which is empty by default.

    \sa setObjectName()
*/
QString QObject::objectName() const { return d_ptr->objectName; }

/*!
    Sets the object's name to \a name.

    \sa objectName(), findChild()
*/
void QObject::setObjectName(const QString &name) {
    if (d_ptr->objectName == name)
        return;
    d_ptr->objectName = name;
    objectNameChanged(name);
}

/*!
    Makes \a parent the parent of this object, removing it from its previous
    parent. A \c nullptr \a parent makes it a top-level object. Does nothing
    if \a parent is this object or one of its descendants.

    \sa parent(), children()
*/
void QObject::setParent(QObject *parent) {
    auto &d = *d_ptr;
    if (d.parent == parent)
        return;
    for (QObject *a = parent; a; a = Access::d(a).parent) {
        if (a == this)
            return;
    }
    if (parent && QtPrivate::objectThreadData(parent) != QtPrivate::objectThreadData(this)) {
        qWarning("QObject::setParent: Cannot set parent, new parent is in a different thread");
        return;
    }
    if (d.parent) {
        auto &siblings = Access::d(d.parent).children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    }
    d.parent = parent;
    if (parent)
        Access::d(parent).children.push_back(this);
}

/*!
    Returns \c true if signals emitted by this object are blocked; otherwise
    returns \c false. Signals are not blocked by default.

    \sa blockSignals()
*/
bool QObject::signalsBlocked() const noexcept { return d_ptr->blocked; }

/*!
    If \a block is \c true, signals emitted by this object are blocked and no
    connected slot is called. If \a block is \c false, blocking is lifted.
    Returns the previous value of signalsBlocked().

    \note Unlike in Qt, destroyed() is also suppressed while signals are
    blocked.

    \sa signalsBlocked(), QSignalBlocker
*/
bool QObject::blockSignals(bool block) noexcept {
    const bool previous = d_ptr->blocked;
    d_ptr->blocked = block;
    return previous;
}

/*!
    Starts a timer that fires every \a intervalMs milliseconds and returns
    its identifier, or 0 if \a intervalMs is negative. The object receives a
    QTimerEvent in timerEvent() each time the timer fires, until killTimer()
    is called or the object is destroyed. An interval of 0 fires whenever the
    event loop is idle.

    \sa killTimer(), timerEvent(), QTimer
*/
int QObject::startTimer(int intervalMs) {
    if (intervalMs < 0)
        return 0;
    return startTimer(std::chrono::milliseconds(intervalMs));
}

/*!
    \overload
    Starts a timer that fires every \a interval and returns its identifier,
    or 0 if \a interval is negative.
*/
int QObject::startTimer(std::chrono::milliseconds interval) {
    if (interval.count() < 0)
        return 0;
    auto *td = QtPrivate::objectThreadData(this);
    if (td != QtPrivate::ThreadData::current()) {
        qWarning("QObject::startTimer: Timers cannot be started from another thread");
        return 0;
    }
    return td->dispatcher.startTimer(this, interval);
}

/*!
    Stops the timer with the identifier \a id, as returned by startTimer().

    \sa startTimer()
*/
void QObject::killTimer(int id) {
    auto *td = QtPrivate::objectThreadData(this);
    if (td != QtPrivate::ThreadData::current()) {
        qWarning("QObject::killTimer: Timers cannot be stopped from another thread");
        return;
    }
    td->dispatcher.killTimer(id);
}

/*!
    Returns the object that emitted the signal that is currently invoking a
    slot, or \c nullptr when no slot is being invoked from a signal.

    The value is meaningful only while a slot runs because of a signal, and
    is tracked per thread.
*/
QObject *QObject::sender() const { return g_sender; }

/*!
    Schedules this object for deletion. The object is deleted when control
    returns to the event loop, so it is safe to call from a slot connected
    to one of its own signals.

    \sa QCoreApplication::postEvent()
*/
void QObject::deleteLater() { QCoreApplication::postEvent(this, new QEvent(QEvent::DeferredDelete)); }

/*!
    Receives \a event, an event sent to the object, and returns \c true if
    it was recognised and handled; otherwise returns \c false.

    The base implementation dispatches \c QEvent::Timer events to
    timerEvent(), runs queued slot invocations, and passes events of type
    \c QEvent::User and above to customEvent().

    \sa timerEvent(), customEvent()
*/
bool QObject::event(QEvent *e) {
    switch (e->type()) {
    case QEvent::Timer:
        timerEvent(static_cast<QTimerEvent *>(e));
        return true;
    case QEvent::MetaCall:
        static_cast<QMetaCallEvent *>(e)->place();
        return true;
    default:
        if (e->type() >= QEvent::User) {
            customEvent(e);
            return true;
        }
        return false;
    }
}

/*!
    Called when a timer started with startTimer() fires. \a event identifies
    the timer through QTimerEvent::timerId(). The base implementation does
    nothing; reimplement it to handle timers.

    \sa startTimer(), event()
*/
void QObject::timerEvent(QTimerEvent *) {}

/*!
    Called for events of type \c QEvent::User and above that are sent to the
    object. The base implementation does nothing; reimplement it to handle
    custom events delivered as \a event.

    \sa event(), QEvent::registerEventType()
*/
void QObject::customEvent(QEvent *) {}

QMetaObject::Connection QObject::connectImpl(const QObject *sender, const QtPrivate::SignalId &id,
                                             QObject *receiver,
                                             std::function<void(void *const *)> invoke,
                                             Qt::ConnectionType type) {
    if (!sender)
        return {};
    auto *s = const_cast<QObject *>(sender);
    auto c = std::make_shared<ConnectionData>();
    c->sender = s;
    c->receiver = receiver;
    c->signal = id;
    c->invoke = std::move(invoke);
    c->type = type;
    {
        std::lock_guard<std::mutex> lk(signalSlotLock());
        Access::d(s).outgoing.push_back(c);
        if (receiver)
            Access::d(receiver).incoming.push_back(c);
    }
    QMetaObject::Connection handle;
    handle.d = c;
    return handle;
}

/*!
    \overload
    Removes the connection \a connection and returns \c true if it was
    connected; otherwise returns \c false.

    \sa connect()
*/
bool QObject::disconnect(const QMetaObject::Connection &connection) {
    auto c = connection.d.lock();
    if (!c)
        return false;
    std::vector<ConnPtr> dead;
    std::lock_guard<std::mutex> lk(signalSlotLock());
    if (!c->alive)
        return false;
    unlinkConn(c, dead);
    return true;
}

bool QObject::disconnectImpl(const QObject *sender, const QtPrivate::SignalId *id,
                             const QObject *receiver) {
    if (!sender)
        return false;
    bool any = false;
    std::vector<ConnPtr> dead;
    std::lock_guard<std::mutex> lk(signalSlotLock());
    const auto out = Access::d(sender).outgoing;
    for (const ConnPtr &c : out) {
        if (id && !(c->signal == *id))
            continue;
        if (receiver && c->receiver != receiver)
            continue;
        unlinkConn(c, dead);
        any = true;
    }
    return any;
}

void QObject::activateImpl(QObject *sender, const QtPrivate::SignalId &id, void *const *argv,
                           const QtPrivate::ArgCloner &clone) {
    auto &d = Access::d(sender);
    if (d.blocked)
        return;

    // Slots may connect, disconnect or delete objects, and other threads may do the same, so
    // work on a snapshot.
    std::vector<ConnPtr> snapshot;
    {
        std::lock_guard<std::mutex> lk(signalSlotLock());
        for (const ConnPtr &c : d.outgoing) {
            if (c->signal == id)
                snapshot.push_back(c);
        }
    }
    for (const ConnPtr &c : snapshot) {
        QObject *target = c->receiver ? c->receiver : sender;
        std::shared_ptr<QtPrivate::BlockingLatch> latch;
        bool direct = false;
        {
            // Posting under the lock keeps the receiver alive: its destructor unlinks the
            // connection first and discards the posted events afterwards.
            std::lock_guard<std::mutex> lk(signalSlotLock());
            if (!c->alive)
                continue;
            // The receiver's thread is compared with the emitting thread at emission time.
            const bool crossThread =
                c->receiver && QtPrivate::objectThreadData(c->receiver) != QtPrivate::ThreadData::current();
            if (c->type == Qt::QueuedConnection || (c->type == Qt::AutoConnection && crossThread)) {
                QtPrivate::postToObject(target, new QMetaCallEvent(c, clone(argv)));
            } else if (c->type == Qt::BlockingQueuedConnection && crossThread) {
                latch = std::make_shared<QtPrivate::BlockingLatch>();
                QtPrivate::postToObject(target, new QMetaCallEvent(c, clone(argv), latch));
            } else {
                direct = true;
            }
        }
        if (latch) {
            latch->wait();
        } else if (direct) {
            SenderScope scope(sender);
            c->invoke(argv);
        }
    }
}

#include "moc_qobject.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
