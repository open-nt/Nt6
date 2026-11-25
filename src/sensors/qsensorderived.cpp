// MIT License - see LICENSE
//
// The sensors that Nt computes from others, on any backend: orientation, tilt and rotation from an accelerometer, a
// compass from an accelerometer and a magnetometer, an altimeter from a pressure sensor and an ambient light level from
// a light sensor. A derived sensor is listed under the identifier of the sensor it reads, for each type that the
// backend has no sensor of itself; a compass and the z of a rotation need a magnetometer as well, the default one.
#include "qsensorbackend_p.h"

#include <QtSensors/qambientlightsensor.h>
#include <QtSensors/qorientationsensor.h>

#include <algorithm>
#include <cerrno>
#include <cmath>

namespace QtSensorsPrivate {

namespace {

constexpr qreal kDegrees = 180.0 / 3.14159265358979323846;

enum class Kind { Orientation, Tilt, Rotation, Compass, Altimeter, AmbientLight };

struct Derived {
    const char *type;
    const char *source;
    Kind kind;
};

const Derived kDerived[] = {
    {"QOrientationSensor", "QAccelerometer", Kind::Orientation},
    {"QTiltSensor", "QAccelerometer", Kind::Tilt},
    {"QRotationSensor", "QAccelerometer", Kind::Rotation},
    {"QCompass", "QAccelerometer", Kind::Compass},
    {"QAltimeter", "QPressureSensor", Kind::Altimeter},
    {"QAmbientLightSensor", "QLightSensor", Kind::AmbientLight},
};

const Derived *derivedOf(const QByteArray &type) {
    for (const Derived &d : kDerived)
        if (type == d.type)
            return &d;
    return nullptr;
}

} // namespace

// The heading of the top of the device (its y axis) from magnetic north, in degrees clockwise from 0 to 360, from
// gravity a (as an accelerometer measures it, pointing up) and the magnetic field m in the frame of the device.
qreal compassAzimuth(qreal ax, qreal ay, qreal az, qreal mx, qreal my, qreal mz) {
    // East is m x a and north is a x east; the heading is the angle of the y axis between them.
    qreal ex = my * az - mz * ay;
    qreal ey = mz * ax - mx * az;
    qreal ez = mx * ay - my * ax;
    const qreal e = std::sqrt(ex * ex + ey * ey + ez * ez);
    const qreal a = std::sqrt(ax * ax + ay * ay + az * az);
    if (e <= 0 || a <= 0)
        return 0;
    ex /= e;
    ey /= e;
    ez /= e;
    const qreal ny = (az * ex - ax * ez) / a;
    qreal azimuth = std::atan2(ey, ny) * kDegrees;
    if (azimuth < 0)
        azimuth += 360;
    return azimuth;
}

// The orientation for gravity (x, y, z), or previous when no axis carries more than 70 percent of it.
int orientationFor(qreal x, qreal y, qreal z, int previous) {
    const qreal g = std::sqrt(x * x + y * y + z * z);
    if (g < 1.0)
        return previous;
    const qreal limit = 0.7 * g;
    if (y > limit)
        return QOrientationReading::TopUp;
    if (y < -limit)
        return QOrientationReading::TopDown;
    if (x > limit)
        return QOrientationReading::RightUp;
    if (x < -limit)
        return QOrientationReading::LeftUp;
    if (z > limit)
        return QOrientationReading::FaceUp;
    if (z < -limit)
        return QOrientationReading::FaceDown;
    return previous;
}

// The altitude in metres for pressure in pascals, in the standard atmosphere.
qreal altitudeFor(qreal pascals) {
    if (pascals <= 0)
        return 0;
    return 44330.0 * (1.0 - std::pow(pascals / 101325.0, 1.0 / 5.255));
}

int lightLevelFor(qreal lux) {
    if (lux < 0)
        return QAmbientLightReading::Undefined;
    if (lux < 10)
        return QAmbientLightReading::Dark;
    if (lux < 50)
        return QAmbientLightReading::Twilight;
    if (lux < 500)
        return QAmbientLightReading::Light;
    if (lux < 10000)
        return QAmbientLightReading::Bright;
    return QAmbientLightReading::Sunny;
}

namespace {

class DerivedDevice : public SensorDevice {
public:
    DerivedDevice(Kind kind, std::unique_ptr<SensorDevice> source, std::unique_ptr<SensorDevice> magnetometer)
        : m_kind(kind), m_source(std::move(source)), m_magnetometer(std::move(magnetometer)) {}

