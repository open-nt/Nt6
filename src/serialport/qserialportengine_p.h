// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtSerialPort/qserialport.h>

#include <termios.h>

// The operating system's side of QSerialPort. Everything here is POSIX, so a
// Windows port replaces this header's implementation and leaves qserialport.cpp
// alone apart from its use of QSocketNotifier.
namespace QtPrivate {

// Where the kernel keeps the input speed in c_cflag: the CIBAUD bits are the CBAUD bits
// shifted by this. It is the kernel's IBSHIFT, which not every libc header defines.
inline constexpr int kSerialInputSpeedShift = 16;

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

// Rewrites tio for raw binary I/O with the given settings. Returns false with
// errno set if the settings cannot be expressed. A rate with no B constant
// leaves a placeholder speed, for SerialEngine::apply() to replace.
bool fillSerialTermios(termios &tio, const SerialSettings &settings, bool readable);

QList<qint32> serialStandardBaudRates();
QString serialSystemLocation(const QString &portName);
QString serialPortName(const QString &systemLocation);

class SerialEngine {
public:
    bool open(const QString &path, QIODeviceBase::OpenMode mode, const SerialSettings &settings);
    void close();
    int handle() const { return m_fd; }

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

    int m_fd = -1;
    int m_errno = 0;
    bool m_readable = false;
    termios m_saved = {};
};

} // namespace QtPrivate
