// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <memory>

class QAnalogOutputPrivate;

class QAnalogOutput : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString channelName READ channelName WRITE setChannelName)
    Q_PROPERTY(int resolution READ resolution WRITE setResolution)
    Q_PROPERTY(double referenceVoltage READ referenceVoltage WRITE setReferenceVoltage)
    Q_PROPERTY(int rawValue READ rawValue WRITE setRawValue)
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

    explicit QAnalogOutput(QObject *parent = nullptr);
    explicit QAnalogOutput(const QString &name, QObject *parent = nullptr);
    ~QAnalogOutput() override;

    void setChannelName(const QString &name);
    QString channelName() const;
    void setResolution(int bits);
    int resolution() const;
    void setReferenceVoltage(double volts);
    double referenceVoltage() const;

    bool open();
    void close();
    bool isOpen() const;

    bool setRawValue(int value);
    int rawValue() const;
    bool setVoltage(double volts);

    AnalogError error() const;
    QString errorString() const;
    void clearError();

signals:
    /*!
        This signal is emitted when the error \a analogError occurs. It is not emitted when the
        error is cleared.

        \sa error(), errorString()
    */
    void errorOccurred(QAnalogOutput::AnalogError analogError) {
        emitSignal(&QAnalogOutput::errorOccurred, analogError);
    }

private:
    friend class QAnalogOutputPrivate;
    std::unique_ptr<QAnalogOutputPrivate> d;
};
