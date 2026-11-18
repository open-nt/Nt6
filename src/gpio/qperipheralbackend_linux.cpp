// MIT License - see LICENSE
//
// QPwmChannel, QAnalogInput, QAnalogOutput, QI2cDevice and QSpiDevice on Linux. PWM goes through
// /sys/class/pwm and the converters through the sysfs files of Industrial I/O, both under
// sysfsRoot() so that the tests can stand in a tree of their own. I2C uses the I2C_RDWR ioctl of
// /dev/i2c-N, so a write and a read can share a transaction, and SPI the SPI_IOC_MESSAGE ioctl of
// /dev/spidevB.C.
#include "qperipheral_p.h"

#include <QtCore/qdir.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qthread.h>

#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/spi/spidev.h>

#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdint>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace QtPrivate {

namespace {

int openFile(const QString &path, int flags) {
    int fd = -1;
    do {
        fd = ::open(QFile::encodeName(path).constData(), flags | O_CLOEXEC);
    } while (fd < 0 && errno == EINTR);
    return fd;
}

// Writes data to the sysfs attribute at path in one write(). Returns 0 or the errno value.
int writeAttribute(const QString &path, const QByteArray &data) {
    const int fd = openFile(path, O_WRONLY | O_TRUNC); // as "echo > file" does; sysfs ignores the truncation
    if (fd < 0)
        return errno;
    ssize_t n = -1;
    do {
        n = ::write(fd, data.constData(), static_cast<std::size_t>(data.size()));
    } while (n < 0 && errno == EINTR);
    const int code = n < 0 ? errno : 0;
    ::close(fd);
    return code;
}

// Reads the sysfs attribute at path, without the trailing newline. Returns 0 or the errno value.
int readAttribute(const QString &path, QByteArray &value) {
    const int fd = openFile(path, O_RDONLY);
    if (fd < 0)
        return errno;
    char buffer[128];
    ssize_t n = -1;
    do {
        n = ::read(fd, buffer, sizeof buffer);
    } while (n < 0 && errno == EINTR);
    const int code = n < 0 ? errno : 0;
    ::close(fd);
    if (code == 0)
        value = QByteArray(buffer, static_cast<qsizetype>(n)).trimmed();
    return code;
}

bool readNumber(const QString &path, double &value) {
    QByteArray text;
    if (readAttribute(path, text) != 0)
        return false;
    bool ok = false;
    value = text.toDouble(&ok);
    return ok;
}

// "name:channel", split at the last colon, so that "iio:device0:2" keeps its device part whole.
bool splitName(const QString &name, QString &device, QString &channel) {
    const qsizetype colon = name.lastIndexOf(QLatin1Char(':'));
    if (colon <= 0 || colon == name.size() - 1)
        return false;
    device = name.left(colon);
    channel = name.mid(colon + 1);
    return true;
}

QString devicePath(const QString &name) {
    return name.startsWith(QLatin1Char('/')) ? name : QStringLiteral("/dev/") + name;
}

// --- PWM -----------------------------------------------------------------------------------------

class LinuxPwmBackend final : public PwmBackend {
public:
    LinuxPwmBackend() = default;
    ~LinuxPwmBackend() override {
        if (m_exported)
            writeAttribute(m_chip + QStringLiteral("/unexport"), QByteArray::number(m_channel));
    }
    LinuxPwmBackend(const LinuxPwmBackend &) = delete;
    LinuxPwmBackend &operator=(const LinuxPwmBackend &) = delete;

