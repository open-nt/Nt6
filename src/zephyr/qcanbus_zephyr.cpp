// MIT License - see LICENSE
//
// The zephyrcan backend: the CAN drivers of Zephyr. An interface is named "canbus" for the controller that
// zephyr,canbus chooses, by an alias can0 to can3, or by the name of its device (can@40c3c000), which
// availableDevices() lists: every device that implements the CAN API. Frames arrive in a callback in interrupt context, which only puts them in a message queue
// and submits a work item; the work item writes an eventfd that a QSocketNotifier of the device watches, as the GPIO
// backend does. Changes of the state of the controller become error frames. Frames are sent with can_send(), which
// waits until the controller has taken the frame.
#include <QtSerialBus/qcanbusdevice.h>
#include <QtSerialBus/qcanbusfactory.h>

#include "qcanbus_p.h"

#include <QtCore/qpointer.h>
#include <QtCore/qsocketnotifier.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util_macro.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <sys/eventfd.h>
#include <unistd.h>
#include <vector>

// Every device that implements the CAN API. The macros declare the bounds of Zephyr's iterable sections where they are
// used, which has to be the global namespace for the names to match those of the linker script.
static std::vector<const device *> ntCanDevices() {
    std::vector<const device *> devices;
    STRUCT_SECTION_FOREACH(device, dev) {
        if (DEVICE_API_IS(can, dev) && device_is_ready(dev))
            devices.push_back(dev);
    }
    return devices;
}

