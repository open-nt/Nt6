// MIT License - see LICENSE
#include "threaddata_p.h"

#include "qobject_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtCore/qthread.h>

#include <mutex>
#include <vector>

namespace QtPrivate {

namespace {

thread_local ThreadData *t_data = nullptr;
thread_local bool t_holderDead = false;

// Guards the pointer swap in moveToThread() against readers that take a reference.
std::mutex &affinityLock() {
    static std::mutex *m = new std::mutex;
    return *m;
}

// Drops the reference of an adopted thread when it exits.
struct Holder {
    ThreadData *td = nullptr;
    ~Holder() {
        t_holderDead = true;
        if (!td)
            return;
        t_data = nullptr;
        td->clearStorage();
        if (QThread *t = td->thread.load())
            ThreadAccess::adoptedThreadExited(t);
        td->release();
    }
};

thread_local Holder t_holder;

} // namespace

ThreadData::ThreadData() = default;

ThreadData::~ThreadData() {
    clearStorage();
    if (QThread *t = thread.load()) {
        if (adopted)
            ThreadAccess::destroyAdopted(t);
    }
}

void ThreadData::clearStorage() {
    while (!storage.empty()) {
        auto old = std::move(storage);
        storage.clear();
        for (auto &entry : old)
            entry.second.destroy(entry.second.data);
    }
}

std::atomic<ThreadData *> g_mainThread{nullptr};

ThreadData *ThreadData::mainThread() { return g_mainThread.load(); }

ThreadData *ThreadData::orphan() {
    static ThreadData *td = [] {
        auto *d = new ThreadData;
        d->isOrphan = true;
        return d;
    }();
    return td;
}

void ThreadData::bindCurrent(ThreadData *td) { t_data = td; }

ThreadData *ThreadData::current() {
    if (t_data)
        return t_data;
    if (t_holderDead)
        return orphan();

    auto *td = new ThreadData;
    td->adopted = true;
    t_data = td;
    t_holder.td = td;
    // The first adopted thread is taken to be the main thread; its data is never freed so that
    // objects with static storage can outlive the thread-local destructors.
    ThreadData *expected = nullptr;
    if (g_mainThread.compare_exchange_strong(expected, td))
        td->retain();
    td->thread.store(ThreadAccess::adopt(td));
    return td;
}

ThreadData *objectThreadData(const QObject *o) { return Access::d(o).threadData.load(); }

ThreadData *acquireThreadData(const QObject *o) {
    std::lock_guard<std::mutex> lk(affinityLock());
    ThreadData *td = Access::d(o).threadData.load();
    td->retain();
    return td;
}

void initObjectThread(QObject *o) {
    ThreadData *td = ThreadData::current();
    td->retain();
    Access::d(o).threadData.store(td);
}

void releaseObjectThread(QObject *o) {
    auto &d = Access::d(o);
    ThreadData *td = d.threadData.load();
    if (d.ownsThreadRef)
        td->release();
}

void postToObject(QObject *receiver, QEvent *event) {
    for (;;) {
        ThreadData *td = acquireThreadData(receiver);
        const bool posted = td->dispatcher.post(receiver, event, td);
        td->release();
        if (posted)
            return;
    }
}

namespace {

void collect(QObject *o, std::vector<QObject *> &out) {
    out.push_back(o);
    for (QObject *c : o->children())
        collect(c, out);
}

} // namespace

} // namespace QtPrivate

/*!
    Returns the thread the object lives in, which is the thread that created it unless
    moveToThread() was called. The thread's event loop delivers the object's events and
    runs its timers. Returns \c nullptr for an object that was moved to no thread, and for
    an object whose QThread has been destroyed.

    \sa moveToThread(), QThread::currentThread()
*/
QThread *QObject::thread() const {
    QtPrivate::ThreadData *td = QtPrivate::acquireThreadData(this);
    QThread *t = td->thread.load();
    td->release();
    return t;
}

/*!
    Changes the thread affinity of this object and its children to
    \a targetThread. The object's timers, socket notifiers and posted events move with
    it, and \c QEvent::ThreadChange is sent to every object that moves. If \a targetThread
    is \c nullptr, the object lives in no thread and receives no events until it is moved
    to one.

    The function must be called from the thread the object lives in, or for an object that
    lives in no thread; otherwise it prints a warning and does nothing. An object with a
    parent cannot be moved.

    \note The object must not be used by its old thread while it moves, and a
    QSocketNotifier or timer that the object owns must not be stopped from the new thread
    before the move has completed.

    \sa thread(), QThread
*/
void QObject::moveToThread(QThread *targetThread) {
    using namespace QtPrivate;
    if (d_ptr->parent) {
        qWarning("QObject::moveToThread: Cannot move objects with a parent");
        return;
    }
    ThreadData *from = objectThreadData(this);
    ThreadData *to = targetThread ? ThreadAccess::data(targetThread) : ThreadData::orphan();
    if (from == to)
        return;
    if (from != ThreadData::current() && !from->isOrphan) {
        qWarning("QObject::moveToThread: Current thread is not the object's thread. Cannot move to target thread");
        return;
    }

    std::vector<QObject *> objs;
    collect(this, objs);
    for (QObject *o : objs) {
        QEvent e(QEvent::ThreadChange);
        QCoreApplication::sendEvent(o, &e);
    }
    std::size_t owned = 0;
    {
        std::lock_guard<std::mutex> lk(affinityLock());
        for (QObject *o : objs) {
            auto &d = Access::d(o);
            to->retain();
            d.threadData.store(to);
            // An adopted QThread object does not hold a reference to its own thread data.
            if (d.ownsThreadRef)
                ++owned;
            d.ownsThreadRef = true;
        }
    }
    for (std::size_t i = 0; i < owned; ++i)
        from->release();
    from->dispatcher.migrate(to->dispatcher, objs);
}
