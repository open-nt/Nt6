// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QHolsterReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(bool holstered READ holstered)
public:
    explicit QHolsterReading(QObject *parent = nullptr);
    ~QHolsterReading() override;

    bool holstered() const;
    void setHolstered(bool holstered);

private:
    bool m_holstered = false;
};

class QHolsterFilter : public QSensorFilter {
public:
    virtual bool filter(QHolsterReading *reading) = 0;

protected:
    // Calls the typed filter(); a subclass can bring it in with a using-declaration to keep -Woverloaded-virtual quiet.
    bool filter(QSensorReading *reading) override;
};

class QHolsterSensor : public QSensor {
    Q_OBJECT
public:
    explicit QHolsterSensor(QObject *parent = nullptr);
    ~QHolsterSensor() override;

    QHolsterReading *reading() const;

    static const char *const sensorType;
};
