// MIT License - see LICENSE
//
// QGpioLine on Linux, through version 2 of the interface of the GPIO character device
// (<linux/gpio.h>, Linux 5.10 and later). The line is requested from /dev/gpiochipN and comes
// back as a descriptor of its own, which reports edges as gpio_v2_line_event records.
#include "qgpiobackend_p.h"

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>

#include <linux/gpio.h>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace QtPrivate {

namespace {

struct ChipLine {
    QString chip; // the path of the chip
    quint32 offset = 0;
};

int openChip(const QString &path) {
    int fd = -1;
    do {
        fd = ::open(QFile::encodeName(path).constData(), O_RDWR | O_CLOEXEC);
    } while (fd < 0 && errno == EINTR);
    return fd;
}

// Finds the line called name on any chip.
bool findLineByName(const QString &name, ChipLine &found, GpioFailure &failure) {
    const QStringList chips = QDir(QStringLiteral("/dev")).entryList({QStringLiteral("gpiochip*")}, QDir::System);
    int deniedCode = 0;
    QString denied;
    for (const QString &chip : chips) {
        const QString path = QStringLiteral("/dev/") + chip;
        const int fd = openChip(path);
        if (fd < 0) {
            if (errno == EACCES || errno == EPERM) {
                deniedCode = errno;
                denied = path;
            }
            continue;
        }
        gpiochip_info info{};
        if (::ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info) == 0) {
            for (quint32 offset = 0; offset < info.lines; ++offset) {
                gpio_v2_line_info line{};
                line.offset = offset;
                if (::ioctl(fd, GPIO_V2_GET_LINEINFO_IOCTL, &line) == 0 &&
                    name == QString::fromUtf8(line.name, static_cast<qsizetype>(::strnlen(line.name, sizeof line.name)))) {
                    ::close(fd);
                    found.chip = path;
                    found.offset = offset;
                    return true;
                }
            }
        }
        ::close(fd);
    }
    if (deniedCode != 0)
        setGpioFailure(failure, deniedCode, QStringLiteral("No GPIO line %1 was found; cannot open %2").arg(name, denied));
    else {
        failure.error = QGpioLine::DeviceNotFoundError;
        failure.message = QStringLiteral("No GPIO line is called %1").arg(name);
    }
    return false;
}

// Parses "gpiochip0:17", "/dev/gpiochip0:17" or a line name.
bool resolve(const QString &name, ChipLine &line, GpioFailure &failure) {
    const qsizetype colon = name.lastIndexOf(QLatin1Char(':'));
    if (colon < 0)
        return findLineByName(name, line, failure);
    bool ok = false;
    const uint offset = name.mid(colon + 1).toUInt(&ok);
    const QString chip = name.left(colon);
    if (!ok || chip.isEmpty()) {
        failure.error = QGpioLine::DeviceNotFoundError;
        failure.message = QStringLiteral("%1 is not a GPIO line name or chip:offset").arg(name);
        return false;
    }
    line.chip = chip.startsWith(QLatin1Char('/')) ? chip : QStringLiteral("/dev/") + chip;
    line.offset = offset;
    return true;
}

class LinuxGpioBackend final : public GpioBackend {
public:
    LinuxGpioBackend() = default;
    ~LinuxGpioBackend() override {
        if (m_fd >= 0)
            ::close(m_fd);
    }
    LinuxGpioBackend(const LinuxGpioBackend &) = delete;
    LinuxGpioBackend &operator=(const LinuxGpioBackend &) = delete;