    bool open(const QString &name, HwFailure &failure) override {
        QString chip;
        QString channel;
        bool ok = false;
        if (splitName(name, chip, channel))
            m_channel = channel.toUInt(&ok);
        if (!ok) {
            setHwFailure(failure, HwError::DeviceNotFound, QStringLiteral("%1 is not pwmchipN:channel").arg(name));
            return false;
        }
        m_chip = chip.startsWith(QLatin1Char('/')) ? chip : sysfsRoot() + QStringLiteral("/class/pwm/") + chip;
        if (!QFileInfo(m_chip).isDir()) {
            setHwFailure(failure, HwError::DeviceNotFound, QStringLiteral("No PWM chip %1").arg(m_chip));
            return false;
        }
        double count = 0;
        if (readNumber(m_chip + QStringLiteral("/npwm"), count) && m_channel >= count) {
            setHwFailure(failure, HwError::DeviceNotFound,
                         QStringLiteral("%1 has no channel %2").arg(m_chip).arg(m_channel));
            return false;
        }
        m_dir = m_chip + QStringLiteral("/pwm") + QString::number(m_channel);
        if (!QFileInfo(m_dir).isDir()) {
            const int code = writeAttribute(m_chip + QStringLiteral("/export"), QByteArray::number(m_channel));
            if (code != 0) {
                setHwFailure(failure, code, QStringLiteral("Cannot export channel %1 of %2").arg(m_channel).arg(m_chip),
                             true);
                return false;
            }
            m_exported = true;
        }
        // udev may still be giving the new files to the group of the user.
        const QByteArray enable = QFile::encodeName(m_dir + QStringLiteral("/enable"));
        QElapsedTimer timer;
        timer.start();
        while (::access(enable.constData(), W_OK) != 0) {
            if (timer.elapsed() > 1000) {
                if (QFileInfo::exists(m_dir + QStringLiteral("/enable")))
                    setHwFailure(failure, EACCES, QStringLiteral("Cannot use %1").arg(m_dir), true);
                else
                    setHwFailure(failure, HwError::Timeout, QStringLiteral("%1 did not appear").arg(m_dir));
                return false;
            }
            QThread::msleep(10);
        }
        double value = 0;
        if (readNumber(m_dir + QStringLiteral("/period"), value))
            m_state.period = static_cast<qint64>(value);
        if (readNumber(m_dir + QStringLiteral("/duty_cycle"), value))
            m_state.pulseWidth = static_cast<qint64>(value);
        if (readNumber(m_dir + QStringLiteral("/enable"), value))
            m_state.enabled = value != 0;
        QByteArray polarity;
        if (readAttribute(m_dir + QStringLiteral("/polarity"), polarity) == 0)
            m_state.inversed = polarity == "inversed";
        return true;
    }

    bool apply(const PwmSettings &s, HwFailure &failure) override {
        if (s.enabled && s.period <= 0) {
            setHwFailure(failure, HwError::Unsupported, QStringLiteral("A PWM channel needs a period to be enabled"));
            return false;
        }
        // The polarity may only change while the channel is off.
        if (m_state.enabled && (!s.enabled || s.inversed != m_state.inversed)) {
            if (!write("enable", 0, failure))
                return false;
            m_state.enabled = false;
        }
        if (s.inversed != m_state.inversed) {
            const int code = writeAttribute(m_dir + QStringLiteral("/polarity"),
                                            s.inversed ? QByteArrayLiteral("inversed") : QByteArrayLiteral("normal"));
            if (code != 0) {
                setHwFailure(failure, code, QStringLiteral("Cannot set the polarity of %1").arg(m_dir), false);
                return false;
            }
            m_state.inversed = s.inversed;
        }
        // The duty cycle may never exceed the period: shrink before, grow after. A period of 0
        // means none has been set, which the kernel would refuse; the channel keeps its own.
        if (s.period > 0) {
            const bool dutyFirst = s.period < m_state.pulseWidth;
            if (dutyFirst && !writeDuty(s.pulseWidth, failure))
                return false;
            if (s.period != m_state.period) {
                if (!write("period", s.period, failure))
                    return false;
                m_state.period = s.period;
            }
            if (!dutyFirst && !writeDuty(s.pulseWidth, failure))
                return false;
        }
        if (s.enabled && !m_state.enabled) {
            if (!write("enable", 1, failure))
                return false;
            m_state.enabled = true;
        }
        return true;
    }

private:
    bool write(const char *attribute, qint64 value, HwFailure &failure) {
        const QString path = m_dir + QLatin1Char('/') + QLatin1String(attribute);
        const int code = writeAttribute(path, QByteArray::number(value));
        if (code != 0) {
            setHwFailure(failure, code, QStringLiteral("Cannot write %1 to %2").arg(value).arg(path), false);
            return false;
        }
        return true;
    }
    bool writeDuty(qint64 pulseWidth, HwFailure &failure) {
        if (pulseWidth == m_state.pulseWidth)
            return true;
        if (!write("duty_cycle", pulseWidth, failure))
            return false;
        m_state.pulseWidth = pulseWidth;
        return true;
    }

