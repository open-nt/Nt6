// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtSensors/qsensor.h>

class QSensorBackend : public QObject {
    Q_OBJECT
public:
    explicit QSensorBackend(QSensor *sensor, QObject *parent = nullptr);
    ~QSensorBackend() override;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isFeatureSupported(QSensor::Feature feature) const;

    void addDataRate(qreal min, qreal max);
    void setDataRates(const QSensor *otherSensor);
    void addOutputRange(qreal min, qreal max, qreal accuracy);
    void setDescription(const QString &description);

    QSensor *sensor() const;
    QSensorReading *reading() const;

    template <typename T> T *setReading(T *reading) {
        if (!reading)
            reading = new T(this);
        setDeviceReading(reading);
        return reading;
    }

    void newReadingAvailable();
    void sensorStopped();
    void sensorBusy(bool busy = true);
    void sensorError(int error);

private:
    void setDeviceReading(QSensorReading *reading);

    QSensor *m_sensor;
    QSensorReading *m_reading = nullptr;
};
