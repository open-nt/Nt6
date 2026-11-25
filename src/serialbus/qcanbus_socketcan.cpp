// MIT License - see LICENSE
//
// The socketcan backend: a raw CAN socket of Linux on an interface such as can0 or vcan0, watched with a
// QSocketNotifier. The configuration is applied with the options of the socket: CAN_RAW_LOOPBACK,
// CAN_RAW_RECV_OWN_MSGS, CAN_RAW_FD_FRAMES, CAN_RAW_FILTER and CAN_RAW_ERR_FILTER. The bit rate is a setting of the
// interface, which needs CAP_NET_ADMIN; it is set with "ip link set can0 type can bitrate 500000", not here.
#include <QtSerialBus/qcanbusdevice.h>
#include <QtSerialBus/qcanbusfactory.h>

#include "qcanbus_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qsocketnotifier.h>

#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <vector>

namespace QtSerialBusPrivate {

namespace {

constexpr int kArphrdCan = 280; // ARPHRD_CAN, the type of a CAN interface in /sys/class/net/<name>/type

QString readSysfs(const QString &interfaceName, const char *file) {
    QFile f(QStringLiteral("/sys/class/net/%1/%2").arg(interfaceName, QLatin1String(file)));
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromLatin1(f.readAll().trimmed());
}

class SocketCanDevice : public QCanBusDevice {
public:
    explicit SocketCanDevice(QString name) : m_name(std::move(name)) {
        // The defaults of SocketCAN: frames reach the other sockets of the machine, error frames are received. The base
        // class stores them; there is no socket yet to apply them to.
        QCanBusDevice::setConfigurationParameter(LoopbackKey, true);
        QCanBusDevice::setConfigurationParameter(ReceiveOwnKey, false);
        QCanBusDevice::setConfigurationParameter(
            ErrorFilterKey, QVariant::fromValue(QCanBusFrame::FrameErrors(QCanBusFrame::AnyError)));
    }
    ~SocketCanDevice() override { closeSocket(); }
    SocketCanDevice(const SocketCanDevice &) = delete;
    SocketCanDevice &operator=(const SocketCanDevice &) = delete;

    void setConfigurationParameter(ConfigurationKey key, const QVariant &value) override {
        QCanBusDevice::setConfigurationParameter(key, value);
        if (m_socket >= 0 && key != BitRateKey && key != DataBitRateKey && !apply(key))
            setError(QStringLiteral("cannot apply configuration parameter %1").arg(int(key)), ConfigurationError);
    }

    bool writeFrame(const QCanBusFrame &frame) override {
        if (state() != ConnectedState) {
            setError(QStringLiteral("cannot write a frame to a device that is not connected"), WriteError);
            return false;
        }
        if (!frame.isValid() || frame.frameType() == QCanBusFrame::ErrorFrame) {
            setError(QStringLiteral("cannot write an invalid frame or an error frame"), WriteError);
            return false;
        }
        if (frame.hasFlexibleDataRateFormat() && !configurationParameter(CanFdKey).toBool()) {
            setError(QStringLiteral("cannot write a CAN FD frame without CanFdKey"), WriteError);
            return false;
        }
        enqueueOutgoingFrame(frame);
        flush();
        return true;
    }