    QString m_chip;
    QString m_dir;
    uint m_channel = 0;
    bool m_exported = false;
    PwmSettings m_state; // what the channel is set to
};

// --- Industrial I/O ------------------------------------------------------------------------------

struct IioChannel {
    QString dir;  // the device directory
    QString stem; // "voltage2", "temp"
    QString type; // "voltage", "temp": the stem without its index, for shared attributes
};

// Resolves "iio:device0:2", "/sys/bus/iio/devices/iio:device0:2" or "ads1015:2".
bool resolveIio(const QString &name, IioChannel &found, HwFailure &failure) {
    QString device;
    QString channel;
    if (!splitName(name, device, channel)) {
        setHwFailure(failure, HwError::DeviceNotFound, QStringLiteral("%1 is not device:channel").arg(name));
        return false;
    }
    const QString root = sysfsRoot() + QStringLiteral("/bus/iio/devices");
    if (device.startsWith(QLatin1Char('/')))
        found.dir = device;
    else if (QFileInfo(root + QLatin1Char('/') + device).isDir())
        found.dir = root + QLatin1Char('/') + device;
    else {
        const QStringList entries = QDir(root).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System);
        for (const QString &entry : entries) {
            QByteArray text;
            if (readAttribute(root + QLatin1Char('/') + entry + QStringLiteral("/name"), text) == 0 &&
                QString::fromUtf8(text) == device) {
                found.dir = root + QLatin1Char('/') + entry;
                break;
            }
        }
    }
    if (found.dir.isEmpty() || !QFileInfo(found.dir).isDir()) {
        setHwFailure(failure, HwError::DeviceNotFound, QStringLiteral("No IIO device is called %1").arg(device));
        return false;
    }
    bool numeric = false;
    channel.toUInt(&numeric);
    found.stem = numeric ? QStringLiteral("voltage") + channel : channel;
    qsizetype end = 0;
    while (end < found.stem.size() && found.stem.at(end).isLetter())
        ++end;
    found.type = found.stem.left(end);
    return true;
}

// Reads the scale and offset of a channel: its own attribute, or the one shared by its type.
void readScale(const IioChannel &c, const QString &direction, double &scale, double &offset, bool &hasScale) {
    const QString prefix = c.dir + QLatin1Char('/') + direction + QLatin1Char('_');
    hasScale = readNumber(prefix + c.stem + QStringLiteral("_scale"), scale) ||
               readNumber(prefix + c.type + QStringLiteral("_scale"), scale);
    if (!readNumber(prefix + c.stem + QStringLiteral("_offset"), offset) &&
        !readNumber(prefix + c.type + QStringLiteral("_offset"), offset))
        offset = 0;
}

class LinuxAnalogInputBackend final : public AnalogInputBackend {
public:
    bool open(const AnalogConfig &config, HwFailure &failure) override {
        IioChannel channel;
        if (!resolveIio(config.name, channel, failure))
            return false;
        m_raw = channel.dir + QStringLiteral("/in_") + channel.stem + QStringLiteral("_raw");
        if (!QFileInfo::exists(m_raw)) {
            setHwFailure(failure, HwError::DeviceNotFound, QStringLiteral("No input channel %1").arg(m_raw));
            return false;
        }
        readScale(channel, QStringLiteral("in"), m_scale, m_offset, m_hasScale);
        return true;
    }

    bool read(qint32 &raw, double &volts, bool &hasVolts, HwFailure &failure) override {
        QByteArray text;
        const int code = readAttribute(m_raw, text);
        bool ok = code == 0;
        const qlonglong value = ok ? text.toLongLong(&ok) : 0;
        if (code != 0 || !ok || value < INT_MIN || value > INT_MAX) {
            if (code != 0)
                setHwFailure(failure, code, QStringLiteral("Cannot read %1").arg(m_raw), false);
            else
                setHwFailure(failure, HwError::Io, QStringLiteral("%1 holds no number").arg(m_raw));
            return false;
        }
        raw = static_cast<qint32>(value);
        hasVolts = m_hasScale;
        volts = (static_cast<double>(raw) + m_offset) * m_scale / 1000.0; // IIO voltages are in mV
        return true;
    }

private:
    QString m_raw;
    double m_scale = 1;
    double m_offset = 0;
    bool m_hasScale = false;
};

class LinuxAnalogOutputBackend final : public AnalogOutputBackend {
public:
    bool open(const AnalogConfig &config, HwFailure &failure) override {
        IioChannel channel;
        if (!resolveIio(config.name, channel, failure))
            return false;
        m_raw = channel.dir + QStringLiteral("/out_") + channel.stem + QStringLiteral("_raw");
        if (!QFileInfo::exists(m_raw)) {
            setHwFailure(failure, HwError::DeviceNotFound, QStringLiteral("No output channel %1").arg(m_raw));
            return false;
        }
        readScale(channel, QStringLiteral("out"), m_scale, m_offset, m_hasScale);
        return true;
    }

