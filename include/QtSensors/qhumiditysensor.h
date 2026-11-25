// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QHumidityReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(qreal relativeHumidity READ relativeHumidity)
    Q_PROPERTY(qreal absoluteHumidity READ absoluteHumidity)
public:
    explicit QHumidityReading(QObject *parent = nullptr);
    ~QHumidityReading() override;

    qreal relativeHumidity() const;
    void setRelativeHumidity(qreal relativeHumidity);
    qreal absoluteHumidity() const;
    void setAbsoluteHumidity(qreal absoluteHumidity);

private:
    qreal m_relativeHumidity = 0;
    qreal m_absoluteHumidity = 0;
};

class QHumidityFilter : public QSensorFilter {
public:
    virtual bool filter(QHumidityReading *reading) = 0;

private:
    bool filter(QSensorReading *reading) override;
};

class QHumiditySensor : public QSensor {
    Q_OBJECT
public:
    explicit QHumiditySensor(QObject *parent = nullptr);
    ~QHumiditySensor() override;

    QHumidityReading *reading() const;

    static const char *const sensorType;
};
