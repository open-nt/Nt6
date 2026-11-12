// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <memory>

class QGpioLinePrivate;

class QGpioLine : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lineName READ lineName WRITE setLineName)
    Q_PROPERTY(Direction direction READ direction WRITE setDirection)
    Q_PROPERTY(bool activeLow READ isActiveLow WRITE setActiveLow)
    Q_PROPERTY(Bias bias READ bias WRITE setBias)
    Q_PROPERTY(int debounceInterval READ debounceInterval WRITE setDebounceInterval)
    Q_PROPERTY(bool value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(GpioError error READ error RESET clearError NOTIFY errorOccurred)
public:
    enum Direction { Input, Output };
    Q_ENUM(Direction)
    enum Bias { AsIs, Disabled, PullUp, PullDown };
    Q_ENUM(Bias)
    enum GpioError {
        NoError,
        DeviceNotFoundError,
        PermissionError,
        BusyError,
        OpenError,
        UnsupportedOperationError,
        ResourceError,
        NotOpenError,
        UnknownError
    };
    Q_ENUM(GpioError)

    explicit QGpioLine(QObject *parent = nullptr);
    explicit QGpioLine(const QString &name, QObject *parent = nullptr);
    ~QGpioLine() override;

    void setLineName(const QString &name);
    QString lineName() const;
    void setDirection(Direction direction);
    Direction direction() const;
    void setActiveLow(bool activeLow);
    bool isActiveLow() const;
    void setBias(Bias bias);
    Bias bias() const;
    void setDebounceInterval(int msecs);
    int debounceInterval() const;
    void setPollInterval(int msecs);
    int pollInterval() const;

    bool open();
    void close();
    bool isOpen() const;
    bool isEventDriven() const;

    bool value() const;
    bool setValue(bool value);

    GpioError error() const;
    QString errorString() const;
    void clearError();

signals:
    /*!
        This signal is emitted when the logical value of an input line changes to \a value, after
        any debounce interval has passed. It is also emitted when setValue() changes the value of
        an output line.

        \sa value(), setDebounceInterval()
    */
    void valueChanged(bool value) { emitSignal(&QGpioLine::valueChanged, value); }
    /*!
        This signal is emitted when the error \a gpioError occurs. It is not emitted when the
        error is cleared.

        \sa error(), errorString()
    */
    void errorOccurred(QGpioLine::GpioError gpioError) { emitSignal(&QGpioLine::errorOccurred, gpioError); }

private:
    friend class QGpioLinePrivate;
    std::unique_ptr<QGpioLinePrivate> d;
};
