// MIT License - see LICENSE
#include <QtCore/qeventloop.h>

#include "dispatcher.h"
#include "threaddata_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qthread.h>
#include <atomic>
#include <map>
#include <mutex>

struct QEventLoop::Data {
    std::atomic<bool> exitFlag{false};
    std::atomic<int> code{0};
    std::atomic<bool> running{false};
    // While the loop runs: the thread it runs in, with a reference. exit() may come from any thread.
    std::mutex mutex;
    QtPrivate::ThreadData *td = nullptr;
};

/*!
    \class QEventLoop
    \brief The QEventLoop class provides a means of entering and leaving an event loop.

    An event loop dispatches timers, posted events and socket notifiers.
    Call exec() to run it until quit() or exit() is called, for example from
    a slot:

    \code
    QEventLoop loop;
    QTimer::singleShot(1000, [&loop] { loop.quit(); });
    loop.exec();
    \endcode

    \differences
    \c ExcludeUserInputEvents and \c ExcludeSocketNotifiers are accepted but
    ignored, and there is no \c QAbstractEventDispatcher. A loop always runs
    the event dispatcher of the thread that calls exec() or processEvents().
    \enddifferences

    \sa QCoreApplication, QTimer, QSocketNotifier
*/

/*!
    Constructs an event loop with the given \a parent.
*/
QEventLoop::QEventLoop(QObject *parent) : QObject(parent), m_d(new Data) {}

/*!
    Destroys the event loop.
*/
QEventLoop::~QEventLoop() = default;

/*!
    Processes pending events once, according to \a flags, and returns
    \c true if any event, timer or notifier was handled. With
    \c WaitForMoreEvents it blocks until something is ready.

    \sa exec()
*/
bool QEventLoop::processEvents(ProcessEventsFlags flags) {
    const bool wait = flags.testFlag(WaitForMoreEvents);
    return QtPrivate::ThreadData::current()->dispatcher.process(wait, m_d->running ? &m_d->exitFlag : nullptr);
}

/*!
    \overload
    Processes the pending events for at most \a maxTime milliseconds, or
    until there are none left, as \a flags selects. It does not wait for new
    events, even with \c WaitForMoreEvents. Call it during a long computation
    to keep the program responsive.

    \sa exec()
*/
void QEventLoop::processEvents(ProcessEventsFlags flags, int maxTime) { processEvents(flags, QDeadlineTimer(maxTime)); }

/*!
    \since 6.7
    \overload
    Processes the pending events until \a deadline expires or there are none
    left, as \a flags selects.
*/
void QEventLoop::processEvents(ProcessEventsFlags flags, QDeadlineTimer deadline) {
    flags &= ~ProcessEventsFlags(WaitForMoreEvents);
    while (processEvents(flags) && !deadline.hasExpired()) {
    }
}

/*!
    Enters the event loop and runs it until exit() or quit() is called, then
    returns the code passed to exit(). \a flags is accepted for
    compatibility and ignored. Returns -1 immediately if the loop is already
    running.

    \sa exit(), quit(), isRunning()
*/
int QEventLoop::exec(ProcessEventsFlags) {
    if (m_d->running.exchange(true))
        return -1;

    auto *td = QtPrivate::ThreadData::current();
    td->retain();
    {
        std::lock_guard<std::mutex> lk(m_d->mutex);
        m_d->td = td;
    }
    m_d->exitFlag = false;
    m_d->code = 0;
    td->dispatcher.addLoop(this);

    struct Guard {
        QEventLoop *self;
        QtPrivate::ThreadData *td;
        ~Guard() {
            td->dispatcher.removeLoop(self);
            {
                std::lock_guard<std::mutex> lk(self->m_d->mutex);
                self->m_d->td = nullptr;
            }
            self->m_d->running = false;
            td->release();
        }
    } guard{this, td};

    // QThread::exit() sets quitNow before it looks for loops, and this check follows addLoop(),
    // so an exit request cannot slip between the two.
    if (td->quitNow.load())
        return -1;
    while (!m_d->exitFlag.load())
        td->dispatcher.process(true, &m_d->exitFlag);
    return m_d->code.load();
}

