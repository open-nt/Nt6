// MIT License - see LICENSE
#pragma once

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qglobal.h>
#include <QtCore/qnamespace.h>

#include <chrono>
#include <climits>
#include <condition_variable>
#include <mutex>
#include <utility>

/*!
    \class QMutex
    \brief The QMutex class provides access serialization between threads.

    A QMutex protects data that several threads access. A thread calls lock()
    before touching the data and unlock() afterwards; any other thread that
    calls lock() in the meantime blocks until the mutex is released. Prefer
    QMutexLocker, which unlocks the mutex automatically when it goes out of
    scope:

    \code
    QMutex mutex;
    int counter = 0;

    void increment()
    {
        QMutexLocker locker(&mutex);
        ++counter;
    }
    \endcode

    A QMutex is not recursive: locking it twice from the same thread without
    unlocking in between is undefined behavior. Use QRecursiveMutex when that
    is needed.

    \differences
    QMutex wraps \c std::timed_mutex. It has no \c RecursionMode constructor
    argument.
    \enddifferences

    \sa QMutexLocker, QRecursiveMutex, QReadWriteLock, QWaitCondition
*/
class QMutex {
public:
    /*!
        Constructs an unlocked mutex.
    */
    QMutex() = default;
    QMutex(const QMutex &) = delete;
    QMutex &operator=(const QMutex &) = delete;

    /*!
        Locks the mutex. If another thread holds the lock, this call blocks
        until the mutex becomes available.

        \sa unlock(), tryLock()
    */
    void lock() { m.lock(); }
    /*!
        Unlocks the mutex. The mutex must be locked by the calling thread.

        \sa lock()
    */
    void unlock() noexcept { m.unlock(); }
    /*!
        Attempts to lock the mutex, waiting up to \a timeoutMs milliseconds
        for it to become available. Returns \c true if the lock was obtained;
        otherwise returns \c false.

        If \a timeoutMs is 0, the call returns immediately. A negative
        \a timeoutMs waits until the lock is obtained, like lock().

        \sa lock(), unlock()
    */
    bool tryLock(int timeoutMs = 0) {
        if (timeoutMs < 0) {
            lock();
            return true;
        }
        if (timeoutMs == 0)
            return m.try_lock();
        return m.try_lock_for(std::chrono::milliseconds(timeoutMs));
    }
    /*!
        \overload
        \since 6.6
        Attempts to lock the mutex until \a deadline expires. Returns \c true
        if the lock was obtained; otherwise returns \c false. A deadline that
        never expires waits like lock().
    */
    bool tryLock(QDeadlineTimer deadline) {
        if (deadline.isForever()) {
            lock();
            return true;
        }
        return m.try_lock_until(deadline.timePoint());
    }
    /*!
        Attempts to lock the mutex, waiting for at most \a duration. Returns
        \c true if the lock was obtained; otherwise returns \c false. This is
        provided for use with \c std::unique_lock.

        \sa tryLock()
    */
    template <class Rep, class Period> bool try_lock_for(std::chrono::duration<Rep, Period> duration) {
        return m.try_lock_for(duration);
    }
    /*!
        Attempts to lock the mutex, waiting until \a timePoint at most.
        Returns \c true if the lock was obtained; otherwise returns \c false.
        This is provided for use with \c std::unique_lock.

        \sa tryLock()
    */
    template <class Clock, class Duration> bool try_lock_until(std::chrono::time_point<Clock, Duration> timePoint) {
        return m.try_lock_until(timePoint);
    }
    /*!
        Attempts to lock the mutex without blocking. Returns \c true if the
        lock was obtained; otherwise returns \c false. This is provided for use
        with \c std::lock and \c std::unique_lock.

        \sa tryLock()
    */
    bool try_lock() { return m.try_lock(); }
    /*!
        Returns \c false; a QMutex is never recursive.

        \sa QRecursiveMutex
    */
    bool isRecursive() const noexcept { return false; }

private:
    std::timed_mutex m;
};

