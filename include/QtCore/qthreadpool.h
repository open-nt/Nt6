// MIT License - see LICENSE
#pragma once

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qobject.h>
#include <QtCore/qrunnable.h>
#include <QtCore/qthread.h>

#include <functional>
#include <memory>
#include <utility>

class QThreadPool : public QObject {
    Q_OBJECT
public:
    explicit QThreadPool(QObject *parent = nullptr);
    ~QThreadPool() override;

    static QThreadPool *globalInstance();

    void start(QRunnable *runnable, int priority = 0);
    void start(std::function<void()> functionToRun, int priority = 0);
    /*!
        \overload
        \since 6.0
        Runs \a functionToRun, which may be a move-only callable, in a pool
        thread, or queues it, with the given \a priority.
    */
    template <typename Callable, QtPrivate::IfRunnableCallable<Callable> = true>
    void start(Callable &&functionToRun, int priority = 0) {
        start(QRunnable::create(std::forward<Callable>(functionToRun)), priority);
    }
    bool tryStart(QRunnable *runnable);
    bool tryStart(std::function<void()> functionToRun);
    /*!
        \overload
        \since 6.0
        Runs \a functionToRun, which may be a move-only callable, in a pool
        thread if one is available and returns \c true; returns \c false
        otherwise, and the callable is destroyed.
    */
    template <typename Callable, QtPrivate::IfRunnableCallable<Callable> = true>
    bool tryStart(Callable &&functionToRun) {
        QRunnable *r = QRunnable::create(std::forward<Callable>(functionToRun));
        if (tryStart(r))
            return true;
        delete r;
        return false;
    }
    void startOnReservedThread(QRunnable *runnable);
    void startOnReservedThread(std::function<void()> functionToRun);
    /*!
        \overload
        \since 6.3
        Runs \a functionToRun, which may be a move-only callable, on a
        reserved thread.
    */
    template <typename Callable, QtPrivate::IfRunnableCallable<Callable> = true>
    void startOnReservedThread(Callable &&functionToRun) {
        startOnReservedThread(QRunnable::create(std::forward<Callable>(functionToRun)));
    }

    int expiryTimeout() const;
    void setExpiryTimeout(int expiryTimeout);

    int maxThreadCount() const;
    void setMaxThreadCount(int maxThreadCount);

    int activeThreadCount() const;

    void setStackSize(uint stackSize);
    uint stackSize() const;
    void setServiceLevel(QThread::QualityOfService serviceLevel);
    QThread::QualityOfService serviceLevel() const;

    void setThreadPriority(QThread::Priority priority);
    QThread::Priority threadPriority() const;

    void reserveThread();
    void releaseThread();

    bool waitForDone(int msecs = -1);
    bool waitForDone(QDeadlineTimer deadline);

    bool tryTake(QRunnable *runnable);
    void clear();

    bool contains(const QThread *thread) const;

private:
    struct Data;
    std::unique_ptr<Data> m_d;
};
