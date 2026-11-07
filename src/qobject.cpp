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
        \li \c Qt::AutoConnection
        \li The default. Nt has one thread for objects, so this is direct.
    \endtable

    \note String-based \c SIGNAL() and \c SLOT() connections are not supported.

    \sa QTimer
*/
#include <QtCore/qobject.h>

#include "dispatcher.h"
#include "qobject_p.h"

#include <QtCore/qcoreapplication.h>

#include <algorithm>

namespace QtPrivate {
ObjectData &Access::d(const QObject *o) { return *o->d_ptr; }

std::shared_ptr<PointerData> Access::guard(const QObject *o) {
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

void unlinkConn(const ConnPtr &c) {
    if (!c->alive)
        return;
    c->alive = false;
    if (c->sender)
        eraseConn(Access::d(c->sender).outgoing, c);
    if (c->receiver)
        eraseConn(Access::d(c->receiver).incoming, c);
}

} // namespace

QMetaObject::Connection::operator bool() const noexcept {
    auto p = d.lock();
    return p && p->alive;
}

void QMetaCallEvent::place() {
    if (!m_conn->alive)
        return;
    SenderScope scope(m_conn->sender);
    m_conn->invoke(m_args.argv.data());
}

QObject::QObject(QObject *parent) : d_ptr(new QtPrivate::ObjectData) { setParent(parent); }

QObject::~QObject() {
    destroyed(this);

    auto &d = *d_ptr;
    if (d.guard)
        d.guard->obj = nullptr;
    const auto out = d.outgoing;
    for (const ConnPtr &c : out)
        unlinkConn(c);
    const auto in = d.incoming;
    for (const ConnPtr &c : in)
        unlinkConn(c);

    auto &disp = QtPrivate::Dispatcher::instance();
    disp.killTimers(this);
    disp.removePosted(this, 0);

    // Each child detaches itself from d.children in its own destructor.
    while (!d.children.empty())
        delete d.children.back();

    if (d.parent) {
        auto &siblings = Access::d(d.parent).children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    }
}

QObject *QObject::parent() const { return d_ptr->parent; }

const QObjectList &QObject::children() const { return d_ptr->children; }

QString QObject::objectName() const { return d_ptr->objectName; }

void QObject::setObjectName(const QString &name) { d_ptr->objectName = name; }

void QObject::setParent(QObject *parent) {
    auto &d = *d_ptr;
    if (d.parent == parent)
        return;
    for (QObject *a = parent; a; a = Access::d(a).parent) {
        if (a == this)
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

bool QObject::signalsBlocked() const noexcept { return d_ptr->blocked; }

bool QObject::blockSignals(bool block) noexcept {
    const bool previous = d_ptr->blocked;
    d_ptr->blocked = block;
    return previous;
}

int QObject::startTimer(int intervalMs) {
    if (intervalMs < 0)
        return 0;
    return startTimer(std::chrono::milliseconds(intervalMs));
}

int QObject::startTimer(std::chrono::milliseconds interval) {
    if (interval.count() < 0)
        return 0;
    return QtPrivate::Dispatcher::instance().startTimer(this, interval);
}

void QObject::killTimer(int id) { QtPrivate::Dispatcher::instance().killTimer(id); }

QObject *QObject::sender() const { return g_sender; }

void QObject::deleteLater() { QCoreApplication::postEvent(this, new QEvent(QEvent::DeferredDelete)); }

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

void QObject::timerEvent(QTimerEvent *) {}

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
    Access::d(s).outgoing.push_back(c);
    if (receiver)
        Access::d(receiver).incoming.push_back(c);
    QMetaObject::Connection handle;
    handle.d = c;
    return handle;
}

bool QObject::disconnect(const QMetaObject::Connection &connection) {
    auto c = connection.d.lock();
    if (!c || !c->alive)
        return false;
    unlinkConn(c);
    return true;
}

bool QObject::disconnect(const QObject *receiver) const {
    return disconnectImpl(this, nullptr, receiver);
}

bool QObject::disconnectImpl(const QObject *sender, const QtPrivate::SignalId *id,
                             const QObject *receiver) {
    if (!sender)
        return false;
    bool any = false;
    const auto out = Access::d(sender).outgoing;
    for (const ConnPtr &c : out) {
        if (id && !(c->signal == *id))
            continue;
        if (receiver && c->receiver != receiver)
            continue;
        unlinkConn(c);
        any = true;
    }
    return any;
}

void QObject::activateImpl(QObject *sender, const QtPrivate::SignalId &id, void *const *argv,
                           QtPrivate::ArgCloner clone) {
    auto &d = Access::d(sender);
    if (d.blocked)
        return;

    // Slots may connect, disconnect or delete objects while we iterate.
    std::vector<ConnPtr> snapshot;
    for (const ConnPtr &c : d.outgoing) {
        if (c->signal == id)
            snapshot.push_back(c);
    }
    for (const ConnPtr &c : snapshot) {
        if (!c->alive)
            continue;
        if (c->type == Qt::QueuedConnection) {
            QObject *target = c->receiver ? c->receiver : sender;
            QCoreApplication::postEvent(target, new QMetaCallEvent(c, clone(argv)));
        } else {
            SenderScope scope(sender);
            c->invoke(argv);
        }
    }
}
