// MIT License - see LICENSE
//
// Nt Sensors is tested against a fake backend, whose sensors report values that the test sets, and the Industrial I/O
// backend of Linux against a tree of files laid out as /sys/bus/iio/devices is, written by the test.
#include <QtCore>
#include <QtSensors>

#include "qsensorbackend_p.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <mutex>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static bool near(qreal a, qreal b, qreal tolerance = 1e-6) { return std::abs(a - b) <= tolerance; }

static void spinEvents(int msecs) {
    QEventLoop loop;
    QTimer::singleShot(msecs, [&] { loop.quit(); });
    loop.exec();
}

template <typename F> static bool waitFor(F cond, int timeoutMs = 3000) {
    QElapsedTimer timer;
    timer.start();
    while (!cond() && timer.elapsed() < timeoutMs)
        spinEvents(5);
    return cond();
}

using namespace QtSensorsPrivate;

namespace {

struct FakeWorld {
    QList<qreal> values{1.0, 2.0, 3.0};
    int failWith = 0; // a negative errno makes reads fail
    int reads = 0;
    QByteArray openedId;
};

FakeWorld *g_world = nullptr;

class FakeDevice : public SensorDevice {
public:
    bool read(QList<qreal> &values, int *error) override {
        ++g_world->reads;
        if (g_world->failWith) {
            *error = g_world->failWith;
            return false;
        }
        values = g_world->values;
        return true;
    }
};

class FakeBackend : public SensorBackend {
public:
    QList<SensorInfo> sensors() override {
        QList<SensorInfo> list;
        SensorInfo info;
        info.type = QAccelerometer::sensorType;
        info.identifier = "accel-b";
        info.description = QStringLiteral("Second accelerometer");
        list.append(info);
        info.identifier = "accel-a";
        info.description = QStringLiteral("First accelerometer");
        info.isDefault = true;
        list.append(info);
        info.type = QPressureSensor::sensorType;
        info.identifier = "baro";
        info.description = QStringLiteral("Barometer");
        list.append(info);
        return list;
    }

