// MIT License - see LICENSE
#include <QtCore/qthreadpool.h>

#include "threaddata_p.h"

#include <QtCore/qdebug.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <vector>

/*!
    \class QThreadPool
    \brief The QThreadPool class manages a collection of threads.

    A thread pool runs QRunnable tasks on a limited number of reusable threads, which saves the
    cost of creating a thread for each task. Every program has a global pool,
    globalInstance(), and may create more.

    \code
    QThreadPool::globalInstance()->start([] { compute(); });
    QThreadPool::globalInstance()->waitForDone();
    \endcode

    start() hands a task to an idle thread, starts a new thread if fewer than
    maxThreadCount() threads are busy, and otherwise queues the task; queued tasks run in
    order of priority, and in order of arrival among equal priorities. A thread that has
    had no work for expiryTimeout() exits. The destructor waits for all tasks, including
    queued ones. The destructor of QCoreApplication waits for the global pool.

    \differences
    There is no \c cancel() or \c setServiceLevel(). The properties are plain functions; they cannot be bound. A
    thread count of 0 or less is accepted but at least one thread is always used. The pool's
    threads are QThread objects, so \c QThread::currentThread() works in a task and
    \c contains() recognises it.

    On Zephyr each thread of a pool takes a stack of \c CONFIG_NT_THREAD_STACK_SIZE bytes from the kernel
    heap, and the default maximum is one thread. See \l{Nt on Zephyr}.
    \enddifferences

    \sa QRunnable, QThread
*/

namespace {
std::atomic<QThreadPool *> g_global{nullptr};
} // namespace

struct QThreadPool::Data {
    using Pool = Data;
    class Worker;
    struct Item {
        QRunnable *runnable;
        int priority;
    };

    std::mutex m;
    // Signalled when the pool becomes idle, or when a thread leaves it.
    std::condition_variable cv;
    std::deque<Item> queue;
    std::vector<Worker *> workers;
    std::vector<Worker *> waiting;
    std::vector<Worker *> expired;
    int requestedMax = QThread::idealThreadCount();
    int expiry = 30000;
    int reserved = 0;
    int activeTasks = 0;
    uint stackSize = 0;
    QThread::QualityOfService serviceLevel = QThread::QualityOfService::Auto;
    QThread::Priority priority = QThread::InheritPriority;
    bool stopping = false;

    int maxThreads() const { return std::max(requestedMax, 1); }
    int active() const { return activeTasks + reserved; }
    bool tooMany() const {
        const int a = active();
        return a > maxThreads() && (a - reserved) > 1;
    }
    // The first task always gets a thread, even if reserved threads use up the limit.
    bool canStart() const { return workers.empty() || active() < maxThreads(); }
    bool idle() const { return queue.empty() && activeTasks == 0; }

    void enqueue(QRunnable *r, int prio) {
        auto it = std::find_if(queue.begin(), queue.end(), [prio](const Item &i) { return i.priority < prio; });
        queue.insert(it, {r, prio});
    }
    bool dispatchLocked(QRunnable *r);
    void startQueuedLocked();
    void reap();
};

class QThreadPool::Data::Worker : public QThread {
public:
    Worker(Pool *owner, QRunnable *first) : pool(owner), next(first) {}

    Pool *pool;
    QRunnable *next;
    std::condition_variable cv;
    bool expireNow = false;

protected:
    void run() override {
        std::unique_lock<std::mutex> lk(pool->m);
        for (;;) {
            QRunnable *r = next;
            next = nullptr;
            while (r) {
                const bool del = r->autoDelete();
                lk.unlock();
                r->run();
                if (del)
                    delete r;
                lk.lock();
                --pool->activeTasks;
                r = nullptr;
                if (!pool->queue.empty() && !pool->tooMany()) {
                    r = pool->queue.front().runnable;
                    pool->queue.pop_front();
                    ++pool->activeTasks;
                } else {
                    pool->cv.notify_all();
                }
            }

            pool->waiting.push_back(this);
            while (!next && !pool->stopping && !expireNow) {
                if (pool->expiry < 0) {
                    cv.wait(lk);
                } else if (cv.wait_for(lk, std::chrono::milliseconds(pool->expiry)) == std::cv_status::timeout) {
                    break;
                }
            }
            if (next)
                continue;
            pool->waiting.erase(std::remove(pool->waiting.begin(), pool->waiting.end(), this), pool->waiting.end());
            pool->workers.erase(std::remove(pool->workers.begin(), pool->workers.end(), this), pool->workers.end());
            pool->expired.push_back(this);
            pool->cv.notify_all();
            return;
        }
    }
};

