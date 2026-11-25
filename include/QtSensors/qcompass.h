// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QCompassReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(qreal azimuth READ azimuth)
    Q_PROPERTY(qreal calibrationLevel READ calibrationLevel)
public:
    explicit QCompassReading(QObject *parent = nullptr);
    ~QCompassReading() override;

    qreal azimuth() const;
    void setAzimuth(qreal azimuth);
    qreal calibrationLevel() const;
    void setCalibrationLevel(qreal calibrationLevel);

private:
    qreal m_azimuth = 0;
    qreal m_calibrationLevel = 1;
};

class QCompassFilter : public QSensorFilter {
public:
    virtual bool filter(QCompassReading *reading) = 0;

protected:
    // Calls the typed filter(); a subclass can bring it in with a using-declaration to keep -Woverloaded-virtual quiet.
    bool filter(QSensorReading *reading) override;
};

class QCompass : public QSensor {
    Q_OBJECT
public:
    explicit QCompass(QObject *parent = nullptr);
    ~QCompass() override;

    QCompassReading *reading() const;

    static const char *const sensorType;
};
