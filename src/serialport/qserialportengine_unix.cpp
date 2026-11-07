// MIT License - see LICENSE
#include "qserialportbaud_p.h"
#include "qserialportengine_p.h"

#include <QtCore/qelapsedtimer.h>

#include <fcntl.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace {

struct Speed {
    qint32 rate;
    speed_t code;
};

const Speed kSpeeds[] = {
    {50, B50},           {75, B75},       {110, B110},     {134, B134},     {150, B150},       {200, B200},
    {300, B300},         {600, B600},     {1200, B1200},   {1800, B1800},   {2400, B2400},     {4800, B4800},
    {9600, B9600},       {19200, B19200}, {38400, B38400}, {57600, B57600}, {115200, B115200}, {230400, B230400},
#ifdef B460800
    {460800, B460800},
#endif
#ifdef B500000
    {500000, B500000},
#endif
#ifdef B576000
    {576000, B576000},
#endif
#ifdef B921600
    {921600, B921600},
#endif
#ifdef B1000000
    {1000000, B1000000},
#endif
#ifdef B1152000
    {1152000, B1152000},
#endif
#ifdef B1500000
    {1500000, B1500000},
#endif
#ifdef B2000000
    {2000000, B2000000},
#endif
#ifdef B2500000
    {2500000, B2500000},
#endif
#ifdef B3000000
    {3000000, B3000000},
#endif
#ifdef B3500000
    {3500000, B3500000},
#endif
#ifdef B4000000
    {4000000, B4000000},
#endif
};

const Speed *findSpeed(qint32 rate) {
    for (const Speed &speed : kSpeeds) {
        if (speed.rate == rate)
            return &speed;
    }
    return nullptr;
}

bool isSoftError(int err) { return err == EAGAIN || err == EWOULDBLOCK; }

} // namespace