/*!
    \class QRecursiveMutex
    \brief The QRecursiveMutex class provides access serialization between threads.

    A QRecursiveMutex can be locked repeatedly by the thread that already owns
    it. Each call to lock() or a successful tryLock() must be paired with a
    call to unlock(); the mutex is released for other threads when the lock
    count returns to zero.

    \differences
    QRecursiveMutex wraps \c std::recursive_timed_mutex.
    \enddifferences

    \sa QMutex, QMutexLocker
*/
class QRecursiveMutex {
public:
    /*!
        Constructs an unlocked recursive mutex.
    */
    QRecursiveMutex() = default;
    QRecursiveMutex(const QRecursiveMutex &) = delete;
    QRecursiveMutex &operator=(const QRecursiveMutex &) = delete;

    /*!
        Locks the mutex. If another thread holds the lock, this call blocks
        until the mutex becomes available. A thread that already owns the
        mutex may call lock() again.

        \sa unlock(), tryLock()
    */
    void lock() { m.lock(); }
    /*!
        Unlocks the mutex once. The calling thread must own the mutex.

        \sa lock()
    */
    void unlock() noexcept { m.unlock(); }
    /*!
        Attempts to lock the mutex, waiting up to \a timeoutMs milliseconds
        for it to become available. Returns \c true if the lock was obtained;
        otherwise returns \c false.

        If \a timeoutMs is 0, the call returns immediately. A negative
        \a timeoutMs waits until the lock is obtained, like lock().

        \sa lock(), unlock()
    */
    bool tryLock(int timeoutMs = 0) {
        if (timeoutMs < 0) {
            lock();
            return true;
        }
        if (timeoutMs == 0)
            return m.try_lock();
        return m.try_lock_for(std::chrono::milliseconds(timeoutMs));
    }
    /*!
        \overload
        \since 6.6
        Attempts to lock the mutex until \a deadline expires. Returns \c true
        if the lock was obtained; otherwise returns \c false. A deadline that
        never expires waits like lock().
    */
    bool tryLock(QDeadlineTimer deadline) {
        if (deadline.isForever()) {
            lock();
            return true;
        }
        return m.try_lock_until(deadline.timePoint());
    }
    /*!
        Attempts to lock the mutex without blocking. Returns \c true if the
        lock was obtained; otherwise returns \c false. This is provided for use
        with \c std::lock and \c std::unique_lock.

        \sa tryLock()
    */
    bool try_lock() { return m.try_lock(); }
    /*!
        Attempts to lock the mutex, waiting for at most \a duration. Returns
        \c true if the lock was obtained; otherwise returns \c false. This is
        provided for use with \c std::unique_lock.

        \sa tryLock()
    */
    template <class Rep, class Period> bool try_lock_for(std::chrono::duration<Rep, Period> duration) {
        return m.try_lock_for(duration);
    }
    /*!
        Attempts to lock the mutex, waiting until \a timePoint at most.
        Returns \c true if the lock was obtained; otherwise returns \c false.
        This is provided for use with \c std::unique_lock.

        \sa tryLock()
    */
    template <class Clock, class Duration> bool try_lock_until(std::chrono::time_point<Clock, Duration> timePoint) {
        return m.try_lock_until(timePoint);
    }

private:
    std::recursive_timed_mutex m;
};

