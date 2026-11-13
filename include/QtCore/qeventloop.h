// MIT License - see LICENSE
#pragma once

#include <utility>

#include <QtCore/qflags.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>

#include <memory>

class QEventLoop : public QObject {
    Q_OBJECT
public:
    /*!
        \enum QEventLoop::ProcessEventsFlag

        This enum controls the types of events processed by processEvents().

        \value AllEvents All pending events are processed.
        \value ExcludeUserInputEvents Accepted for compatibility; it has no effect because Nt has no user input events.
        \value ExcludeSocketNotifiers Accepted for compatibility; it has no effect, socket notifiers are always processed.
        \value WaitForMoreEvents Block until an event is available instead of returning immediately.
    */
    enum ProcessEventsFlag {
        AllEvents = 0x00,
        ExcludeUserInputEvents = 0x01,
        ExcludeSocketNotifiers = 0x02,
        WaitForMoreEvents = 0x04
    };
    Q_DECLARE_FLAGS(ProcessEventsFlags, ProcessEventsFlag)

    explicit QEventLoop(QObject *parent = nullptr);
    ~QEventLoop() override;

    bool processEvents(ProcessEventsFlags flags = AllEvents);
    void processEvents(ProcessEventsFlags flags, int maxTime);
    void processEvents(ProcessEventsFlags flags, QDeadlineTimer deadline);
    int exec(ProcessEventsFlags flags = AllEvents);
    void exit(int returnCode = 0);
    bool isRunning() const;
    void wakeUp();

public slots:
    void quit();

private:
    struct Data;
    std::unique_ptr<Data> m_d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QEventLoop::ProcessEventsFlags)

class QThread;

class QEventLoopLocker {
public:
    QEventLoopLocker();
    explicit QEventLoopLocker(QEventLoop *loop);
    explicit QEventLoopLocker(QThread *thread);
    ~QEventLoopLocker();
    /*!
        \since 6.7
        Exchanges what this locker keeps running with what \a other does.
    */
    void swap(QEventLoopLocker &other) noexcept {
        std::swap(m_kind, other.m_kind);
        m_target.swap(other.m_target);
        std::swap(m_key, other.m_key);
    }
    Q_DISABLE_COPY(QEventLoopLocker)

private:
    enum class Kind { Application, Loop, Thread };
    Kind m_kind;
    QPointer<QObject> m_target;
    const QObject *m_key;
};
