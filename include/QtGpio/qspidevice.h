// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <memory>

class QSpiDevicePrivate;

class QSpiDevice : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString deviceName READ deviceName WRITE setDeviceName)
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
    Q_PROPERTY(quint32 speed READ speed WRITE setSpeed)
    Q_PROPERTY(int bitsPerWord READ bitsPerWord WRITE setBitsPerWord)
    Q_PROPERTY(BitOrder bitOrder READ bitOrder WRITE setBitOrder)
    Q_PROPERTY(SpiError error READ error RESET clearError NOTIFY errorOccurred)
public:
    enum Mode { Mode0, Mode1, Mode2, Mode3 };
    Q_ENUM(Mode)
    enum BitOrder { MsbFirst, LsbFirst };
    Q_ENUM(BitOrder)
    enum SpiError {
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
    Q_ENUM(SpiError)

    explicit QSpiDevice(QObject *parent = nullptr);
    explicit QSpiDevice(const QString &name, QObject *parent = nullptr);
    ~QSpiDevice() override;

    void setDeviceName(const QString &name);
    QString deviceName() const;
    bool setMode(Mode mode);
    Mode mode() const;
    bool setSpeed(quint32 hertz);
    quint32 speed() const;
    bool setBitsPerWord(int bits);
    int bitsPerWord() const;
    bool setBitOrder(BitOrder order);
    BitOrder bitOrder() const;

    bool open();
    void close();
    bool isOpen() const;

    QByteArray transfer(const QByteArray &data);
    bool write(const QByteArray &data);
    QByteArray read(qsizetype size);
    QByteArray writeRead(const QByteArray &data, qsizetype size);

    SpiError error() const;
    QString errorString() const;
    void clearError();

signals:
    /*!
        This signal is emitted when the error \a spiError occurs. It is not emitted when the
        error is cleared.

        \sa error(), errorString()
    */
    void errorOccurred(QSpiDevice::SpiError spiError) { emitSignal(&QSpiDevice::errorOccurred, spiError); }

private:
    friend class QSpiDevicePrivate;
    std::unique_ptr<QSpiDevicePrivate> d;
};
