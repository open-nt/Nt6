// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QGyroscopeReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(qreal x READ x)
    Q_PROPERTY(qreal y READ y)
    Q_PROPERTY(qreal z READ z)
public:
    explicit QGyroscopeReading(QObject *parent = nullptr);
    ~QGyroscopeReading() override;

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

class QGyroscopeFilter : public QSensorFilter {
public:
    virtual bool filter(QGyroscopeReading *reading) = 0;

protected:
    // Calls the typed filter(); a subclass can bring it in with a using-declaration to keep -Woverloaded-virtual quiet.
    bool filter(QSensorReading *reading) override;
};

class QGyroscope : public QSensor {
    Q_OBJECT
public:
    explicit QGyroscope(QObject *parent = nullptr);
    ~QGyroscope() override;

    QGyroscopeReading *reading() const;

    static const char *const sensorType;
};
