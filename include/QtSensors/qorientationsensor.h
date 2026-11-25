// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QOrientationReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(Orientation orientation READ orientation)
public:
    enum Orientation { Undefined = 0, TopUp, TopDown, LeftUp, RightUp, FaceUp, FaceDown };
    Q_ENUM(Orientation)

    explicit QOrientationReading(QObject *parent = nullptr);
    ~QOrientationReading() override;

    Orientation orientation() const;
    void setOrientation(Orientation orientation);

private:
    Orientation m_orientation = Undefined;
};

class QOrientationFilter : public QSensorFilter {
public:
    virtual bool filter(QOrientationReading *reading) = 0;

protected:
    // Calls the typed filter(); a subclass can bring it in with a using-declaration to keep -Woverloaded-virtual quiet.
    bool filter(QSensorReading *reading) override;
};

class QOrientationSensor : public QSensor {
    Q_OBJECT
public:
    explicit QOrientationSensor(QObject *parent = nullptr);
    ~QOrientationSensor() override;

    QOrientationReading *reading() const;

    static const char *const sensorType;
};
