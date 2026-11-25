// MIT License - see LICENSE
//
// The platform part of Nt Sensors.
//
// A SensorBackend lists the sensors of the machine and opens them; qsensorbackend_iio.cpp reads the Industrial I/O
// devices of Linux, src/zephyr/qsensorbackend_zephyr.cpp the sensor drivers of Zephyr, and qsensorbackend_null.cpp has
// none. An open SensorDevice is read from the thread of its QSensor, each time the sensor's timer fires. The tests
// replace the backend with setSensorBackend().
#pragma once

#include <QtSensors/qsensor.h>

#include <memory>

class QSensorReading;

namespace QtSensorsPrivate {

// One sensor of one type. A device that measures several things, such as pressure, temperature and humidity, is
// listed once for each.
struct SensorInfo {
    QByteArray identifier;
    QByteArray type; // QAccelerometer::sensorType and the others
    QString description;
    bool isDefault = false;
    qrangelist dataRates{{1, 100}};
    qoutputrangelist outputRanges;
};

class SensorDevice {
public:
    SensorDevice() = default;
    virtual ~SensorDevice() = default;
    SensorDevice(const SensorDevice &) = delete;
    SensorDevice &operator=(const SensorDevice &) = delete;

    // Takes one sample: values in the units of Qt Sensors, in the order of the properties of the reading of the type
    // (x, y, z for an accelerometer). Returns false and sets error, a negative errno value, on failure.
    virtual bool read(QList<qreal> &values, int *error) = 0;
};

class SensorBackend {
public:
    SensorBackend() = default;
    virtual ~SensorBackend() = default;
    SensorBackend(const SensorBackend &) = delete;
    SensorBackend &operator=(const SensorBackend &) = delete;

    virtual QList<SensorInfo> sensors() = 0;
    // Opens the sensor identifier as type. Returns nullptr and sets error, a negative errno value, on failure.
    virtual std::unique_ptr<SensorDevice> open(const QByteArray &identifier, const QByteArray &type, int *error) = 0;
};

// The backend in use, created on first use.
SensorBackend *sensorBackend();
// Replaces the backend; for tests. Sensors of the old one must be stopped.
void setSensorBackend(std::unique_ptr<SensorBackend> backend);
// The platform's backend.
std::unique_ptr<SensorBackend> createPlatformSensorBackend();
// The Industrial I/O backend of Linux on the devices under root (normally /sys/bus/iio/devices); for tests. Only
// defined where that backend is built.
std::unique_ptr<SensorBackend> createIioSensorBackend(const QString &root);

// Makes the reading that a sensor of type reports into; a plain reading for a type Nt does not know.
QSensorReading *createReading(const QByteArray &type, QObject *parent);
// Writes values, in the order of the reading's properties, into reading.
void setReadingValues(QSensorReading *reading, const QList<qreal> &values);

} // namespace QtSensorsPrivate