    QString interpretErrorFrame(const QCanBusFrame &errorFrame) override {
        if (errorFrame.frameType() != QCanBusFrame::ErrorFrame)
            return QString();
        static const struct {
            QCanBusFrame::FrameError error;
            const char *text;
        } kTexts[] = {
            {QCanBusFrame::TransmissionTimeoutError, "transmission timeout"},
            {QCanBusFrame::LostArbitrationError, "lost arbitration"},
            {QCanBusFrame::ControllerError, "controller problem"},
            {QCanBusFrame::ProtocolViolationError, "protocol violation"},
            {QCanBusFrame::TransceiverError, "transceiver problem"},
            {QCanBusFrame::MissingAcknowledgmentError, "no acknowledgment"},
            {QCanBusFrame::BusOffError, "bus off"},
            {QCanBusFrame::BusError, "bus error"},
            {QCanBusFrame::ControllerRestartError, "controller restarted"},
        };
        QStringList parts;
        for (const auto &t : kTexts)
            if (errorFrame.error() & t.error)
                parts.append(QString::fromLatin1(t.text));
        const QByteArray data = errorFrame.payload();
        if ((errorFrame.error() & QCanBusFrame::ControllerError) && data.size() > 1) {
            const auto c = static_cast<unsigned char>(data[1]);
            if (c & (CAN_ERR_CRTL_RX_OVERFLOW | CAN_ERR_CRTL_TX_OVERFLOW))
                parts.append(QStringLiteral("buffer overflow"));
            if (c & (CAN_ERR_CRTL_RX_WARNING | CAN_ERR_CRTL_TX_WARNING))
                parts.append(QStringLiteral("error warning"));
            if (c & (CAN_ERR_CRTL_RX_PASSIVE | CAN_ERR_CRTL_TX_PASSIVE))
                parts.append(QStringLiteral("error passive"));
        }
        return parts.isEmpty() ? QStringLiteral("unknown error") : parts.join(QStringLiteral(", "));
    }

    QCanBusDeviceInfo deviceInfo() const override {
        return createDeviceInfo(QStringLiteral("socketcan"), m_name, QString(), QString(), QString(), 0,
                                m_name.startsWith(QLatin1String("vcan")),
                                readSysfs(m_name, "mtu").toInt() >= int(CANFD_MTU)); // 72, or 2060 for CAN XL
    }

protected:
    bool open() override {
        m_socket = ::socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, CAN_RAW);
        if (m_socket < 0) {
            setError(QStringLiteral("cannot open a CAN socket: %1").arg(QString::fromLocal8Bit(std::strerror(errno))),
                     ConnectionError);
            return false;
        }
        const unsigned index = if_nametoindex(m_name.toLocal8Bit().constData());
        if (index == 0) {
            setError(QStringLiteral("there is no CAN interface called %1").arg(m_name), ConnectionError);
            closeSocket();
            return false;
        }
        int on = 1;
        ::setsockopt(m_socket, SOL_SOCKET, SO_TIMESTAMP, &on, sizeof on);
        for (ConfigurationKey key : {LoopbackKey, ReceiveOwnKey, CanFdKey, RawFilterKey, ErrorFilterKey}) {
            if (!apply(key)) {
                setError(QStringLiteral("cannot apply configuration parameter %1: %2")
                             .arg(int(key))
                             .arg(QString::fromLocal8Bit(std::strerror(errno))),
                         ConfigurationError);
                closeSocket();
                return false;
            }
        }
        if (configurationParameter(BitRateKey).isValid() || configurationParameter(DataBitRateKey).isValid())
            qWarning("QCanBusDevice: the bit rate of a SocketCAN interface is set with ip link; BitRateKey is ignored");
        sockaddr_can address{};
        address.can_family = AF_CAN;
        address.can_ifindex = int(index);
        if (::bind(m_socket, reinterpret_cast<sockaddr *>(&address), sizeof address) < 0) {
            setError(QStringLiteral("cannot bind to %1: %2").arg(m_name, QString::fromLocal8Bit(std::strerror(errno))),
                     ConnectionError);
            closeSocket();
            return false;
        }
        m_readNotifier = new QSocketNotifier(qintptr(m_socket), QSocketNotifier::Read, this);
        QObject::connect(m_readNotifier, &QSocketNotifier::activated, this, [this] { readFrames(); });
        m_writeNotifier = new QSocketNotifier(qintptr(m_socket), QSocketNotifier::Write, this);
        m_writeNotifier->setEnabled(false);
        QObject::connect(m_writeNotifier, &QSocketNotifier::activated, this, [this] { flush(); });
        return true;
    }