/*!
    \class QMutexLocker
    \brief The QMutexLocker class is a convenience class that simplifies locking and unlocking mutexes.

    A QMutexLocker locks the mutex it is given when it is constructed and
    unlocks it when it is destroyed, so the mutex is released on every path out
    of a scope, including exceptions and early returns:

    \code
    QMutex mutex;

    int complexFunction(int flag)
    {
        QMutexLocker locker(&mutex);
        if (flag < 0)
            return 0;
        return flag * 2;
    }
    \endcode

    The class works with any type that provides \c lock() and \c unlock(),
    such as QMutex and QRecursiveMutex. A QMutexLocker can be moved but not
    copied.

    \differences
    A null pointer is accepted and results in a locker that never locks
    anything. The locker works with any type that has \c lock() and
    \c unlock(), not only the Nt mutex classes.
    \enddifferences

    \sa QMutex, QRecursiveMutex, QReadLocker, QWriteLocker
*/
template <typename MutexT>
class QMutexLocker {
public:
    /*!
        Constructs a QMutexLocker and locks \a mtx. The mutex is unlocked when
        the locker is destroyed. If \a mtx is \c nullptr, the locker does
        nothing.

        \sa unlock()
    */
    explicit QMutexLocker(MutexT *mtx) : mt(mtx) { if (mt) { mt->lock(); locked = true; } }
    QMutexLocker(const QMutexLocker &) = delete;
    /*!
        Move-constructs a locker from \a o, taking over the mutex and its
        locked state. \a o no longer refers to a mutex afterwards.
    */
    QMutexLocker(QMutexLocker &&o) noexcept : mt(o.mt), locked(o.locked) { o.mt = nullptr; o.locked = false; }
    QMutexLocker &operator=(const QMutexLocker &) = delete;
    /*!
        Move-assigns \a o to this locker. The mutex this locker held is
        unlocked first; then the locker takes over the mutex and locked state
        of \a o, which no longer refers to a mutex.
    */
    QMutexLocker &operator=(QMutexLocker &&o) noexcept {
        QMutexLocker moved(std::move(o));
        swap(moved);
        return *this;
    }
    /*!
        Swaps the mutex and the locked state of this locker with those of
        \a other.
    */
    void swap(QMutexLocker &other) noexcept {
        std::swap(mt, other.mt);
        std::swap(locked, other.locked);
    }
    /*!
        Destroys the locker and unlocks the mutex if it is currently locked by
        this locker.
    */
    ~QMutexLocker() { unlock(); }
    /*!
        Unlocks the mutex if this locker holds it.

        \sa relock()
    */
    void unlock() { if (mt && locked) { mt->unlock(); locked = false; } }
    /*!
        Relocks the mutex after it was unlocked with unlock(). Does nothing if
        the mutex is already locked by this locker.

        \sa unlock()
    */
    void relock() { if (mt && !locked) { mt->lock(); locked = true; } }
    /*!
        Returns the mutex this locker operates on, or \c nullptr if there is
        none.
    */
    MutexT *mutex() const { return mt; }
    /*!
        Returns \c true if this locker currently holds the mutex; otherwise
        returns \c false.

        \sa unlock(), relock()
    */
    bool isLocked() const { return locked; }

private:
    MutexT *mt;
    bool locked = false;
};

/*!
    \class QReadWriteLock
    \brief The QReadWriteLock class provides read-write locking.

    A QReadWriteLock lets many threads read shared data at the same time while
    giving a writing thread exclusive access. It suits data that is read often
    and written rarely.

    \code
    QReadWriteLock lock;

    void reader()
    {
        QReadLocker locker(&lock);
        // read shared data
    }

    void writer()
    {
        QWriteLocker locker(&lock);
        // modify shared data
    }
    \endcode

    Writers are preferred: once a thread is waiting to lock for writing, new
    readers block until it has finished. The lock is not recursive.

    \differences
    There is no \c RecursionMode constructor argument, so a thread that
    holds the lock for reading must not lock it again while a writer waits.
    \enddifferences

    \sa QReadLocker, QWriteLocker, QMutex
*/
class QReadWriteLock {
public:
    /*!
        Constructs an unlocked read-write lock.
    */
    QReadWriteLock() = default;
    QReadWriteLock(const QReadWriteLock &) = delete;
    QReadWriteLock &operator=(const QReadWriteLock &) = delete;

