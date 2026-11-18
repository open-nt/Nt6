// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <memory>

class QI2cDevicePrivate;

class QI2cDevice : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString busName READ busName WRITE setBusName)
    Q_PROPERTY(int address READ address WRITE setAddress)
    Q_PROPERTY(I2cError error READ error RESET clearError NOTIFY errorOccurred)
public:
    enum I2cError {
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
    Q_ENUM(I2cError)

    explicit QI2cDevice(QObject *parent = nullptr);
    QI2cDevice(const QString &bus, int address, QObject *parent = nullptr);
    ~QI2cDevice() override;

    void setBusName(const QString &bus);
    QString busName() const;
    void setAddress(int address);
    int address() const;

    bool open();
    void close();
    bool isOpen() const;

    bool write(const QByteArray &data);
    QByteArray read(qsizetype size);
    QByteArray writeRead(const QByteArray &data, qsizetype size);
    bool writeRegister(quint8 reg, const QByteArray &data);
    QByteArray readRegister(quint8 reg, qsizetype size);

    I2cError error() const;
    QString errorString() const;
    void clearError();

signals:
    /*!
        This signal is emitted when the error \a i2cError occurs. It is not emitted when the
        error is cleared.

        \sa error(), errorString()
    */
    void errorOccurred(QI2cDevice::I2cError i2cError) { emitSignal(&QI2cDevice::errorOccurred, i2cError); }

private:
    friend class QI2cDevicePrivate;
    std::unique_ptr<QI2cDevicePrivate> d;
};
