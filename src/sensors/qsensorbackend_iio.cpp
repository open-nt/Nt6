// MIT License - see LICENSE
//
// The sensor backend on Linux: the Industrial I/O devices of the kernel, under /sys/bus/iio/devices. Each device
// directory (iio:device0) has a name and a file for each channel: in_accel_x_input with the value in the standard
// unit, or in_accel_x_raw with in_accel_x_scale (or in_accel_scale) and an optional offset, giving
// (raw + offset) * scale. A device is listed once for each kind of channel it has, so a combined sensor such as a
// BME280 is a pressure, a temperature and a humidity sensor. The values are read from sysfs each time the sensor is
// sampled, which suits rates up to a few hundred a second; the buffered interface of IIO is not used.
//
// The lid of a laptop is a QLidSensor, read from the ACPI button under /proc/acpi/button/lid (LID0/state holds
// "state:      open" or "closed").

#include "qsensorbackend_p.h"

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>

#include <cerrno>
#include <cmath>
#include <optional>

namespace QtSensorsPrivate {

namespace {

constexpr qreal kPi = 3.14159265358979323846;

// One kind of IIO channel and how its standard unit becomes the unit of Qt Sensors.
struct ChannelKind {
    const char *type;     // the QSensor type
    const char *prefixes; // channel prefixes tried in order, separated by '|'
    const char *axes;     // "xyz" for three axes, "" for one value
    qreal factor;         // from the IIO unit to the Qt Sensors unit
};

const ChannelKind kKinds[] = {
    {"QAccelerometer", "in_accel", "xyz", 1.0},                          // m/s^2
    {"QGyroscope", "in_anglvel", "xyz", 180.0 / kPi},                    // rad/s to degrees/s
    {"QMagnetometer", "in_magn", "xyz", 1e-4},                           // gauss to tesla
    {"QPressureSensor", "in_pressure", "", 1000.0},                      // kPa to Pa
    {"QAmbientTemperatureSensor", "in_temp_ambient|in_temp", "", 0.001}, // milli degrees Celsius
    {"QHumiditySensor", "in_humidityrelative", "", 0.001},               // milli percent
    {"QLightSensor", "in_illuminance|in_illuminance0", "", 1.0},         // lux
    {"QIRProximitySensor", "in_proximity|in_proximity0", "", 1.0},       // as the driver scales it
};

std::optional<qreal> readNumber(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    bool ok = false;
    const qreal value = file.readAll().trimmed().toDouble(&ok);
    if (!ok)
        return std::nullopt;
    return value;
}

// The value of channel (such as in_accel_x) of the device in dir, in the IIO unit: the _input file, or
// (_raw + offset) * scale, where scale and offset may be shared by the channels of a prefix (in_accel_scale).
std::optional<qreal> readChannel(const QString &dir, const QString &channel, const QString &prefix) {
    if (const auto input = readNumber(dir + QLatin1Char('/') + channel + QLatin1String("_input")))
        return input;
    const auto raw = readNumber(dir + QLatin1Char('/') + channel + QLatin1String("_raw"));
    if (!raw)
        return std::nullopt;
    auto attribute = [&](const char *name, qreal fallback) {
        if (const auto own = readNumber(dir + QLatin1Char('/') + channel + QLatin1Char('_') + QLatin1String(name)))
            return *own;
        if (const auto shared = readNumber(dir + QLatin1Char('/') + prefix + QLatin1Char('_') + QLatin1String(name)))
            return *shared;
        return fallback;
    };
    return (*raw + attribute("offset", 0.0)) * attribute("scale", 1.0);
}

// The channels of kind on the device in dir: the prefix that it has, or an empty string.
QString findPrefix(const QString &dir, const ChannelKind &kind) {
    const QString axes = QString::fromLatin1(kind.axes);
    for (const QString &prefix : QString::fromLatin1(kind.prefixes).split(QLatin1Char('|'))) {
        const QString first = axes.isEmpty() ? prefix : prefix + QLatin1Char('_') + axes.at(0);
        if (readChannel(dir, first, prefix))
            return prefix;
    }
    return QString();
}

const ChannelKind *kindOf(const QByteArray &type) {
    for (const ChannelKind &kind : kKinds)
        if (type == kind.type)
            return &kind;
    return nullptr;
}

class IioDevice : public SensorDevice {
public:
    IioDevice(QString dir, QString prefix, const ChannelKind &kind)
        : m_dir(std::move(dir)), m_prefix(std::move(prefix)), m_kind(kind) {
        if (QByteArray(kind.type) == "QPressureSensor")
            m_temperaturePrefix = findPrefix(m_dir, *kindOf("QAmbientTemperatureSensor"));
    }

