// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <memory>

class QPwmChannelPrivate;

class QPwmChannel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString channelName READ channelName WRITE setChannelName)
    Q_PROPERTY(qint64 period READ period WRITE setPeriod)
    Q_PROPERTY(qint64 pulseWidth READ pulseWidth WRITE setPulseWidth)
    Q_PROPERTY(double dutyCycle READ dutyCycle WRITE setDutyCycle)
    Q_PROPERTY(Polarity polarity READ polarity WRITE setPolarity)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled)
    Q_PROPERTY(PwmError error READ error RESET clearError NOTIFY errorOccurred)
public:
    enum Polarity { Normal, Inversed };
    Q_ENUM(Polarity)
    enum PwmError {
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
    Q_ENUM(PwmError)

    explicit QPwmChannel(QObject *parent = nullptr);
    explicit QPwmChannel(const QString &name, QObject *parent = nullptr);
    ~QPwmChannel() override;

    void setChannelName(const QString &name);
    QString channelName() const;

    bool setPeriod(qint64 nsecs);
    qint64 period() const;
    bool setFrequency(double hertz);
    double frequency() const;
    bool setPulseWidth(qint64 nsecs);
    qint64 pulseWidth() const;
    bool setDutyCycle(double fraction);
    double dutyCycle() const;
    bool setPolarity(Polarity polarity);
    Polarity polarity() const;
    bool setEnabled(bool enabled);
    bool isEnabled() const;

    bool open();
    void close();
    bool isOpen() const;

    PwmError error() const;
    QString errorString() const;
    void clearError();

signals:
    /*!
        This signal is emitted when the error \a pwmError occurs. It is not emitted when the
        error is cleared.

        \sa error(), errorString()
    */
    void errorOccurred(QPwmChannel::PwmError pwmError) { emitSignal(&QPwmChannel::errorOccurred, pwmError); }

private:
    friend class QPwmChannelPrivate;
    std::unique_ptr<QPwmChannelPrivate> d;
};
