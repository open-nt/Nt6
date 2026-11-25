// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QLightReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(qreal lux READ lux)
public:
    explicit QLightReading(QObject *parent = nullptr);
    ~QLightReading() override;

    qreal lux() const;
    void setLux(qreal lux);

private:
    qreal m_lux = 0;
};

class QLightFilter : public QSensorFilter {
public:
    virtual bool filter(QLightReading *reading) = 0;

protected:
    // Calls the typed filter(); a subclass can bring it in with a using-declaration to keep -Woverloaded-virtual quiet.
    bool filter(QSensorReading *reading) override;
};

class QLightSensor : public QSensor {
    Q_OBJECT
    Q_PROPERTY(qreal fieldOfView READ fieldOfView NOTIFY fieldOfViewChanged)
public:
    explicit QLightSensor(QObject *parent = nullptr);
    ~QLightSensor() override;

    QLightReading *reading() const;
    qreal fieldOfView() const;
    void setFieldOfView(qreal fieldOfView);

    static const char *const sensorType;

signals:
    /*!
        This signal is emitted when the field of view changes to \a fieldOfView.
    */
    void fieldOfViewChanged(qreal fieldOfView) { emitSignal(&QLightSensor::fieldOfViewChanged, fieldOfView); }

private:
    qreal m_fieldOfView = 0;
};
