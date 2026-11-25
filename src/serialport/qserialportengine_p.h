// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtCore/qsocketnotifier.h>
#include <QtSerialPort/qserialport.h>

#ifndef __ZEPHYR__
#include <termios.h>
#endif

// The operating system's side of QSerialPort: POSIX terminals in qserialportengine_unix.cpp and the UART drivers of
// Zephyr in src/zephyr/qserialportengine_zephyr.cpp. A Windows port replaces this header's implementation and leaves
// qserialport.cpp alone apart from its use of QSocketNotifier.
namespace QtPrivate {

#ifndef __ZEPHYR__
// Where the kernel keeps the input speed in c_cflag: the CIBAUD bits are the CBAUD bits
// shifted by this. It is the kernel's IBSHIFT, which not every libc header defines.
inline constexpr int kSerialInputSpeedShift = 16;
#endif

// What a port should be configured to. QSerialPort keeps one of these, so that
// settings made while the port is closed take effect in open().
struct SerialSettings {
    qint32 inputBaudRate = QSerialPort::Baud9600;
    qint32 outputBaudRate = QSerialPort::Baud9600;
    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    QSerialPort::Parity parity = QSerialPort::NoParity;
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl;
};

// The operation a failure belongs to, which decides the error it is reported as.
enum class SerialOperation { Open, Read, Write, Control };

#ifndef __ZEPHYR__
// Rewrites tio for raw binary I/O with the given settings. Returns false with
// errno set if the settings cannot be expressed. A rate with no B constant
// leaves a placeholder speed, for SerialEngine::apply() to replace.
bool fillSerialTermios(termios &tio, const SerialSettings &settings, bool readable);
#endif

QList<qint32> serialStandardBaudRates();
QString serialSystemLocation(const QString &portName);
QString serialPortName(const QString &systemLocation);

class SerialEngine {
public:
    bool open(const QString &path, QIODeviceBase::OpenMode mode, const SerialSettings &settings);
    void close();
    int handle() const { return m_fd; }

    // What the write notifier of QSerialPort watches. A terminal is watched for writing; on Zephyr an eventfd, which
    // the driver signals when its buffer has room again, is watched for reading.
#ifdef __ZEPHYR__
    int writeNotifierHandle() const { return m_txFd; }
    static constexpr QSocketNotifier::Type kWriteNotifierType = QSocketNotifier::Read;
#else
    int writeNotifierHandle() const { return m_fd; }
    static constexpr QSocketNotifier::Type kWriteNotifierType = QSocketNotifier::Write;
#endif

    bool apply(const SerialSettings &settings);
    bool pins(QSerialPort::PinoutSignals *out);
    bool setPins(QSerialPort::PinoutSignals which, bool on);
    bool setBreak(bool on);
    bool discard(QSerialPort::Directions directions);

    // Both return the byte count, 0 if the call would block, and -1 on failure.
    qint64 read(char *data, qint64 maxSize);
    qint64 write(const char *data, qint64 maxSize);

    // Waits up to msecs (forever if negative). Returns 1 when something is
    // ready, 0 on timeout and -1 on failure. A hangup counts as ready.
    int wait(bool wantRead, bool wantWrite, int msecs, bool *readable, bool *writable);

    QSerialPort::SerialPortError error(SerialOperation operation) const;
    QString errorString() const;

private:
    bool failed();

    int m_fd = -1; // on Zephyr, the eventfd that the driver signals when bytes arrive
    int m_errno = 0;
    bool m_readable = false;
#ifdef __ZEPHYR__
    int m_txFd = -1;
    struct ZephyrUart *m_uart = nullptr;
#else
    termios m_saved = {};
#endif
};

} // namespace QtPrivate