    bool read(QList<qreal> &values, int *error) override {
        const QString axes = QString::fromLatin1(m_kind.axes);
        if (axes.isEmpty()) {
            const auto value = readChannel(m_dir, m_prefix, m_prefix);
            if (!value)
                return fail(error);
            values.append(*value * m_kind.factor);
        } else {
            for (const QChar axis : axes) {
                const auto value = readChannel(m_dir, m_prefix + QLatin1Char('_') + axis, m_prefix);
                if (!value)
                    return fail(error);
                values.append(*value * m_kind.factor);
            }
        }
        const QByteArray type(m_kind.type);
        if (type == "QMagnetometer") {
            values.append(1.0); // calibration level: not known
        } else if (type == "QPressureSensor") {
            const auto temperature = m_temperaturePrefix.isEmpty()
                                         ? std::nullopt
                                         : readChannel(m_dir, m_temperaturePrefix, m_temperaturePrefix);
            values.append(temperature ? *temperature * 0.001 : 0.0);
        } else if (type == "QHumiditySensor") {
            values.append(0.0); // absolute humidity: not measured
        }
        return true;
    }

private:
    static bool fail(int *error) {
        *error = -EIO;
        return false;
    }

    QString m_dir;
    QString m_prefix;
    QString m_temperaturePrefix;
    const ChannelKind &m_kind;
};

// The lid of a laptop: values are backLidClosed and frontLidClosed.
class LidDevice : public SensorDevice {
public:
    explicit LidDevice(QString stateFile) : m_stateFile(std::move(stateFile)) {}

    bool read(QList<qreal> &values, int *error) override {
        QFile file(m_stateFile);
        if (!file.open(QIODevice::ReadOnly)) {
            *error = -EIO;
            return false;
        }
        const QByteArray state = file.readAll();
        values = {state.contains("closed") ? 1.0 : 0.0, 0.0};
        return true;
    }

private:
    QString m_stateFile;
};

class IioBackend : public SensorBackend {
public:
    IioBackend(QString root, QString lidRoot) : m_root(std::move(root)), m_lidRoot(std::move(lidRoot)) {}

    QList<SensorInfo> sensors() override {
        QList<SensorInfo> list = lids();
        QDir root(m_root);
        const QStringList devices =
            root.entryList({QStringLiteral("iio:device*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const ChannelKind &kind : kKinds) {
            bool first = true;
            for (const QString &device : devices) {
                const QString dir = root.filePath(device);
                if (findPrefix(dir, kind).isEmpty())
                    continue;
                SensorInfo info;
                info.identifier = device.toUtf8();
                info.type = kind.type;
                QFile name(dir + QLatin1String("/name"));
                if (name.open(QIODevice::ReadOnly))
                    info.description = QString::fromUtf8(name.readAll().trimmed());
                info.isDefault = first;
                first = false;
                list.append(info);
            }
        }
        return list;
    }

    std::unique_ptr<SensorDevice> open(const QByteArray &identifier, const QByteArray &type, int *error) override {
        if (type == "QLidSensor") {
            for (const SensorInfo &lid : lids()) {
                if (lid.identifier == identifier)
                    return std::make_unique<LidDevice>(QDir(m_lidRoot).filePath(QString::fromUtf8(identifier)) +
                                                       QLatin1String("/state"));
            }
            *error = -ENODEV;
            return nullptr;
        }
        const ChannelKind *kind = kindOf(type);
        if (!kind || identifier.contains('/')) {
            *error = -ENODEV;
            return nullptr;
        }
        const QString dir = QDir(m_root).filePath(QString::fromUtf8(identifier));
        const QString prefix = findPrefix(dir, *kind);
        if (prefix.isEmpty()) {
            *error = -ENODEV;
            return nullptr;
        }
        return std::make_unique<IioDevice>(dir, prefix, *kind);
    }

private:
    QList<SensorInfo> lids() const {
        QList<SensorInfo> list;
        if (m_lidRoot.isEmpty())
            return list;
        const QDir root(m_lidRoot);
        for (const QString &name : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            if (!QFile::exists(root.filePath(name) + QLatin1String("/state")))
                continue;
            SensorInfo info;
            info.identifier = name.toUtf8();
            info.type = "QLidSensor";
            info.description = QStringLiteral("ACPI lid %1").arg(name);
            info.isDefault = list.isEmpty();
            list.append(info);
        }
        return list;
    }

    QString m_root;
    QString m_lidRoot;
};

} // namespace

std::unique_ptr<SensorBackend> createIioSensorBackend(const QString &root, const QString &lidRoot) {
    return std::make_unique<IioBackend>(root, lidRoot);
}

std::unique_ptr<SensorBackend> createPlatformSensorBackend() {
    return createIioSensorBackend(QStringLiteral("/sys/bus/iio/devices"), QStringLiteral("/proc/acpi/button/lid"));
}

} // namespace QtSensorsPrivate
