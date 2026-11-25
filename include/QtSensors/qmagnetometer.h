// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QMagnetometerReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(qreal x READ x)
    Q_PROPERTY(qreal y READ y)
    Q_PROPERTY(qreal z READ z)
    Q_PROPERTY(qreal calibrationLevel READ calibrationLevel)
public:
    explicit QMagnetometerReading(QObject *parent = nullptr);
    ~QMagnetometerReading() override;

    qreal x() const;
    void setX(qreal x);
    qreal y() const;
    void setY(qreal y);
    qreal z() const;
    void setZ(qreal z);
    qreal calibrationLevel() const;
    void setCalibrationLevel(qreal calibrationLevel);

private:
    qreal m_x = 0;
    qreal m_y = 0;
    qreal m_z = 0;
    qreal m_calibrationLevel = 1;
};

class QMagnetometerFilter : public QSensorFilter {
public:
    virtual bool filter(QMagnetometerReading *reading) = 0;

private:
    bool filter(QSensorReading *reading) override;
};

class QMagnetometer : public QSensor {
    Q_OBJECT
    Q_PROPERTY(bool returnGeoValues READ returnGeoValues WRITE setReturnGeoValues NOTIFY returnGeoValuesChanged)
public:
    explicit QMagnetometer(QObject *parent = nullptr);
    ~QMagnetometer() override;

    QMagnetometerReading *reading() const;
    bool returnGeoValues() const;
    void setReturnGeoValues(bool returnGeoValues);

    static const char *const sensorType;

signals:
    /*!
        This signal is emitted when returnGeoValues() changes to \a returnGeoValues.
    */
    void returnGeoValuesChanged(bool returnGeoValues) {
        emitSignal(&QMagnetometer::returnGeoValuesChanged, returnGeoValues);
    }

private:
    bool m_returnGeoValues = false;
};