    bool read(QList<qreal> &values, int *error) override {
        QList<qreal> in;
        if (!m_source->read(in, error))
            return false;
        QList<qreal> magnetic;
        if (m_magnetometer && !m_magnetometer->read(magnetic, error))
            return false;
        const auto at = [&in](qsizetype i) { return i < in.size() ? in.at(i) : qreal(0); };
        switch (m_kind) {
        case Kind::Orientation:
            m_orientation = orientationFor(at(0), at(1), at(2), m_orientation);
            values = {qreal(m_orientation)};
            return true;
        case Kind::Tilt: {
            const qreal x = at(0), y = at(1), z = at(2);
            // yRotation, then xRotation, in the order of the properties of QTiltReading.
            values = {std::atan2(x, std::sqrt(y * y + z * z)) * kDegrees,
                      std::atan2(y, std::sqrt(x * x + z * z)) * kDegrees};
            return true;
        }
        case Kind::Rotation: {
            const qreal x = at(0), y = at(1), z = at(2);
            const qreal rx = std::atan2(y, std::sqrt(x * x + z * z)) * kDegrees;
            qreal ry = std::atan2(x, std::sqrt(y * y + z * z)) * kDegrees;
            if (z < 0) // face down: the roll goes on past 90 degrees
                ry = ry >= 0 ? 180 - ry : -180 - ry;
            qreal rz = 0;
            if (magnetic.size() >= 3) {
                rz = -compassAzimuth(x, y, z, magnetic[0], magnetic[1], magnetic[2]);
                if (rz <= -180)
                    rz += 360;
            }
            values = {rx, ry, rz};
            return true;
        }
        case Kind::Compass:
            if (magnetic.size() < 3) {
                *error = -ENODEV;
                return false;
            }
            values = {compassAzimuth(at(0), at(1), at(2), magnetic[0], magnetic[1], magnetic[2]), 1.0};
            return true;
        case Kind::Altimeter:
            values = {altitudeFor(at(0))};
            return true;
        case Kind::AmbientLight:
            values = {qreal(lightLevelFor(at(0)))};
            return true;
        }
        *error = -ENOTSUP;
        return false;
    }

private:
    Kind m_kind;
    std::unique_ptr<SensorDevice> m_source;
    std::unique_ptr<SensorDevice> m_magnetometer;
    int m_orientation = QOrientationReading::Undefined;
};

class DerivedBackend : public SensorBackend {
public:
    explicit DerivedBackend(std::unique_ptr<SensorBackend> base) : m_base(std::move(base)) {}

    QList<SensorInfo> sensors() override {
        QList<SensorInfo> list = m_base->sensors();
        const QList<SensorInfo> base = list;
        const auto has = [&base](const QByteArray &type) {
            return std::any_of(base.cbegin(), base.cend(), [&](const SensorInfo &i) { return i.type == type; });
        };
        const bool magnetometer = has("QMagnetometer");
        for (const Derived &d : kDerived) {
            if (has(d.type) || (d.kind == Kind::Compass && !magnetometer))
                continue;
            for (const SensorInfo &source : base) {
                if (source.type != d.source)
                    continue;
                SensorInfo info = source;
                info.type = d.type;
                info.outputRanges.clear();
                list.append(info);
            }
        }
        return list;
    }

    std::unique_ptr<SensorDevice> open(const QByteArray &identifier, const QByteArray &type, int *error) override {
        const Derived *d = derivedOf(type);
        if (!d)
            return m_base->open(identifier, type, error);
        // A backend that has the type itself reads it.
        const QList<SensorInfo> base = m_base->sensors();
        for (const SensorInfo &info : base)
            if (info.type == type && info.identifier == identifier)
                return m_base->open(identifier, type, error);
        std::unique_ptr<SensorDevice> source = m_base->open(identifier, d->source, error);
        if (!source)
            return nullptr;
        std::unique_ptr<SensorDevice> magnetometer;
        if (d->kind == Kind::Compass || d->kind == Kind::Rotation) {
            for (const SensorInfo &info : base) {
                if (info.type == "QMagnetometer" && (info.isDefault || !magnetometer)) {
                    int ignored = 0;
                    if (auto m = m_base->open(info.identifier, "QMagnetometer", &ignored))
                        magnetometer = std::move(m);
                    if (info.isDefault)
                        break;
                }
            }
            if (d->kind == Kind::Compass && !magnetometer) {
                *error = -ENODEV;
                return nullptr;
            }
        }
        return std::make_unique<DerivedDevice>(d->kind, std::move(source), std::move(magnetometer));
    }

private:
    std::unique_ptr<SensorBackend> m_base;
};

} // namespace

std::unique_ptr<SensorBackend> createDerivedSensorBackend(std::unique_ptr<SensorBackend> base) {
    return std::make_unique<DerivedBackend>(std::move(base));
}

} // namespace QtSensorsPrivate
