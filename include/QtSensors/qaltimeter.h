// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QAltimeterReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(qreal altitude READ altitude)
public:
    explicit QAltimeterReading(QObject *parent = nullptr);
    ~QAltimeterReading() override;

    qreal altitude() const;
    void setAltitude(qreal altitude);

private:
    qreal m_altitude = 0;
};

class QAltimeterFilter : public QSensorFilter {
public:
    virtual bool filter(QAltimeterReading *reading) = 0;

protected:
    // Calls the typed filter(); a subclass can bring it in with a using-declaration to keep -Woverloaded-virtual quiet.
    bool filter(QSensorReading *reading) override;
};

class QAltimeter : public QSensor {
    Q_OBJECT
public:
    explicit QAltimeter(QObject *parent = nullptr);
    ~QAltimeter() override;

    QAltimeterReading *reading() const;

    static const char *const sensorType;
};
