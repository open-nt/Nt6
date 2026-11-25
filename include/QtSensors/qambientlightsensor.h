// MIT License - see LICENSE
#pragma once

#include <QtSensors/qsensor.h>

class QAmbientLightReading : public QSensorReading {
    Q_OBJECT
    Q_PROPERTY(LightLevel lightLevel READ lightLevel)
public:
    enum LightLevel { Undefined = 0, Dark, Twilight, Light, Bright, Sunny };
    Q_ENUM(LightLevel)

    explicit QAmbientLightReading(QObject *parent = nullptr);
    ~QAmbientLightReading() override;

    LightLevel lightLevel() const;
    void setLightLevel(LightLevel lightLevel);

private:
    LightLevel m_lightLevel = Undefined;
};

class QAmbientLightFilter : public QSensorFilter {
public:
    virtual bool filter(QAmbientLightReading *reading) = 0;

protected:
    // Calls the typed filter(); a subclass can bring it in with a using-declaration to keep -Woverloaded-virtual quiet.
    bool filter(QSensorReading *reading) override;
};

class QAmbientLightSensor : public QSensor {
    Q_OBJECT
public:
    explicit QAmbientLightSensor(QObject *parent = nullptr);
    ~QAmbientLightSensor() override;

    QAmbientLightReading *reading() const;

    static const char *const sensorType;
};