    /*!
        Locks the lock for reading. This call blocks while another thread holds
        the lock for writing or is waiting to.

        \sa tryLockForRead(), unlock()
    */
    void lockForRead() {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&] { return !writer && waitingWriters == 0; });
        ++readers;
    }
    /*!
        Locks the lock for writing. This call blocks until no other thread
        holds the lock for reading or writing.

        \sa tryLockForWrite(), unlock()
    */
    void lockForWrite() {
        std::unique_lock<std::mutex> lk(m);
        ++waitingWriters;
        cv.wait(lk, [&] { return !writer && readers == 0; });
        --waitingWriters;
        writer = true;
    }
    /*!
        Attempts to lock the lock for reading, waiting up to \a timeoutMs
        milliseconds. Returns \c true if the lock was obtained; otherwise
        returns \c false. If \a timeoutMs is 0, the call returns immediately;
        if it is negative, the call waits like lockForRead().

        \sa lockForRead(), unlock()
    */
    bool tryLockForRead(int timeoutMs = 0) { return tryLockForRead(QDeadlineTimer(static_cast<qint64>(timeoutMs))); }
    /*!
        \overload
        \since 6.6
        Attempts to lock the lock for reading until \a deadline expires.
        Returns \c true if the lock was obtained; otherwise returns \c false.
    */
    bool tryLockForRead(QDeadlineTimer deadline) {
        std::unique_lock<std::mutex> lk(m);
        if (!waitUntil(lk, deadline, [&] { return !writer && waitingWriters == 0; }))
            return false;
        ++readers;
        return true;
    }
    /*!
        Attempts to lock the lock for writing, waiting up to \a timeoutMs
        milliseconds. Returns \c true if the lock was obtained; otherwise
        returns \c false. If \a timeoutMs is 0, the call returns immediately;
        if it is negative, the call waits like lockForWrite().

        \sa lockForWrite(), unlock()
    */
    bool tryLockForWrite(int timeoutMs = 0) { return tryLockForWrite(QDeadlineTimer(static_cast<qint64>(timeoutMs))); }
    /*!
        \overload
        \since 6.6
        Attempts to lock the lock for writing until \a deadline expires.
        Returns \c true if the lock was obtained; otherwise returns \c false.
    */
    bool tryLockForWrite(QDeadlineTimer deadline) {
        std::unique_lock<std::mutex> lk(m);
        ++waitingWriters;
        const bool ok = waitUntil(lk, deadline, [&] { return !writer && readers == 0; });
        --waitingWriters;
        if (ok)
            writer = true;
        else
            cv.notify_all();
        return ok;
    }
    /*!
        Unlocks the lock. The calling thread must hold it for reading or for
        writing; if the lock is held for writing, the write lock is released,
        otherwise one read lock is.

        \sa lockForRead(), lockForWrite()
    */
    void unlock() {
        {
            std::lock_guard<std::mutex> lk(m);
            if (writer)
                writer = false;
            else if (readers > 0)
                --readers;
        }
        cv.notify_all();
    }

private:
    friend class QWaitCondition;
    template <typename Pred> bool waitUntil(std::unique_lock<std::mutex> &lk, QDeadlineTimer deadline, Pred pred) {
        if (deadline.isForever()) {
            cv.wait(lk, pred);
            return true;
        }
        return cv.wait_until(lk, deadline.timePoint(), pred);
    }
    bool lockedForWrite() {
        std::lock_guard<std::mutex> lk(m);
        return writer;
    }

    std::mutex m;
    std::condition_variable cv;
    int readers = 0;
    int waitingWriters = 0;
    bool writer = false;
};

/*!
    \class QReadLocker
    \brief The QReadLocker class is a convenience class that simplifies locking and unlocking read-write locks for read access.

    A QReadLocker locks a QReadWriteLock for reading when it is constructed and
    unlocks it when it is destroyed.

    \code
    QReadWriteLock lock;

    QString value()
    {
        QReadLocker locker(&lock);
        return sharedValue;
    }
    \endcode

    \differences
    The locker is neither movable nor copyable. The lock pointer must not be
    \c nullptr.
    \enddifferences

    \sa QWriteLocker, QReadWriteLock
*/
class QReadLocker {
public:
    /*!
        Constructs a QReadLocker and locks \a l for reading. The lock is
        released when the locker is destroyed.
    */
    explicit QReadLocker(QReadWriteLock *l) : lk(l) { lk->lockForRead(); locked = true; }
    /*!
        Destroys the locker and unlocks the lock if it is currently held by
        this locker.
    */
    ~QReadLocker() { unlock(); }
    QReadLocker(const QReadLocker &) = delete;
    /*!
        Unlocks the lock if this locker holds it.

        \sa relock()
    */
    void unlock() { if (locked) { lk->unlock(); locked = false; } }
    /*!
        Relocks the lock for reading after it was unlocked with unlock(). Does
        nothing if the lock is already held by this locker.

        \sa unlock()
    */
    void relock() { if (!locked) { lk->lockForRead(); locked = true; } }
    /*!
        Returns the read-write lock this locker operates on.
    */
    QReadWriteLock *readWriteLock() const { return lk; }

private:
    QReadWriteLock *lk;
    bool locked = false;
};