    void close() override { closeSocket(); }

private:
    bool apply(ConfigurationKey key) {
        const QVariant value = configurationParameter(key);
        switch (key) {
        case LoopbackKey: {
            const int on = value.isValid() ? (value.toBool() ? 1 : 0) : 1;
            return ::setsockopt(m_socket, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &on, sizeof on) == 0;
        }
        case ReceiveOwnKey: {
            const int on = value.toBool() ? 1 : 0;
            return ::setsockopt(m_socket, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &on, sizeof on) == 0;
        }
        case CanFdKey: {
            const int on = value.toBool() ? 1 : 0;
            return ::setsockopt(m_socket, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &on, sizeof on) == 0 || !on;
        }
        case RawFilterKey: {
            const QList<Filter> filters = value.value<QList<Filter>>();
            std::vector<can_filter> raw;
            if (filters.isEmpty()) {
                raw.push_back({0, 0}); // everything
            }
            for (const Filter &f : filters) {
                can_filter r{};
                r.can_id = f.frameId & CAN_EFF_MASK;
                r.can_mask = f.frameIdMask & CAN_EFF_MASK;
                if (f.type == QCanBusFrame::RemoteRequestFrame) {
                    r.can_id |= CAN_RTR_FLAG;
                    r.can_mask |= CAN_RTR_FLAG;
                } else if (f.type == QCanBusFrame::DataFrame) {
                    r.can_mask |= CAN_RTR_FLAG;
                }
                if (f.format == Filter::MatchBaseFormat) {
                    r.can_mask |= CAN_EFF_FLAG;
                } else if (f.format == Filter::MatchExtendedFormat) {
                    r.can_id |= CAN_EFF_FLAG;
                    r.can_mask |= CAN_EFF_FLAG;
                }
                raw.push_back(r);
            }
            return ::setsockopt(m_socket, SOL_CAN_RAW, CAN_RAW_FILTER, raw.data(),
                                socklen_t(raw.size() * sizeof(can_filter))) == 0;
        }
        case ErrorFilterKey: {
            const can_err_mask_t mask =
                value.isValid() ? can_err_mask_t(int(value.value<QCanBusFrame::FrameErrors>())) : can_err_mask_t(0);
            return ::setsockopt(m_socket, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &mask, sizeof mask) == 0;
        }
        default:
            return true;
        }
    }

    void readFrames() {
        QList<QCanBusFrame> frames;
        for (;;) {
            canfd_frame raw{};
            iovec io{&raw, sizeof raw};
            char control[CMSG_SPACE(sizeof(timeval))];
            msghdr message{};
            message.msg_iov = &io;
            message.msg_iovlen = 1;
            message.msg_control = control;
            message.msg_controllen = sizeof control;
            const ssize_t n = ::recvmsg(m_socket, &message, MSG_DONTWAIT);
            if (n < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                    setError(QStringLiteral("cannot read from %1: %2")
                                 .arg(m_name, QString::fromLocal8Bit(std::strerror(errno))),
                             ReadError);
                break;
            }
            QCanBusFrame frame;
            const bool fd = n == ssize_t(CANFD_MTU);
            const quint32 id = raw.can_id;
            if (id & CAN_ERR_FLAG) {
                frame.setFrameType(QCanBusFrame::ErrorFrame);
                frame.setError(QCanBusFrame::FrameErrors::fromInt(int(id & CAN_ERR_MASK)));
            } else {
                frame.setExtendedFrameFormat((id & CAN_EFF_FLAG) != 0);
                frame.setFrameId(id & ((id & CAN_EFF_FLAG) ? CAN_EFF_MASK : CAN_SFF_MASK));
                frame.setExtendedFrameFormat((id & CAN_EFF_FLAG) != 0);
                if (id & CAN_RTR_FLAG)
                    frame.setFrameType(QCanBusFrame::RemoteRequestFrame);
            }
            frame.setFlexibleDataRateFormat(fd);
            if (fd) {
                frame.setBitrateSwitch((raw.flags & CANFD_BRS) != 0);
                frame.setErrorStateIndicator((raw.flags & CANFD_ESI) != 0);
            }
            frame.setPayload(QByteArray(reinterpret_cast<const char *>(raw.data), raw.len));
            frame.setLocalEcho((message.msg_flags & MSG_CONFIRM) !=
                               0); // a frame of this socket, with CAN_RAW_RECV_OWN_MSGS
            for (cmsghdr *c = CMSG_FIRSTHDR(&message); c; c = CMSG_NXTHDR(&message, c)) {
                if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SO_TIMESTAMP) {
                    timeval tv{};
                    std::memcpy(&tv, CMSG_DATA(c), sizeof tv);
                    frame.setTimeStamp(QCanBusFrame::TimeStamp(tv.tv_sec, tv.tv_usec));
                }
            }
            if (frame.frameType() == QCanBusFrame::RemoteRequestFrame)
                frame.setPayload(QByteArray(raw.len, '\0'));
            frames.append(frame);
        }
        enqueueReceivedFrames(frames);
    }

