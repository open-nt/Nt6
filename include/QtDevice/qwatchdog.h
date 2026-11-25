// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <memory>

class QWatchdogPrivate;

class QWatchdog : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString deviceName READ deviceName WRITE setDeviceName)
    Q_PROPERTY(int timeout READ timeout WRITE setTimeout)
    Q_PROPERTY(bool autoFeed READ autoFeed WRITE setAutoFeed)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(WatchdogError error READ error RESET clearError NOTIFY errorOccurred)
public:
    enum WatchdogError {
        NoError,
        DeviceNotFoundError,
        PermissionError,
        BusyError,
        OpenError,
        UnsupportedOperationError,
        ResourceError,
        NotOpenError,
        TimeoutError,
        IoError,
        UnknownError
    };
    Q_ENUM(WatchdogError)

    enum Option { NoOption = 0x0, PauseInDebugger = 0x1, PauseWhileSleeping = 0x2 };
    Q_DECLARE_FLAGS(Options, Option)
    Q_FLAG(Options)

    explicit QWatchdog(QObject *parent = nullptr);
    explicit QWatchdog(const QString &name, QObject *parent = nullptr);
    ~QWatchdog() override;

    void setDeviceName(const QString &name);
    QString deviceName() const;
    void setTimeout(int msecs);
    int timeout() const;
    void setOptions(Options options);
    Options options() const;
    void setAutoFeed(bool enable);
    bool autoFeed() const;

    bool start();
    bool feed();
    bool stop();
    bool isActive() const;

    bool causedLastReset() const;

    WatchdogError error() const;
    QString errorString() const;
    void clearError();

Q_SIGNALS:
    void activeChanged(bool active);
    void errorOccurred(QWatchdog::WatchdogError error);

private:
    friend class QWatchdogPrivate;
    std::unique_ptr<QWatchdogPrivate> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QWatchdog::Options)