namespace QtSerialBusPrivate {

namespace {

constexpr int kQueueLength = 64;

struct ControllerName {
    const char *name;   // "canbus" or an alias
    const char *device; // the name that device_get_binding() takes
};

#define NT_CAN_ALIAS(alias)                                                                                            \
    COND_CODE_1(DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(alias)), ({STRINGIFY(alias), DEVICE_DT_NAME(DT_ALIAS(alias))}, ), ())

// The names that an interface can have besides the name of its device: the controller that zephyr,canbus chooses, and
// the aliases can0 to can3.
const ControllerName kControllers[] = {
#if DT_HAS_CHOSEN(zephyr_canbus)
    {"canbus", DEVICE_DT_NAME(DT_CHOSEN(zephyr_canbus))},
#endif
    NT_CAN_ALIAS(can0) NT_CAN_ALIAS(can1) NT_CAN_ALIAS(can2) NT_CAN_ALIAS(can3){nullptr, nullptr},
};

const char *deviceNameOf(const QByteArray &name) {
    for (const ControllerName &c : kControllers)
        if (c.name && name == c.name)
            return c.device;
    return nullptr;
}

// Kept apart from the device so that CONTAINER_OF() sees a plain struct; the interrupt handlers reach it.
struct CanHooks {
    k_msgq queue;
    char buffer[kQueueLength * sizeof(can_frame)];
    k_work work;
    can_state_change_callback stateCallback;
    int fd = -1;
    std::atomic<int> dropped{0};
};

void onWork(k_work *work) {
    CanHooks *hooks = CONTAINER_OF(work, CanHooks, work);
    eventfd_write(hooks->fd, 1);
}

void onFrame(const device *, can_frame *frame, void *userData) {
    auto *hooks = static_cast<CanHooks *>(userData);
    if (k_msgq_put(&hooks->queue, frame, K_NO_WAIT) != 0)
        ++hooks->dropped;
    k_work_submit(&hooks->work);
}

// A change of state is queued as a frame marked with a bit of the flags that the drivers leave clear: id holds the
// QCanBusFrame::FrameErrors and data[1] the details of a controller problem, in the manner of SocketCAN, and data[6]
// and data[7] the error counters.
constexpr uint8_t kStateChangeFlag = 0x80;

void onState(const device *, can_state_change_callback *callback, can_state state, can_bus_err_cnt counters) {
    CanHooks *hooks = CONTAINER_OF(callback, CanHooks, stateCallback);
    can_frame frame{};
    frame.dlc = 8;
    frame.data[6] = counters.tx_err_cnt;
    frame.data[7] = counters.rx_err_cnt;
    switch (state) {
    case CAN_STATE_BUS_OFF:
        frame.id = QCanBusFrame::BusOffError;
        break;
    case CAN_STATE_ERROR_WARNING:
        frame.id = QCanBusFrame::ControllerError;
        frame.data[1] = 0x04 | 0x08; // CAN_ERR_CRTL_RX_WARNING | CAN_ERR_CRTL_TX_WARNING
        break;
    case CAN_STATE_ERROR_PASSIVE:
        frame.id = QCanBusFrame::ControllerError;
        frame.data[1] = 0x10 | 0x20; // CAN_ERR_CRTL_RX_PASSIVE | CAN_ERR_CRTL_TX_PASSIVE
        break;
    case CAN_STATE_ERROR_ACTIVE:
        frame.id = QCanBusFrame::ControllerRestartError;
        break;
    default:
        return;
    }
    frame.flags = kStateChangeFlag;
    if (k_msgq_put(&hooks->queue, &frame, K_NO_WAIT) != 0)
        ++hooks->dropped;
    k_work_submit(&hooks->work);
}

QCanBusDevice::CanBusStatus statusOf(can_state state) {
    switch (state) {
    case CAN_STATE_ERROR_ACTIVE:
        return QCanBusDevice::CanBusStatus::Good;
    case CAN_STATE_ERROR_WARNING:
        return QCanBusDevice::CanBusStatus::Warning;
    case CAN_STATE_ERROR_PASSIVE:
        return QCanBusDevice::CanBusStatus::Error;
    case CAN_STATE_BUS_OFF:
        return QCanBusDevice::CanBusStatus::BusOff;
    default:
        return QCanBusDevice::CanBusStatus::Unknown;
    }
}

class ZephyrCanDevice : public QCanBusDevice {
public:
    ZephyrCanDevice(QString name, const device *dev) : m_name(std::move(name)), m_device(dev) {
        k_msgq_init(&m_hooks.queue, m_hooks.buffer, sizeof(can_frame), kQueueLength);
        k_work_init(&m_hooks.work, onWork);
        can_init_state_change_callback(&m_hooks.stateCallback, onState);
        setCanBusStatusGetter([this] {
            can_state state = CAN_STATE_STOPPED;
            return can_get_state(m_device, &state, nullptr) == 0 ? statusOf(state) : CanBusStatus::Unknown;
        });
        setResetControllerFunction([this] {
            // A controller that went bus off comes back by being stopped and started again.
            can_stop(m_device);
            if (can_start(m_device) != 0)
                setError(QStringLiteral("cannot restart the CAN controller"), OperationError);
        });
    }
    ~ZephyrCanDevice() override { shutDown(); }
    ZephyrCanDevice(const ZephyrCanDevice &) = delete;
    ZephyrCanDevice &operator=(const ZephyrCanDevice &) = delete;

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
        can_frame raw{};
        raw.id = frame.frameId();
        const QByteArray payload = frame.payload();
        raw.dlc = can_bytes_to_dlc(uint8_t(payload.size()));
        if (frame.hasExtendedFrameFormat())
            raw.flags |= CAN_FRAME_IDE;
        if (frame.frameType() == QCanBusFrame::RemoteRequestFrame)
            raw.flags |= CAN_FRAME_RTR;
        else
            std::memcpy(raw.data, payload.constData(), size_t(payload.size()));
        if (frame.hasFlexibleDataRateFormat()) {
            raw.flags |= CAN_FRAME_FDF;
            if (frame.hasBitrateSwitch())
                raw.flags |= CAN_FRAME_BRS;
        }
        const int result = can_send(m_device, &raw, K_MSEC(100), nullptr, nullptr);
        if (result != 0) {
            setError(QStringLiteral("cannot send the frame (error %1)").arg(result), WriteError);
            return false;
        }
        QPointer<ZephyrCanDevice> self(this);
        QMetaObject::invokeMethod(
            this,
            [self] {
                if (self && self->state() == ConnectedState)
                    emit self->framesWritten(1);
            },
            Qt::QueuedConnection);
        return true;
    }

