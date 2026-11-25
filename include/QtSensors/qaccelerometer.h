// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QAccelerometerReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(qreal x READ x)
    Q_PROPERTY(qreal y READ y)
    Q_PROPERTY(qreal z READ z)
public:
    explicit QAccelerometerReading(QObject *parent = nullptr);
    ~QAccelerometerReading() override;

    qreal x() const;
    void setX(qreal x);
    qreal y() const;
    void setY(qreal y);
    qreal z() const;
    void setZ(qreal z);

private:
    qreal m_x = 0;
    qreal m_y = 0;
    qreal m_z = 0;
};

class QAccelerometerFilter : public QSensorFilter {
public:
    virtual bool filter(QAccelerometerReading *reading) = 0;

private:
    bool filter(QSensorReading *reading) override;
};

class QAccelerometer : public QSensor {
    Q_OBJECT
    Q_PROPERTY(AccelerationMode accelerationMode READ accelerationMode WRITE setAccelerationMode NOTIFY
                   accelerationModeChanged)
public:
    enum AccelerationMode { Combined, Gravity, User };
    Q_ENUM(AccelerationMode)

    explicit QAccelerometer(QObject *parent = nullptr);
    ~QAccelerometer() override;

    QAccelerometerReading *reading() const;
    AccelerationMode accelerationMode() const;
    void setAccelerationMode(AccelerationMode accelerationMode);

    static const char *const sensorType;

signals:
    /*!
        This signal is emitted when the acceleration mode changes to \a accelerationMode.
    */
    void accelerationModeChanged(QAccelerometer::AccelerationMode accelerationMode) {
        emitSignal(&QAccelerometer::accelerationModeChanged, accelerationMode);
    }

private:
    AccelerationMode m_accelerationMode = Combined;
};
