// MIT License - see LICENSE
#pragma once

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>

#include <chrono>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>

class QAbstractEventDispatcher;

namespace QtPrivate {
struct ThreadAccess;
struct ThreadData;
} // namespace QtPrivate

class QThread : public QObject {
    Q_OBJECT
public:
    /*!
        \enum QThread::Priority

        This enum describes how the operating system should schedule a thread.

        \value IdlePriority Runs only when no other thread is runnable (\c SCHED_IDLE on Linux).
        \value LowestPriority Stored and returned by priority(); the scheduler of a normal Linux process ignores it.
        \value LowPriority Stored and returned by priority(); the scheduler of a normal Linux process ignores it.
        \value NormalPriority The default scheduling of the system.
        \value HighPriority Stored and returned by priority(); the scheduler of a normal Linux process ignores it.
        \value HighestPriority Stored and returned by priority(); the scheduler of a normal Linux process ignores it.
        \value TimeCriticalPriority Stored and returned by priority(); the scheduler of a normal Linux process ignores it.
        \value InheritPriority The thread gets the priority of the thread that called start().
    */
    enum Priority {
        IdlePriority,
        LowestPriority,
        LowPriority,
        NormalPriority,
        HighPriority,
        HighestPriority,
        TimeCriticalPriority,
        InheritPriority
    };

    explicit QThread(QObject *parent = nullptr);
    ~QThread() override;

    static Qt::HANDLE currentThreadId() noexcept;
    static QThread *currentThread();
    static int idealThreadCount() noexcept;
    static void yieldCurrentThread();
    static bool isMainThread();
    bool isCurrentThread() const noexcept;

    void setPriority(Priority priority);
    Priority priority() const;

    bool isFinished() const;
    bool isRunning() const;

    void requestInterruption();
    bool isInterruptionRequested() const;

    void setStackSize(uint stackSize);
    uint stackSize() const;

    int loopLevel() const;

    QAbstractEventDispatcher *eventDispatcher() const;

    /*!
        Creates a thread that calls \a f with \a args, copied into the thread, instead of running
        an event loop, and returns it. The thread is not started. The caller owns it and can
        connect finished() to deleteLater().

        \sa start()
    */
    template <typename Function, typename... Args> static QThread *create(Function &&f, Args &&...args) {
        return createImpl([fn = std::decay_t<Function>(std::forward<Function>(f)),
                           tup = std::make_tuple(std::decay_t<Args>(std::forward<Args>(args))...)]() mutable {
            std::apply(std::move(fn), std::move(tup));
        });
    }

    bool wait(QDeadlineTimer deadline = QDeadlineTimer(QDeadlineTimer::Forever));
    bool wait(unsigned long time);

    static void sleep(unsigned long secs);
    static void msleep(unsigned long msecs);
    static void usleep(unsigned long usecs);
    static void sleep(std::chrono::nanoseconds nsecs);

public slots:
    void start(Priority priority = InheritPriority);
    void exit(int retcode = 0);
    void quit();

signals:
    /*!
        This signal is emitted from the new thread when it starts executing,
        before run() is called. Slots in objects that live in other threads
        are called through their event loop.

        \sa finished(), start()
    */
    void started() { emitSignal(&QThread::started); }
    /*!
        This signal is emitted from the thread when it is about to finish,
        after run() has returned. Objects that live in the thread and were
        scheduled with deleteLater() are deleted right after it. Connect it to
        deleteLater() of the QThread to delete the thread object when it ends.

        \sa started(), isFinished()
    */
    void finished() { emitSignal(&QThread::finished); }

protected:
    virtual void run();
    int exec();

private:
    friend struct QtPrivate::ThreadAccess;
    struct Data;
    QThread(QtPrivate::ThreadData *adopted, int);
    static QThread *createImpl(std::function<void()> body);

    std::shared_ptr<Data> m_d;
};