    QString interpretErrorFrame(const QCanBusFrame &errorFrame) override {
        if (errorFrame.frameType() != QCanBusFrame::ErrorFrame)
            return QString();
        const QCanBusFrame::FrameErrors errors = errorFrame.error();
        const QByteArray data = errorFrame.payload();
        const QString counters = data.size() >= 8 ? QStringLiteral(" (transmit errors %1, receive errors %2)")
                                                        .arg(static_cast<unsigned char>(data[6]))
                                                        .arg(static_cast<unsigned char>(data[7]))
                                                  : QString();
        if (errors & QCanBusFrame::BusOffError)
            return QStringLiteral("bus off") + counters;
        if (errors & QCanBusFrame::ControllerRestartError)
            return QStringLiteral("error active again") + counters;
        if ((errors & QCanBusFrame::ControllerError) && data.size() > 1)
            return ((static_cast<unsigned char>(data[1]) & 0x30) ? QStringLiteral("error passive")
                                                                 : QStringLiteral("error warning")) +
                   counters;
        return QStringLiteral("unknown error") + counters;
    }

    QCanBusDeviceInfo deviceInfo() const override {
        can_mode_t capabilities = 0;
        can_get_capabilities(m_device, &capabilities);
        return createDeviceInfo(QStringLiteral("zephyrcan"), m_name, QString(), QString::fromUtf8(m_device->name),
                                QString(), 0, false, (capabilities & CAN_MODE_FD) != 0);
    }

protected:
    bool open() override {
        m_hooks.fd = eventfd(0, EFD_NONBLOCK);
        if (m_hooks.fd < 0) {
            setError(QStringLiteral("cannot make an eventfd for the CAN device"), ConnectionError);
            return false;
        }
        can_stop(m_device); // it may be running from an earlier connection
        can_mode_t mode = CAN_MODE_NORMAL;
        if (configurationParameter(LoopbackKey).toBool())
            mode |= CAN_MODE_LOOPBACK;
        if (configurationParameter(CanFdKey).toBool())
            mode |= CAN_MODE_FD;
        int result = can_set_mode(m_device, mode);
        if (result == 0 && configurationParameter(BitRateKey).isValid())
            result = can_set_bitrate(m_device, configurationParameter(BitRateKey).toUInt());
        if (result == 0 && (mode & CAN_MODE_FD) && configurationParameter(DataBitRateKey).isValid())
            result = can_set_bitrate_data(m_device, configurationParameter(DataBitRateKey).toUInt());
        if (result != 0) {
            setError(QStringLiteral("cannot configure the CAN controller (error %1)").arg(result), ConfigurationError);
            shutDown();
            return false;
        }
        if (!addFilters())
            return false;
        can_add_state_change_callback(m_device, &m_hooks.stateCallback);
        m_notifier = new QSocketNotifier(qintptr(m_hooks.fd), QSocketNotifier::Read, this);
        QObject::connect(m_notifier, &QSocketNotifier::activated, this, [this] { readFrames(); });
        result = can_start(m_device);
        if (result != 0) {
            setError(QStringLiteral("cannot start the CAN controller (error %1)").arg(result), ConnectionError);
            shutDown();
            return false;
        }
        return true;
    }

    void close() override { shutDown(); }

private:
    bool addFilters() {
        const QList<Filter> filters = configurationParameter(RawFilterKey).value<QList<Filter>>();
        std::vector<can_filter> raw;
        if (filters.isEmpty()) {
            raw.push_back({0, 0, 0});              // every base frame
            raw.push_back({0, 0, CAN_FILTER_IDE}); // every extended frame
        }
        for (const Filter &f : filters) {
            if (f.format & Filter::MatchBaseFormat)
                raw.push_back({f.frameId & CAN_STD_ID_MASK, f.frameIdMask & CAN_STD_ID_MASK, 0});
            if (f.format & Filter::MatchExtendedFormat)
                raw.push_back({f.frameId & CAN_EXT_ID_MASK, f.frameIdMask & CAN_EXT_ID_MASK, CAN_FILTER_IDE});
        }
        for (const can_filter &filter : raw) {
            const int id = can_add_rx_filter(m_device, onFrame, &m_hooks, &filter);
            if (id < 0) {
                setError(QStringLiteral("cannot add a filter to the CAN controller (error %1)").arg(id),
                         ConfigurationError);
                shutDown();
                return false;
            }
            m_filterIds.push_back(id);
        }
        return true;
    }

