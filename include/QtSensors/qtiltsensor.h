// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QTiltReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(qreal yRotation READ yRotation)
    Q_PROPERTY(qreal xRotation READ xRotation)
public:
    explicit QTiltReading(QObject *parent = nullptr);
    ~QTiltReading() override;

    qreal yRotation() const;
    void setYRotation(qreal yRotation);
    qreal xRotation() const;
    void setXRotation(qreal xRotation);

private:
    qreal m_yRotation = 0;
    qreal m_xRotation = 0;
};

class QTiltFilter : public QSensorFilter {
public:
    virtual bool filter(QTiltReading *reading) = 0;

protected:
    // Calls the typed filter(); a subclass can bring it in with a using-declaration to keep -Woverloaded-virtual quiet.
    bool filter(QSensorReading *reading) override;
};

class QTiltSensor : public QSensor {
    Q_OBJECT
public:
    explicit QTiltSensor(QObject *parent = nullptr);
    ~QTiltSensor() override;

    QTiltReading *reading() const;
    Q_INVOKABLE void calibrate();

    static const char *const sensorType;

private:
    qreal m_xOffset = 0;
    qreal m_yOffset = 0;
};
