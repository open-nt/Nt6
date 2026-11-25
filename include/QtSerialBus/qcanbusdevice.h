// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qvariant.h>
#include <QtSerialBus/qcanbusdeviceinfo.h>
#include <QtSerialBus/qcanbusframe.h>

#include <functional>
#include <memory>

class QCanBusDevicePrivate;

class QCanBusDevice : public QObject {
    Q_OBJECT
public:
    enum CanBusError {
        NoError,
        ReadError,
        WriteError,
        ConnectionError,
        ConfigurationError,
        UnknownError,
        OperationError,
        TimeoutError
    };
    Q_ENUM(CanBusError)

    enum CanBusDeviceState { UnconnectedState, ConnectingState, ConnectedState, ClosingState };
    Q_ENUM(CanBusDeviceState)

    enum class CanBusStatus { Unknown, Good, Warning, Error, BusOff };
    Q_ENUM(CanBusStatus)

    enum ConfigurationKey {
        RawFilterKey = 0,
        ErrorFilterKey,
        LoopbackKey,
        ReceiveOwnKey,
        BitRateKey,
        CanFdKey,
        DataBitRateKey,
        ProtocolKey,
        UserKey = 30
    };
    Q_ENUM(ConfigurationKey)

    struct Filter {
        enum FormatFilter {
            MatchBaseFormat = 0x0001,
            MatchExtendedFormat = 0x0002,
            MatchBaseAndExtendedFormat = 0x0003
        };
        Q_DECLARE_FLAGS(FormatFilters, FormatFilter)

        friend constexpr bool operator==(const Filter &a, const Filter &b) noexcept {
            return a.frameId == b.frameId && a.frameIdMask == b.frameIdMask && a.type == b.type && a.format == b.format;
        }
        friend constexpr bool operator!=(const Filter &a, const Filter &b) noexcept { return !(a == b); }

        QCanBusFrame::FrameId frameId = 0;
        QCanBusFrame::FrameId frameIdMask = 0;
        QCanBusFrame::FrameType type = QCanBusFrame::InvalidFrame;
        FormatFilter format = MatchBaseAndExtendedFormat;
    };

    enum Direction { Input = 1, Output = 2, AllDirections = Input | Output };
    Q_DECLARE_FLAGS(Directions, Direction)

    explicit QCanBusDevice(QObject *parent = nullptr);
    ~QCanBusDevice() override;

    virtual void setConfigurationParameter(ConfigurationKey key, const QVariant &value);
    QVariant configurationParameter(ConfigurationKey key) const;
    QList<ConfigurationKey> configurationKeys() const;

    virtual bool writeFrame(const QCanBusFrame &frame) = 0;
    QCanBusFrame readFrame();
    QList<QCanBusFrame> readAllFrames();
    qint64 framesAvailable() const;
    qint64 framesToWrite() const;

    void resetController();
    bool hasBusStatus() const;
    CanBusStatus busStatus();

    void clear(QCanBusDevice::Directions direction = Direction::AllDirections);

    virtual bool waitForFramesWritten(int msecs);
    virtual bool waitForFramesReceived(int msecs);

    bool connectDevice();
    void disconnectDevice();

    CanBusDeviceState state() const;
    CanBusError error() const;
    QString errorString() const;

    virtual QString interpretErrorFrame(const QCanBusFrame &errorFrame) = 0;
    virtual QCanBusDeviceInfo deviceInfo() const;

signals:
    /*!
        This signal is emitted when the error \a error occurs.

        \sa error(), errorString()
    */
    void errorOccurred(QCanBusDevice::CanBusError error) { emitSignal(&QCanBusDevice::errorOccurred, error); }
    /*!
        This signal is emitted when frames have been received; read them with readFrame() or
        readAllFrames().
    */
    void framesReceived() { emitSignal(&QCanBusDevice::framesReceived); }
    /*!
        This signal is emitted when \a framesCount frames have been written to the bus.
    */
    void framesWritten(qint64 framesCount) { emitSignal(&QCanBusDevice::framesWritten, framesCount); }
    /*!
        This signal is emitted when the state of the device changes to \a state.
    */
    void stateChanged(QCanBusDevice::CanBusDeviceState state) { emitSignal(&QCanBusDevice::stateChanged, state); }

protected:
    void setState(QCanBusDevice::CanBusDeviceState newState);
    void setError(const QString &errorText, QCanBusDevice::CanBusError errorId);
    void clearError();

    void enqueueReceivedFrames(const QList<QCanBusFrame> &newFrames);
    void enqueueOutgoingFrame(const QCanBusFrame &newFrame);
    QCanBusFrame dequeueOutgoingFrame();
    bool hasOutgoingFrames() const;

    void setResetControllerFunction(std::function<void()> resetter);
    void setCanBusStatusGetter(std::function<CanBusStatus()> busStatusGetter);

    virtual bool open() = 0;
    virtual void close() = 0;

    static QCanBusDeviceInfo createDeviceInfo(const QString &plugin, const QString &name, bool isVirtual,
                                              bool isFlexibleDataRateCapable);
    static QCanBusDeviceInfo createDeviceInfo(const QString &plugin, const QString &name, const QString &serialNumber,
                                              const QString &description, const QString &alias, int channel,
                                              bool isVirtual, bool isFlexibleDataRateCapable);

private:
    std::unique_ptr<QCanBusDevicePrivate> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QCanBusDevice::Directions)
Q_DECLARE_OPERATORS_FOR_FLAGS(QCanBusDevice::Filter::FormatFilters)
Q_DECLARE_METATYPE(QCanBusDevice::Filter)
Q_DECLARE_METATYPE(QList<QCanBusDevice::Filter>)