    std::unique_ptr<SensorDevice> open(const QByteArray &identifier, const QByteArray &, int *error) override {
        if (identifier == "missing") {
            *error = -ENODEV;
            return nullptr;
        }
        g_world->openedId = identifier;
        return std::make_unique<FakeDevice>();
    }
};

class Doubler : public QAccelerometerFilter {
public:
    using QAccelerometerFilter::filter;
    bool filter(QAccelerometerReading *reading) override {
        reading->setX(reading->x() * 2);
        return true;
    }
};

class DropNegativeZ : public QAccelerometerFilter {
public:
    using QAccelerometerFilter::filter;
    bool filter(QAccelerometerReading *reading) override { return reading->z() >= 0; }
};

void testDiscovery() {
    // The derived sensors follow those they are computed from; a compass needs a magnetometer, which there is not.
    CHECK((QSensor::sensorTypes() == QList<QByteArray>{"QAccelerometer", "QPressureSensor", "QOrientationSensor",
                                                       "QTiltSensor", "QRotationSensor", "QAltimeter"}));
    CHECK((QSensor::sensorsForType("QTiltSensor") == QList<QByteArray>{"accel-a", "accel-b"}));
    CHECK(QSensor::sensorsForType("QCompass").isEmpty());
    CHECK((QSensor::sensorsForType("QAccelerometer") == QList<QByteArray>{"accel-a", "accel-b"}));
    CHECK(QSensor::defaultSensorForType("QAccelerometer") == "accel-a");
    CHECK(QSensor::defaultSensorForType("QGyroscope").isEmpty());

    QAccelerometer accelerometer;
    CHECK(accelerometer.type() == "QAccelerometer");
    CHECK(QByteArray(QAccelerometer::sensorType) == "QAccelerometer");
    CHECK(!accelerometer.isConnectedToBackend());
    CHECK(accelerometer.availableDataRates().isEmpty());
    CHECK(accelerometer.connectToBackend());
    CHECK(accelerometer.identifier() == "accel-a");
    CHECK(accelerometer.description() == QStringLiteral("First accelerometer"));
    CHECK(!accelerometer.availableDataRates().isEmpty());
    CHECK(accelerometer.isFeatureSupported(QSensor::SkipDuplicates));
    CHECK(!accelerometer.isFeatureSupported(QSensor::Buffering));

    // No sensor of the type: start() fails with -ENODEV.
    QGyroscope gyroscope;
    QList<int> errors;
    QObject::connect(&gyroscope, &QSensor::sensorError, [&](int error) { errors.append(error); });
    CHECK(!gyroscope.start());
    CHECK(!gyroscope.isActive());
    CHECK(gyroscope.error() == -ENODEV);
    CHECK(errors == QList<int>{-ENODEV});
}

void testReadings() {
    g_world->values = {1.0, 2.0, 3.0};
    QAccelerometer accelerometer;
    accelerometer.setIdentifier("accel-b");
    accelerometer.setDataRate(100);
    int changes = 0;
    int activeChanges = 0;
    QObject::connect(&accelerometer, &QSensor::readingChanged, [&] { ++changes; });
    QObject::connect(&accelerometer, &QSensor::activeChanged, [&] { ++activeChanges; });
    CHECK(accelerometer.start());
    CHECK(accelerometer.isActive() && activeChanges == 1);
    CHECK(g_world->openedId == "accel-b");
    CHECK(accelerometer.identifier() == "accel-b");
    CHECK(waitFor([&] { return changes >= 1; }));
    QAccelerometerReading *r = accelerometer.reading();
    CHECK(r && r->x() == 1.0 && r->y() == 2.0 && r->z() == 3.0);
    CHECK(r->timestamp() > 0);
    CHECK(r->valueCount() == 3);
    CHECK(r->value(1).toDouble() == 2.0);
    CHECK(!r->value(3).isValid());

    // New values come in, with later timestamps.
    const quint64 first = r->timestamp();
    g_world->values = {4.0, 5.0, 6.0};
    CHECK(waitFor([&] { return accelerometer.reading()->x() == 4.0; }));
    CHECK(accelerometer.reading()->timestamp() > first);
    // The identifier is fixed once connected.
    accelerometer.setIdentifier("accel-a");
    CHECK(accelerometer.identifier() == "accel-b");

    accelerometer.stop();
    CHECK(!accelerometer.isActive() && activeChanges == 2);
    const int reads = g_world->reads;
    spinEvents(50);
    CHECK(g_world->reads == reads);

    // Equal readings are dropped with skipDuplicates.
    QAccelerometer quiet;
    quiet.setSkipDuplicates(true);
    quiet.setDataRate(200);
    int quietChanges = 0;
    QObject::connect(&quiet, &QSensor::readingChanged, [&] { ++quietChanges; });
    CHECK(quiet.start());
    spinEvents(80);
    CHECK(quietChanges == 1);
    g_world->values = {7.0, 8.0, 9.0};
    CHECK(waitFor([&] { return quietChanges == 2; }));
    spinEvents(40);
    CHECK(quietChanges == 2);
    quiet.stop();

    // A generic QSensor of a known type reports into the typed reading.
    QSensor generic("QAccelerometer");
    CHECK(qobject_cast<QAccelerometerReading *>(generic.reading()) != nullptr);
    QSensor unknown("QSomethingElse");
    CHECK(unknown.reading() && unknown.reading()->valueCount() == 0);
    // A pressure reading has the temperature as its second value.
    QPressureSensor barometer;
    g_world->values = {101325.0, 21.5};
    CHECK(barometer.start());
    CHECK(waitFor([&] { return barometer.reading()->pressure() == 101325.0; }));
    CHECK(barometer.reading()->temperature() == 21.5);
    CHECK(barometer.isFeatureSupported(QSensor::PressureSensorTemperature));
    barometer.stop();
}

void testFilters() {
    g_world->values = {1.0, 2.0, 3.0};
    QAccelerometer accelerometer;
    accelerometer.setDataRate(200);
    Doubler doubler;
    DropNegativeZ dropper;
    accelerometer.addFilter(&doubler);
    accelerometer.addFilter(&dropper);
    accelerometer.addFilter(&doubler); // only once
    CHECK(accelerometer.filters().size() == 2);
    int changes = 0;
    QObject::connect(&accelerometer, &QSensor::readingChanged, [&] { ++changes; });
    CHECK(accelerometer.start());
    CHECK(waitFor([&] { return changes >= 1; }));
    CHECK(accelerometer.reading()->x() == 2.0);
    // A dropped reading leaves the reading of the sensor as it was.
    g_world->values = {10.0, 0.0, -1.0};
    const int before = changes;
    spinEvents(60);
    CHECK(changes == before);
    CHECK(accelerometer.reading()->x() == 2.0);
    accelerometer.removeFilter(&dropper);
    CHECK(waitFor([&] { return accelerometer.reading()->x() == 20.0; }));
    accelerometer.stop();
    {
        // A filter that goes away first removes itself.
        Doubler shortLived;
        accelerometer.addFilter(&shortLived);
        CHECK(accelerometer.filters().size() == 2);
    }
    CHECK(accelerometer.filters().size() == 1);
}

void testErrors() {
    g_world->values = {1.0, 2.0, 3.0};
    QAccelerometer accelerometer;
    accelerometer.setDataRate(200);
    QList<int> errors;
    QObject::connect(&accelerometer, &QSensor::sensorError, [&](int error) { errors.append(error); });
    CHECK(accelerometer.start());
    g_world->failWith = -EIO;
    CHECK(waitFor([&] { return !accelerometer.isActive(); }));
    CHECK(errors == QList<int>{-EIO});
    CHECK(accelerometer.error() == -EIO);
    g_world->failWith = 0;
    CHECK(accelerometer.start());
    CHECK(accelerometer.error() == 0);
    accelerometer.stop();

    QAccelerometer missing;
    missing.setIdentifier("missing");
    CHECK(!missing.start());
    CHECK(missing.error() == -ENODEV);

    // setActive() starts and stops; the data rate changes while running.
    QAccelerometer toggled;
    toggled.setActive(true);
    CHECK(toggled.isActive());
    int rateChanges = 0;
    QObject::connect(&toggled, &QSensor::dataRateChanged, [&] { ++rateChanges; });
    toggled.setDataRate(50);
    toggled.setDataRate(50);
    CHECK(toggled.dataRate() == 50 && rateChanges == 1);
    toggled.setActive(false);
    CHECK(!toggled.isActive());
    toggled.setAlwaysOn(true);
    CHECK(toggled.isAlwaysOn());
}

void testDerivedMath() {
    // Heading of the top of a device lying face up, with the field pointing north and down.
    const qreal g = 9.81;
    const qreal h = 20e-6;
    const qreal v = 40e-6;
    CHECK(near(compassAzimuth(0, 0, g, 0, h, -v), 0.0, 1e-9));   // top to the north
    CHECK(near(compassAzimuth(0, 0, g, -h, 0, -v), 90.0, 1e-9)); // top to the east: north is to the left
    CHECK(near(compassAzimuth(0, 0, g, 0, -h, -v), 180.0, 1e-9));
    CHECK(near(compassAzimuth(0, 0, g, h, 0, -v), 270.0, 1e-9));
    // Tilting the device does not move the heading: upright, top up and facing north (the back to the north).
    CHECK(near(compassAzimuth(0, g, 0, 0, -v, -h), compassAzimuth(0, g, 0, 0, -v, -h), 1e-9));
    const qreal tilted = compassAzimuth(0, g * std::sin(0.3), g * std::cos(0.3), 0,
                                        h * std::cos(0.3) - v * std::sin(0.3), -h * std::sin(0.3) - v * std::cos(0.3));
    CHECK(near(tilted, 0.0, 1e-6));

    CHECK(orientationFor(0, g, 0, QOrientationReading::Undefined) == QOrientationReading::TopUp);
    CHECK(orientationFor(0, -g, 0, 0) == QOrientationReading::TopDown);
    CHECK(orientationFor(g, 0, 0, 0) == QOrientationReading::RightUp);
    CHECK(orientationFor(-g, 0, 0, 0) == QOrientationReading::LeftUp);
    CHECK(orientationFor(0, 0, g, 0) == QOrientationReading::FaceUp);
    CHECK(orientationFor(0, 0, -g, 0) == QOrientationReading::FaceDown);
    // Along the diagonal of all three axes none has 70 percent of gravity: the last orientation stays.
    const qreal third = g / std::sqrt(3.0);
    CHECK(orientationFor(third, third, third, QOrientationReading::FaceUp) == QOrientationReading::FaceUp);
    CHECK(orientationFor(0, 0, 0, QOrientationReading::TopUp) == QOrientationReading::TopUp);

    CHECK(near(altitudeFor(101325.0), 0.0, 1e-6));
    CHECK(near(altitudeFor(89874.6), 1000.0, 1.0)); // the standard atmosphere at 1000 m
    CHECK(altitudeFor(0) == 0);

    CHECK(lightLevelFor(-1) == QAmbientLightReading::Undefined);
    CHECK(lightLevelFor(1) == QAmbientLightReading::Dark);
    CHECK(lightLevelFor(20) == QAmbientLightReading::Twilight);
    CHECK(lightLevelFor(300) == QAmbientLightReading::Light);
    CHECK(lightLevelFor(1000) == QAmbientLightReading::Bright);
    CHECK(lightLevelFor(50000) == QAmbientLightReading::Sunny);
}

void testDerivedSensors() {
    // Orientation, tilt and rotation from the fake accelerometer.
    g_world->values = {0.0, 9.81, 0.0};
    QOrientationSensor orientation;
    QTiltSensor tilt;
    QRotationSensor rotation;
    QAltimeter altimeter;
    for (QSensor *sensor : std::initializer_list<QSensor *>{&orientation, &tilt, &rotation, &altimeter}) {
        sensor->setDataRate(200);
        CHECK(sensor->start());
    }
    CHECK(waitFor([&] { return orientation.reading()->orientation() == QOrientationReading::TopUp; }));
    CHECK(waitFor([&] { return near(tilt.reading()->xRotation(), 90.0, 1e-6); }));
    CHECK(near(tilt.reading()->yRotation(), 0.0, 1e-6));
    CHECK(waitFor([&] { return near(rotation.reading()->x(), 90.0, 1e-6); }));
    CHECK(rotation.reading()->z() == 0.0);
    CHECK(orientation.identifier() == "accel-a");
    // The altimeter reads the barometer, whose first value the fake also serves.
    g_world->values = {101325.0, 20.0, 0.0};
    CHECK(waitFor([&] { return near(altimeter.reading()->altitude(), 0.0, 1e-6); }));

    // calibrate() takes the current tilt as level.
    g_world->values = {0.0, 4.905, 8.496}; // tilted 30 degrees about x
    CHECK(waitFor([&] { return near(tilt.reading()->xRotation(), 30.0, 0.01); }));
    tilt.calibrate();
    CHECK(waitFor([&] { return near(tilt.reading()->xRotation(), 0.0, 0.01); }));
    g_world->values = {0.0, 0.0, 9.81};
    CHECK(waitFor([&] { return near(tilt.reading()->xRotation(), -30.0, 0.01); }));
    for (QSensor *sensor : std::initializer_list<QSensor *>{&orientation, &tilt, &rotation, &altimeter})
        sensor->stop();

    // A value travels as the enum through a filter of its type.
    class OnlyFaceUp : public QOrientationFilter {
    public:
        using QOrientationFilter::filter;
        bool filter(QOrientationReading *reading) override {
            return reading->orientation() == QOrientationReading::FaceUp;
        }
    } onlyFaceUp;
    QOrientationSensor filtered;
    filtered.setDataRate(200);
    filtered.addFilter(&onlyFaceUp);
    g_world->values = {0.0, 9.81, 0.0};
    int changes = 0;
    QObject::connect(&filtered, &QSensor::readingChanged, [&] { ++changes; });
    CHECK(filtered.start());
    spinEvents(50);
    CHECK(changes == 0);
    g_world->values = {0.0, 0.0, 9.81};
    CHECK(waitFor([&] { return filtered.reading()->orientation() == QOrientationReading::FaceUp; }));
    CHECK(filtered.reading()->value(0).isValid());
    filtered.stop();
}

class LidBackend : public QSensorBackend {
public:
    explicit LidBackend(QSensor *sensor) : QSensorBackend(sensor) {
        m_reading = setReading<QLidReading>(nullptr);
        setDescription(QStringLiteral("Test lid"));
        addDataRate(1, 50);
        addOutputRange(0, 1, 1);
        ++s_created;
    }
    ~LidBackend() override { ++s_destroyed; }
    void start() override {
        ++s_starts;
        if (s_refuse)
            sensorStopped();
    }
    void stop() override { ++s_stops; }
    bool isFeatureSupported(QSensor::Feature feature) const override { return feature == QSensor::AlwaysOn; }

