// MIT License - see LICENSE
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

#include <QtCore/qthread.h>

#include "qobject_p.h"
#include "threaddata_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtCore/qeventloop.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <climits>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>

/*!
    \class QThread
    \brief The QThread class provides a platform-independent way to manage threads.

    A QThread object manages one thread of control. By default the thread runs an event loop
    from run(), so objects that were moved to it with QObject::moveToThread() receive timers,
    posted events and queued signals there:

    \code
    QThread thread;
    auto *worker = new Worker;
    worker->moveToThread(&thread);
    QObject::connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(&producer, &Producer::ready, worker, &Worker::process);
    thread.start();
    // ...
    thread.quit();
    thread.wait();
    \endcode

    Reimplement run() instead to run your own code in the thread, or use create() to run a
    function. The thread that creates a QThread owns the object, and the object lives in the
    creating thread, not in the thread it manages. Destroying a QThread that is still running
    prints a message and aborts the process, as in Qt.

    Every thread has a QThread object. Threads that were not started by QThread, such as the
    main thread, get one when they first use Nt; currentThread() returns it.

    \differences
    There is no \c terminate() or \c setTerminationEnabled(), and no \c eventDispatcher() or
    \c setEventDispatcher() (Nt has no QAbstractEventDispatcher). Priorities are stored and
    returned by priority(), but are applied only for \c IdlePriority (as \c SCHED_IDLE) and,
    if the process has a real-time scheduling policy, as a scaled real-time priority. The
    \c started() and \c finished() signals have no \c QPrivateSignal argument, and an adopted
    thread does not emit finished(). A thread that has finished keeps its objects, timers and
    posted events; they run again if the thread is restarted. \c currentThreadId() is the
    \c pthread_t cast to a pointer.
    \enddifferences

    \sa QObject::moveToThread(), QThreadPool, QThreadStorage, {Thread Support in Nt}, {Signals and Slots}
*/

namespace QtPrivate {
namespace {

void applyPriority(pthread_t handle, QThread::Priority priority) {
    if (priority == QThread::InheritPriority)
        return;
    int policy = 0;
    sched_param param{};
    if (pthread_getschedparam(handle, &policy, &param) != 0)
        return;
    if (priority == QThread::IdlePriority) {
#ifdef SCHED_IDLE
        policy = SCHED_IDLE;
        param.sched_priority = 0;
#endif
    } else {
#ifdef SCHED_IDLE
        if (policy == SCHED_IDLE)
            policy = SCHED_OTHER;
#endif
        const int lo = sched_get_priority_min(policy);
        const int hi = sched_get_priority_max(policy);
        if (hi > lo)
            param.sched_priority = lo + (hi - lo) * (priority - QThread::LowestPriority) /
                                            (QThread::TimeCriticalPriority - QThread::LowestPriority);
    }
    pthread_setschedparam(handle, policy, &param); // best effort: real-time policies need privileges
}

} // namespace
} // namespace QtPrivate

struct QThread::Data {
    std::mutex mutex;
    std::condition_variable cond;
    bool running = false;
    bool finished = false;
    bool inFinish = false;
    bool exited = false;
    bool joinable = false;
    bool adopted = false;
    int returnCode = 0;
    Priority priority = InheritPriority;
    uint stackSize = 0;
    std::atomic<bool> interruption{false};
    pthread_t handle{};
    QtPrivate::ThreadData *td = nullptr;
};

namespace QtPrivate {

QThread *ThreadAccess::adopt(ThreadData *td) { return new QThread(td, 0); }

void ThreadAccess::destroyAdopted(QThread *thread) { delete thread; }

ThreadData *ThreadAccess::data(const QThread *thread) { return thread->m_d->td; }

void ThreadAccess::adoptedThreadExited(QThread *thread) {
    auto &d = *thread->m_d;
    {
        std::lock_guard<std::mutex> lk(d.mutex);
        d.running = false;
        d.finished = true;
    }
    d.cond.notify_all();
}

// The QThread cannot be destroyed while the thread runs, so `self` stays valid until the end.
void *ThreadAccess::threadMain(void *p) {
    auto *self = static_cast<QThread *>(p);
    const std::shared_ptr<QThread::Data> d = self->m_d;
    ThreadData *td = d->td;
    ThreadData::bindCurrent(td);
    applyPriority(pthread_self(), d->priority);

    self->started();
    self->run();

    {
        std::lock_guard<std::mutex> lk(d->mutex);
        d->inFinish = true;
    }
    self->finished();
    // Objects scheduled with deleteLater() die with the thread.
    td->dispatcher.sendPosted(nullptr, QEvent::DeferredDelete);
    td->clearStorage();
    ThreadData::bindCurrent(nullptr);
    {
        std::lock_guard<std::mutex> lk(d->mutex);
        d->running = false;
        d->finished = true;
        d->inFinish = false;
    }
    d->cond.notify_all();
    return nullptr;
}

} // namespace QtPrivate