    bool write(qint32 raw, HwFailure &failure) override {
        const int code = writeAttribute(m_raw, QByteArray::number(raw));
        if (code != 0) {
            setHwFailure(failure, code, QStringLiteral("Cannot write %1 to %2").arg(raw).arg(m_raw), false);
            return false;
        }
        return true;
    }

    bool rawForVolts(double volts, qint32 &raw) const override {
        if (!m_hasScale || !(m_scale > 0))
            return false;
        const double value = std::round(volts * 1000.0 / m_scale - m_offset);
        if (value < 0 || value > INT_MAX)
            return false;
        raw = static_cast<qint32>(value);
        return true;
    }

private:
    QString m_raw;
    double m_scale = 1;
    double m_offset = 0;
    bool m_hasScale = false;
};

// --- I2C -----------------------------------------------------------------------------------------

class LinuxI2cBackend final : public I2cBackend {
public:
    LinuxI2cBackend() = default;
    ~LinuxI2cBackend() override {
        if (m_fd >= 0)
            ::close(m_fd);
    }
    LinuxI2cBackend(const LinuxI2cBackend &) = delete;
    LinuxI2cBackend &operator=(const LinuxI2cBackend &) = delete;

    bool open(const QString &bus, HwFailure &failure) override {
        m_path = devicePath(bus);
        m_fd = openFile(m_path, O_RDWR);
        if (m_fd < 0) {
            const int code = errno;
            setHwFailure(failure, code, QStringLiteral("Cannot open %1").arg(m_path), true);
            return false;
        }
        unsigned long functions = 0;
        if (::ioctl(m_fd, I2C_FUNCS, &functions) < 0) {
            const int code = errno;
            setHwFailure(failure, code, QStringLiteral("%1 is not an I2C bus").arg(m_path), true);
            return false;
        }
        if ((functions & I2C_FUNC_I2C) == 0) {
            setHwFailure(failure, HwError::Unsupported,
                         QStringLiteral("%1 only supports SMBus, not plain I2C transfers").arg(m_path));
            return false;
        }
        return true;
    }

    bool transfer(quint16 address, const QByteArray &data, qsizetype readSize, QByteArray &read,
                  HwFailure &failure) override {
        QByteArray out = data; // the kernel takes a non-const buffer
        i2c_msg messages[2] = {};
        __u32 count = 0;
        if (!out.isEmpty()) {
            messages[count].addr = address;
            messages[count].len = static_cast<__u16>(out.size());
            messages[count].buf = reinterpret_cast<__u8 *>(out.data());
            ++count;
        }
        if (readSize > 0) {
            read.resize(readSize);
            messages[count].addr = address;
            messages[count].flags = I2C_M_RD;
            messages[count].len = static_cast<__u16>(readSize);
            messages[count].buf = reinterpret_cast<__u8 *>(read.data());
            ++count;
        }
        i2c_rdwr_ioctl_data request{};
        request.msgs = messages;
        request.nmsgs = count;
        int result = -1;
        do {
            result = ::ioctl(m_fd, I2C_RDWR, &request);
        } while (result < 0 && errno == EINTR);
        if (result < 0) {
            const int code = errno;
            setHwFailure(
                failure, code,
                QStringLiteral("I2C transfer to 0x%1 on %2 failed").arg(address, 2, 16, QLatin1Char('0')).arg(m_path),
                false);
            return false;
        }
        return true;
    }

private:
    QString m_path;
    int m_fd = -1;
};

// --- SPI -----------------------------------------------------------------------------------------

class LinuxSpiBackend final : public SpiBackend {
public:
    LinuxSpiBackend() = default;
    ~LinuxSpiBackend() override {
        if (m_fd >= 0)
            ::close(m_fd);
    }
    LinuxSpiBackend(const LinuxSpiBackend &) = delete;
    LinuxSpiBackend &operator=(const LinuxSpiBackend &) = delete;

    bool open(const SpiConfig &config, HwFailure &failure) override {
        m_path = devicePath(config.name);
        m_fd = openFile(m_path, O_RDWR);
        if (m_fd < 0) {
            const int code = errno;
            setHwFailure(failure, code, QStringLiteral("Cannot open %1").arg(m_path), true);
            return false;
        }
        __u32 mode = 0;
        if (::ioctl(m_fd, SPI_IOC_RD_MODE32, &mode) < 0) {
            const int code = errno;
            setHwFailure(failure, code, QStringLiteral("%1 is not an SPI device").arg(m_path), true);
            return false;
        }
        return configure(config, failure);
    }