// Counts `r` as active and gives it to an idle thread or a new one. The caller holds the lock.
bool QThreadPool::Data::dispatchLocked(QRunnable *r) {
    ++activeTasks;
    if (!waiting.empty()) {
        Worker *w = waiting.back();
        waiting.pop_back();
        w->next = r;
        w->cv.notify_one();
        return true;
    }
    auto *w = new Worker(this, r);
    if (stackSize > 0)
        w->setStackSize(stackSize);
    w->setServiceLevel(serviceLevel);
    w->start(priority);
    if (!w->isRunning()) {
        --activeTasks;
        delete w;
        qWarning("QThreadPool: Failed to start a thread");
        return false;
    }
    workers.push_back(w);
    return true;
}

void QThreadPool::Data::startQueuedLocked() {
    while (!queue.empty() && canStart()) {
        const Item item = queue.front();
        queue.pop_front();
        if (!dispatchLocked(item.runnable)) {
            queue.push_front(item);
            break;
        }
    }
}

// Threads that left the pool are joined and deleted by whoever comes along next.
void QThreadPool::Data::reap() {
    std::vector<Worker *> dead;
    {
        std::lock_guard<std::mutex> lk(m);
        dead.swap(expired);
    }
    for (Worker *w : dead) {
        w->wait();
        delete w;
    }
}

/*!
    Constructs a thread pool with the given \a parent. The maximum number of threads is
    QThread::idealThreadCount() and an idle thread expires after 30 seconds.
*/
QThreadPool::QThreadPool(QObject *parent) : QObject(parent), m_d(new Data) {}

/*!
    Destroys the pool. It waits for all queued and running tasks to finish, then ends and
    joins its threads.
*/
QThreadPool::~QThreadPool() {
    waitForDone();
    {
        std::unique_lock<std::mutex> lk(m_d->m);
        m_d->stopping = true;
        for (auto *w : m_d->waiting)
            w->cv.notify_one();
        m_d->cv.wait(lk, [this] { return m_d->workers.empty(); });
    }
    m_d->reap();
    QThreadPool *self = this;
    g_global.compare_exchange_strong(self, nullptr);
}

/*!
    Returns the global thread pool, which is created on first use. It is destroyed at exit,
    after QCoreApplication has waited for its tasks.
*/
QThreadPool *QThreadPool::globalInstance() {
    static QThreadPool instance;
    g_global.store(&instance);
    return &instance;
}

namespace QtPrivate {

void waitForGlobalThreadPool() {
    if (QThreadPool *pool = g_global.load())
        pool->waitForDone();
}

} // namespace QtPrivate

/*!
    Reserves a thread to run \a runnable and returns \c true, or returns \c false if all
    threads are in use. The runnable is not queued. The pool takes ownership of the runnable
    if it has autoDelete() set.

    \sa start()
*/
bool QThreadPool::tryStart(QRunnable *runnable) {
    if (!runnable)
        return false;
    m_d->reap();
    std::lock_guard<std::mutex> lk(m_d->m);
    return m_d->canStart() && m_d->dispatchLocked(runnable);
}

/*!
    \overload
    Runs \a functionToRun in a pool thread if one is available and returns \c true;
    returns \c false otherwise.
*/
bool QThreadPool::tryStart(std::function<void()> functionToRun) {
    QRunnable *r = QRunnable::create(std::move(functionToRun));
    if (tryStart(r))
        return true;
    delete r;
    return false;
}

/*!
    Runs \a runnable in a pool thread, or queues it if all threads are busy. Queued runnables
    run in order of \a priority, higher first, and in the order they were added if the
    priorities are equal. The pool deletes the runnable after it ran if
    \l{QRunnable::autoDelete()}{autoDelete()} is \c true.

    \sa tryStart(), QRunnable
*/
void QThreadPool::start(QRunnable *runnable, int priority) {
    if (!runnable)
        return;
    m_d->reap();
    std::lock_guard<std::mutex> lk(m_d->m);
    m_d->enqueue(runnable, priority);
    m_d->startQueuedLocked();
}

/*!
    \overload
    Runs \a functionToRun in a pool thread, or queues it, with the given \a priority.
*/
void QThreadPool::start(std::function<void()> functionToRun, int priority) {
    start(QRunnable::create(std::move(functionToRun)), priority);
}

