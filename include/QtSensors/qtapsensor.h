// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QTapReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(TapDirection tapDirection READ tapDirection)
    Q_PROPERTY(bool doubleTap READ isDoubleTap)
public:
    enum TapDirection {
        Undefined = 0,
        X = 0x0001,
        Y = 0x0002,
        Z = 0x0004,
        X_Pos = 0x0011,
        Y_Pos = 0x0022,
        Z_Pos = 0x0044,
        X_Neg = 0x0101,
        Y_Neg = 0x0202,
        Z_Neg = 0x0404,
        X_Both = 0x0111,
        Y_Both = 0x0222,
        Z_Both = 0x0444
    };
    Q_ENUM(TapDirection)

    explicit QTapReading(QObject *parent = nullptr);
    ~QTapReading() override;

    TapDirection tapDirection() const;
    void setTapDirection(TapDirection tapDirection);
    bool isDoubleTap() const;
    void setDoubleTap(bool doubleTap);

private:
    TapDirection m_tapDirection = Undefined;
    bool m_doubleTap = false;
};

class QTapFilter : public QSensorFilter {
public:
    virtual bool filter(QTapReading *reading) = 0;

protected:
    // Calls the typed filter(); a subclass can bring it in with a using-declaration to keep -Woverloaded-virtual quiet.
    bool filter(QSensorReading *reading) override;
};

class QTapSensor : public QSensor {
    Q_OBJECT
    Q_PROPERTY(bool returnDoubleTapEvents READ returnDoubleTapEvents WRITE setReturnDoubleTapEvents NOTIFY
                   returnDoubleTapEventsChanged)
public:
    explicit QTapSensor(QObject *parent = nullptr);
    ~QTapSensor() override;

    QTapReading *reading() const;
    bool returnDoubleTapEvents() const;
    void setReturnDoubleTapEvents(bool returnDoubleTapEvents);

    static const char *const sensorType;

signals:
    /*!
        This signal is emitted when returnDoubleTapEvents() changes to \a returnDoubleTapEvents.
    */
    void returnDoubleTapEventsChanged(bool returnDoubleTapEvents) {
        emitSignal(&QTapSensor::returnDoubleTapEventsChanged, returnDoubleTapEvents);
    }

private:
    bool m_returnDoubleTapEvents = true;
};
