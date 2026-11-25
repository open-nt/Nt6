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
    CHECK((QSensor::sensorTypes() == QList<QByteArray>{"QAccelerometer", "QPressureSensor"}));
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

    auto backend = createIioSensorBackend(root);
    const QList<SensorInfo> all = backend->sensors();
    QMap<QByteArray, QByteArray> byType;
    for (const SensorInfo &info : all)
        byType.insert(info.type, info.identifier);
    CHECK(all.size() == 7);
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
