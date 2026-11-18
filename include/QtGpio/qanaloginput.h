// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <memory>

class QAnalogInputPrivate;

class QAnalogInput : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString channelName READ channelName WRITE setChannelName)
    Q_PROPERTY(int resolution READ resolution WRITE setResolution)
    Q_PROPERTY(double referenceVoltage READ referenceVoltage WRITE setReferenceVoltage)
    Q_PROPERTY(int sampleInterval READ sampleInterval WRITE setSampleInterval)
    Q_PROPERTY(int rawValue READ rawValue NOTIFY valueChanged)
    Q_PROPERTY(double voltage READ voltage NOTIFY valueChanged)
    Q_PROPERTY(AnalogError error READ error RESET clearError NOTIFY errorOccurred)
public:
    enum AnalogError {
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
    Q_ENUM(AnalogError)

    explicit QAnalogInput(QObject *parent = nullptr);
    explicit QAnalogInput(const QString &name, QObject *parent = nullptr);
    ~QAnalogInput() override;

    void setChannelName(const QString &name);
    QString channelName() const;
    void setResolution(int bits);
    int resolution() const;
    void setReferenceVoltage(double volts);
    double referenceVoltage() const;
    void setSampleInterval(int msecs);
    int sampleInterval() const;

    bool open();
    void close();
    bool isOpen() const;

    bool read();
    int rawValue() const;
    double voltage() const;
    bool hasVoltage() const;

    AnalogError error() const;
    QString errorString() const;
    void clearError();

signals:
    /*!
        This signal is emitted when a sample differs from the one before, with the new
        \a rawValue. voltage() holds the same sample in volts.

        \sa read(), setSampleInterval()
    */
    void valueChanged(int rawValue) { emitSignal(&QAnalogInput::valueChanged, rawValue); }
    /*!
        This signal is emitted when the error \a analogError occurs. It is not emitted when the
        error is cleared.

        \sa error(), errorString()
    */
    void errorOccurred(QAnalogInput::AnalogError analogError) { emitSignal(&QAnalogInput::errorOccurred, analogError); }

private:
    friend class QAnalogInputPrivate;
    std::unique_ptr<QAnalogInputPrivate> d;
};
