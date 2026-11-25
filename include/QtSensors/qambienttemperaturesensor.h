// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QAmbientTemperatureReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(qreal temperature READ temperature)
public:
    explicit QAmbientTemperatureReading(QObject *parent = nullptr);
    ~QAmbientTemperatureReading() override;

    qreal temperature() const;
    void setTemperature(qreal temperature);

private:
    qreal m_temperature = 0;
};

class QAmbientTemperatureFilter : public QSensorFilter {
public:
    virtual bool filter(QAmbientTemperatureReading *reading) = 0;

private:
    bool filter(QSensorReading *reading) override;
};

class QAmbientTemperatureSensor : public QSensor {
    Q_OBJECT
public:
    explicit QAmbientTemperatureSensor(QObject *parent = nullptr);
    ~QAmbientTemperatureSensor() override;

    QAmbientTemperatureReading *reading() const;

    static const char *const sensorType;
};
