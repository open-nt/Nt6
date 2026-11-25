// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qversionnumber.h>

#include <memory>

class QIODevice;
class QFirmwareUpdatePrivate;

class QFirmwareUpdate : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString target READ target WRITE setTarget)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(qint64 bytesWritten READ bytesWritten NOTIFY progress)
    Q_PROPERTY(UpdateError error READ error RESET clearError NOTIFY errorOccurred)
public:
    enum UpdateError {
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
        UnknownError,
        InvalidImageError
    };
    Q_ENUM(UpdateError)

    enum State { Idle, Writing, Written, Pending, Failed };
    Q_ENUM(State)

    explicit QFirmwareUpdate(QObject *parent = nullptr);
    ~QFirmwareUpdate() override;

    void setTarget(const QString &target);
    QString target() const;

    bool begin(qint64 size = -1);
    bool write(const QByteArray &data);
    bool write(const char *data, qint64 size);
    bool writeFrom(QIODevice *device);
    bool finish();
    void abort();
    bool requestUpgrade(bool permanent = false);

    State state() const;
    qint64 bytesWritten() const;
    qint64 expectedSize() const;
    QVersionNumber imageVersion() const;

    static QVersionNumber runningVersion();
    static bool isRunningImageConfirmed();
    static bool confirmRunningImage();
    static bool reboot();

    UpdateError error() const;
    QString errorString() const;
    void clearError();

Q_SIGNALS:
    void stateChanged(QFirmwareUpdate::State state);
    void progress(qint64 bytesWritten, qint64 expectedSize);
    void errorOccurred(QFirmwareUpdate::UpdateError error);

private:
    friend class QFirmwareUpdatePrivate;
    std::unique_ptr<QFirmwareUpdatePrivate> d;
};
