// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qobject.h>

#include <memory>

class QEventLoop : public QObject {
    Q_OBJECT
public:
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