namespace QtPrivate {

bool fillSerialTermios(termios &tio, const SerialSettings &settings, bool readable) {
    // Raw bytes in both directions: no line editing, signals or translation.
    tio.c_iflag &= ~static_cast<tcflag_t>(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | INPCK | IGNPAR |
                                          IXON | IXOFF | IXANY);
    tio.c_oflag &= ~static_cast<tcflag_t>(OPOST);
    tio.c_lflag &= ~static_cast<tcflag_t>(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tio.c_cflag &= ~static_cast<tcflag_t>(CSIZE | CSTOPB | PARENB | PARODD);
#ifdef CMSPAR
    tio.c_cflag &= ~static_cast<tcflag_t>(CMSPAR);
#endif
#ifdef CRTSCTS
    tio.c_cflag &= ~static_cast<tcflag_t>(CRTSCTS);
#endif
    // The modem lines must not turn into hangups, and reads must not depend on them.
    tio.c_cflag |= CLOCAL;
    if (readable)
        tio.c_cflag |= CREAD;
    else
        tio.c_cflag &= ~static_cast<tcflag_t>(CREAD);
    // VMIN 1 makes an empty non-blocking read fail with EAGAIN, whereas a read
    // returning 0 means the device has gone. VMIN 0 would make the two look alike.
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;

    switch (settings.dataBits) {
    case QSerialPort::Data5:
        tio.c_cflag |= CS5;
        break;
    case QSerialPort::Data6:
        tio.c_cflag |= CS6;
        break;
    case QSerialPort::Data7:
        tio.c_cflag |= CS7;
        break;
    case QSerialPort::Data8:
        tio.c_cflag |= CS8;
        break;
    default:
        errno = EINVAL;
        return false;
    }

    switch (settings.parity) {
    case QSerialPort::NoParity:
        break;
    case QSerialPort::EvenParity:
        tio.c_cflag |= PARENB;
        break;
    case QSerialPort::OddParity:
        tio.c_cflag |= PARENB | PARODD;
        break;
#ifdef CMSPAR
    case QSerialPort::SpaceParity:
        tio.c_cflag |= PARENB | CMSPAR;
        break;
    case QSerialPort::MarkParity:
        tio.c_cflag |= PARENB | PARODD | CMSPAR;
        break;
#endif
    default:
        errno = ENOTSUP;
        return false;
    }
    if (settings.parity != QSerialPort::NoParity)
        tio.c_iflag |= INPCK | IGNPAR;

    switch (settings.stopBits) {
    case QSerialPort::OneStop:
        break;
    case QSerialPort::TwoStop:
        tio.c_cflag |= CSTOPB;
        break;
    default: // termios has no 1.5 stop bits setting
        errno = ENOTSUP;
        return false;
    }

    switch (settings.flowControl) {
    case QSerialPort::NoFlowControl:
        break;
    case QSerialPort::SoftwareControl:
        tio.c_iflag |= IXON | IXOFF;
        break;
#ifdef CRTSCTS
    case QSerialPort::HardwareControl:
        tio.c_cflag |= CRTSCTS;
        break;
#endif
    default:
        errno = ENOTSUP;
        return false;
    }

    const Speed *input = findSpeed(settings.inputBaudRate);
    const Speed *output = findSpeed(settings.outputBaudRate);
    const speed_t placeholder = B9600;
    const speed_t inputCode = input && output ? input->code : placeholder;
    const speed_t outputCode = input && output ? output->code : placeholder;
#ifdef CIBAUD
    // glibc 2.42 and newer define B9600 as 9600 and handle a separate input speed. Older glibc
    // and musl define the kernel's codes, and cfsetispeed() there sets the output speed too, so
    // a different input speed is lost. Write the bits that the kernel reads; zero in CIBAUD
    // means the same speed as the output.
    if (B9600 != 9600) {
        tio.c_cflag &= ~static_cast<tcflag_t>(CBAUD | CIBAUD);
        tio.c_cflag |= outputCode;
        if (inputCode != outputCode)
            tio.c_cflag |= (inputCode << kSerialInputSpeedShift) & CIBAUD;
        return true;
    }
#endif
    return ::cfsetispeed(&tio, inputCode) == 0 && ::cfsetospeed(&tio, outputCode) == 0;
}

QList<qint32> serialStandardBaudRates() {
    QList<qint32> rates;
    for (const Speed &speed : kSpeeds)
        rates.append(speed.rate);
    return rates;
}

QString serialSystemLocation(const QString &portName) {
    if (portName.isEmpty() || portName.startsWith(QString("/")) || portName.startsWith(QString("./")) ||
        portName.startsWith(QString("../")))
        return portName;
    return QString("/dev/") + portName;
}

QString serialPortName(const QString &systemLocation) {
    return systemLocation.startsWith(QString("/dev/")) ? systemLocation.mid(5) : systemLocation;
}

bool SerialEngine::failed() {
    m_errno = errno;
    return false;
}

bool SerialEngine::open(const QString &path, QIODeviceBase::OpenMode mode, const SerialSettings &settings) {
    const bool reads = mode.testFlag(QIODeviceBase::ReadOnly);
    const bool writes = mode.testFlag(QIODeviceBase::WriteOnly);
    const int access = reads && writes ? O_RDWR : reads ? O_RDONLY : O_WRONLY;
    int fd;
    do {
        fd = ::open(path.toUtf8().constData(), access | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0)
        return failed();

    // flock() also keeps out root, which TIOCEXCL does not. Any failure other
    // than contention only means the file system has no such locks.
    bool usable = ::flock(fd, LOCK_EX | LOCK_NB) == 0 || errno != EWOULDBLOCK;
    if (usable && ::ioctl(fd, TIOCEXCL) != 0)
        usable = errno != EBUSY;
    if (usable)
        usable = ::tcgetattr(fd, &m_saved) == 0;
    if (!usable) {
        const int err = errno == EWOULDBLOCK ? EBUSY : errno;
        ::close(fd);
        errno = err;
        return failed();
    }

    m_fd = fd;
    m_readable = reads;
    if (!apply(settings)) {
        const int err = m_errno;
        close();
        m_errno = err;
        return false;
    }
    return true;
}

void SerialEngine::close() {
    if (m_fd < 0)
        return;
    ::tcsetattr(m_fd, TCSANOW, &m_saved);
    ::ioctl(m_fd, TIOCNXCL);
    ::close(m_fd);
    m_fd = -1;
}

bool SerialEngine::apply(const SerialSettings &settings) {
    termios before;
    if (::tcgetattr(m_fd, &before) != 0) { std::fprintf(stderr, "TRACE tcgetattr errno=%d\n", errno);
        return failed(); }
    termios tio = before;
    if (!fillSerialTermios(tio, settings, m_readable)) { std::fprintf(stderr, "TRACE fill errno=%d\n", errno);
        return failed(); }
    if (::tcsetattr(m_fd, TCSANOW, &tio) != 0) { std::fprintf(stderr, "TRACE tcsetattr errno=%d cflag=%o readable=%d\n", errno, static_cast<unsigned>(tio.c_cflag), m_readable);
        return failed(); }
    if (findSpeed(settings.inputBaudRate) && findSpeed(settings.outputBaudRate))
        return true;
    if (setSerialCustomBaudRate(m_fd, settings.inputBaudRate, settings.outputBaudRate))
        return true;
    std::fprintf(stderr, "TRACE custom baud errno=%d\n", errno);
    failed();
    ::tcsetattr(m_fd, TCSANOW, &before);
    return false;
}

bool SerialEngine::pins(QSerialPort::PinoutSignals *out) {
    int bits = 0;
    if (::ioctl(m_fd, TIOCMGET, &bits) != 0)
        return failed();
    QSerialPort::PinoutSignals result = QSerialPort::NoSignal;
    const struct {
        int bit;
        QSerialPort::PinoutSignal pin;
    } map[] = {
        {TIOCM_DTR, QSerialPort::DataTerminalReadySignal},       {TIOCM_RTS, QSerialPort::RequestToSendSignal},
        {TIOCM_CAR, QSerialPort::DataCarrierDetectSignal},       {TIOCM_DSR, QSerialPort::DataSetReadySignal},
        {TIOCM_RNG, QSerialPort::RingIndicatorSignal},           {TIOCM_CTS, QSerialPort::ClearToSendSignal},
        {TIOCM_ST, QSerialPort::SecondaryTransmittedDataSignal}, {TIOCM_SR, QSerialPort::SecondaryReceivedDataSignal}};
    for (const auto &entry : map) {
        if (bits & entry.bit)
            result |= entry.pin;
    }
    *out = result;
    return true;
}

bool SerialEngine::setPins(QSerialPort::PinoutSignals which, bool on) {
    int bits = 0;
    if (which.testFlag(QSerialPort::DataTerminalReadySignal))
        bits |= TIOCM_DTR;
    if (which.testFlag(QSerialPort::RequestToSendSignal))
        bits |= TIOCM_RTS;
    return ::ioctl(m_fd, on ? TIOCMBIS : TIOCMBIC, &bits) == 0 || failed();
}

bool SerialEngine::setBreak(bool on) { return ::ioctl(m_fd, on ? TIOCSBRK : TIOCCBRK) == 0 || failed(); }

bool SerialEngine::discard(QSerialPort::Directions directions) {
    const bool input = directions.testFlag(QSerialPort::Input);
    const bool output = directions.testFlag(QSerialPort::Output);
    if (!input && !output)
        return true;
    const int queue = input && output ? TCIOFLUSH : input ? TCIFLUSH : TCOFLUSH;
    return ::tcflush(m_fd, queue) == 0 || failed();
}

qint64 SerialEngine::read(char *data, qint64 maxSize) {
    for (;;) {
        const ssize_t n = ::read(m_fd, data, static_cast<std::size_t>(maxSize));
        if (n > 0)
            return n;
        if (n == 0) { // see VMIN in fillSerialTermios()
            m_errno = ENODEV;
            return -1;
        }
        if (errno == EINTR)
            continue;
        if (isSoftError(errno))
            return 0;
        failed();
        return -1;
    }
}

qint64 SerialEngine::write(const char *data, qint64 maxSize) {
    for (;;) {
        const ssize_t n = ::write(m_fd, data, static_cast<std::size_t>(maxSize));
        if (n >= 0)
            return n;
        if (errno == EINTR)
            continue;
        if (isSoftError(errno))
            return 0;
        failed();
        return -1;
    }
}

int SerialEngine::wait(bool wantRead, bool wantWrite, int msecs, bool *readable, bool *writable) {
    const short events = static_cast<short>((wantRead ? POLLIN : 0) | (wantWrite ? POLLOUT : 0));
    pollfd p = {m_fd, events, 0};
    QElapsedTimer timer;
    timer.start();
    int left = msecs;
    int rc;
    while ((rc = ::poll(&p, 1, left)) < 0 && errno == EINTR) {
        if (msecs >= 0)
            left = static_cast<int>(qMax<qint64>(0, msecs - timer.elapsed()));
    }
    if (rc <= 0) {
        if (rc < 0)
            failed();
        return rc;
    }
    // A hangup is reported to whichever direction is waited for, which then
    // fails and reports it.
    const bool gone = p.revents & (POLLHUP | POLLERR | POLLNVAL);
    *readable = (p.revents & POLLIN) || (gone && wantRead);
    *writable = (p.revents & POLLOUT) || (gone && !wantRead);
    return 1;
}

QSerialPort::SerialPortError SerialEngine::error(SerialOperation operation) const {
    QSerialPort::SerialPortError code = QSerialPort::UnknownError;
    switch (m_errno) {
    case ENOENT:
    case ENODEV:
    case ENXIO:
    case EIO:
        code = operation == SerialOperation::Open
                   ? (m_errno == EIO ? QSerialPort::OpenError : QSerialPort::DeviceNotFoundError)
                   : QSerialPort::ResourceError;
        break;
    case EACCES:
    case EPERM:
    case EBUSY:
    case EROFS:
        code = QSerialPort::PermissionError;
        break;
    case EMFILE:
    case ENFILE:
    case ENOMEM:
        code = QSerialPort::ResourceError;
        break;
    case ENOTTY:
    case EINVAL:
    case ENOTSUP:
        code = QSerialPort::UnsupportedOperationError;
        break;
    default:
        break;
    }
    if (code != QSerialPort::UnknownError)
        return code;
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
