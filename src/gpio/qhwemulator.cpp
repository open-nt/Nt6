// MIT License - see LICENSE
//
// The backends of QGpioLine, QSpiDevice and QI2cDevice that talk to a hardware emulator instead of
// the devices of the system (see qhwemulatorprotocol_p.h). Each object has a connection of its
// own, every request waits for its reply, and so the emulator sees the operations of a program in
// the order the program made them, whichever objects they went through.
#include "qhwemulator_p.h"
#include "qgpiobackend_p.h"
#include "qhwemulatorprotocol_p.h"
#include "qperipheral_p.h"

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace QtPrivate {

namespace HwEmulator {

QString emulatorSocketPath(const QString &value) {
    return value.startsWith(QChar('/')) ? value : QDir::tempPath() + QChar('/') + value;
}

} // namespace HwEmulator

namespace {

using namespace HwEmulator;

// How long a reply may take before the call fails with a timeout.
constexpr int ReplyTimeoutMs = 5000;

// One connection to the emulator.
class Connection {
public:
    Connection() = default;
    ~Connection() {
        if (m_fd >= 0)
            ::close(m_fd);
    }
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    int descriptor() const { return m_fd; }

    // Connects; returns 0 or an errno value.
    int connect() {
        const QByteArray path = QFile::encodeName(emulatorSocketPath(hwEmulatorName()));
        sockaddr_un address{};
        if (path.size() >= static_cast<qsizetype>(sizeof(address.sun_path)))
            return ENAMETOOLONG;
        address.sun_family = AF_UNIX;
        std::memcpy(address.sun_path, path.constData(), static_cast<std::size_t>(path.size()));
        m_fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (m_fd < 0)
            return errno;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the sockets API takes a sockaddr
        if (::connect(m_fd, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
            const int code = errno;
            ::close(m_fd);
            m_fd = -1;
            return code;
        }
        return 0;
    }

    // Sends a request and waits for its reply, keeping the levels pushed meanwhile. Returns 0 or
    // an errno value: the emulator's, or that of the connection.
    int request(MessageType type, const QByteArray &payload, QByteArray &replyData) {
        if (m_fd < 0)
            return ENOTCONN;
        const QByteArray out = frame(type, payload);
        qsizetype sent = 0;
        while (sent < out.size()) {
            const ssize_t n =
                ::send(m_fd, out.constData() + sent, static_cast<std::size_t>(out.size() - sent), MSG_NOSIGNAL);
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0)
                return n < 0 ? errno : EPIPE;
            sent += n;
        }
        for (;;) {
            quint8 t = 0;
            QByteArray p;
            bool bad = false;
            while (takeFrame(m_buffer, t, p, bad)) {
                if (t == GpioLevel && !p.isEmpty()) {
                    m_levels.push_back(u8At(p, 0) != 0);
                } else if (t == Reply && p.size() >= 4) {
                    replyData = p.mid(4);
                    return static_cast<int>(u32At(p, 0));
                }
            }
            if (bad)
                return EPROTO;
            const int code = fill(ReplyTimeoutMs);
            if (code)
                return code;
        }
    }

    // Reads what is there without waiting and takes the pushed levels out, in order. Returns 0 or
    // an errno value; ENODEV when the emulator went away.
    int drainLevels(std::vector<bool> &levels) {
        for (;;) {
            const int code = fill(0);
            if (code == EAGAIN)
                break;
            if (code)
                return code;
        }
        quint8 t = 0;
        QByteArray p;
        bool bad = false;
        while (takeFrame(m_buffer, t, p, bad)) {
            if (t == GpioLevel && !p.isEmpty())
                m_levels.push_back(u8At(p, 0) != 0);
        }
        if (bad)
            return EPROTO;
        levels.insert(levels.end(), m_levels.begin(), m_levels.end());
        m_levels.clear();
        return 0;
    }

private:
    // Reads once into the buffer, waiting up to timeoutMs. Returns 0, EAGAIN when nothing came,
    // ETIMEDOUT when waiting was asked for, ENODEV at the end of the stream, or an errno value.
    int fill(int timeoutMs) {
        pollfd p{m_fd, POLLIN, 0};
        int ready = 0;
        do {
            ready = ::poll(&p, 1, timeoutMs);
        } while (ready < 0 && errno == EINTR);
        if (ready < 0)
            return errno;
        if (ready == 0)
            return timeoutMs > 0 ? ETIMEDOUT : EAGAIN;
        char chunk[4096];
        ssize_t n = 0;
        do {
            n = ::recv(m_fd, chunk, sizeof chunk, MSG_DONTWAIT);
        } while (n < 0 && errno == EINTR);
        if (n < 0)
            return (errno == EAGAIN || errno == EWOULDBLOCK) ? EAGAIN : errno;
        if (n == 0)
            return ENODEV;
        m_buffer.append(chunk, static_cast<qsizetype>(n));
        return 0;
    }

    int m_fd = -1;
    QByteArray m_buffer;
    std::vector<bool> m_levels;
};

QString emulatorMessage(const QString &what, const QString &name) {
    return QStringLiteral("%1 %2 on the emulator %3").arg(what, name, emulatorSocketPath(hwEmulatorName()));
}

class EmulatorGpio final : public GpioBackend {
public:
    bool open(const GpioConfig &config, GpioFailure &failure) override {
        m_activeLow = config.activeLow;
        m_name = config.name;
        int code = m_connection.connect();
        QByteArray reply;
        if (!code) {
            QByteArray payload;
            appendU8(payload, config.direction == QGpioLine::Output ? 1 : 0);
            appendU8(payload, physical(config.initialValue) ? 1 : 0);
            appendU8(payload, static_cast<quint8>(config.bias));
            payload.append(config.name.toUtf8());
            code = m_connection.request(OpenGpio, payload, reply);
        }
        if (code) {
            setGpioFailure(failure, code, emulatorMessage(QStringLiteral("Cannot open the line"), m_name));
            return false;
        }
        m_level = !reply.isEmpty() && u8At(reply, 0) != 0;
        return true;
    }
    int eventDescriptor() const override { return m_connection.descriptor(); }
    bool readEvents(std::vector<bool> &levels, GpioFailure &failure) override {
        std::vector<bool> raw;
        if (const int code = m_connection.drainLevels(raw)) {
            setGpioFailure(failure, code, emulatorMessage(QStringLiteral("Lost the line"), m_name));
            return false;
        }
        levels.clear();
        for (const bool level : raw) {
            m_level = level;
            levels.push_back(physical(level));
        }
        return true;
    }
    bool readValue(bool &value, GpioFailure &failure) override {
        std::vector<bool> ignored;
        if (!readEvents(ignored, failure))
            return false;
        value = physical(m_level);
        return true;
    }
    bool writeValue(bool value, GpioFailure &failure) override {
        QByteArray payload;
        appendU8(payload, physical(value) ? 1 : 0);
        QByteArray reply;
        if (const int code = m_connection.request(GpioWrite, payload, reply)) {
            setGpioFailure(failure, code, emulatorMessage(QStringLiteral("Cannot set the line"), m_name));
            return false;
        }
        m_level = physical(value);
        return true;
    }

private:
    // Converts between logical and physical levels; the conversion is its own inverse.
    bool physical(bool level) const { return level != m_activeLow; }

    Connection m_connection;
    QString m_name;
    bool m_activeLow = false;
    bool m_level = false; // physical
};

QByteArray spiSettings(const SpiConfig &config) {
    QByteArray payload;
    appendU8(payload, static_cast<quint8>(config.mode | (config.lsbFirst ? 0x80 : 0)));
    appendU32(payload, config.speed);
    appendU8(payload, static_cast<quint8>(config.bitsPerWord));
    return payload;
}

class EmulatorSpi final : public SpiBackend {
public:
    bool open(const SpiConfig &config, HwFailure &failure) override {
        m_name = config.name;
        int code = m_connection.connect();
        QByteArray reply;
        if (!code)
            code = m_connection.request(OpenSpi, spiSettings(config) + config.name.toUtf8(), reply);
        if (code) {
            setHwFailure(failure, code, emulatorMessage(QStringLiteral("Cannot open"), m_name), true);
            return false;
        }
        return true;
    }
    bool configure(const SpiConfig &config, HwFailure &failure) override {
        QByteArray reply;
        if (const int code = m_connection.request(SpiConfigure, spiSettings(config), reply)) {
            setHwFailure(failure, code, emulatorMessage(QStringLiteral("Cannot configure"), m_name), false);
            return false;
        }
        return true;
    }
    bool transfer(const QByteArray &data, qsizetype readSize, bool duplex, QByteArray &read,
                  HwFailure &failure) override {
        QByteArray payload;
        appendU8(payload, duplex ? 1 : 0);
        appendU32(payload, static_cast<quint32>(readSize));
        payload.append(data);
        if (const int code = m_connection.request(SpiTransfer, payload, read)) {
            setHwFailure(failure, code, emulatorMessage(QStringLiteral("Transfer failed on"), m_name), false);
            return false;
        }
        const qsizetype expected = duplex ? data.size() : readSize;
        read = read.left(expected);
        read.append(QByteArray(expected - read.size(), '\0'));
        return true;
    }

private:
    Connection m_connection;
    QString m_name;
};

class EmulatorI2c final : public I2cBackend {
public:
    bool open(const QString &bus, HwFailure &failure) override {
        m_name = bus;
        int code = m_connection.connect();
        QByteArray reply;
        if (!code)
            code = m_connection.request(OpenI2c, bus.toUtf8(), reply);
        if (code) {
            setHwFailure(failure, code, emulatorMessage(QStringLiteral("Cannot open"), m_name), true);
            return false;
        }
        return true;
    }
    bool transfer(quint16 address, const QByteArray &data, qsizetype readSize, QByteArray &read,
                  HwFailure &failure) override {
        QByteArray payload;
        appendU16(payload, address);
        appendU32(payload, static_cast<quint32>(readSize));
        payload.append(data);
        if (const int code = m_connection.request(I2cTransfer, payload, read)) {
            setHwFailure(
                failure, code,
                emulatorMessage(QStringLiteral("Transfer to 0x%1 failed on").arg(address, 2, 16, QChar('0')), m_name),
                false);
            return false;
        }
        read = read.left(readSize);
        read.append(QByteArray(readSize - read.size(), '\0'));
        return true;
    }

private:
    Connection m_connection;
    QString m_name;
};

} // namespace

QString hwEmulatorName() {
    const char *value = std::getenv("NT_GPIO_EMULATOR");
    return value ? QString::fromLocal8Bit(value) : QString();
}

bool hwEmulatorEnabled() { return !hwEmulatorName().isEmpty(); }

std::unique_ptr<GpioBackend> createEmulatorGpioBackend() { return std::make_unique<EmulatorGpio>(); }
std::unique_ptr<SpiBackend> createEmulatorSpiBackend() { return std::make_unique<EmulatorSpi>(); }
std::unique_ptr<I2cBackend> createEmulatorI2cBackend() { return std::make_unique<EmulatorI2c>(); }

} // namespace QtPrivate