    void flush() {
        qint64 written = 0;
        while (hasOutgoingFrames()) {
            const QCanBusFrame frame = dequeueOutgoingFrame();
            canfd_frame raw{};
            raw.can_id = frame.frameId();
            if (frame.hasExtendedFrameFormat())
                raw.can_id |= CAN_EFF_FLAG;
            if (frame.frameType() == QCanBusFrame::RemoteRequestFrame)
                raw.can_id |= CAN_RTR_FLAG;
            const QByteArray payload = frame.payload();
            raw.len = static_cast<__u8>(payload.size());
            if (frame.frameType() != QCanBusFrame::RemoteRequestFrame)
                std::memcpy(raw.data, payload.constData(), size_t(payload.size()));
            size_t size = CAN_MTU;
            if (frame.hasFlexibleDataRateFormat()) {
                size = CANFD_MTU;
                if (frame.hasBitrateSwitch())
                    raw.flags |= CANFD_BRS;
                if (frame.hasErrorStateIndicator())
                    raw.flags |= CANFD_ESI;
            }
            const ssize_t n = ::write(m_socket, &raw, size);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS) {
                    // The queue of the interface is full: send it again when the socket can take it.
                    QList<QCanBusFrame> rest{frame};
                    while (hasOutgoingFrames())
                        rest.append(dequeueOutgoingFrame());
                    for (const QCanBusFrame &f : rest)
                        enqueueOutgoingFrame(f);
                    if (m_writeNotifier)
                        m_writeNotifier->setEnabled(true);
                    break;
                }
                setError(
                    QStringLiteral("cannot write to %1: %2").arg(m_name, QString::fromLocal8Bit(std::strerror(errno))),
                    WriteError);
                continue;
            }
            ++written;
        }
        if (!hasOutgoingFrames() && m_writeNotifier)
            m_writeNotifier->setEnabled(false);
        if (written > 0)
            emit framesWritten(written);
    }

    void closeSocket() {
        delete m_readNotifier;
        m_readNotifier = nullptr;
        delete m_writeNotifier;
        m_writeNotifier = nullptr;
        if (m_socket >= 0)
            ::close(m_socket);
        m_socket = -1;
    }

    QString m_name;
    int m_socket = -1;
    QSocketNotifier *m_readNotifier = nullptr;
    QSocketNotifier *m_writeNotifier = nullptr;
};

class SocketCanFactory : public QCanBusFactory {
public:
    QCanBusDevice *createDevice(const QString &interfaceName, QString *errorMessage) const override {
        if (errorMessage)
            errorMessage->clear();
        return new SocketCanDevice(interfaceName);
    }

    QList<QCanBusDeviceInfo> availableDevices(QString *errorMessage) const override {
        if (errorMessage)
            errorMessage->clear();
        QList<QCanBusDeviceInfo> list;
        const QDir net(QStringLiteral("/sys/class/net"));
        for (const QString &name : net.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System, QDir::Name)) {
            if (readSysfs(name, "type").toInt() != kArphrdCan)
                continue;
            SocketCanDevice device(name);
            list.append(device.deviceInfo());
        }
        return list;
    }
};

} // namespace

QCanBusFactory *socketCanFactory() {
    static SocketCanFactory factory;
    return &factory;
}

} // namespace QtSerialBusPrivate
