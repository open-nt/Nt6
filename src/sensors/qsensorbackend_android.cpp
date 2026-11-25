// MIT License - see LICENSE
//
// The sensor backend on Android: the sensors of the NDK's ASensorManager. A sensor is enabled on an event queue of the
// looper of the thread that opens it, at the data rate of the QSensor, and each sample takes the newest event that has
// come since the last, without blocking; when none has, the sample is skipped. The values are converted to the units
// of Qt Sensors: microtesla to tesla, radians to degrees a second and hectopascals to pascals. A proximity sensor is
// close below 4 cm, since most report either 0 or their range of about 5 cm.

#include "qsensorbackend_p.h"

#include <android/looper.h>
#include <android/sensor.h>

#include <QtCore/qfile.h>

#include <algorithm>
#include <cerrno>

namespace QtSensorsPrivate {

namespace {

constexpr qreal kDegrees = 180.0 / 3.14159265358979323846;
constexpr float kCloseCentimetres = 4.0f;

const char *qtType(int androidType) {
    switch (androidType) {
    case ASENSOR_TYPE_ACCELEROMETER:
        return "QAccelerometer";
    case ASENSOR_TYPE_MAGNETIC_FIELD:
        return "QMagnetometer";
    case ASENSOR_TYPE_GYROSCOPE:
        return "QGyroscope";
    case ASENSOR_TYPE_LIGHT:
        return "QLightSensor";
    case ASENSOR_TYPE_PRESSURE:
        return "QPressureSensor";
    case ASENSOR_TYPE_PROXIMITY:
        return "QProximitySensor";
    case ASENSOR_TYPE_RELATIVE_HUMIDITY:
        return "QHumiditySensor";
    case ASENSOR_TYPE_AMBIENT_TEMPERATURE:
        return "QAmbientTemperatureSensor";
    default:
        return nullptr;
    }
}

// The name of the package, which ASensorManager wants: the name of the process on Android.
QByteArray packageName() {
    QFile cmdline(QStringLiteral("/proc/self/cmdline"));
    QByteArray name;
    if (cmdline.open(QIODevice::ReadOnly))
        name = cmdline.readAll();
    const qsizetype end = name.indexOf('\0');
    if (end >= 0)
        name.truncate(end);
    return name.isEmpty() ? QByteArray("org.nt.app") : name;
}

ASensorManager *sensorManager() {
    static const QByteArray package = packageName();
    return ASensorManager_getInstanceForPackage(package.constData());
}

class AndroidSensorDevice : public SensorDevice {
public:
    AndroidSensorDevice(ASensorManager *manager, ASensorEventQueue *queue, ASensorRef sensor)
        : m_manager(manager), m_queue(queue), m_sensor(sensor), m_type(ASensor_getType(sensor)) {}
    ~AndroidSensorDevice() override {
        ASensorEventQueue_disableSensor(m_queue, m_sensor);
        ASensorManager_destroyEventQueue(m_manager, m_queue);
    }
    AndroidSensorDevice(const AndroidSensorDevice &) = delete;
    AndroidSensorDevice &operator=(const AndroidSensorDevice &) = delete;

    bool read(QList<qreal> &values, int *error) override {
        ASensorEvent events[16];
        bool have = false;
        ASensorEvent newest{};
        for (;;) {
            const ssize_t n = ASensorEventQueue_getEvents(m_queue, events, 16);
            if (n <= 0)
                break;
            newest = events[n - 1];
            have = true;
        }
        if (!have) {
            *error = -EAGAIN;
            return false;
        }
        const auto d = [&newest](int i) { return qreal(newest.data[i]); };
        switch (m_type) {
        case ASENSOR_TYPE_ACCELEROMETER:
            values = {d(0), d(1), d(2)};
            break;
        case ASENSOR_TYPE_MAGNETIC_FIELD:
            values = {d(0) * 1e-6, d(1) * 1e-6, d(2) * 1e-6, 1.0};
            break;
        case ASENSOR_TYPE_GYROSCOPE:
            values = {d(0) * kDegrees, d(1) * kDegrees, d(2) * kDegrees};
            break;
        case ASENSOR_TYPE_PRESSURE:
            values = {d(0) * 100.0, 0.0};
            break;
        case ASENSOR_TYPE_PROXIMITY:
            values = {newest.data[0] < kCloseCentimetres ? 1.0 : 0.0};
            break;
        case ASENSOR_TYPE_RELATIVE_HUMIDITY:
            values = {d(0), 0.0};
            break;
        default: // light in lux, ambient temperature in degrees Celsius
            values = {d(0)};
            break;
        }
        return true;
    }

private:
    ASensorManager *m_manager;
    ASensorEventQueue *m_queue;
    ASensorRef m_sensor;
    int m_type;
};

class AndroidSensorBackend : public SensorBackend {
public:
    QList<SensorInfo> sensors() override {
        QList<SensorInfo> list;
        ASensorManager *manager = sensorManager();
        if (!manager)
            return list;
        ASensorList all = nullptr;
        const int count = ASensorManager_getSensorList(manager, &all);
        for (int i = 0; i < count; ++i) {
            const char *type = qtType(ASensor_getType(all[i]));
            if (!type)
                continue;
            SensorInfo info;
            info.identifier = ASensor_getName(all[i]);
            info.type = type;
            info.description = QStringLiteral("%1 (%2)").arg(QString::fromUtf8(ASensor_getName(all[i])),
                                                             QString::fromUtf8(ASensor_getVendor(all[i])));
            info.isDefault = ASensorManager_getDefaultSensor(manager, ASensor_getType(all[i])) == all[i];
            const int minDelay = ASensor_getMinDelay(all[i]); // microseconds, 0 for sensors that report on change
            info.dataRates = {{1, minDelay > 0 ? std::max(1, 1000000 / minDelay) : 100}};
            list.append(info);
        }
        return list;
    }

    std::unique_ptr<SensorDevice> open(const QByteArray &identifier, const QByteArray &type, int *error) override {
        ASensorManager *manager = sensorManager();
        ASensorList all = nullptr;
        const int count = manager ? ASensorManager_getSensorList(manager, &all) : 0;
        ASensorRef sensor = nullptr;
        for (int i = 0; i < count && !sensor; ++i) {
            const char *qt = qtType(ASensor_getType(all[i]));
            if (qt && type == qt && identifier == ASensor_getName(all[i]))
                sensor = all[i];
        }
        if (!sensor) {
            *error = -ENODEV;
            return nullptr;
        }
        ALooper *looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
        ASensorEventQueue *queue = ASensorManager_createEventQueue(manager, looper, 0, nullptr, nullptr);
        if (!queue || ASensorEventQueue_enableSensor(queue, sensor) < 0) {
            if (queue)
                ASensorManager_destroyEventQueue(manager, queue);
            *error = -EIO;
            return nullptr;
        }
        ASensorEventQueue_setEventRate(queue, sensor, std::max(ASensor_getMinDelay(sensor), 10000));
        return std::make_unique<AndroidSensorDevice>(manager, queue, sensor);
    }
};

} // namespace

std::unique_ptr<SensorBackend> createPlatformSensorBackend() { return std::make_unique<AndroidSensorBackend>(); }

} // namespace QtSensorsPrivate