    void close(bool closed) {
        m_reading->setBackLidClosed(closed);
        m_reading->setTimestamp(1234);
        newReadingAvailable();
    }
    using QSensorBackend::sensorBusy;
    using QSensorBackend::sensorError;

    static inline int s_created = 0;
    static inline int s_destroyed = 0;
    static inline int s_starts = 0;
    static inline int s_stops = 0;
    static inline bool s_refuse = false;
    static inline LidBackend *s_last = nullptr;

private:
    QLidReading *m_reading;
};

class LidFactory : public QSensorBackendFactory {
public:
    QSensorBackend *createBackend(QSensor *sensor) override {
        auto *backend = new LidBackend(sensor);
        LidBackend::s_last = backend;
        return backend;
    }
};

void testManager() {
    static LidFactory factory;
    CHECK(!QSensorManager::isBackendRegistered("QLidSensor", "test-lid"));
    QSensorManager::registerBackend("QLidSensor", "test-lid", &factory);
    QSensorManager::registerBackend("QLidSensor", "test-lid-2", &factory);
    CHECK(QSensorManager::isBackendRegistered("QLidSensor", "test-lid"));
    CHECK(QSensor::sensorTypes().contains("QLidSensor"));
    CHECK((QSensor::sensorsForType("QLidSensor") == QList<QByteArray>{"test-lid", "test-lid-2"}));
    QSensorManager::setDefaultBackend("QLidSensor", "test-lid-2");
    CHECK(QSensor::defaultSensorForType("QLidSensor") == "test-lid-2");
    QSensorManager::setDefaultBackend("QLidSensor", "test-lid");

    {
        QLidSensor lid;
        QList<bool> backChanges;
        QObject::connect(lid.reading(), &QLidReading::backLidChanged, [&](bool closed) { backChanges.append(closed); });
        int changes = 0;
        QObject::connect(&lid, &QSensor::readingChanged, [&] { ++changes; });
        CHECK(lid.start());
        CHECK(lid.isActive() && LidBackend::s_starts == 1);
        CHECK(lid.identifier() == "test-lid");
        CHECK(lid.description() == QStringLiteral("Test lid"));
        CHECK((lid.availableDataRates() == qrangelist{qrange(1, 50)}));
        CHECK(lid.outputRanges().size() == 1);
        CHECK(lid.isFeatureSupported(QSensor::AlwaysOn));
        CHECK(!lid.isFeatureSupported(QSensor::SkipDuplicates));
        LidBackend::s_last->close(true);
        CHECK(changes == 1);
        CHECK(lid.reading()->backLidClosed() && !lid.reading()->frontLidClosed());
        CHECK(lid.reading()->timestamp() == 1234);
        CHECK(backChanges == QList<bool>{true});

        int busyChanges = 0;
        QObject::connect(&lid, &QSensor::busyChanged, [&] { ++busyChanges; });
        LidBackend::s_last->sensorBusy();
        CHECK(lid.isBusy() && busyChanges == 1);
        LidBackend::s_last->sensorBusy(false);
        CHECK(!lid.isBusy() && busyChanges == 2);
        LidBackend::s_last->sensorError(-EIO);
        CHECK(lid.error() == -EIO);
        lid.stop();
        CHECK(!lid.isActive() && LidBackend::s_stops == 1);
        // A reading after the sensor stopped is not reported.
        LidBackend::s_last->close(false);
        CHECK(changes == 1);
    }
    CHECK(LidBackend::s_created == LidBackend::s_destroyed);

    // A backend that cannot start stops the sensor.
    LidBackend::s_refuse = true;
    QLidSensor refused;
    refused.setIdentifier("test-lid-2");
    CHECK(!refused.start());
    CHECK(!refused.isActive());
    LidBackend::s_refuse = false;

    // QSensorManager::createBackend() makes one for a sensor directly.
    QLidSensor other;
    QSensorBackend *backend = QSensorManager::createBackend(&other);
    CHECK(backend && backend->sensor() == &other && backend->reading());
    delete backend;

    QSensorManager::unregisterBackend("QLidSensor", "test-lid");
    QSensorManager::unregisterBackend("QLidSensor", "test-lid-2");
    CHECK(!QSensorManager::isBackendRegistered("QLidSensor", "test-lid"));
    CHECK(!QSensor::sensorTypes().contains("QLidSensor"));
}

void testReadingTypes() {
    // Every type has its reading, with its values in order.
    const QList<QByteArray> types = {
        "QAccelerometer",  "QAltimeter",     "QAmbientLightSensor", "QAmbientTemperatureSensor", "QCompass",
        "QGyroscope",      "QHolsterSensor", "QHumiditySensor",     "QIRProximitySensor",        "QLidSensor",
        "QLightSensor",    "QMagnetometer",  "QOrientationSensor",  "QPressureSensor",           "QProximitySensor",
        "QRotationSensor", "QTapSensor",     "QTiltSensor"};
    const QList<int> counts = {3, 1, 1, 1, 2, 3, 1, 2, 1, 2, 1, 4, 1, 2, 1, 3, 2, 2};
    for (qsizetype i = 0; i < types.size(); ++i) {
        QSensor sensor(types[i]);
        CHECK(sensor.reading()->valueCount() == counts[i]);
        if (sensor.reading()->valueCount() != counts[i])
            std::printf("  %s has %d values\n", types[i].constData(), sensor.reading()->valueCount());
    }
    QTapReading tap;
    tap.setTapDirection(QTapReading::Z_Neg);
    tap.setDoubleTap(true);
    CHECK(tap.tapDirection() == QTapReading::Z_Neg && tap.isDoubleTap());
    QRotationReading rotation;
    rotation.setFromEuler(1, 2, 3);
    CHECK(rotation.x() == 1 && rotation.y() == 2 && rotation.z() == 3);
    QMagnetometerReading magnetometer;
    CHECK(magnetometer.calibrationLevel() == 1);
    QTapSensor tapSensor;
    CHECK(tapSensor.returnDoubleTapEvents());
    tapSensor.setReturnDoubleTapEvents(false);
    CHECK(!tapSensor.returnDoubleTapEvents());
    QRotationSensor rotationSensor;
    CHECK(!rotationSensor.hasZ());
}

void writeFile(const QString &path, const QByteArray &contents) {
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(contents + '\n');
}

void testIio(const QTemporaryDir &dir) {
    const QString root = dir.filePath(QStringLiteral("iio"));
    // An IMU with raw values, a shared scale and a per-axis offset.
    const QString imu = root + QLatin1String("/iio:device0");
    QDir().mkpath(imu);
    writeFile(imu + QLatin1String("/name"), "lsm6dso");
    writeFile(imu + QLatin1String("/in_accel_x_raw"), "100");
    writeFile(imu + QLatin1String("/in_accel_y_raw"), "-200");
    writeFile(imu + QLatin1String("/in_accel_z_raw"), "981");
    writeFile(imu + QLatin1String("/in_accel_z_offset"), "1");
    writeFile(imu + QLatin1String("/in_accel_scale"), "0.01");
    writeFile(imu + QLatin1String("/in_anglvel_x_raw"), "1");
    writeFile(imu + QLatin1String("/in_anglvel_y_raw"), "0");
    writeFile(imu + QLatin1String("/in_anglvel_z_raw"), "-2");
    writeFile(imu + QLatin1String("/in_anglvel_scale"), "0.5");
    writeFile(imu + QLatin1String("/in_magn_x_input"), "0.4");
    writeFile(imu + QLatin1String("/in_magn_y_input"), "0");
    writeFile(imu + QLatin1String("/in_magn_z_input"), "-0.2");
    // An environmental sensor with processed values.
    const QString env = root + QLatin1String("/iio:device1");
    QDir().mkpath(env);
    writeFile(env + QLatin1String("/name"), "bme280");
    writeFile(env + QLatin1String("/in_pressure_input"), "101.325");
    writeFile(env + QLatin1String("/in_temp_input"), "21500");
    writeFile(env + QLatin1String("/in_humidityrelative_input"), "45250");
    // A light sensor, and a directory that is not a device.
    const QString light = root + QLatin1String("/iio:device2");
    QDir().mkpath(light);
    writeFile(light + QLatin1String("/in_illuminance_raw"), "300");
    writeFile(light + QLatin1String("/in_illuminance_scale"), "0.5");
    QDir().mkpath(root + QLatin1String("/trigger0"));

    // A proximity sensor, and an ACPI lid.
    const QString prox = root + QLatin1String("/iio:device3");
    QDir().mkpath(prox);
    writeFile(prox + QLatin1String("/in_proximity_raw"), "40");
    writeFile(prox + QLatin1String("/in_proximity_scale"), "0.01");
    const QString lids = dir.filePath(QStringLiteral("lid"));
    QDir().mkpath(lids + QLatin1String("/LID0"));
    writeFile(lids + QLatin1String("/LID0/state"), "state:      closed");

    auto backend = createIioSensorBackend(root, lids);
    const QList<SensorInfo> all = backend->sensors();
    QMap<QByteArray, QByteArray> byType;
    for (const SensorInfo &info : all)
        byType.insert(info.type, info.identifier);
    CHECK(all.size() == 9);
    CHECK(byType.value("QIRProximitySensor") == "iio:device3");
    CHECK(byType.value("QLidSensor") == "LID0");
    CHECK(byType.value("QAccelerometer") == "iio:device0");
    CHECK(byType.value("QGyroscope") == "iio:device0");
    CHECK(byType.value("QMagnetometer") == "iio:device0");
    CHECK(byType.value("QPressureSensor") == "iio:device1");
    CHECK(byType.value("QAmbientTemperatureSensor") == "iio:device1");
    CHECK(byType.value("QHumiditySensor") == "iio:device1");
    CHECK(byType.value("QLightSensor") == "iio:device2");
    for (const SensorInfo &info : all) {
        if (info.identifier == "iio:device1")
            CHECK(info.description == QStringLiteral("bme280"));
        CHECK(info.isDefault);
    }

    int dummyError = 0;
    auto readAll = [&](const QByteArray &id, const QByteArray &type) {
        QList<qreal> values;
        int error = 0;
        auto device = backend->open(id, type, &error);
        if (!device || !device->read(values, &error))
            values.clear();
        return values;
    };
    const QList<qreal> accel = readAll("iio:device0", "QAccelerometer");
    CHECK(accel.size() == 3 && near(accel[0], 1.0) && near(accel[1], -2.0) && near(accel[2], 9.82));
    const QList<qreal> gyro = readAll("iio:device0", "QGyroscope");
    CHECK(gyro.size() == 3 && near(gyro[0], 0.5 * 180.0 / M_PI) && near(gyro[2], -1.0 * 180.0 / M_PI));
    const QList<qreal> magn = readAll("iio:device0", "QMagnetometer");
    CHECK(magn.size() == 4 && near(magn[0], 4e-5) && near(magn[2], -2e-5) && magn[3] == 1.0);
    const QList<qreal> pressure = readAll("iio:device1", "QPressureSensor");
    CHECK(pressure.size() == 2 && near(pressure[0], 101325.0) && near(pressure[1], 21.5));
    const QList<qreal> temperature = readAll("iio:device1", "QAmbientTemperatureSensor");
    CHECK(temperature.size() == 1 && near(temperature[0], 21.5));
    const QList<qreal> humidity = readAll("iio:device1", "QHumiditySensor");
    CHECK(humidity.size() == 2 && near(humidity[0], 45.25));
    const QList<qreal> reflectance = readAll("iio:device3", "QIRProximitySensor");
    CHECK(reflectance.size() == 1 && near(reflectance[0], 0.4));
    QList<qreal> lid = readAll("LID0", "QLidSensor");
    CHECK((lid == QList<qreal>{1.0, 0.0}));
    writeFile(lids + QLatin1String("/LID0/state"), "state:      open");
    lid = readAll("LID0", "QLidSensor");
    CHECK((lid == QList<qreal>{0.0, 0.0}));
    CHECK(!backend->open("LID9", "QLidSensor", &dummyError) && dummyError == -ENODEV);
    const QList<qreal> lux = readAll("iio:device2", "QLightSensor");
    CHECK(lux.size() == 1 && near(lux[0], 150.0));

    int error = 0;
    CHECK(!backend->open("iio:device2", "QAccelerometer", &error) && error == -ENODEV);
    CHECK(!backend->open("../iio:device0", "QAccelerometer", &error));
    // A channel that goes away fails the read.
    auto device = backend->open("iio:device2", "QLightSensor", &error);
    QFile::remove(light + QLatin1String("/in_illuminance_raw"));
    QList<qreal> values;
    CHECK(device && !device->read(values, &error) && error == -EIO);

    // Through QSensor, with the IIO backend in use.
    setSensorBackend(createIioSensorBackend(root));
    QHumiditySensor sensor;
    CHECK(sensor.start());
    CHECK(waitFor([&] { return near(sensor.reading()->relativeHumidity(), 45.25); }));
    CHECK(sensor.description() == QStringLiteral("bme280"));
    sensor.stop();

    // The lid of this machine, if it has one: only read.
    if (QFile::exists(QStringLiteral("/proc/acpi/button/lid"))) {
        auto real =
            createIioSensorBackend(QStringLiteral("/sys/bus/iio/devices"), QStringLiteral("/proc/acpi/button/lid"));
        for (const SensorInfo &info : real->sensors()) {
            if (info.type != "QLidSensor")
                continue;
            int lidError = 0;
            auto lidDevice = real->open(info.identifier, info.type, &lidError);
            QList<qreal> lidValues;
            CHECK(lidDevice && lidDevice->read(lidValues, &lidError) && lidValues.size() == 2);
            std::printf("this machine: lid %s is %s\n", info.identifier.constData(),
                        lidValues.value(0) != 0 ? "closed" : "open");
        }
    }
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    FakeWorld world;
    g_world = &world;
    setSensorBackend(std::make_unique<FakeBackend>());

    testDiscovery();
    testReadings();
    testFilters();
    testErrors();
    testDerivedMath();
    testDerivedSensors();
    testManager();
    testReadingTypes();
    QTemporaryDir dir;
    testIio(dir);
    setSensorBackend(nullptr);

    if (g_failures) {
        std::printf("%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("all sensors tests passed\n");
    return 0;
}