namespace {

class FunctorThread : public QThread {
public:
    explicit FunctorThread(std::function<void()> body) : m_body(std::move(body)) {}

protected:
    void run() override { m_body(); }

private:
    std::function<void()> m_body;
};

} // namespace

/*!
    Constructs a thread object with the given \a parent. The thread is not started; call
    start().
*/
QThread::QThread(QObject *parent) : QObject(parent), m_d(std::make_shared<Data>()) {
    m_d->td = new QtPrivate::ThreadData;
    m_d->td->thread.store(this);
}

QThread::QThread(QtPrivate::ThreadData *adopted, int) : QObject(nullptr), m_d(std::make_shared<Data>()) {
    m_d->td = adopted;
    m_d->adopted = true;
    m_d->running = true;
    // The thread data owns this object, so the object holds no reference to it.
    QtPrivate::Access::d(this).ownsThreadRef = false;
    adopted->release();
}

/*!
    Destroys the thread object. If the thread is still running, this prints a message and
    aborts the process, so call quit() or exit() and wait() first. A thread that is in
    the middle of finishing is waited for.
*/
QThread::~QThread() {
    Data &d = *m_d;
    if (d.adopted)
        return;
    std::unique_lock<std::mutex> lk(d.mutex);
    if (d.inFinish && d.td != QtPrivate::ThreadData::current())
        d.cond.wait(lk, [&d] { return !d.running; });
    if (d.running) {
        lk.unlock();
        qFatal("QThread: Destroyed while thread is still running");
    }
    const bool join = d.joinable;
    d.joinable = false;
    lk.unlock();
    if (join)
        pthread_join(d.handle, nullptr);
    d.td->thread.store(nullptr);
    d.td->release();
}

/*!
    Returns the thread handle of the calling thread, an opaque value that identifies the
    thread. On Linux it is the \c pthread_t.

    \sa currentThread()
*/
Qt::HANDLE QThread::currentThreadId() noexcept {
    // Qt::HANDLE is a pointer type, as in Qt, so the thread id has to be cast.
    return reinterpret_cast<Qt::HANDLE>(pthread_self()); // NOLINT(performance-no-int-to-ptr)
}

/*!
    Returns the QThread object that stands for the calling thread. For a thread that was not
    started by QThread, the object is created the first time the thread uses Nt.

    \sa currentThreadId(), isMainThread()
*/
QThread *QThread::currentThread() { return QtPrivate::ThreadData::current()->thread.load(); }

/*!
    Returns the number of threads the machine can run at the same time: the number of
    processors the process may use. Returns 1 if that cannot be determined.
*/
int QThread::idealThreadCount() noexcept {
    int n = 0;
#ifdef __linux__
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof set, &set) == 0)
        n = CPU_COUNT(&set);
#endif
    if (n < 1)
        n = static_cast<int>(std::thread::hardware_concurrency());
    return std::max(n, 1);
}

/*!
    Yields execution of the current thread to another runnable thread, if any.
*/
void QThread::yieldCurrentThread() { sched_yield(); }

/*!
    Returns \c true if the calling thread is the main thread: the thread of the QCoreApplication
    object, or, without one, the first thread that used Nt.

    \since 6.4
*/
bool QThread::isMainThread() {
    if (QCoreApplication *app = QCoreApplication::instance())
        return app->thread() == currentThread();
    return QtPrivate::ThreadData::current() == QtPrivate::ThreadData::mainThread();
}

/*!
    Sets the priority of the running thread to \a priority. It does nothing and prints a
    warning if the thread is not running, and \c InheritPriority is ignored.

    \sa priority(), start()
*/
void QThread::setPriority(Priority priority) {
    if (priority == InheritPriority) {
        qWarning("QThread::setPriority: Argument cannot be InheritPriority");
        return;
    }
    Data &d = *m_d;
    std::lock_guard<std::mutex> lk(d.mutex);
    if (!d.running) {
        qWarning("QThread::setPriority: Cannot set priority, thread is not running");
        return;
    }
    d.priority = priority;
    if (!d.adopted)
        QtPrivate::applyPriority(d.handle, priority);
}

