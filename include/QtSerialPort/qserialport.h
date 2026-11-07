// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qstring.h>

#include <memory>

class QSerialPortInfo;
class QSerialPortPrivate;

class QSerialPort : public QIODevice {
    Q_OBJECT
public:
    enum Direction { Input = 1, Output = 2, AllDirections = Input | Output };
    Q_DECLARE_FLAGS(Directions, Direction)

    enum BaudRate {
        Baud1200 = 1200,
        Baud2400 = 2400,
        Baud4800 = 4800,
        Baud9600 = 9600,
        Baud19200 = 19200,
        Baud38400 = 38400,
        Baud57600 = 57600,
        Baud115200 = 115200
    };
    enum DataBits { Data5 = 5, Data6 = 6, Data7 = 7, Data8 = 8 };
    enum Parity { NoParity = 0, EvenParity = 2, OddParity = 3, SpaceParity = 4, MarkParity = 5 };
    enum StopBits { OneStop = 1, OneAndHalfStop = 3, TwoStop = 2 };
    enum FlowControl { NoFlowControl, HardwareControl, SoftwareControl };
    enum PinoutSignal {
        NoSignal = 0x00,
        DataTerminalReadySignal = 0x04,
        DataCarrierDetectSignal = 0x08,
        DataSetReadySignal = 0x10,
        RingIndicatorSignal = 0x20,
        RequestToSendSignal = 0x40,
        ClearToSendSignal = 0x80,
        SecondaryTransmittedDataSignal = 0x100,
        SecondaryReceivedDataSignal = 0x200
    };
    Q_DECLARE_FLAGS(PinoutSignals, PinoutSignal)

    enum SerialPortError {
        NoError,
        DeviceNotFoundError,
        PermissionError,
        OpenError,
        WriteError,
        ReadError,
        ResourceError,
        UnsupportedOperationError,
        UnknownError,
        TimeoutError,
        NotOpenError
    };

    using Handle = int;

    explicit QSerialPort(QObject *parent = nullptr);
    explicit QSerialPort(const QString &name, QObject *parent = nullptr);
    explicit QSerialPort(const QSerialPortInfo &info, QObject *parent = nullptr);
    ~QSerialPort() override;

    void setPortName(const QString &name);
    QString portName() const;
    void setPort(const QSerialPortInfo &info);

    bool open(OpenMode mode) override;
    void close() override;

    bool setBaudRate(qint32 baudRate, Directions directions = AllDirections);
    qint32 baudRate(Directions directions = AllDirections) const;
    bool setDataBits(DataBits dataBits);
    DataBits dataBits() const;
    bool setParity(Parity parity);
    Parity parity() const;
    bool setStopBits(StopBits stopBits);
    StopBits stopBits() const;
    bool setFlowControl(FlowControl flowControl);
    FlowControl flowControl() const;

    bool setDataTerminalReady(bool set);
    bool isDataTerminalReady();
    bool setRequestToSend(bool set);
    bool isRequestToSend();
    PinoutSignals pinoutSignals();
    bool setBreakEnabled(bool set = true);
    bool isBreakEnabled() const;

    bool flush();
    bool clear(Directions directions = AllDirections);
    SerialPortError error() const;
    void clearError();
    qint64 readBufferSize() const;
    void setReadBufferSize(qint64 size);
    Handle handle() const;

    bool isSequential() const override;
    qint64 bytesAvailable() const override;
    qint64 bytesToWrite() const override;
    bool canReadLine() const override;
    bool waitForReadyRead(int msecs = 30000) override;
    bool waitForBytesWritten(int msecs = 30000) override;

signals:
    /*!
        This signal is emitted when the baud rate changes to \a baudRate for
        the given \a directions.

        \sa setBaudRate()
    */
    void baudRateChanged(qint32 baudRate, QSerialPort::Directions directions) {
        emitSignal(&QSerialPort::baudRateChanged, baudRate, directions);
    }
    /*!
        This signal is emitted when the number of data bits changes to
        \a dataBits.

        \sa setDataBits()
    */
    void dataBitsChanged(QSerialPort::DataBits dataBits) { emitSignal(&QSerialPort::dataBitsChanged, dataBits); }
    /*!
        This signal is emitted when the parity changes to \a parity.

        \sa setParity()
    */
    void parityChanged(QSerialPort::Parity parity) { emitSignal(&QSerialPort::parityChanged, parity); }
    /*!
        This signal is emitted when the number of stop bits changes to
        \a stopBits.

        \sa setStopBits()
    */
    void stopBitsChanged(QSerialPort::StopBits stopBits) { emitSignal(&QSerialPort::stopBitsChanged, stopBits); }
    /*!
        This signal is emitted when the flow control changes to \a flowControl.

        \sa setFlowControl()
    */
    void flowControlChanged(QSerialPort::FlowControl flowControl) {
        emitSignal(&QSerialPort::flowControlChanged, flowControl);
    }
    /*!
        This signal is emitted when setDataTerminalReady() changes the DTR line
        to \a set.
    */
    void dataTerminalReadyChanged(bool set) { emitSignal(&QSerialPort::dataTerminalReadyChanged, set); }
    /*!
        This signal is emitted when setRequestToSend() changes the RTS line to
        \a set.
    */
    void requestToSendChanged(bool set) { emitSignal(&QSerialPort::requestToSendChanged, set); }
    /*!
        This signal is emitted when the break state changes to \a set.

        \sa setBreakEnabled()
    */
    void breakEnabledChanged(bool set) { emitSignal(&QSerialPort::breakEnabledChanged, set); }
    /*!
        This signal is emitted when the error \a serialPortError occurs. It is
        not emitted when the error is cleared.

        \sa error(), errorString()
    */
    void errorOccurred(QSerialPort::SerialPortError serialPortError) {
        emitSignal(&QSerialPort::errorOccurred, serialPortError);
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    friend class QSerialPortPrivate;
    std::unique_ptr<QSerialPortPrivate> d;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QSerialPort::Directions)
Q_DECLARE_OPERATORS_FOR_FLAGS(QSerialPort::PinoutSignals)
