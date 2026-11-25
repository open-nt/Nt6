// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QRotationReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(qreal x READ x)
    Q_PROPERTY(qreal y READ y)
    Q_PROPERTY(qreal z READ z)
public:
    explicit QRotationReading(QObject *parent = nullptr);
    ~QRotationReading() override;

    qreal x() const;
    qreal y() const;
    qreal z() const;
    void setFromEuler(qreal x, qreal y, qreal z);

private:
    qreal m_x = 0;
    qreal m_y = 0;
    qreal m_z = 0;
};

class QRotationFilter : public QSensorFilter {
public:
    virtual bool filter(QRotationReading *reading) = 0;

protected:
    // Calls the typed filter(); a subclass can bring it in with a using-declaration to keep -Woverloaded-virtual quiet.
    bool filter(QSensorReading *reading) override;
};

class QRotationSensor : public QSensor {
    Q_OBJECT
    Q_PROPERTY(bool hasZ READ hasZ NOTIFY hasZChanged)
public:
    explicit QRotationSensor(QObject *parent = nullptr);
    ~QRotationSensor() override;

    QRotationReading *reading() const;
    bool hasZ() const;
    void setHasZ(bool hasZ);

    static const char *const sensorType;

signals:
    /*!
        This signal is emitted when hasZ() changes to \a hasZ.
    */
    void hasZChanged(bool hasZ) { emitSignal(&QRotationSensor::hasZChanged, hasZ); }

private:
    bool m_hasZ = false;
};
