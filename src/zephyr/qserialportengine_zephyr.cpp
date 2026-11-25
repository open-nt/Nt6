// MIT License - see LICENSE
//
// QSerialPort on the UART drivers of Zephyr. A port is named by the node label of its UART in the devicetree (lpuart1)
// or by the name of its device (serial@4007c000), which availablePorts() lists: every device that implements the UART
// API. The interrupt-driven API of the driver moves bytes between the UART and two ring buffers. The interrupt handler
// only submits work items, which write two eventfds: one that the read notifier of QSerialPort watches, signalled when
// bytes arrive, and one that the write notifier watches, signalled when the transmit buffer has room again after it
// was full. That is the pattern of the GPIO and CAN backends.
#include "qserialportengine_p.h"
#include "qserialportscanner_p.h"

#include <QtCore/qelapsedtimer.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/util_macro.h>

#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <utility>
#include <vector>

// Every device that implements the UART API. The macros declare the bounds of Zephyr's iterable sections where they are
// used, which has to be the global namespace for the names to match those of the linker script.
static std::vector<const device *> ntUartDevices() {
    std::vector<const device *> devices;
    STRUCT_SECTION_FOREACH(device, dev) {
        if (DEVICE_API_IS(uart, dev) && device_is_ready(dev))
            devices.push_back(dev);
    }
    return devices;
}

