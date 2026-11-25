// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QProximityReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(bool close READ close)
public:
    explicit QProximityReading(QObject *parent = nullptr);
    ~QProximityReading() override;

    bool close() const;
    void setClose(bool close);

private:
    bool m_close = false;
};

class QProximityFilter : public QSensorFilter {
public:
    virtual bool filter(QProximityReading *reading) = 0;

protected:
    // Calls the typed filter(); a subclass can bring it in with a using-declaration to keep -Woverloaded-virtual quiet.
    bool filter(QSensorReading *reading) override;
};

class QProximitySensor : public QSensor {
    Q_OBJECT
public:
    explicit QProximitySensor(QObject *parent = nullptr);
    ~QProximitySensor() override;

    QProximityReading *reading() const;

    static const char *const sensorType;
};
