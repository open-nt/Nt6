// MIT License - see LICENSE
//
// QWatchdog on the watchdog devices of Linux (/dev/watchdog, the ioctls of <linux/watchdog.h>), and QFirmwareUpdate
// writing an image to a file or partition. Booting the image is left to the system.
#include "qdevice_p.h"

#include <fcntl.h>
#include <linux/watchdog.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>

namespace QtDevicePrivate {

namespace {

QString devicePath(const QString &name) {
    if (name.isEmpty())
        return QStringLiteral("/dev/watchdog");
    return name.startsWith(QChar(u'/')) ? name : QStringLiteral("/dev/") + name;
}

class LinuxWatchdogBackend final : public WatchdogBackend {
public:
    ~LinuxWatchdogBackend() override {
        if (m_fd >= 0)
            ::close(m_fd); // without the magic character, so a driver that honours it keeps running
    }

    bool start(WatchdogConfig &config, Failure &failure) override {
        const QString path = devicePath(config.name);
        m_fd = ::open(path.toUtf8().constData(), O_WRONLY | O_CLOEXEC);
        if (m_fd < 0) {
            setFailure(failure, errno, path);
            return false;
        }
        int seconds = (config.timeout + 999) / 1000;
        if (::ioctl(m_fd, WDIOC_SETTIMEOUT, &seconds) != 0) {
            // Some drivers have a fixed timeout; read what it is instead.
            if (::ioctl(m_fd, WDIOC_GETTIMEOUT, &seconds) != 0)
                seconds = 0;
        }
        if (seconds > 0)
            config.timeout = seconds * 1000;
        int options = WDIOS_ENABLECARD;
        ::ioctl(m_fd, WDIOC_SETOPTIONS, &options); // not every driver has it; opening already armed the watchdog
        return true;
    }

    bool feed(Failure &failure) override {
        if (::ioctl(m_fd, WDIOC_KEEPALIVE, 0) == 0)
            return true;
        if (::write(m_fd, "\0", 1) == 1) // the drivers without the ioctl count any write
            return true;
        setFailure(failure, errno, QStringLiteral("Feeding the watchdog"));
        return false;
    }

    bool stop(Failure &failure) override {
        watchdog_info info = {};
        const bool magic = ::ioctl(m_fd, WDIOC_GETSUPPORT, &info) == 0 && (info.options & WDIOF_MAGICCLOSE);
        if (!magic) {
            setFailure(failure, Error::Unsupported, QStringLiteral("The watchdog driver cannot be stopped"));
            return false;
        }
        if (::write(m_fd, "V", 1) != 1) {
            setFailure(failure, errno, QStringLiteral("Stopping the watchdog"));
            return false;
        }
        ::close(m_fd);
        m_fd = -1;
        return true;
    }

    bool causedLastReset(const QString &name) override {
        // WDIOC_GETBOOTSTATUS needs the device open, and opening it arms the watchdog: only ask an open one.
        if (m_fd < 0) {
            Q_UNUSED(name);
            return false;
        }
        int status = 0;
        return ::ioctl(m_fd, WDIOC_GETBOOTSTATUS, &status) == 0 && (status & WDIOF_CARDRESET);
    }

private:
    int m_fd = -1;
};

class LinuxFirmwareBackend final : public FirmwareBackend {
public:
    ~LinuxFirmwareBackend() override { abort(); }

    bool begin(const QString &target, qint64 size, Failure &failure) override {
        Q_UNUSED(size);
        if (target.isEmpty()) {
            setFailure(failure, Error::NotOpen, QStringLiteral("No target was set for the image"));
            return false;
        }
        m_path = target;
        m_fd = ::open(target.toUtf8().constData(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
        if (m_fd < 0) {
            setFailure(failure, errno, target);
            return false;
        }
        return true;
    }

    bool write(const char *data, qsizetype size, Failure &failure) override {
        while (size > 0) {
            const ssize_t n = ::write(m_fd, data, static_cast<size_t>(size));
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                setFailure(failure, errno, m_path);
                return false;
            }
            data += n;
            size -= n;
        }
        return true;
    }

    bool finish(QByteArray &header, Failure &failure) override {
        if (::fsync(m_fd) != 0 && errno != EINVAL) {
            setFailure(failure, errno, m_path);
            return false;
        }
        ::close(m_fd);
        m_fd = -1;
        const int in = ::open(m_path.toUtf8().constData(), O_RDONLY | O_CLOEXEC);
        if (in < 0) {
            setFailure(failure, errno, m_path);
            return false;
        }
        header.resize(kImageHeaderSize);
        const ssize_t n = ::read(in, header.data(), static_cast<size_t>(kImageHeaderSize));
        ::close(in);
        header.resize(n > 0 ? static_cast<qsizetype>(n) : 0);
        return true;
    }

    void abort() override {
        if (m_fd >= 0)
            ::close(m_fd);
        m_fd = -1;
    }

    bool requestUpgrade(bool, Failure &failure) override { return unsupported(failure); }
    bool isConfirmed() override { return true; }
    bool confirm(Failure &failure) override { return unsupported(failure); }
    QByteArray runningHeader() override { return QByteArray(); }
    bool reboot(Failure &failure) override { return unsupported(failure); }

private:
    static bool unsupported(Failure &failure) {
        setFailure(failure, Error::Unsupported, QStringLiteral("Booting an image is left to the system on Linux"));
        return false;
    }

    QString m_path;
    int m_fd = -1;
};

} // namespace

std::unique_ptr<WatchdogBackend> createPlatformWatchdogBackend() { return std::make_unique<LinuxWatchdogBackend>(); }
std::unique_ptr<FirmwareBackend> createPlatformFirmwareBackend() { return std::make_unique<LinuxFirmwareBackend>(); }

} // namespace QtDevicePrivate
