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

class QMutex {
public:
    QMutex() = default;
    QMutex(const QMutex &) = delete;
    QMutex &operator=(const QMutex &) = delete;

    void lock() { m.lock(); }
    void unlock() noexcept { m.unlock(); }
    bool tryLock(int timeoutMs = 0) {
        if (timeoutMs <= 0)
            return m.try_lock();
        return m.try_lock_for(std::chrono::milliseconds(timeoutMs));
    }
    bool try_lock() { return m.try_lock(); }
    bool isRecursive() const noexcept { return false; }

private:
    std::timed_mutex m;
};

class QRecursiveMutex {
public:
    QRecursiveMutex() = default;
    QRecursiveMutex(const QRecursiveMutex &) = delete;
    QRecursiveMutex &operator=(const QRecursiveMutex &) = delete;

    void lock() { m.lock(); }
    void unlock() noexcept { m.unlock(); }
    bool tryLock(int timeoutMs = 0) {
        if (timeoutMs <= 0)
            return m.try_lock();
        return m.try_lock_for(std::chrono::milliseconds(timeoutMs));
    }

private:
    std::recursive_timed_mutex m;
};

template <typename MutexT>
class QMutexLocker {
public:
    explicit QMutexLocker(MutexT *mtx) : mt(mtx) { if (mt) { mt->lock(); locked = true; } }
    QMutexLocker(const QMutexLocker &) = delete;
    QMutexLocker(QMutexLocker &&o) noexcept : mt(o.mt), locked(o.locked) { o.mt = nullptr; o.locked = false; }
    ~QMutexLocker() { unlock(); }
    void unlock() { if (mt && locked) { mt->unlock(); locked = false; } }
    void relock() { if (mt && !locked) { mt->lock(); locked = true; } }
    MutexT *mutex() const { return mt; }
    bool isLocked() const { return locked; }

private:
    MutexT *mt;
    bool locked = false;
};

class QReadWriteLock {
public:
    QReadWriteLock() = default;
    QReadWriteLock(const QReadWriteLock &) = delete;
    QReadWriteLock &operator=(const QReadWriteLock &) = delete;

    void lockForRead() {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&] { return !writer && waitingWriters == 0; });
        ++readers;
    }
    void lockForWrite() {
        std::unique_lock<std::mutex> lk(m);
        ++waitingWriters;
        cv.wait(lk, [&] { return !writer && readers == 0; });
        --waitingWriters;
        writer = true;
    }
    bool tryLockForRead(int timeoutMs = 0) {
        std::unique_lock<std::mutex> lk(m);
        if (!cv.wait_for(lk, std::chrono::milliseconds(timeoutMs < 0 ? 0 : timeoutMs), [&] { return !writer && waitingWriters == 0; }))
            return false;
        ++readers;
        return true;
    }
    bool tryLockForWrite(int timeoutMs = 0) {
        std::unique_lock<std::mutex> lk(m);
        ++waitingWriters;
        const bool ok = cv.wait_for(lk, std::chrono::milliseconds(timeoutMs < 0 ? 0 : timeoutMs), [&] { return !writer && readers == 0; });
        --waitingWriters;
        if (ok)
            writer = true;
        else
            cv.notify_all();
        return ok;
    }
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
    std::mutex m;
    std::condition_variable cv;
    int readers = 0;
    int waitingWriters = 0;
    bool writer = false;
};

class QReadLocker {
public:
    explicit QReadLocker(QReadWriteLock *l) : lk(l) { lk->lockForRead(); locked = true; }
    ~QReadLocker() { unlock(); }
    QReadLocker(const QReadLocker &) = delete;
    void unlock() { if (locked) { lk->unlock(); locked = false; } }
    void relock() { if (!locked) { lk->lockForRead(); locked = true; } }
    QReadWriteLock *readWriteLock() const { return lk; }

private:
    QReadWriteLock *lk;
    bool locked = false;
};

class QWriteLocker {
public:
    explicit QWriteLocker(QReadWriteLock *l) : lk(l) { lk->lockForWrite(); locked = true; }
    ~QWriteLocker() { unlock(); }
    QWriteLocker(const QWriteLocker &) = delete;
    void unlock() { if (locked) { lk->unlock(); locked = false; } }
    void relock() { if (!locked) { lk->lockForWrite(); locked = true; } }
    QReadWriteLock *readWriteLock() const { return lk; }

private:
    QReadWriteLock *lk;
    bool locked = false;
};

class QWaitCondition {
public:
    QWaitCondition() = default;
    QWaitCondition(const QWaitCondition &) = delete;

    // Returns false on timeout.
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
    bool wait(QMutex *mutex, QDeadlineTimer deadline) {
        const qint64 r = deadline.remainingTime();
        return wait(mutex, r < 0 ? ULONG_MAX : static_cast<unsigned long>(r));
    }
    void wakeOne() { cv.notify_one(); }
    void wakeAll() { cv.notify_all(); }
    void notify_one() { cv.notify_one(); }
    void notify_all() { cv.notify_all(); }

private:
    std::condition_variable_any cv;
};

class QSemaphore {
public:
    explicit QSemaphore(int n = 0) : avail(n) {}
    QSemaphore(const QSemaphore &) = delete;

    void acquire(int n = 1) {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&] { return avail >= n; });
        avail -= n;
    }
    bool tryAcquire(int n = 1, int timeoutMs = 0) {
        std::unique_lock<std::mutex> lk(m);
        if (!cv.wait_for(lk, std::chrono::milliseconds(timeoutMs < 0 ? 0 : timeoutMs), [&] { return avail >= n; }))
            return false;
        avail -= n;
        return true;
    }
    void release(int n = 1) {
        {
            std::lock_guard<std::mutex> lk(m);
            avail += n;
        }
        cv.notify_all();
    }
    int available() const {
        std::lock_guard<std::mutex> lk(m);
        return avail;
    }

private:
    mutable std::mutex m;
    std::condition_variable cv;
    int avail;
};

class QSemaphoreReleaser {
public:
    QSemaphoreReleaser() = default;
    explicit QSemaphoreReleaser(QSemaphore &s, int n = 1) : sem(&s), count(n) {}
    ~QSemaphoreReleaser() { if (sem) sem->release(count); }
    QSemaphoreReleaser(const QSemaphoreReleaser &) = delete;
    QSemaphore *cancel() { return std::exchange(sem, nullptr); }

private:
    QSemaphore *sem = nullptr;
    int count = 0;
};
