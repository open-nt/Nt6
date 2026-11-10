// MIT License - see LICENSE
#include <QtCore/qeventlooplocker.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qeventloop.h>
#include <QtCore/qpointer.h>
#include <QtCore/qthread.h>

#include <atomic>
#include <map>
#include <mutex>

namespace QtPrivate {

struct LockerCounter {
    enum Kind { Application, Loop, Thread };

    explicit LockerCounter(Kind k, QObject *o = nullptr) : kind(k), target(o) {}

    std::atomic<int> count{0};
    Kind kind;
    QPointer<QObject> target;
};

} // namespace QtPrivate

namespace {

using QtPrivate::LockerCounter;

std::atomic<bool> g_quitLockEnabled{true};

struct Registry {
    std::mutex mutex;
    std::shared_ptr<LockerCounter> application = std::make_shared<LockerCounter>(LockerCounter::Application);
    std::map<const QObject *, std::weak_ptr<LockerCounter>> targets;
};

Registry &registry() {
    static Registry *r = new Registry;
    return *r;
}

// Lockers of one loop or thread share a counter. A counter whose target is gone is not reused for a new
// object that happens to have the same address.
std::shared_ptr<LockerCounter> counterFor(LockerCounter::Kind kind, QObject *target) {
    Registry &r = registry();
    std::lock_guard<std::mutex> lk(r.mutex);
    if (kind == LockerCounter::Application)
        return r.application;
    for (auto it = r.targets.begin(); it != r.targets.end();) {
        it = it->second.expired() ? r.targets.erase(it) : std::next(it);
    }
    auto found = r.targets.find(target);
    if (found != r.targets.end()) {
        if (auto c = found->second.lock(); c && c->target.data() == target)
            return c;
    }
    auto c = std::make_shared<LockerCounter>(kind, target);
    r.targets[target] = c;
    return c;
}

} // namespace

/*!
    \class QEventLoopLocker
    \brief The QEventLoopLocker class provides a means to quit an event loop when it is no longer needed.

    An event loop locker keeps its target alive: while at least one locker exists, the target does not quit. When
    the last one is destroyed the target quits. Without arguments the target is the application: QCoreApplication::quit()
    is called when the last locker is destroyed, unless QCoreApplication::isQuitLockEnabled() is \c false.
    A locker for a QEventLoop calls QEventLoop::quit(), and one for a QThread calls QThread::quit().

    \code
    class Job {
    public:
        Job() : m_locker(std::make_unique<QEventLoopLocker>()) {} // the application keeps running ...
        void done() { m_locker.reset(); }                         // ... until the last job is done

    private:
        std::unique_ptr<QEventLoopLocker> m_locker;
    };
    \endcode

    A locker is safe to create and destroy in any thread. Nothing quits unless a locker was created and
    destroyed: an application without lockers runs until quit() is called.

    \differences
    The locker holds a guarded pointer to its loop or thread, and does nothing if the target was destroyed
    first. Nt has no windows, so there is no \c quitOnLastWindowClosed interplay, and the application is asked to
    quit even if its event loop is not running yet, which Qt does not do.
    \enddifferences

    \sa QEventLoop, QCoreApplication::quit(), QThread::quit()
*/

/*!
    Constructs a locker that keeps the application from quitting. When the last locker of the application is
    destroyed, the application quits if QCoreApplication::isQuitLockEnabled() is \c true.
*/
QEventLoopLocker::QEventLoopLocker() : m_counter(counterFor(LockerCounter::Application, nullptr)) { ++m_counter->count; }

/*!
    Constructs a locker that keeps \a loop from quitting. When the last locker of \a loop is destroyed,
    the loop quits. \a loop must not be \c nullptr.
*/
QEventLoopLocker::QEventLoopLocker(QEventLoop *loop) : m_counter(counterFor(LockerCounter::Loop, loop)) {
    ++m_counter->count;
}

/*!
    Constructs a locker that keeps the event loop of \a thread from quitting. When the last locker of
    \a thread is destroyed, QThread::quit() is called. \a thread must not be \c nullptr.
*/
QEventLoopLocker::QEventLoopLocker(QThread *thread) : m_counter(counterFor(LockerCounter::Thread, thread)) {
    ++m_counter->count;
}

/*!
    Destroys the locker. If it was the last one for its target, the target quits.
*/
QEventLoopLocker::~QEventLoopLocker() {
    if (--m_counter->count != 0)
        return;
    switch (m_counter->kind) {
    case LockerCounter::Application:
        if (g_quitLockEnabled)
            QCoreApplication::quit();
        break;
    case LockerCounter::Loop:
        if (auto *loop = static_cast<QEventLoop *>(m_counter->target.data()))
            loop->quit();
        break;
    case LockerCounter::Thread:
        if (auto *thread = static_cast<QThread *>(m_counter->target.data()))
            thread->quit();
        break;
    }
}

/*!
    Returns \c true if the application quits when the last QEventLoopLocker is destroyed. It is
    \c true by default.

    \sa setQuitLockEnabled(), QEventLoopLocker
*/
bool QCoreApplication::isQuitLockEnabled() { return g_quitLockEnabled; }

/*!
    Sets whether the application quits when the last QEventLoopLocker is destroyed to \a enabled.

    \sa isQuitLockEnabled(), QEventLoopLocker
*/
void QCoreApplication::setQuitLockEnabled(bool enabled) { g_quitLockEnabled = enabled; }