/*!
    Runs \a runnable on a thread that was reserved with reserveThread(), and releases that
    reservation, so activeThreadCount() does not change. If \a runnable is \c nullptr, the
    thread is simply released.

    \since 6.3
    \sa reserveThread(), releaseThread()
*/
void QThreadPool::startOnReservedThread(QRunnable *runnable) {
    if (!runnable) {
        releaseThread();
        return;
    }
    m_d->reap();
    std::lock_guard<std::mutex> lk(m_d->m);
    if (m_d->reserved > 0)
        --m_d->reserved;
    if (!m_d->dispatchLocked(runnable))
        m_d->enqueue(runnable, 0);
}

/*!
    \overload
    \since 6.3
    Runs \a functionToRun on a reserved thread.
*/
void QThreadPool::startOnReservedThread(std::function<void()> functionToRun) {
    startOnReservedThread(QRunnable::create(std::move(functionToRun)));
}

/*!
    Returns the time in milliseconds that an idle thread waits for work before
    it exits. The default is 30000. A negative value means that threads never expire.

    \sa setExpiryTimeout()
*/
int QThreadPool::expiryTimeout() const {
    std::lock_guard<std::mutex> lk(m_d->m);
    return m_d->expiry;
}

/*!
    Sets the time an idle thread waits for work before it exits to \a expiryTimeout
    milliseconds. A negative value keeps threads alive until the pool is destroyed or
    waitForDone() is called. The new value applies the next time a thread becomes idle.

    \sa expiryTimeout()
*/
void QThreadPool::setExpiryTimeout(int expiryTimeout) {
    std::lock_guard<std::mutex> lk(m_d->m);
    m_d->expiry = expiryTimeout;
}

/*!
    Returns the maximum number of threads the pool uses. The default is
    QThread::idealThreadCount().

    \sa setMaxThreadCount(), activeThreadCount()
*/
int QThreadPool::maxThreadCount() const {
    std::lock_guard<std::mutex> lk(m_d->m);
    return m_d->requestedMax;
}

/*!
    Sets the maximum number of threads the pool uses to \a maxThreadCount. The pool always
    uses at least one thread, even if the limit is 0 or negative. Raising the limit starts
    queued tasks; lowering it does not stop running ones.

    \sa maxThreadCount(), reserveThread()
*/
void QThreadPool::setMaxThreadCount(int maxThreadCount) {
    std::lock_guard<std::mutex> lk(m_d->m);
    m_d->requestedMax = maxThreadCount;
    m_d->startQueuedLocked();
}

/*!
    Returns the number of threads that run a task, plus the reserved threads. It can be
    larger than maxThreadCount() because of reserveThread().

    \sa maxThreadCount(), reserveThread()
*/
int QThreadPool::activeThreadCount() const {
    std::lock_guard<std::mutex> lk(m_d->m);
    return m_d->active();
}

/*!
    \since 6.9
    Sets the service level of the threads of the pool to \a serviceLevel, for the threads that start afterward.

    \sa serviceLevel(), QThread::setServiceLevel()
*/
void QThreadPool::setServiceLevel(QThread::QualityOfService serviceLevel) {
    std::lock_guard<std::mutex> lk(m_d->m);
    m_d->serviceLevel = serviceLevel;
}

/*!
    \since 6.9
    Returns the service level of the threads of the pool.
*/
QThread::QualityOfService QThreadPool::serviceLevel() const {
    std::lock_guard<std::mutex> lk(m_d->m);
    return m_d->serviceLevel;
}

/*!
    Sets the stack size of threads that the pool starts from now on to \a stackSize bytes.
    0 selects the system default.

    \sa QThread::setStackSize()
*/
void QThreadPool::setStackSize(uint stackSize) {
    std::lock_guard<std::mutex> lk(m_d->m);
    m_d->stackSize = stackSize;
}

/*!
    Returns the stack size used for new threads, or 0 for the system default.

    \sa setStackSize()
*/
uint QThreadPool::stackSize() const {
    std::lock_guard<std::mutex> lk(m_d->m);
    return m_d->stackSize;
}

/*!
    Sets the priority of the pool's threads, current and future, to \a priority.
    \c InheritPriority, the default, gives threads the priority of the thread that starts them.

    \sa threadPriority(), QThread::setPriority()
*/
void QThreadPool::setThreadPriority(QThread::Priority priority) {
    std::lock_guard<std::mutex> lk(m_d->m);
    m_d->priority = priority;
    if (priority == QThread::InheritPriority)
        return;
    for (auto *w : m_d->workers)
        w->setPriority(priority);
}

