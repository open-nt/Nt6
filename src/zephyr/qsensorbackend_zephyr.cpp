// MIT License - see LICENSE
//
// The sensor backend on Zephyr: its sensor drivers, read with sensor_sample_fetch() and sensor_channel_get() in the
// thread of the QSensor, each time the sensor is sampled. The sensors listed are the devices that the usual aliases of
// the devicetree name (accel0, gyro0, magn0, ambient-temp0, die-temp0, press0, humidity0, light0, and a second of
// some); any other sensor device can be opened by the name that device_get_binding() takes, set as the identifier.
// The values are converted to the units of Qt Sensors: gyroscopes from radians to degrees a second, magnetometers from
// gauss to tesla and pressure from kilopascals to pascals.
#include "qsensorbackend_p.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/util_macro.h>

#include <algorithm>
#include <cerrno>

namespace QtSensorsPrivate {

namespace {

constexpr double kPi = 3.14159265358979323846;

struct AliasSensor {
    const char *device; // the name device_get_binding() takes
    const char *type;
};

#define NT_SENSOR_ALIAS(alias, sensorType)                                                                             \
    COND_CODE_1(DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(alias)), ({DEVICE_DT_NAME(DT_ALIAS(alias)), sensorType}, ), ())

const AliasSensor kAliasSensors[] = {
    NT_SENSOR_ALIAS(accel0, "QAccelerometer") NT_SENSOR_ALIAS(accel1, "QAccelerometer")
    NT_SENSOR_ALIAS(gyro0, "QGyroscope") NT_SENSOR_ALIAS(gyro1, "QGyroscope")
    NT_SENSOR_ALIAS(magn0, "QMagnetometer") NT_SENSOR_ALIAS(magn1, "QMagnetometer")
    NT_SENSOR_ALIAS(press0, "QPressureSensor")
    NT_SENSOR_ALIAS(ambient_temp0, "QAmbientTemperatureSensor") NT_SENSOR_ALIAS(die_temp0, "QAmbientTemperatureSensor")
    NT_SENSOR_ALIAS(humidity0, "QHumiditySensor")
    NT_SENSOR_ALIAS(light0, "QLightSensor")
    {nullptr, nullptr},
};

class ZephyrSensorDevice : public SensorDevice {
public:
    ZephyrSensorDevice(const device *dev, QByteArray type) : m_device(dev), m_type(std::move(type)) {}

    bool read(QList<qreal> &values, int *error) override {
        const int fetched = sensor_sample_fetch(m_device);
        if (fetched < 0) {
            *error = fetched;
            return false;
        }
        if (m_type == "QAccelerometer")
            return channel(SENSOR_CHAN_ACCEL_XYZ, 3, 1.0, values, error);
        if (m_type == "QGyroscope")
            return channel(SENSOR_CHAN_GYRO_XYZ, 3, 180.0 / kPi, values, error);
        if (m_type == "QMagnetometer") {
            if (!channel(SENSOR_CHAN_MAGN_XYZ, 3, 1e-4, values, error))
                return false;
            values.append(1.0); // calibration level: not known
            return true;
        }
        if (m_type == "QPressureSensor") {
            if (!channel(SENSOR_CHAN_PRESS, 1, 1000.0, values, error))
                return false;
            int ignored = 0;
            if (!channel(SENSOR_CHAN_AMBIENT_TEMP, 1, 1.0, values, &ignored))
                values.append(0.0);
            return true;
        }
        if (m_type == "QAmbientTemperatureSensor") {
            int ignored = 0;
            return channel(SENSOR_CHAN_AMBIENT_TEMP, 1, 1.0, values, &ignored) ||
                   channel(SENSOR_CHAN_DIE_TEMP, 1, 1.0, values, error);
        }
        if (m_type == "QHumiditySensor") {
            if (!channel(SENSOR_CHAN_HUMIDITY, 1, 1.0, values, error))
                return false;
            values.append(0.0); // absolute humidity: not measured
            return true;
        }
        if (m_type == "QLightSensor")
            return channel(SENSOR_CHAN_LIGHT, 1, 1.0, values, error);
        *error = -ENOTSUP;
        return false;
    }

private:
    bool channel(sensor_channel chan, int count, double factor, QList<qreal> &values, int *error) {
        sensor_value raw[3] = {};
        const int result = sensor_channel_get(m_device, chan, raw);
        if (result < 0) {
            *error = result;
            return false;
        }
        for (int i = 0; i < count; ++i)
            values.append(sensor_value_to_double(&raw[i]) * factor);
        return true;
    }

    const device *m_device;
    QByteArray m_type;
};

class ZephyrSensorBackend : public SensorBackend {
public:
    QList<SensorInfo> sensors() override {
        QList<SensorInfo> list;
        for (const AliasSensor &alias : kAliasSensors) {
            if (!alias.device)
                continue;
            const device *dev = device_get_binding(alias.device);
            if (!dev || !device_is_ready(dev))
                continue;
            SensorInfo info;
            info.identifier = alias.device;
            info.type = alias.type;
            info.description = QString::fromUtf8(alias.device);
            info.isDefault = std::none_of(list.cbegin(), list.cend(),
                                          [&](const SensorInfo &other) { return other.type == info.type; });
            list.append(info);
        }
        return list;
    }

    std::unique_ptr<SensorDevice> open(const QByteArray &identifier, const QByteArray &type, int *error) override {
        const device *dev = device_get_binding(identifier.constData());
        if (!dev || !device_is_ready(dev)) {
            *error = -ENODEV;
            return nullptr;
        }
        return std::make_unique<ZephyrSensorDevice>(dev, type);
    }
};

} // namespace

std::unique_ptr<SensorBackend> createPlatformSensorBackend() { return std::make_unique<ZephyrSensorBackend>(); }

} // namespace QtSensorsPrivate