/*!
    \class QWriteLocker
    \brief The QWriteLocker class is a convenience class that simplifies locking and unlocking read-write locks for write access.

    A QWriteLocker locks a QReadWriteLock for writing when it is constructed and
    unlocks it when it is destroyed.

    \code
    QReadWriteLock lock;

    void setValue(const QString &value)
    {
        QWriteLocker locker(&lock);
        sharedValue = value;
    }
    \endcode

    \differences
    The locker is neither movable nor copyable. The lock pointer must not be
    \c nullptr.
    \enddifferences

    \sa QReadLocker, QReadWriteLock
*/
class QWriteLocker {
public:
    /*!
        Constructs a QWriteLocker and locks \a l for writing. The lock is
        released when the locker is destroyed.
    */
    explicit QWriteLocker(QReadWriteLock *l) : lk(l) { lk->lockForWrite(); locked = true; }
    /*!
        Destroys the locker and unlocks the lock if it is currently held by
        this locker.
    */
    ~QWriteLocker() { unlock(); }
    QWriteLocker(const QWriteLocker &) = delete;
    /*!
        Unlocks the lock if this locker holds it.

        \sa relock()
    */
    void unlock() { if (locked) { lk->unlock(); locked = false; } }
    /*!
        Relocks the lock for writing after it was unlocked with unlock(). Does
        nothing if the lock is already held by this locker.

        \sa unlock()
    */
    void relock() { if (!locked) { lk->lockForWrite(); locked = true; } }
    /*!
        Returns the read-write lock this locker operates on.
    */
    QReadWriteLock *readWriteLock() const { return lk; }

private:
    QReadWriteLock *lk;
    bool locked = false;
};

/*!
    \class QWaitCondition
    \brief The QWaitCondition class provides a condition variable for synchronizing threads.

    A QWaitCondition lets a thread sleep until another thread signals that
    something has changed. The waiting thread passes a locked QMutex to wait();
    the mutex is released while the thread sleeps and locked again before
    wait() returns.

    \code
    QMutex mutex;
    QWaitCondition ready;
    bool done = false;

    void consumer()
    {
        QMutexLocker locker(&mutex);
        while (!done)
            ready.wait(&mutex);
    }

    void producer()
    {
        {
            QMutexLocker locker(&mutex);
            done = true;
        }
        ready.wakeAll();
    }
    \endcode

    \differences
    The \c notify_one() and \c notify_all() members, named as in the C++
    standard library, are additions.
    \enddifferences

    \sa QMutex, QSemaphore
*/
class QWaitCondition {
public:
    /*!
        Constructs a wait condition.
    */
    QWaitCondition() = default;
    QWaitCondition(const QWaitCondition &) = delete;

    /*!
        Releases \a mutex and waits on the condition for up to \a timeMs
        milliseconds. The mutex must be locked by the calling thread, and it is
        locked again before this function returns.

        Returns \c true if the thread was woken by wakeOne() or wakeAll(), or
        \c false if the time elapsed. The default \a timeMs, \c ULONG_MAX, waits
        indefinitely.

        \sa wakeOne(), wakeAll()
    */
    bool wait(QMutex *mutex, unsigned long timeMs = ULONG_MAX) {
        std::unique_lock<QMutex> lk(*mutex, std::adopt_lock);
        bool ok = true;
        if (timeMs == ULONG_MAX)
            cv.wait(lk);
        else
            ok = cv.wait_for(lk, std::chrono::milliseconds(timeMs)) == std::cv_status::no_timeout;
        lk.release();
        return ok;
    }
    /*!
        \overload

        Releases \a mutex and waits until woken or until \a deadline expires.
        A deadline that never expires waits indefinitely.
    */
    bool wait(QMutex *mutex, QDeadlineTimer deadline) {
        std::unique_lock<QMutex> lk(*mutex, std::adopt_lock);
        const bool ok = waitOn(lk, deadline);
        lk.release();
        return ok;
    }
    /*!
        \overload

        Releases \a lock and waits on the condition for up to \a timeMs
        milliseconds. The lock must be held by the calling thread, for reading
        or for writing, and it is locked again in the same mode before this
        function returns. The default \a timeMs, \c ULONG_MAX, waits
        indefinitely.
    */
    bool wait(QReadWriteLock *lock, unsigned long timeMs = ULONG_MAX) {
        return wait(lock, timeMs == ULONG_MAX ? QDeadlineTimer(QDeadlineTimer::Forever)
                                              : QDeadlineTimer(std::chrono::milliseconds(static_cast<qint64>(timeMs))));
    }
    /*!
        \overload

        Releases \a lock and waits until woken or until \a deadline expires.
        The lock is locked again, in the mode the calling thread held it,
        before this function returns.
    */
    bool wait(QReadWriteLock *lock, QDeadlineTimer deadline) {
        ReadWriteLockable l{lock, lock->lockedForWrite()};
        std::unique_lock<ReadWriteLockable> lk(l, std::adopt_lock);
        const bool ok = waitOn(lk, deadline);
        lk.release();
        return ok;
    }
    /*!
        Wakes one thread waiting on the condition. Which thread is woken is
        unspecified.

        \sa wakeAll(), wait()
    */
    void wakeOne() { cv.notify_one(); }
    /*!
        Wakes all threads waiting on the condition.

        \sa wakeOne(), wait()
    */
    void wakeAll() { cv.notify_all(); }
    /*!
        Equivalent to wakeOne(); provided for compatibility with the C++
        standard library naming.
    */
    void notify_one() { cv.notify_one(); }
    /*!
        Equivalent to wakeAll(); provided for compatibility with the C++
        standard library naming.
    */
    void notify_all() { cv.notify_all(); }

private:
    struct ReadWriteLockable {
        QReadWriteLock *rw;
        bool write;
        void lock() {
            if (write)
                rw->lockForWrite();
            else
                rw->lockForRead();
        }
        void unlock() { rw->unlock(); }
    };
    template <typename Lock> bool waitOn(Lock &lk, QDeadlineTimer deadline) {
        if (deadline.isForever()) {
            cv.wait(lk);
            return true;
        }
        return cv.wait_until(lk, deadline.timePoint()) == std::cv_status::no_timeout;
    }

