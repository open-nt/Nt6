// MIT License - see LICENSE
#pragma once

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qobject.h>
#include <QtCore/qrunnable.h>
#include <QtCore/qthread.h>

#include <functional>
#include <memory>

class QThreadPool : public QObject {
    Q_OBJECT
public:
    explicit QThreadPool(QObject *parent = nullptr);
    ~QThreadPool() override;

    static QThreadPool *globalInstance();

    void start(QRunnable *runnable, int priority = 0);
    void start(std::function<void()> functionToRun, int priority = 0);
    bool tryStart(QRunnable *runnable);
    bool tryStart(std::function<void()> functionToRun);
    void startOnReservedThread(QRunnable *runnable);
    void startOnReservedThread(std::function<void()> functionToRun);

    int expiryTimeout() const;
    void setExpiryTimeout(int expiryTimeout);

    int maxThreadCount() const;
    void setMaxThreadCount(int maxThreadCount);

    int activeThreadCount() const;

    void setStackSize(uint stackSize);
    uint stackSize() const;

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
