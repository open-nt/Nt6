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

#include <functional>
#include <memory>

class QSensorBackend;
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
    // (x, y, z for an accelerometer; a bool or enumeration as a number). Returns false and sets error, a negative errno
    // value, on failure; -EAGAIN means that there is no new value yet, which is not an error.
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
// The backend of Linux on the Industrial I/O devices under root (normally /sys/bus/iio/devices) and the ACPI lids under
// lidRoot (normally /proc/acpi/button/lid); for tests. Only defined where that backend is built.
std::unique_ptr<SensorBackend> createIioSensorBackend(const QString &root, const QString &lidRoot = QString());

// Wraps a backend so that it also lists the sensors that Nt computes from others: orientation, tilt and rotation from an
// accelerometer, a compass from an accelerometer and a magnetometer, an altimeter from a pressure sensor and an ambient
// light level from a light sensor, for each type that the backend has no sensor of. setSensorBackend() and the
// platform's backend are wrapped.
std::unique_ptr<SensorBackend> createDerivedSensorBackend(std::unique_ptr<SensorBackend> base);

// Makes the reading that a sensor of type reports into; a plain reading for a type Nt does not know.
QSensorReading *createReading(const QByteArray &type, QObject *parent);
// Writes values, in the order of the reading's properties, into reading.
void setReadingValues(QSensorReading *reading, const QList<qreal> &values);
// The computations of the derived sensors, for tests: the heading of the top of the device in degrees from magnetic
// north for gravity a and field m in the frame of the device, the QOrientationReading::Orientation for gravity, the
// altitude in metres for a pressure in pascals, and the QAmbientLightReading::LightLevel for lux.
qreal compassAzimuth(qreal ax, qreal ay, qreal az, qreal mx, qreal my, qreal mz);
int orientationFor(qreal x, qreal y, qreal z, int previous);
qreal altitudeFor(qreal pascals);
int lightLevelFor(qreal lux);

// The values of reading, in the order of its properties.
QList<qreal> readingValues(const QSensorReading *reading);

// What the classes of Nt Sensors reach in a QSensor. Defined in qsensor.cpp.
struct SensorAccess {
    // Delivers values measured at timestamp (microseconds) to the sensor, as a reading of its type.
    static void deliver(QSensor *sensor, const QList<qreal> &values, quint64 timestamp);
    // A QSensorBackend reports that it stopped, became busy or free, or failed.
    static void stopped(QSensor *sensor);
    static void setBusy(QSensor *sensor, bool busy);
    static void error(QSensor *sensor, int error);
    static void addDataRate(QSensor *sensor, int minimum, int maximum);
    static void addOutputRange(QSensor *sensor, const qoutputrange &range);
    static void setDescription(QSensor *sensor, const QString &description);
    // Runs on the values of each sample before filters see them, as QTiltSensor::calibrate() needs.
    static void setAdjuster(QSensor *sensor, std::function<void(QList<qreal> &)> adjuster);
    static QList<qreal> lastValues(const QSensor *sensor);
};

// The backends registered with QSensorManager. Defined in qsensormanager.cpp.
QList<QByteArray> registeredIdentifiers(const QByteArray &type); // the default first
QList<QByteArray> registeredTypes();
QSensorBackend *createRegisteredBackend(QSensor *sensor, const QByteArray &identifier);

} // namespace QtSensorsPrivate