    void readFrames() {
        eventfd_t count = 0;
        eventfd_read(m_hooks.fd, &count);
        QList<QCanBusFrame> frames;
        can_frame raw{};
        while (k_msgq_get(&m_hooks.queue, &raw, K_NO_WAIT) == 0) {
            QCanBusFrame frame;
            const qint64 now = k_ticks_to_us_floor64(k_uptime_ticks());
            frame.setTimeStamp(QCanBusFrame::TimeStamp::fromMicroSeconds(now));
            if (raw.flags & kStateChangeFlag) { // a change of state, made into an error frame by onState()
                frame.setFrameType(QCanBusFrame::ErrorFrame);
                frame.setError(QCanBusFrame::FrameErrors::fromInt(int(raw.id)));
                frame.setPayload(QByteArray(reinterpret_cast<const char *>(raw.data), 8));
                if (!(QCanBusFrame::FrameErrors::fromInt(errorFilter()) & frame.error()))
                    continue;
            } else {
                frame.setFrameId(raw.id);
                frame.setExtendedFrameFormat((raw.flags & CAN_FRAME_IDE) != 0);
                const uint8_t length = can_dlc_to_bytes(raw.dlc);
                if (raw.flags & CAN_FRAME_RTR) {
                    frame.setFrameType(QCanBusFrame::RemoteRequestFrame);
                    frame.setPayload(QByteArray(length, '\0'));
                } else {
                    frame.setPayload(QByteArray(reinterpret_cast<const char *>(raw.data), length));
                }
                frame.setFlexibleDataRateFormat((raw.flags & CAN_FRAME_FDF) != 0);
                frame.setBitrateSwitch((raw.flags & CAN_FRAME_BRS) != 0);
                frame.setErrorStateIndicator((raw.flags & CAN_FRAME_ESI) != 0);
                if (!passesFilters(frame, configurationParameter(RawFilterKey)))
                    continue;
            }
            frames.append(frame);
        }
        const int dropped = m_hooks.dropped.exchange(0);
        if (dropped > 0)
            setError(QStringLiteral("%1 frames were lost: they were not read in time").arg(dropped), ReadError);
        enqueueReceivedFrames(frames);
    }

    int errorFilter() const {
        const QVariant value = configurationParameter(ErrorFilterKey);
        return value.isValid() ? int(value.value<QCanBusFrame::FrameErrors>()) : int(QCanBusFrame::AnyError);
    }

    void shutDown() {
        for (int id : m_filterIds)
            can_remove_rx_filter(m_device, id);
        m_filterIds.clear();
        can_remove_state_change_callback(m_device, &m_hooks.stateCallback);
        can_stop(m_device);
        k_work_sync sync;
        k_work_cancel_sync(&m_hooks.work, &sync);
        k_msgq_purge(&m_hooks.queue);
        delete m_notifier;
        m_notifier = nullptr;
        if (m_hooks.fd >= 0)
            ::close(m_hooks.fd);
        m_hooks.fd = -1;
    }

    QString m_name;
    const device *m_device;
    CanHooks m_hooks;
    std::vector<int> m_filterIds;
    QSocketNotifier *m_notifier = nullptr;
};

class ZephyrCanFactory : public QCanBusFactory {
public:
    QCanBusDevice *createDevice(const QString &interfaceName, QString *errorMessage) const override {
        const QByteArray name = interfaceName.toUtf8();
        const char *deviceName = deviceNameOf(name);
        const device *dev = device_get_binding(deviceName ? deviceName : name.constData());
        if (!dev || !device_is_ready(dev)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("there is no CAN controller called %1").arg(interfaceName);
            return nullptr;
        }
        if (errorMessage)
            errorMessage->clear();
        return new ZephyrCanDevice(interfaceName, dev);
    }

    QList<QCanBusDeviceInfo> availableDevices(QString *errorMessage) const override {
        if (errorMessage)
            errorMessage->clear();
        QList<QCanBusDeviceInfo> list;
        for (const device *dev : ntCanDevices()) {
            ZephyrCanDevice candidate(QString::fromUtf8(dev->name), dev);
            list.append(candidate.deviceInfo());
        }
        return list;
    }
};

} // namespace

QCanBusFactory *zephyrCanFactory() {
    static ZephyrCanFactory factory;
    return &factory;
}

} // namespace QtSerialBusPrivate