    bool configure(const SpiConfig &config, HwFailure &failure) override {
        // Keep the flags the devicetree set, such as an active-high chip select.
        __u32 mode = 0;
        if (::ioctl(m_fd, SPI_IOC_RD_MODE32, &mode) < 0) {
            const int code = errno;
            return fail(failure, code, QStringLiteral("%1 is not an SPI device").arg(m_path));
        }
        mode &= ~static_cast<__u32>(SPI_CPHA | SPI_CPOL | SPI_LSB_FIRST);
        mode |= static_cast<__u32>(config.mode & 3);
        if (config.lsbFirst)
            mode |= SPI_LSB_FIRST;
        if (::ioctl(m_fd, SPI_IOC_WR_MODE32, &mode) < 0) {
            const int code = errno;
            return fail(failure, code, QStringLiteral("Cannot set SPI mode %1 on %2").arg(config.mode).arg(m_path));
        }
        __u8 bits = static_cast<__u8>(config.bitsPerWord);
        if (::ioctl(m_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
            const int code = errno;
            return fail(failure, code,
                        QStringLiteral("Cannot use %1-bit words on %2").arg(config.bitsPerWord).arg(m_path));
        }
        __u32 speed = config.speed;
        if (::ioctl(m_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
            const int code = errno;
            return fail(failure, code, QStringLiteral("Cannot clock %1 at %2 Hz").arg(m_path).arg(config.speed));
        }
        m_speed = config.speed;
        m_bits = bits;
        return true;
    }

    bool transfer(const QByteArray &data, qsizetype readSize, bool duplex, QByteArray &read,
                  HwFailure &failure) override {
        if (data.size() > INT_MAX || readSize > INT_MAX) {
            setHwFailure(failure, HwError::Unsupported, QStringLiteral("The SPI transfer is too long"));
            return false;
        }
        QByteArray out = data;
        spi_ioc_transfer parts[2] = {};
        int count = 0;
        if (duplex) {
            read.resize(out.size());
            parts[0].tx_buf = reinterpret_cast<std::uintptr_t>(out.data());
            parts[0].rx_buf = reinterpret_cast<std::uintptr_t>(read.data());
            parts[0].len = static_cast<__u32>(out.size());
            count = 1;
        } else {
            if (!out.isEmpty()) {
                parts[count].tx_buf = reinterpret_cast<std::uintptr_t>(out.data());
                parts[count].len = static_cast<__u32>(out.size());
                ++count;
            }
            if (readSize > 0) {
                read = QByteArray(readSize, '\0');
                parts[count].rx_buf = reinterpret_cast<std::uintptr_t>(read.data());
                parts[count].len = static_cast<__u32>(readSize);
                ++count;
            }
        }
        for (int i = 0; i < count; ++i) {
            parts[i].speed_hz = m_speed;
            parts[i].bits_per_word = m_bits;
        }
        int result = -1;
        do {
            result = count == 1 ? ::ioctl(m_fd, SPI_IOC_MESSAGE(1), parts) : ::ioctl(m_fd, SPI_IOC_MESSAGE(2), parts);
        } while (result < 0 && errno == EINTR);
        if (result < 0) {
            const int code = errno;
            return fail(failure, code, QStringLiteral("SPI transfer on %1 failed").arg(m_path));
        }
        return true;
    }

private:
    static bool fail(HwFailure &failure, int code, const QString &what) {
        setHwFailure(failure, code, what, false);
        return false;
    }

    QString m_path;
    int m_fd = -1;
    quint32 m_speed = 0;
    __u8 m_bits = 8;
};

} // namespace

std::unique_ptr<PwmBackend> createPlatformPwmBackend() { return std::make_unique<LinuxPwmBackend>(); }
std::unique_ptr<AnalogInputBackend> createPlatformAnalogInputBackend() {
    return std::make_unique<LinuxAnalogInputBackend>();
}
std::unique_ptr<AnalogOutputBackend> createPlatformAnalogOutputBackend() {
    return std::make_unique<LinuxAnalogOutputBackend>();
}
std::unique_ptr<I2cBackend> createPlatformI2cBackend() { return std::make_unique<LinuxI2cBackend>(); }
std::unique_ptr<SpiBackend> createPlatformSpiBackend() { return std::make_unique<LinuxSpiBackend>(); }

} // namespace QtPrivate