namespace QtPrivate {

// Kept apart from the engine so that CONTAINER_OF() sees a plain struct; the interrupt handler reaches it.
struct ZephyrUart {
    const device *dev = nullptr;
    uart_config saved = {};
    bool restore = false;
    k_spinlock lock = {};
    ring_buf rx = {};
    ring_buf tx = {};
    uint8_t rxBuffer[CONFIG_NT_SERIALPORT_BUFFER_SIZE] = {};
    uint8_t txBuffer[CONFIG_NT_SERIALPORT_BUFFER_SIZE] = {};
    bool txWaiting = false; // the transmit buffer was full when QSerialPort last wrote
    k_work rxWork = {};
    k_work txWork = {};
    int rxFd = -1;
    int txFd = -1;
};

namespace {

struct LabelName {
    const char *label;  // a node label of the devicetree
    const char *device; // the name of the node's device
};

#define NT_UART_LABEL(label, node) {STRINGIFY(label), DEVICE_DT_NAME(node)},
#define NT_UART_NODE(node) DT_FOREACH_NODELABEL_VARGS(node, NT_UART_LABEL, node)

// The node labels of every enabled node. Which of them are UARTs is only known at run time, from the API of the device
// that has the name, so a label resolves only when its device is a UART.
const LabelName kLabels[] = {DT_FOREACH_STATUS_OKAY_NODE(NT_UART_NODE){nullptr, nullptr}};

std::mutex openMutex;
std::vector<const device *> openDevices; // the ports that are open, which a second open() finds busy

bool isUart(const device *dev) {
    for (const device *uart : ntUartDevices()) {
        if (uart == dev)
            return true;
    }
    return false;
}

void onRxWork(k_work *work) { eventfd_write(CONTAINER_OF(work, ZephyrUart, rxWork)->rxFd, 1); }
void onTxWork(k_work *work) { eventfd_write(CONTAINER_OF(work, ZephyrUart, txWork)->txFd, 1); }

void onInterrupt(const device *dev, void *userData) {
    auto *uart = static_cast<ZephyrUart *>(userData);
    uart_irq_update(dev);
    while (uart_irq_is_pending(dev) > 0) {
        if (uart_irq_rx_ready(dev) > 0) {
            bool got = false;
            uint8_t chunk[32];
            int n;
            while ((n = uart_fifo_read(dev, chunk, sizeof chunk)) > 0) {
                const k_spinlock_key_t key = k_spin_lock(&uart->lock);
                // What does not fit is lost, as a terminal loses what overflows its buffer.
                ring_buf_put(&uart->rx, chunk, static_cast<uint32_t>(n));
                k_spin_unlock(&uart->lock, key);
                got = true;
            }
            if (got)
                k_work_submit(&uart->rxWork);
        }
        if (uart_irq_tx_ready(dev) > 0) {
            const k_spinlock_key_t key = k_spin_lock(&uart->lock);
            uint8_t *data = nullptr;
            const uint32_t ready = ring_buf_get_ptr(&uart->tx, &data, 0);
            const int filled = ready > 0 ? uart_fifo_fill(dev, data, static_cast<int>(ready)) : 0;
            if (filled > 0)
                ring_buf_consume(&uart->tx, static_cast<std::size_t>(filled));
            const bool empty = ring_buf_is_empty(&uart->tx);
            const bool wake = uart->txWaiting && ring_buf_space_get(&uart->tx) >= CONFIG_NT_SERIALPORT_BUFFER_SIZE / 2;
            if (wake)
                uart->txWaiting = false;
            k_spin_unlock(&uart->lock, key);
            if (empty)
                uart_irq_tx_disable(dev);
            if (wake)
                k_work_submit(&uart->txWork);
        }
        uart_irq_update(dev);
    }
}

// Takes what an eventfd has counted, so that it signals again only for what happens next.
void drain(int fd) {
    eventfd_t value;
    eventfd_read(fd, &value);
}

bool fillConfig(uart_config &config, const SerialSettings &settings) {
    if (settings.inputBaudRate != settings.outputBaudRate || settings.outputBaudRate <= 0) {
        errno = ENOTSUP; // a UART has one rate for both directions
        return false;
    }
    config.baudrate = static_cast<uint32_t>(settings.outputBaudRate);
    switch (settings.dataBits) {
    case QSerialPort::Data5:
        config.data_bits = UART_CFG_DATA_BITS_5;
        break;
    case QSerialPort::Data6:
        config.data_bits = UART_CFG_DATA_BITS_6;
        break;
    case QSerialPort::Data7:
        config.data_bits = UART_CFG_DATA_BITS_7;
        break;
    case QSerialPort::Data8:
        config.data_bits = UART_CFG_DATA_BITS_8;
        break;
    default:
        errno = EINVAL;
        return false;
    }
    switch (settings.parity) {
    case QSerialPort::NoParity:
        config.parity = UART_CFG_PARITY_NONE;
        break;
    case QSerialPort::EvenParity:
        config.parity = UART_CFG_PARITY_EVEN;
        break;
    case QSerialPort::OddParity:
        config.parity = UART_CFG_PARITY_ODD;
        break;
    case QSerialPort::SpaceParity:
        config.parity = UART_CFG_PARITY_SPACE;
        break;
    case QSerialPort::MarkParity:
        config.parity = UART_CFG_PARITY_MARK;
        break;
    default:
        errno = EINVAL;
        return false;
    }
    switch (settings.stopBits) {
    case QSerialPort::OneStop:
        config.stop_bits = UART_CFG_STOP_BITS_1;
        break;
    case QSerialPort::OneAndHalfStop:
        config.stop_bits = UART_CFG_STOP_BITS_1_5;
        break;
    case QSerialPort::TwoStop:
        config.stop_bits = UART_CFG_STOP_BITS_2;
        break;
    default:
        errno = EINVAL;
        return false;
    }
    switch (settings.flowControl) {
    case QSerialPort::NoFlowControl:
        config.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;
        break;
    case QSerialPort::HardwareControl:
        config.flow_ctrl = UART_CFG_FLOW_CTRL_RTS_CTS;
        break;
    default: // the drivers have no XON/XOFF
        errno = ENOTSUP;
        return false;
    }
    return true;
}

} // namespace

QList<qint32> serialStandardBaudRates() {
    return {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600, 1000000};
}

QString serialSystemLocation(const QString &portName) {
    const QByteArray name = portName.toUtf8();
    for (const LabelName &entry : kLabels) {
        if (entry.label && name == entry.label && isUart(device_get_binding(entry.device)))
            return QString::fromUtf8(entry.device);
    }
    return portName;
}

QString serialPortName(const QString &systemLocation) {
    const QByteArray name = systemLocation.toUtf8();
    for (const LabelName &entry : kLabels) {
        if (entry.label && name == entry.device)
            return QString::fromUtf8(entry.label);
    }
    return systemLocation;
}

QList<QSerialPortInfo> SerialPortScanner::scan(const QString &, const QString &) {
    QList<QSerialPortInfo> ports;
    for (const device *dev : ntUartDevices()) {
        QSerialPortInfo info;
        info.m_systemLocation = QString::fromUtf8(dev->name);
        info.m_portName = serialPortName(info.m_systemLocation);
        info.m_description = QString("UART");
        ports.append(info);
    }
    std::sort(ports.begin(), ports.end(),
              [](const QSerialPortInfo &a, const QSerialPortInfo &b) { return a.portName() < b.portName(); });
    return ports;
}

bool SerialEngine::failed() {
    m_errno = errno;
    return false;
}

bool SerialEngine::open(const QString &path, QIODeviceBase::OpenMode mode, const SerialSettings &settings) {
    const device *dev = device_get_binding(path.toUtf8().constData());
    if (!dev || !isUart(dev)) {
        errno = ENODEV;
        return failed();
    }
    {
        std::lock_guard<std::mutex> locker(openMutex);
        if (std::find(openDevices.begin(), openDevices.end(), dev) != openDevices.end()) {
            errno = EBUSY;
            return failed();
        }
        openDevices.push_back(dev);
    }

    auto *uart = new ZephyrUart;
    uart->dev = dev;
    ring_buf_init(&uart->rx, sizeof uart->rxBuffer, uart->rxBuffer);
    ring_buf_init(&uart->tx, sizeof uart->txBuffer, uart->txBuffer);
    k_work_init(&uart->rxWork, onRxWork);
    k_work_init(&uart->txWork, onTxWork);
    uart->restore = uart_config_get(dev, &uart->saved) == 0;
    uart->rxFd = eventfd(0, EFD_NONBLOCK);
    uart->txFd = uart->rxFd >= 0 ? eventfd(0, EFD_NONBLOCK) : -1;
    m_uart = uart;
    m_fd = uart->rxFd;
    m_txFd = uart->txFd;
    m_readable = mode.testFlag(QIODeviceBase::ReadOnly);
    if (uart->txFd < 0) {
        const int err = errno ? errno : EMFILE; // Zephyr has CONFIG_ZVFS_EVENTFD_ADD_SIZE_NT eventfds for Nt
        close();
        m_errno = err;
        return false;
    }
    if (!apply(settings)) {
        const int err = m_errno;
        close();
        m_errno = err;
        return false;
    }

    // Some drivers have the interrupt-driven API only with CONFIG_UART_INTERRUPT_DRIVEN and a hardware interrupt.
    const int rc = uart_irq_callback_user_data_set(dev, onInterrupt, uart);
    if (rc != 0) {
        close();
        m_errno = rc == -ENOSYS || rc == -ENOTSUP ? ENOTSUP : -rc;
        return false;
    }
    if (m_readable)
        uart_irq_rx_enable(dev);
    return true;
}

void SerialEngine::close() {
    if (!m_uart)
        return;
    ZephyrUart *uart = m_uart;
    uart_irq_rx_disable(uart->dev);
    uart_irq_tx_disable(uart->dev);
    uart_irq_callback_user_data_set(uart->dev, nullptr, nullptr);
    k_work_sync sync;
    k_work_cancel_sync(&uart->rxWork, &sync);
    k_work_cancel_sync(&uart->txWork, &sync);
    if (uart->restore)
        uart_configure(uart->dev, &uart->saved);
    if (uart->rxFd >= 0)
        ::close(uart->rxFd);
    if (uart->txFd >= 0)
        ::close(uart->txFd);
    {
        std::lock_guard<std::mutex> locker(openMutex);
        openDevices.erase(std::remove(openDevices.begin(), openDevices.end(), uart->dev), openDevices.end());
    }
    delete uart;
    m_uart = nullptr;
    m_fd = m_txFd = -1;
}

bool SerialEngine::apply(const SerialSettings &settings) {
    uart_config config = {};
    if (!fillConfig(config, settings))
        return failed();
    const int rc = uart_configure(m_uart->dev, &config);
    if (rc == 0)
        return true;
    if (rc == -ENOSYS) {
        // The driver cannot be configured at run time (or CONFIG_UART_USE_RUNTIME_CONFIGURE is off) and keeps the
        // settings of the devicetree. They are accepted if they are what was asked for, or if the driver cannot tell.
        uart_config current = {};
        const int got = uart_config_get(m_uart->dev, &current);
        if (got == -ENOSYS)
            return true;
        if (got == 0 && current.baudrate == config.baudrate && current.parity == config.parity &&
            current.stop_bits == config.stop_bits && current.data_bits == config.data_bits &&
            current.flow_ctrl == config.flow_ctrl)
            return true;
    }
    // For a setting that the hardware lacks, the driver refuses.
    m_errno = rc == -ENOSYS || rc == -ENOTSUP ? ENOTSUP : -rc;
    return false;
}

bool SerialEngine::pins(QSerialPort::PinoutSignals *out) {
#ifdef CONFIG_UART_LINE_CTRL
    QSerialPort::PinoutSignals result = QSerialPort::NoSignal;
    const struct {
        uint32_t control;
        QSerialPort::PinoutSignal pin;
    } map[] = {{UART_LINE_CTRL_DTR, QSerialPort::DataTerminalReadySignal},
               {UART_LINE_CTRL_RTS, QSerialPort::RequestToSendSignal},
               {UART_LINE_CTRL_DCD, QSerialPort::DataCarrierDetectSignal},
               {UART_LINE_CTRL_DSR, QSerialPort::DataSetReadySignal}};
    for (const auto &entry : map) {
        uint32_t value = 0;
        const int rc = uart_line_ctrl_get(m_uart->dev, entry.control, &value);
        if (rc == -ENOSYS || rc == -ENOTSUP)
            continue; // a line that the UART does not have stays low
        if (rc != 0) {
            m_errno = -rc;
            return false;
        }
        if (value)
            result |= entry.pin;
    }
    *out = result;
    return true;
#else
    *out = QSerialPort::NoSignal;
    return true;
#endif
}

bool SerialEngine::setPins(QSerialPort::PinoutSignals which, bool on) {
#ifdef CONFIG_UART_LINE_CTRL
    for (const auto &[pin, control] : {std::pair{QSerialPort::DataTerminalReadySignal, UART_LINE_CTRL_DTR},
                                       std::pair{QSerialPort::RequestToSendSignal, UART_LINE_CTRL_RTS}}) {
        if (!which.testFlag(pin))
            continue;
        const int rc = uart_line_ctrl_set(m_uart->dev, control, on ? 1U : 0U);
        if (rc != 0) {
            m_errno = rc == -ENOSYS || rc == -ENOTSUP ? ENOTSUP : -rc;
            return false;
        }
    }
    return true;
#else
    Q_UNUSED(which);
    Q_UNUSED(on);
    m_errno = ENOTSUP;
    return false;
#endif
}

bool SerialEngine::setBreak(bool on) {
    if (!on)
        return true; // the line is never held in a break
    m_errno = ENOTSUP;
    return false;
}

bool SerialEngine::discard(QSerialPort::Directions directions) {
    const k_spinlock_key_t key = k_spin_lock(&m_uart->lock);
    if (directions.testFlag(QSerialPort::Input))
        ring_buf_reset(&m_uart->rx);
    if (directions.testFlag(QSerialPort::Output))
        ring_buf_reset(&m_uart->tx);
    k_spin_unlock(&m_uart->lock, key);
    return true;
}

qint64 SerialEngine::read(char *data, qint64 maxSize) {
    drain(m_uart->rxFd);
    const k_spinlock_key_t key = k_spin_lock(&m_uart->lock);
    const uint32_t n = ring_buf_get(&m_uart->rx, reinterpret_cast<uint8_t *>(data), static_cast<uint32_t>(maxSize));
    const bool more = !ring_buf_is_empty(&m_uart->rx);
    k_spin_unlock(&m_uart->lock, key);
    // QSerialPort may stop reading before the buffer is empty, when its own buffer is full; the notifier must fire
    // again once it reads on.
    if (more)
        eventfd_write(m_uart->rxFd, 1);
    return n;
}

qint64 SerialEngine::write(const char *data, qint64 maxSize) {
    drain(m_uart->txFd);
    const k_spinlock_key_t key = k_spin_lock(&m_uart->lock);
    const uint32_t n =
        ring_buf_put(&m_uart->tx, reinterpret_cast<const uint8_t *>(data), static_cast<uint32_t>(maxSize));
    if (n < maxSize)
        m_uart->txWaiting = true; // checked under the lock, so the interrupt handler cannot miss it
    k_spin_unlock(&m_uart->lock, key);
    if (n > 0)
        uart_irq_tx_enable(m_uart->dev);
    return n;
}

int SerialEngine::wait(bool wantRead, bool wantWrite, int msecs, bool *readable, bool *writable) {
    // What the buffers already hold or have room for is ready without waiting.
    {
        const k_spinlock_key_t key = k_spin_lock(&m_uart->lock);
        *readable = wantRead && !ring_buf_is_empty(&m_uart->rx);
        *writable = wantWrite && ring_buf_space_get(&m_uart->tx) > 0;
        if (wantWrite && !*writable)
            m_uart->txWaiting = true;
        k_spin_unlock(&m_uart->lock, key);
    }
    if (*readable || *writable)
        return 1;
    pollfd p[2] = {{m_uart->rxFd, static_cast<short>(wantRead ? POLLIN : 0), 0},
                   {m_uart->txFd, static_cast<short>(wantWrite ? POLLIN : 0), 0}};
    QElapsedTimer timer;
    timer.start();
    int left = msecs;
    int rc;
    while ((rc = ::poll(p, 2, left)) < 0 && errno == EINTR) {
        if (msecs >= 0)
            left = static_cast<int>(qMax<qint64>(0, msecs - timer.elapsed()));
    }
    if (rc <= 0) {
        if (rc < 0)
            failed();
        return rc;
    }
    *readable = (p[0].revents & POLLIN) != 0;
    *writable = (p[1].revents & POLLIN) != 0;
    return 1;
}

QSerialPort::SerialPortError SerialEngine::error(SerialOperation operation) const {
    switch (m_errno) {
    case ENOENT:
    case ENODEV:
        return operation == SerialOperation::Open ? QSerialPort::DeviceNotFoundError : QSerialPort::ResourceError;
    case EBUSY:
    case EACCES:
        return QSerialPort::PermissionError;
    case EMFILE:
    case ENFILE:
    case ENOMEM:
    case EIO:
        return QSerialPort::ResourceError;
    case EINVAL:
    case ENOTSUP:
    case ENOSYS:
        return QSerialPort::UnsupportedOperationError;
    default:
        break;
    }
    switch (operation) {
    case SerialOperation::Open:
        return QSerialPort::OpenError;
    case SerialOperation::Read:
        return QSerialPort::ReadError;
    case SerialOperation::Write:
        return QSerialPort::WriteError;
    case SerialOperation::Control:
        break;
    }
    return QSerialPort::UnknownError;
}

QString SerialEngine::errorString() const { return QString::fromUtf8(std::strerror(m_errno)); }

} // namespace QtPrivate