/*!
    Tells the event loop to exit with the return code \a returnCode, which
    exec() then returns. It is safe to call from any thread.

    \sa quit(), exec()
*/
void QEventLoop::exit(int returnCode) {
    m_d->code = returnCode;
    m_d->exitFlag = true;
    wakeUp();
}

/*!
    Returns \c true if the event loop is running in exec(); otherwise
    returns \c false.

    \sa exec(), exit()
*/
bool QEventLoop::isRunning() const { return m_d->running; }

/*!
    Wakes the event loop if it is blocked waiting for events, so that it
    re-checks its state. It is safe to call from any thread.
*/
void QEventLoop::wakeUp() {
    std::lock_guard<std::mutex> lk(m_d->mutex);
    if (m_d->td)
        m_d->td->dispatcher.wakeUp();
}

/*!
    Tells the event loop to exit with return code 0. This is the same as
    calling exit(0).

    \sa exit()
*/
void QEventLoop::quit() { exit(0); }

namespace {

std::mutex g_lockerMutex;

// Lockers per target; the application's key is null.
std::map<const QObject *, int> &lockerCounts() {
    static auto *counts = new std::map<const QObject *, int>;
    return *counts;
}

} // namespace

/*!
    \class QEventLoopLocker
    \brief The QEventLoopLocker class provides a means to quit an event loop when it is no longer needed.

    A locker keeps count of the work that is still going on. When the last
    locker for a target is destroyed, the target quits. It lets code that is
    spread over several objects, such as a number of asynchronous jobs, end the
    program once the last one is done, without a central counter:

    \code
    void Job::start()
    {
        m_lock = std::make_unique<QEventLoopLocker>();
        // ...
    }
    void Job::finish() { m_lock.reset(); }   // the last job to finish quits the application
    \endcode

    The locker created without arguments counts for the application and quits
    it with QCoreApplication::quit(), unless QCoreApplication::setQuitLockEnabled()
    turned that off. One created with a QEventLoop or QThread counts for that
    object, which then has quit() called; a target that was destroyed in the
    meantime is ignored.

    \differences
    None.
    \enddifferences

    \sa QCoreApplication::setQuitLockEnabled(), QEventLoop::quit()
*/

/*!
    Constructs a locker for the application.
*/
QEventLoopLocker::QEventLoopLocker() : m_kind(Kind::Application), m_key(nullptr) {
    std::lock_guard<std::mutex> lock(g_lockerMutex);
    ++lockerCounts()[nullptr];
}

/*!
    \overload

    Constructs a locker for \a loop, which quits when the last locker for it is
    destroyed.
*/
QEventLoopLocker::QEventLoopLocker(QEventLoop *loop) : m_kind(Kind::Loop), m_target(loop), m_key(loop) {
    std::lock_guard<std::mutex> lock(g_lockerMutex);
    ++lockerCounts()[loop];
}

/*!
    \overload

    Constructs a locker for \a thread, which quits when the last locker for it
    is destroyed.
*/
QEventLoopLocker::QEventLoopLocker(QThread *thread) : m_kind(Kind::Thread), m_target(thread), m_key(thread) {
    std::lock_guard<std::mutex> lock(g_lockerMutex);
    ++lockerCounts()[thread];
}

/*!
    Destroys the locker. If it was the last one for its target, the target quits.
*/
QEventLoopLocker::~QEventLoopLocker() {
    const QObject *key = m_key;
    bool last = false;
    {
        std::lock_guard<std::mutex> lock(g_lockerMutex);
        auto &counts = lockerCounts();
        const auto it = counts.find(key);
        if (it != counts.end() && --it->second == 0) {
            counts.erase(it);
            last = true;
        }
    }
    if (!last)
        return;
    switch (m_kind) {
    case Kind::Application:
        if (QCoreApplication::isQuitLockEnabled())
            QCoreApplication::quit();
        break;
    case Kind::Loop:
        if (m_target)
            static_cast<QEventLoop *>(m_target.data())->quit();
        break;
    case Kind::Thread:
        if (m_target)
            static_cast<QThread *>(m_target.data())->quit();
        break;
    }
}

#include "moc_qeventloop.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
