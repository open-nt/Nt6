// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QIRProximityReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(qreal reflectance READ reflectance)
public:
    explicit QIRProximityReading(QObject *parent = nullptr);
    ~QIRProximityReading() override;

    qreal reflectance() const;
    void setReflectance(qreal reflectance);

private:
    qreal m_reflectance = 0;
};

class QIRProximityFilter : public QSensorFilter {
public:
    virtual bool filter(QIRProximityReading *reading) = 0;

protected:
    // Calls the typed filter(); a subclass can bring it in with a using-declaration to keep -Woverloaded-virtual quiet.
    bool filter(QSensorReading *reading) override;
};

class QIRProximitySensor : public QSensor {
    Q_OBJECT
public:
    explicit QIRProximitySensor(QObject *parent = nullptr);
    ~QIRProximitySensor() override;

    QIRProximityReading *reading() const;

    static const char *const sensorType;
};