/*!
    Returns the priority the thread was started with or last given by setPriority(), which is
    \c InheritPriority for a thread that was started without one.

    \sa setPriority(), start()
*/
QThread::Priority QThread::priority() const {
    std::lock_guard<std::mutex> lk(m_d->mutex);
    return m_d->priority;
}

/*!
    Returns \c true if the thread has finished, that is, it ran and has completed. A thread
    that was never started, and a thread that is running, is not finished.

    \sa isRunning()
*/
bool QThread::isFinished() const {
    std::lock_guard<std::mutex> lk(m_d->mutex);
    return m_d->finished;
}

/*!
    Returns \c true if the thread is running, including while it is finishing.

    \sa isFinished()
*/
bool QThread::isRunning() const {
    std::lock_guard<std::mutex> lk(m_d->mutex);
    return m_d->running;
}

/*!
    Requests that the thread stop what it is doing. Nothing happens unless the thread's code
    polls isInterruptionRequested() and returns. The request is cleared when the thread is
    started again. It is safe to call from any thread.

    \sa isInterruptionRequested()
*/
void QThread::requestInterruption() {
    std::lock_guard<std::mutex> lk(m_d->mutex);
    if (!m_d->running || m_d->finished || m_d->inFinish)
        return;
    m_d->interruption.store(true);
}

/*!
    Returns \c true if requestInterruption() was called for this thread since it started.
    Long-running code in the thread should poll this and return soon after it becomes true.

    \sa requestInterruption()
*/
bool QThread::isInterruptionRequested() const { return m_d->interruption.load(); }

/*!
    Sets the maximum stack size of the thread to \a stackSize bytes, if that is not 0. The
    size is used the next time the thread is started; a running thread is not affected. A
    value below the system minimum is raised to it.

    \sa stackSize()
*/
void QThread::setStackSize(uint stackSize) {
    std::lock_guard<std::mutex> lk(m_d->mutex);
    m_d->stackSize = stackSize;
}

/*!
    Returns the stack size set with setStackSize(), or 0 for the system default.

    \sa setStackSize()
*/
uint QThread::stackSize() const {
    std::lock_guard<std::mutex> lk(m_d->mutex);
    return m_d->stackSize;
}

/*!
    Returns the number of event loops that are running in the thread, counting nested ones.
    It is 0 when the thread is not in an event loop.

    \sa exec(), QEventLoop::exec()
*/
int QThread::loopLevel() const { return m_d->td->dispatcher.loopLevel(); }

QThread *QThread::createImpl(std::function<void()> body) { return new FunctorThread(std::move(body)); }

/*!
    Begins execution of the thread by calling run(), with the given \a priority, which defaults
    to inheriting the priority of the creating thread. The started() signal is emitted from
    the new thread first. Does nothing if the thread is already running. A thread that has
    finished can be started again.

    \sa run(), wait(), setPriority()
*/
void QThread::start(Priority priority) {
    Data &d = *m_d;
    std::unique_lock<std::mutex> lk(d.mutex);
    if (d.adopted)
        return;
    if (d.inFinish) {
        if (d.td == QtPrivate::ThreadData::current())
            return;
        d.cond.wait(lk, [&d] { return !d.running; });
    }
    if (d.running)
        return;
    if (d.joinable) {
        const pthread_t old = d.handle;
        d.joinable = false;
        lk.unlock();
        pthread_join(old, nullptr);
        lk.lock();
        if (d.running)
            return;
    }

    d.running = true;
    d.finished = false;
    d.exited = false;
    d.returnCode = 0;
    d.priority = priority;
    d.interruption.store(false);
    d.td->quitNow.store(false);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (d.stackSize > 0)
        pthread_attr_setstacksize(&attr,
                                  std::max<std::size_t>(d.stackSize, static_cast<std::size_t>(PTHREAD_STACK_MIN)));
    const int err = pthread_create(&d.handle, &attr, &QtPrivate::ThreadAccess::threadMain, this);
    pthread_attr_destroy(&attr);
    if (err != 0) {
        d.running = false;
        lk.unlock();
        qWarning("QThread::start: Thread creation error: %s", std::strerror(err));
        return;
    }
    d.joinable = true;
}