    bool open(const GpioConfig &config, GpioFailure &failure) override {
        ChipLine line;
        if (!resolve(config.name, line, failure))
            return false;
        const int chip = openChip(line.chip);
        if (chip < 0) {
            setGpioFailure(failure, errno, QStringLiteral("Cannot open %1").arg(line.chip));
            return false;
        }

        gpio_v2_line_request request{};
        request.offsets[0] = line.offset;
        request.num_lines = 1;
        std::strncpy(request.consumer, "Nt", sizeof request.consumer - 1);
        quint64 flags = config.activeLow ? GPIO_V2_LINE_FLAG_ACTIVE_LOW : 0;
        switch (config.bias) {
        case QGpioLine::AsIs:
            break;
        case QGpioLine::Disabled:
            flags |= GPIO_V2_LINE_FLAG_BIAS_DISABLED;
            break;
        case QGpioLine::PullUp:
            flags |= GPIO_V2_LINE_FLAG_BIAS_PULL_UP;
            break;
        case QGpioLine::PullDown:
            flags |= GPIO_V2_LINE_FLAG_BIAS_PULL_DOWN;
            break;
        }
        const bool input = config.direction == QGpioLine::Input;
        if (input) {
            flags |= GPIO_V2_LINE_FLAG_INPUT;
        } else {
            flags |= GPIO_V2_LINE_FLAG_OUTPUT;
            request.config.num_attrs = 1;
            request.config.attrs[0].mask = 1;
            request.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
            request.config.attrs[0].attr.values = config.initialValue ? 1 : 0;
        }
        request.config.flags = input ? flags | GPIO_V2_LINE_FLAG_EDGE_RISING | GPIO_V2_LINE_FLAG_EDGE_FALLING : flags;
        int result = ::ioctl(chip, GPIO_V2_GET_LINE_IOCTL, &request);
        m_edges = input && result == 0;
        if (result < 0 && input && (errno == ENXIO || errno == EINVAL || errno == EOPNOTSUPP)) {
            // The chip cannot interrupt on this line: request it without edges, to be polled.
            request.config.flags = flags;
            result = ::ioctl(chip, GPIO_V2_GET_LINE_IOCTL, &request);
        }
        const int code = errno;
        ::close(chip);
        if (result < 0) {
            const QString what = QStringLiteral("Cannot request line %1 of %2").arg(line.offset).arg(line.chip);
            setGpioFailure(failure, code, what);
            if (code == EINVAL && config.bias != QGpioLine::AsIs) {
                failure.error = QGpioLine::UnsupportedOperationError;
                failure.message = what + QStringLiteral(": the bias is not supported");
            }
            return false;
        }
        m_fd = request.fd;
        const int fileFlags = ::fcntl(m_fd, F_GETFL);
        if (fileFlags < 0 || ::fcntl(m_fd, F_SETFL, fileFlags | O_NONBLOCK) < 0) {
            setGpioFailure(failure, errno, QStringLiteral("Cannot configure the GPIO line"));
            return false;
        }
        return true;
    }

    int eventDescriptor() const override { return m_edges ? m_fd : -1; }

    bool readEvents(std::vector<bool> &levels, GpioFailure &failure) override {
        gpio_v2_line_event events[16];
        for (;;) {
            const ssize_t n = ::read(m_fd, events, sizeof events);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    return true;
                setGpioFailure(failure, errno, QStringLiteral("Cannot read the GPIO line"));
                return false;
            }
            const std::size_t count = static_cast<std::size_t>(n) / sizeof events[0];
            for (std::size_t i = 0; i < count; ++i)
                levels.push_back(events[i].id == GPIO_V2_LINE_EVENT_RISING_EDGE);
            if (count < sizeof events / sizeof events[0])
                return true;
        }
    }

    bool readValue(bool &value, GpioFailure &failure) override {
        gpio_v2_line_values values{};
        values.mask = 1;
        if (::ioctl(m_fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &values) < 0) {
            setGpioFailure(failure, errno, QStringLiteral("Cannot read the GPIO line"));
            return false;
        }
        value = (values.bits & 1) != 0;
        return true;
    }

    bool writeValue(bool value, GpioFailure &failure) override {
        gpio_v2_line_values values{};
        values.mask = 1;
        values.bits = value ? 1 : 0;
        if (::ioctl(m_fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0) {
            setGpioFailure(failure, errno, QStringLiteral("Cannot set the GPIO line"));
            return false;
        }
        return true;
    }

private:
    int m_fd = -1;
    bool m_edges = false;
};

} // namespace

std::unique_ptr<GpioBackend> createPlatformGpioBackend() { return std::make_unique<LinuxGpioBackend>(); }

} // namespace QtPrivate