    std::condition_variable_any cv;
};

/*!
    \class QSemaphore
    \brief The QSemaphore class provides a general counting semaphore.

    A semaphore guards a number of identical resources. acquire() takes
    resources, blocking until enough are available, and release() gives them
    back.

    \code
    QSemaphore freeSlots(8);

    void useSlot()
    {
        freeSlots.acquire();
        // work with one slot
        freeSlots.release();
    }
    \endcode

    \differences
    The semaphore is built on \c std::mutex and \c std::condition_variable,
    and release() wakes every waiting thread.
    \enddifferences

    \sa QSemaphoreReleaser, QMutex, QWaitCondition
*/
class QSemaphore {
public:
    /*!
        Constructs a semaphore with \a n resources available.
    */
    explicit QSemaphore(int n = 0) : avail(n) {}
    QSemaphore(const QSemaphore &) = delete;

    /*!
        Acquires \a n resources, blocking until that many are available.

        \sa release(), tryAcquire(), available()
    */
    void acquire(int n = 1) {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&] { return avail >= n; });
        avail -= n;
    }
    /*!
        Tries to acquire \a n resources, waiting up to \a timeoutMs
        milliseconds for them to become available. Returns \c true if the
        resources were acquired; otherwise returns \c false and acquires
        nothing. If \a timeoutMs is 0, the call returns immediately; if it
        is negative, the call waits like acquire().

        \sa acquire(), release()
    */
    bool tryAcquire(int n, int timeoutMs) { return tryAcquire(n, QDeadlineTimer(static_cast<qint64>(timeoutMs))); }
    /*!
        \overload
        Tries to acquire \a n resources without waiting. Returns \c true if
        they were acquired; otherwise returns \c false.
    */
    bool tryAcquire(int n = 1) { return tryAcquire(n, QDeadlineTimer()); }
    /*!
        \overload
        \since 6.6
        Tries to acquire \a n resources, waiting until \a deadline expires.
        Returns \c true if the resources were acquired; otherwise returns
        \c false and acquires nothing.
    */
    bool tryAcquire(int n, QDeadlineTimer deadline) {
        std::unique_lock<std::mutex> lk(m);
        const auto ready = [&] { return avail >= n; };
        if (deadline.isForever())
            cv.wait(lk, ready);
        else if (!cv.wait_until(lk, deadline.timePoint(), ready))
            return false;
        avail -= n;
        return true;
    }
    /*!
        \since 6.3
        Tries to acquire one resource without waiting. Returns \c true on
        success. This is provided for compatibility with
        \c std::counting_semaphore.

        \sa tryAcquire()
    */
    bool try_acquire() noexcept { return tryAcquire(1); }
    /*!
        \since 6.3
        Tries to acquire one resource, waiting for at most \a timeout.
        Returns \c true on success. This is provided for compatibility with
        \c std::counting_semaphore.

        \sa tryAcquire()
    */
    template <class Rep, class Period> bool try_acquire_for(const std::chrono::duration<Rep, Period> &timeout) {
        return tryAcquire(1, QDeadlineTimer(timeout));
    }
    /*!
        \since 6.3
        Tries to acquire one resource, waiting until \a timePoint at most.
        Returns \c true on success. This is provided for compatibility with
        \c std::counting_semaphore.

        \sa tryAcquire()
    */
    template <class Clock, class Duration>
    bool try_acquire_until(const std::chrono::time_point<Clock, Duration> &timePoint) {
        return try_acquire_for(timePoint - Clock::now());
    }
    /*!
        Releases \a n resources, waking threads that are waiting to acquire
        them. The count may be raised above the value given to the constructor.

        \sa acquire(), available()
    */
    void release(int n = 1) {
        {
            std::lock_guard<std::mutex> lk(m);
            avail += n;
        }
        cv.notify_all();
    }
    /*!
        Returns the number of resources currently available. The value may
        change as soon as it is returned.

        \sa acquire(), release()
    */
    int available() const {
        std::lock_guard<std::mutex> lk(m);
        return avail;
    }

