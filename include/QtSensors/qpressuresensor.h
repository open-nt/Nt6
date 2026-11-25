// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QPressureReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(qreal pressure READ pressure)
    Q_PROPERTY(qreal temperature READ temperature)
public:
    explicit QPressureReading(QObject *parent = nullptr);
    ~QPressureReading() override;

    qreal pressure() const;
    void setPressure(qreal pressure);
    qreal temperature() const;
    void setTemperature(qreal temperature);

private:
    qreal m_pressure = 0;
    qreal m_temperature = 0;
};

class QPressureFilter : public QSensorFilter {
public:
    virtual bool filter(QPressureReading *reading) = 0;

protected:
    // Calls the typed filter(); a subclass can bring it in with a using-declaration to keep -Woverloaded-virtual quiet.
    bool filter(QSensorReading *reading) override;
};

class QPressureSensor : public QSensor {
    Q_OBJECT
public:
    explicit QPressureSensor(QObject *parent = nullptr);
    ~QPressureSensor() override;

    QPressureReading *reading() const;

    static const char *const sensorType;
};