/*!
    Returns the priority of the pool's threads.

    \sa setThreadPriority()
*/
QThread::Priority QThreadPool::threadPriority() const {
    std::lock_guard<std::mutex> lk(m_d->m);
    return m_d->priority;
}

/*!
    Reserves a thread without running anything on it: activeThreadCount() increases by one
    even if it is already at maxThreadCount(). Use it when the calling thread is going to
    block for a long time on something the pool does. Call releaseThread() when done.

    \sa releaseThread(), startOnReservedThread()
*/
void QThreadPool::reserveThread() {
    std::lock_guard<std::mutex> lk(m_d->m);
    ++m_d->reserved;
}

/*!
    Releases a thread that reserveThread() reserved, which may start a queued task.

    \sa reserveThread()
*/
void QThreadPool::releaseThread() {
    m_d->reap();
    std::lock_guard<std::mutex> lk(m_d->m);
    if (m_d->reserved > 0)
        --m_d->reserved;
    m_d->startQueuedLocked();
    m_d->cv.notify_all();
}

/*!
    Waits up to \a msecs milliseconds for all queued and running tasks to finish and the
    pool's threads to exit; a negative value waits as long as it takes. Returns \c true if
    the pool is idle, and \c false if the time ran out. Threads that exit are replaced when
    new tasks arrive.

    \sa start()
*/
bool QThreadPool::waitForDone(int msecs) { return waitForDone(QDeadlineTimer(msecs)); }

/*!
    \overload
    Waits until \a deadline has passed.
*/
bool QThreadPool::waitForDone(QDeadlineTimer deadline) {
    Data &d = *m_d;
    {
        std::unique_lock<std::mutex> lk(d.m);
        auto waitUntil = [&](auto pred) {
            while (!pred()) {
                if (deadline.isForever()) {
                    d.cv.wait(lk);
                } else {
                    if (deadline.hasExpired())
                        return false;
                    d.cv.wait_until(lk, deadline.timePoint());
                }
            }
            return true;
        };
        if (!waitUntil([&d] { return d.idle(); }))
            return false;
        // Idle threads go away, like in Qt; they are recreated on demand.
        const std::vector<Data::Worker *> leaving = d.waiting;
        for (auto *w : leaving) {
            w->expireNow = true;
            w->cv.notify_one();
        }
        d.waiting.clear();
        waitUntil([&] {
            return std::none_of(leaving.begin(), leaving.end(), [&d](Data::Worker *w) {
                return std::find(d.workers.begin(), d.workers.end(), w) != d.workers.end();
            });
        });
    }
    d.reap();
    return true;
}

/*!
    Removes \a runnable from the queue if it has not started and returns \c true. Returns
    \c false if it is not queued, for instance because it is running. The pool does not delete
    the runnable, whatever autoDelete() says; the caller owns it again.

    \sa clear()
*/
bool QThreadPool::tryTake(QRunnable *runnable) {
    std::lock_guard<std::mutex> lk(m_d->m);
    auto it = std::find_if(m_d->queue.begin(), m_d->queue.end(),
                           [runnable](const Data::Item &i) { return i.runnable == runnable; });
    if (it == m_d->queue.end())
        return false;
    m_d->queue.erase(it);
    m_d->cv.notify_all();
    return true;
}

/*!
    Removes all queued runnables that have not started, and deletes those with
    \l{QRunnable::autoDelete()}{autoDelete()} set to \c true. Running tasks are not
    affected.

    \sa tryTake()
*/
void QThreadPool::clear() {
    std::deque<Data::Item> items;
    {
        std::lock_guard<std::mutex> lk(m_d->m);
        items.swap(m_d->queue);
        m_d->cv.notify_all();
    }
    for (const auto &i : items) {
        if (i.runnable->autoDelete())
            delete i.runnable;
    }
}

/*!
    Returns \c true if \a thread is one of the threads of this pool, so a task can tell
    whether it runs in the pool.

    \since 6.0
*/
bool QThreadPool::contains(const QThread *thread) const {
    std::lock_guard<std::mutex> lk(m_d->m);
    auto has = [thread](const std::vector<Data::Worker *> &v) {
        return std::find(v.begin(), v.end(), thread) != v.end();
    };
    return has(m_d->workers) || has(m_d->expired);
}

#include "moc_qthreadpool.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