/*!
    The starting point of the thread. After start() calls it, the thread runs exec() and so
    handles the events of the objects that live in it. Reimplement it to run other code; when
    it returns, the thread ends.

    \sa exec(), start()
*/
void QThread::run() { exec(); }

/*!
    Enters the thread's event loop and waits until exit() or quit() is called, then returns the
    code passed to exit(). If exit() was called before exec(), it returns at once. Call it
    from run() only.

    \sa quit(), exit(), loopLevel()
*/
int QThread::exec() {
    Data &d = *m_d;
    {
        std::lock_guard<std::mutex> lk(d.mutex);
        d.td->quitNow.store(false);
        if (d.exited) {
            d.exited = false;
            return d.returnCode;
        }
    }
    QEventLoop loop;
    int code = loop.exec();
    std::lock_guard<std::mutex> lk(d.mutex);
    if (d.exited)
        code = d.returnCode;
    d.exited = false;
    return code;
}

/*!
    Tells the thread's event loops to exit with the return code \a retcode, so that exec()
    returns it. After that run() usually returns and the thread finishes. Does nothing if the
    thread is not running. A call that arrives before the thread has entered exec() makes
    exec() return at once, so it is safe to call right after start().

    \sa quit(), exec()
*/
void QThread::exit(int retcode) {
    Data &d = *m_d;
    std::lock_guard<std::mutex> lk(d.mutex);
    if (!d.running)
        return;
    d.exited = true;
    d.returnCode = retcode;
    d.td->quitNow.store(true);
    d.td->dispatcher.exitAll(retcode);
}

/*!
    Tells the thread's event loop to exit with return code 0. This is the same as calling
    exit(0).

    \sa exit()
*/
void QThread::quit() { exit(0); }

/*!
    Blocks the calling thread until the thread has finished, or until \a deadline has passed.
    Returns \c true if the thread finished or was never started, and \c false if the deadline
    expired. Returns \c false at once, with a warning, if a thread waits for itself. A thread
    that has finished has already emitted finished() when this returns.

    \sa isFinished(), start()
*/
bool QThread::wait(QDeadlineTimer deadline) {
    Data &d = *m_d;
    std::unique_lock<std::mutex> lk(d.mutex);
    if (d.running && d.td == QtPrivate::ThreadData::current()) {
        qWarning("QThread::wait: Thread tried to wait on itself");
        return false;
    }
    while (d.running) {
        if (deadline.isForever()) {
            d.cond.wait(lk);
        } else {
            if (deadline.hasExpired())
                return false;
            d.cond.wait_until(lk, deadline.timePoint());
        }
    }
    return true;
}

/*!
    \overload
    Waits for at most \a time milliseconds. A value of \c ULONG_MAX waits until the thread
    finishes.
*/
bool QThread::wait(unsigned long time) {
    if (time == ULONG_MAX)
        return wait(QDeadlineTimer(QDeadlineTimer::Forever));
    return wait(QDeadlineTimer(static_cast<qint64>(time)));
}

/*!
    Blocks the calling thread for \a secs seconds.

    \sa msleep(), usleep()
*/
void QThread::sleep(unsigned long secs) { sleep(std::chrono::seconds(static_cast<std::chrono::seconds::rep>(secs))); }

/*!
    Blocks the calling thread for \a msecs milliseconds.

    \sa sleep(), usleep()
*/
void QThread::msleep(unsigned long msecs) {
    sleep(std::chrono::milliseconds(static_cast<std::chrono::milliseconds::rep>(msecs)));
}

/*!
    Blocks the calling thread for \a usecs microseconds.

    \sa sleep(), msleep()
*/
void QThread::usleep(unsigned long usecs) {
    sleep(std::chrono::microseconds(static_cast<std::chrono::microseconds::rep>(usecs)));
}

/*!
    \overload
    Blocks the calling thread for \a nsecs nanoseconds.

    \since 6.6
*/
void QThread::sleep(std::chrono::nanoseconds nsecs) {
    if (nsecs.count() <= 0)
        return;
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(nsecs);
    timespec ts{};
    ts.tv_sec = static_cast<time_t>(secs.count());
    ts.tv_nsec = static_cast<long>((nsecs - secs).count());
    timespec rest{};
    while (nanosleep(&ts, &rest) == -1 && errno == EINTR)
        ts = rest;
}

#include "moc_qthread.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