private:
    mutable std::mutex m;
    std::condition_variable cv;
    int avail;
};

/*!
    \class QSemaphoreReleaser
    \brief The QSemaphoreReleaser class provides exception-safe deferral of a QSemaphore::release() call.

    A QSemaphoreReleaser releases a number of resources on a QSemaphore when it
    is destroyed, unless cancel() was called first.

    \code
    QSemaphore sem;

    void work()
    {
        QSemaphoreReleaser releaser(sem);
        // the semaphore is released when this function returns
    }
    \endcode

    \differences
    None.
    \enddifferences

    \sa QSemaphore
*/
class QSemaphoreReleaser {
public:
    /*!
        Constructs a releaser that does nothing.
    */
    QSemaphoreReleaser() = default;
    /*!
        Constructs a releaser that releases \a n resources on \a s when it is
        destroyed.
    */
    explicit QSemaphoreReleaser(QSemaphore &s, int n = 1) : sem(&s), count(n) {}
    /*!
        \overload
        Constructs a releaser that releases \a n resources on \a s when it is
        destroyed. If \a s is \c nullptr, the releaser does nothing.
    */
    explicit QSemaphoreReleaser(QSemaphore *s, int n = 1) : sem(s), count(n) {}
    /*!
        Move-constructs a releaser from \a other, which no longer releases
        anything afterwards.
    */
    QSemaphoreReleaser(QSemaphoreReleaser &&other) noexcept
        : sem(std::exchange(other.sem, nullptr)), count(other.count) {}
    /*!
        Move-assigns \a other to this releaser and returns a reference to it.
        A release pending on this releaser happens first.
    */
    QSemaphoreReleaser &operator=(QSemaphoreReleaser &&other) noexcept {
        QSemaphoreReleaser moved(std::move(other));
        swap(moved);
        return *this;
    }
    /*!
        Destroys the releaser and releases the resources on the semaphore,
        unless it was cancelled.
    */
    ~QSemaphoreReleaser() { if (sem) sem->release(count); }
    QSemaphoreReleaser(const QSemaphoreReleaser &) = delete;
    QSemaphoreReleaser &operator=(const QSemaphoreReleaser &) = delete;
    /*!
        Swaps the semaphore and count of this releaser with those of
        \a other.
    */
    void swap(QSemaphoreReleaser &other) noexcept {
        std::swap(sem, other.sem);
        std::swap(count, other.count);
    }
    /*!
        Returns the semaphore the releaser operates on, or \c nullptr if there
        is none or the release was cancelled.

        \sa cancel()
    */
    QSemaphore *semaphore() const noexcept { return sem; }
    /*!
        Cancels the pending release and returns the semaphore the releaser was
        operating on, or \c nullptr if there was none. The destructor then does
        nothing.
    */
    QSemaphore *cancel() { return std::exchange(sem, nullptr); }

private:
    QSemaphore *sem = nullptr;
    int count = 0;
};
