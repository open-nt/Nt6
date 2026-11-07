// MIT License - see LICENSE
//
// A pseudo terminal stands in for the hardware: the port opens the slave side
// and the test plays the device on the master side. A pty has no modem lines
// and ignores CSIZE and PARENB, so those paths are covered through the failures
// they produce and through fillSerialTermios() directly.
#include <QtCore>
#include <QtSerialPort>

#include "qserialportengine_p.h"
#include "qserialportscanner_p.h"

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

// Runs the loop until quit() or the timeout; returns false on timeout.
static bool runLoop(QEventLoop &loop, int timeoutMs = 5000) {
    bool timedOut = false;
    QTimer t;
    t.setSingleShot(true);
    QObject::connect(&t, &QTimer::timeout, [&] {
        timedOut = true;
        loop.quit();
    });
    t.start(timeoutMs);
    loop.exec();
    return !timedOut;
}

static void spinEvents(int msecs) {
    QEventLoop loop;
    QTimer::singleShot(msecs, [&] { loop.quit(); });
    loop.exec();
}

// The input speed of tio. glibc before 2.42 and musl cannot report a separate one, so
// the bits are read here; zero in CIBAUD means the same as the output speed.
static speed_t inputSpeed(const termios &tio) {
    if (B9600 == 9600)
        return ::cfgetispeed(&tio);
    const tcflag_t input = (tio.c_cflag & CIBAUD) >> QtPrivate::kSerialInputSpeedShift;
    return input != 0 ? input : (tio.c_cflag & CBAUD);
}

namespace {

class Pty {
public:
    Pty() {
        master = ::posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
        CHECK(master >= 0 && ::grantpt(master) == 0 && ::unlockpt(master) == 0);
        slave = QString::fromUtf8(::ptsname(master));
    }
    ~Pty() { closeMaster(); }
    Pty(const Pty &) = delete;
    Pty &operator=(const Pty &) = delete;

    void closeMaster() {
        if (master >= 0)
            ::close(master);
        master = -1;
    }
    void feed(const QByteArray &data) const {
        CHECK(::write(master, data.constData(), static_cast<std::size_t>(data.size())) == data.size());
    }
    // Everything the port has written so far.
    QByteArray drain() const {
        QByteArray out;
        char buf[65536];
        ssize_t n;
        while ((n = ::read(master, buf, sizeof buf)) > 0)
            out.append(buf, static_cast<qsizetype>(n));
        return out;
    }
    QByteArray waitForBytes(qsizetype count) const {
        QByteArray out;
        QElapsedTimer t;
        t.start();
        while (out.size() < count && t.elapsed() < 5000) {
            pollfd p = {master, POLLIN, 0};
            ::poll(&p, 1, 50);
            out += drain();
        }
        return out;
    }

    int master = -1;
    QString slave;
};

} // namespace

static void testClosedPort() {
    QSerialPort port;
    CHECK(port.portName().isEmpty() && !port.isOpen() && port.handle() == -1);
    CHECK(port.baudRate() == 9600 && port.dataBits() == QSerialPort::Data8 && port.parity() == QSerialPort::NoParity);
    CHECK(port.stopBits() == QSerialPort::OneStop && port.flowControl() == QSerialPort::NoFlowControl);
    CHECK(port.isSequential() && port.error() == QSerialPort::NoError);

    port.setPortName("ttyUSB7");
    CHECK(port.portName() == "ttyUSB7");
    port.setPortName("/dev/ttyUSB8");
    CHECK(port.portName() == "ttyUSB8");
    port.setPortName("/tmp/link");
    CHECK(port.portName() == "/tmp/link");

    QList<QSerialPort::SerialPortError> errors;
    QObject::connect(&port, &QSerialPort::errorOccurred, [&](QSerialPort::SerialPortError e) { errors.append(e); });

    // Each failure reports once.
    port.close();
    CHECK(errors.size() == 1 && errors.last() == QSerialPort::NotOpenError);
    CHECK(!port.setDataTerminalReady(true) && !port.setBreakEnabled() && !port.clear());
    CHECK(errors.size() == 4 && port.error() == QSerialPort::NotOpenError);
    CHECK(port.pinoutSignals() == QSerialPort::PinoutSignals(QSerialPort::NoSignal));
    CHECK(!port.waitForReadyRead(0) && !port.waitForBytesWritten(0) && port.write("x") == -1 && port.read(1).isEmpty());
    port.clearError();
    CHECK(port.error() == QSerialPort::NoError && errors.size() == 7);

    CHECK(!port.setBaudRate(0) && !port.setBaudRate(-5) && port.baudRate() == 9600);
    CHECK(port.error() == QSerialPort::UnsupportedOperationError);

    // Settings stick while closed, and each reports the change once.
    int rates = 0, bits = 0, parities = 0, stops = 0, flows = 0;
    QObject::connect(&port, &QSerialPort::baudRateChanged, [&](qint32, QSerialPort::Directions) { ++rates; });
    QObject::connect(&port, &QSerialPort::dataBitsChanged, [&](QSerialPort::DataBits) { ++bits; });
    QObject::connect(&port, &QSerialPort::parityChanged, [&](QSerialPort::Parity) { ++parities; });
    QObject::connect(&port, &QSerialPort::stopBitsChanged, [&](QSerialPort::StopBits) { ++stops; });
    QObject::connect(&port, &QSerialPort::flowControlChanged, [&](QSerialPort::FlowControl) { ++flows; });
    for (int i = 0; i < 2; ++i) {
        CHECK(port.setBaudRate(QSerialPort::Baud115200) && port.setDataBits(QSerialPort::Data7));
        CHECK(port.setParity(QSerialPort::EvenParity) && port.setStopBits(QSerialPort::TwoStop));
        CHECK(port.setFlowControl(QSerialPort::SoftwareControl));
    }
    CHECK(rates == 1 && bits == 1 && parities == 1 && stops == 1 && flows == 1);
    CHECK(port.baudRate() == 115200 && port.dataBits() == QSerialPort::Data7);

    CHECK(port.setBaudRate(4800, QSerialPort::Input));
    CHECK(port.baudRate(QSerialPort::Input) == 4800 && port.baudRate(QSerialPort::Output) == 115200);
    CHECK(port.baudRate() == -1);
}

static void testOpenFailures() {
    QSerialPort missing("/dev/nt-no-such-device");
    CHECK(!missing.open(QIODevice::ReadWrite) && missing.error() == QSerialPort::DeviceNotFoundError);
    CHECK(!missing.isOpen() && missing.errorString() != "Unknown error");

    QSerialPort unnamed;
    CHECK(!unnamed.open(QIODevice::ReadWrite) && unnamed.error() == QSerialPort::DeviceNotFoundError);

    // A device that is no terminal is refused, and leaves nothing open.
    QSerialPort notTerminal("/dev/null");
    CHECK(!notTerminal.open(QIODevice::ReadWrite) && notTerminal.error() == QSerialPort::UnsupportedOperationError);
    CHECK(!notTerminal.isOpen() && notTerminal.handle() == -1);

    Pty pty;
    QSerialPort port(pty.slave);
    CHECK(!port.open(QIODevice::NotOpen) && port.error() == QSerialPort::UnsupportedOperationError);
    CHECK(!port.open(QIODevice::ReadWrite | QIODevice::Text) && port.error() == QSerialPort::UnsupportedOperationError);
    CHECK(!port.open(QIODevice::ReadWrite | QIODevice::Append) && !port.isOpen());

    // Settings the system cannot do fail the open and leave the device free.
    CHECK(port.setStopBits(QSerialPort::OneAndHalfStop));
    CHECK(!port.open(QIODevice::ReadWrite) && port.error() == QSerialPort::UnsupportedOperationError);
    CHECK(!port.isOpen() && port.handle() == -1);
    CHECK(port.setStopBits(QSerialPort::OneStop) && port.open(QIODevice::ReadWrite));
    CHECK(port.error() == QSerialPort::NoError && port.handle() >= 0);
    CHECK(!port.open(QIODevice::ReadWrite) && port.error() == QSerialPort::OpenError && port.isOpen());
    CHECK(!port.setStopBits(QSerialPort::OneAndHalfStop) && port.stopBits() == QSerialPort::OneStop);
}

static void testExclusiveAccess() {
    Pty pty;
    QSerialPort first(pty.slave);
    QSerialPort second(pty.slave);
    CHECK(first.open(QIODevice::ReadWrite));
    CHECK(!second.open(QIODevice::ReadWrite) && second.error() == QSerialPort::PermissionError);
    first.close();
    CHECK(second.open(QIODevice::ReadWrite));
}

static void testEventDrivenIo() {
    Pty pty;
    QSerialPort port(pty.slave);
    CHECK(port.open(QIODevice::ReadWrite));

    QEventLoop loop;
    QByteArray received;
    qint64 written = 0;
    QObject::connect(&port, &QIODevice::bytesWritten, [&](qint64 n) { written += n; });
    QObject::connect(&port, &QIODevice::readyRead, [&] {
        received += port.readAll();
        if (received.size() >= 6)
            loop.quit();
    });

    // Bytes that a terminal in its default mode would translate or act on.
    const QByteArray binary("\r\n\x03\x04\x11\x13\x7f\0\xff", 9);
    CHECK(port.write(binary) == binary.size());
    CHECK(port.bytesToWrite() == binary.size());
    spinEvents(50);
    CHECK(pty.waitForBytes(binary.size()) == binary);
    CHECK(port.bytesToWrite() == 0 && written == binary.size());

    pty.feed("reply\n");
    CHECK(runLoop(loop));
    CHECK(received == "reply\n");

    const QByteArray raw("\r\n\x03\x11\x7f\0", 6);
    received.clear();
    pty.feed(raw);
    CHECK(runLoop(loop));
    CHECK(received == raw);
}

static void testBlockingIo() {
    Pty pty;
    QSerialPort port(pty.slave);
    CHECK(port.open(QIODevice::ReadWrite));

    CHECK(!port.waitForReadyRead(10) && port.error() == QSerialPort::TimeoutError);
    CHECK(!port.waitForBytesWritten(10));

    int ready = 0;
    QObject::connect(&port, &QIODevice::readyRead, [&] { ++ready; });
    pty.feed("one\ntwo");
    CHECK(port.waitForReadyRead(2000) && ready == 1);
    CHECK(port.bytesAvailable() == 7 && port.canReadLine());
    char c = 0;
    CHECK(port.peek(&c, 1) == 1 && c == 'o' && port.bytesAvailable() == 7);
    CHECK(port.readLine() == "one\n");
    CHECK(!port.canReadLine() && port.bytesAvailable() == 3);
    CHECK(port.readAll() == "two" && port.atEnd());

    CHECK(port.write("ping") == 4 && port.waitForBytesWritten(2000) && port.bytesToWrite() == 0);
    CHECK(pty.waitForBytes(4) == "ping");

    // flush() moves data with no event loop and no wait.
    CHECK(!port.flush());
    CHECK(port.write("pong") == 4 && port.flush() && port.bytesToWrite() == 0);
    CHECK(pty.waitForBytes(4) == "pong");

    // More than the driver holds: the rest stays queued until the far end reads.
    QByteArray big(1024 * 1024, '\0');
    for (qsizetype i = 0; i < big.size(); ++i)
        big[i] = static_cast<char>(i * 31);
    CHECK(port.write(big) == big.size());
    port.clearError();
    CHECK(!port.waitForBytesWritten(50) && port.error() == QSerialPort::TimeoutError);
    CHECK(port.bytesToWrite() > 0 && port.bytesToWrite() < big.size());
    QByteArray got;
    QElapsedTimer t;
    t.start();
    while (got.size() < big.size() && t.elapsed() < 20000) {
        got += pty.drain();
        port.waitForBytesWritten(5);
    }
    got += pty.drain();
    CHECK(got == big && port.bytesToWrite() == 0);
}

static void testSettingsReachTheDevice() {
    Pty pty;
    // Stays open to look at the device, also before and after the port uses it.
    const int observer = ::open(pty.slave.toUtf8().constData(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    CHECK(observer >= 0);
    termios original;
    CHECK(::tcgetattr(observer, &original) == 0);
    CHECK((original.c_lflag & ICANON) != 0);

    QSerialPort port(pty.slave);
    CHECK(port.setBaudRate(QSerialPort::Baud57600) && port.setStopBits(QSerialPort::TwoStop));
    CHECK(port.open(QIODevice::ReadWrite));
    termios tio;
    CHECK(::tcgetattr(observer, &tio) == 0);
    CHECK(::cfgetospeed(&tio) == B57600 && inputSpeed(tio) == B57600);
    CHECK((tio.c_cflag & CSTOPB) != 0 && (tio.c_lflag & (ICANON | ECHO | ISIG)) == 0 && (tio.c_oflag & OPOST) == 0);

    // Changes on an open port apply at once.
    CHECK(port.setBaudRate(QSerialPort::Baud115200));
    CHECK(port.setFlowControl(QSerialPort::HardwareControl));
    CHECK(::tcgetattr(observer, &tio) == 0);
    CHECK(::cfgetospeed(&tio) == B115200 && (tio.c_cflag & CRTSCTS) != 0 && (tio.c_iflag & IXON) == 0);
    CHECK(!port.setRequestToSend(true) && port.error() == QSerialPort::UnsupportedOperationError);
    CHECK(port.setFlowControl(QSerialPort::SoftwareControl));
    CHECK(::tcgetattr(observer, &tio) == 0);
    CHECK((tio.c_cflag & CRTSCTS) == 0 && (tio.c_iflag & (IXON | IXOFF)) == (IXON | IXOFF));

    // A rate with no B constant, and back to one that has.
    CHECK(port.setBaudRate(250000) && port.baudRate() == 250000);
    CHECK(::tcgetattr(observer, &tio) == 0);
    CHECK((tio.c_cflag & CBAUD) != B115200);
    CHECK(port.setBaudRate(QSerialPort::Baud9600));
    CHECK(::tcgetattr(observer, &tio) == 0);
    CHECK(::cfgetospeed(&tio) == B9600);

    // The device is left as it was found.
    port.close();
    CHECK(::tcgetattr(observer, &tio) == 0);
    CHECK(tio.c_iflag == original.c_iflag && tio.c_oflag == original.c_oflag);
    CHECK(tio.c_cflag == original.c_cflag && tio.c_lflag == original.c_lflag);
    ::close(observer);
}

static void testTermiosFlags() {
    using QtPrivate::fillSerialTermios;
    using QtPrivate::SerialSettings;

    const auto fill = [](const SerialSettings &settings, bool readable = true) {
        termios tio = {};
        tio.c_iflag = ICRNL | IXON | ISTRIP | INPCK;
        tio.c_oflag = OPOST;
        tio.c_lflag = ICANON | ECHO | ISIG | IEXTEN;
        tio.c_cflag = CS8 | PARENB | PARODD | CSTOPB | CMSPAR | CRTSCTS;
        CHECK(fillSerialTermios(tio, settings, readable));
        return tio;
    };

    termios tio = fill(SerialSettings());
    CHECK((tio.c_cflag & CSIZE) == CS8 && (tio.c_cflag & (PARENB | PARODD | CMSPAR | CSTOPB | CRTSCTS)) == 0);
    CHECK((tio.c_cflag & (CLOCAL | CREAD)) == (CLOCAL | CREAD));
    CHECK(tio.c_iflag == 0 && tio.c_oflag == 0 && tio.c_lflag == 0);
    CHECK(tio.c_cc[VMIN] == 1 && tio.c_cc[VTIME] == 0);
    CHECK(::cfgetospeed(&tio) == B9600 && inputSpeed(tio) == B9600);
    CHECK((fill(SerialSettings(), false).c_cflag & CREAD) == 0);

    SerialSettings s;
    const struct {
        QSerialPort::DataBits bits;
        tcflag_t flag;
    } sizes[] = {
        {QSerialPort::Data5, CS5}, {QSerialPort::Data6, CS6}, {QSerialPort::Data7, CS7}, {QSerialPort::Data8, CS8}};
    for (const auto &size : sizes) {
        s.dataBits = size.bits;
        CHECK((fill(s).c_cflag & CSIZE) == size.flag);
    }

    const struct {
        QSerialPort::Parity parity;
        tcflag_t flags;
    } parities[] = {{QSerialPort::EvenParity, PARENB},
                    {QSerialPort::OddParity, PARENB | PARODD},
                    {QSerialPort::SpaceParity, PARENB | CMSPAR},
                    {QSerialPort::MarkParity, PARENB | PARODD | CMSPAR}};
    for (const auto &entry : parities) {
        s.parity = entry.parity;
        tio = fill(s);
        CHECK((tio.c_cflag & (PARENB | PARODD | CMSPAR)) == entry.flags);
        CHECK((tio.c_iflag & (INPCK | IGNPAR)) == (INPCK | IGNPAR));
    }

    s = SerialSettings();
    s.inputBaudRate = 2400;
    s.outputBaudRate = 19200;
    tio = fill(s);
    CHECK(::cfgetospeed(&tio) == B19200 && inputSpeed(tio) == B2400);
    s.inputBaudRate = 19200;
    tio = fill(s);
    CHECK(::cfgetospeed(&tio) == B19200 && inputSpeed(tio) == B19200);

    s = SerialSettings();
    s.dataBits = static_cast<QSerialPort::DataBits>(9);
    termios bad = {};
    CHECK(!fillSerialTermios(bad, s, true));
    s = SerialSettings();
    s.stopBits = QSerialPort::OneAndHalfStop;
    CHECK(!fillSerialTermios(bad, s, true));
}

static void testOpenModes() {
    Pty pty;
    QSerialPort writer(pty.slave);
    CHECK(writer.open(QIODevice::WriteOnly) && writer.isWritable() && !writer.isReadable());
    CHECK(writer.write("out") == 3 && writer.waitForBytesWritten(2000));
    CHECK(pty.waitForBytes(3) == "out");
    CHECK(!writer.waitForReadyRead(0));
    writer.close();

    QSerialPort reader(pty.slave);
    CHECK(reader.open(QIODevice::ReadOnly) && reader.isReadable() && !reader.isWritable());
    CHECK(reader.write("x") == -1);
    pty.feed("in");
    CHECK(reader.waitForReadyRead(2000) && reader.readAll() == "in");
}

static void testBuffers() {
    Pty pty;
    QSerialPort port(pty.slave);
    CHECK(port.open(QIODevice::ReadWrite));

    // A limit holds data back in the driver until it is read.
    port.setReadBufferSize(16);
    CHECK(port.readBufferSize() == 16);
    QByteArray sent;
    for (int i = 0; i < 100; ++i)
        sent.append(static_cast<char>('a' + i % 26));
    pty.feed(sent);
    CHECK(port.waitForReadyRead(2000));
    CHECK(port.bytesAvailable() == 16);
    CHECK(port.waitForReadyRead(0) && port.bytesAvailable() == 16); // nothing to wait for while full
    QByteArray got = port.read(6);
    spinEvents(20);
    CHECK(port.bytesAvailable() == 16); // reading made room and more came in
    port.setReadBufferSize(0);
    spinEvents(20);
    got += port.readAll();
    CHECK(got == sent);

    pty.feed("stale");
    CHECK(port.waitForReadyRead(2000) && port.peek(2) == "st");
    CHECK(port.clear(QSerialPort::Input) && port.bytesAvailable() == 0);
    CHECK(port.write("gone") == 4 && port.clear(QSerialPort::Output) && port.bytesToWrite() == 0);
    spinEvents(20);
    CHECK(pty.drain().isEmpty());

    pty.feed("pending");
    CHECK(port.waitForReadyRead(2000) && port.bytesAvailable() > 0);
    CHECK(port.write("lost") == 4);
    port.close();
    CHECK(port.bytesAvailable() == 0 && port.bytesToWrite() == 0);
    CHECK(pty.drain().isEmpty());
}

static void testLinesAndBreak() {
    Pty pty;
    QSerialPort port(pty.slave);
    CHECK(port.open(QIODevice::ReadWrite));

    // A pty has no modem lines; each attempt must fail cleanly, and only once.
    int errors = 0;
    QObject::connect(&port, &QSerialPort::errorOccurred, [&](QSerialPort::SerialPortError) { ++errors; });
    CHECK(!port.setDataTerminalReady(true) && errors == 1);
    CHECK(port.error() == QSerialPort::UnsupportedOperationError);
    CHECK(!port.setRequestToSend(true) && errors == 2);
    CHECK(port.pinoutSignals() == QSerialPort::PinoutSignals(QSerialPort::NoSignal) && errors == 3);
    CHECK(!port.isDataTerminalReady() && !port.isRequestToSend());

    QList<int> changes;
    QObject::connect(&port, &QSerialPort::breakEnabledChanged, [&](bool on) { changes.append(on ? 1 : 0); });
    CHECK(port.setBreakEnabled() && port.isBreakEnabled());
    CHECK(port.setBreakEnabled(true) && changes.size() == 1);
    CHECK(port.setBreakEnabled(false) && !port.isBreakEnabled() && changes.size() == 2 && !changes.last());

    // close() does not leave the line in the break state.
    CHECK(port.setBreakEnabled());
    port.close();
    CHECK(!port.isBreakEnabled());
}

static void testDeviceRemoved() {
    auto pty = std::make_unique<Pty>();
    QSerialPort port(pty->slave);
    CHECK(port.open(QIODevice::ReadWrite));
    pty->feed("last");
    CHECK(port.waitForReadyRead(2000));

    QEventLoop loop;
    QList<QSerialPort::SerialPortError> errors;
    QObject::connect(&port, &QSerialPort::errorOccurred, [&](QSerialPort::SerialPortError e) {
        errors.append(e);
        loop.quit();
    });
    pty.reset();
    CHECK(runLoop(loop));
    CHECK(errors.size() == 1 && errors.last() == QSerialPort::ResourceError);

    // The port goes idle: no more events, no failure loop, and the data stays.
    spinEvents(50);
    CHECK(errors.size() == 1 && port.isOpen() && port.readAll() == "last");
    CHECK(port.write("x") == -1 && !port.waitForReadyRead(1000) && !port.waitForBytesWritten(1000));
    port.close();
    CHECK(!port.isOpen() && port.handle() == -1);

    // Noticed by a blocking wait too, with data queued for writing.
    Pty blocking;
    QSerialPort writer(blocking.slave);
    CHECK(writer.open(QIODevice::ReadWrite));
    CHECK(writer.write("queued") == 6);
    blocking.closeMaster();
    CHECK(!writer.waitForBytesWritten(2000) && writer.error() == QSerialPort::ResourceError);
    CHECK(writer.bytesToWrite() == 0);
}

static void testLifetime() {
    // Deleting the port from a slot.
    Pty pty;
    QEventLoop loop;
    auto *port = new QSerialPort(pty.slave);
    CHECK(port->open(QIODevice::ReadWrite));
    QObject::connect(port, &QIODevice::readyRead, [&] {
        delete port;
        port = nullptr;
        loop.quit();
    });
    pty.feed("x");
    CHECK(runLoop(loop));
    CHECK(port == nullptr);

    // The same inside a blocking wait.
    auto *waiting = new QSerialPort(pty.slave);
    CHECK(waiting->open(QIODevice::ReadWrite));
    QObject::connect(waiting, &QIODevice::readyRead, [&] {
        delete waiting;
        waiting = nullptr;
    });
    pty.feed("y");
    CHECK(waiting->waitForReadyRead(2000) && waiting == nullptr);

    // Closing from a slot, and in the destructor with pending data.
    QSerialPort closing(pty.slave);
    CHECK(closing.open(QIODevice::ReadWrite));
    QObject::connect(&closing, &QIODevice::readyRead, [&] { closing.close(); });
    pty.feed("z");
    CHECK(closing.waitForReadyRead(2000) && !closing.isOpen());

    {
        QSerialPort pending(pty.slave);
        CHECK(pending.open(QIODevice::ReadWrite) && pending.write("left") == 4);
    }
    // A closed port can be opened again, and its old notifiers are gone.
    QSerialPort again(pty.slave);
    for (int i = 0; i < 3; ++i) {
        CHECK(again.open(QIODevice::ReadWrite));
        spinEvents(5);
        again.close();
    }
    CHECK(again.open(QIODevice::ReadWrite));
    pty.feed("back");
    CHECK(again.waitForReadyRead(2000) && again.readAll() == "back");
}

static void write(const QString &path, const QByteArray &contents) {
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly));
    file.write(contents);
}

static void link(const QString &target, const QString &path) {
    QDir().mkpath(QFileInfo(path).path());
    CHECK(::symlink(target.toUtf8().constData(), path.toUtf8().constData()) == 0);
}

static void testPortList() {
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString sys = dir.path() + "/sys";
    const QString dev = dir.path() + "/dev";

    // A USB adapter: the identity is on an ancestor of the tty device.
    const QString usb = sys + "/devices/pci0000:00/usb1/1-1";
    write(usb + "/idVendor", "0403\n");
    write(usb + "/idProduct", "6001\n");
    write(usb + "/manufacturer", "FTDI\n");
    write(usb + "/product", "FT232R USB UART\n");
    write(usb + "/serial", "A50285BI\n");
    const QString usbTty = usb + "/1-1:1.0/ttyUSB0/tty/ttyUSB0";
    write(usbTty + "/dev", "188:0\n");
    link(usbTty, sys + "/class/tty/ttyUSB0");
    link("../../../ttyUSB0", usbTty + "/device");
    write(dev + "/ttyUSB0", "");

    // A PCI card, whose identifiers look different.
    const QString pci = sys + "/devices/pci0000:00/0000:00:16.3";
    write(pci + "/vendor", "0x8086\n");
    write(pci + "/device", "0x1c3d\n");
    const QString pciTty = pci + "/tty/ttyS4";
    write(pciTty + "/type", "4\n");
    link(pciTty, sys + "/class/tty/ttyS4");
    link("../../../0000:00:16.3", pciTty + "/device");
    write(dev + "/ttyS4", "");

    // A port on the mainboard, which identifies itself no further.
    const QString platform = sys + "/devices/platform/serial8250";
    const QString realTty = platform + "/tty/ttyS0";
    write(realTty + "/type", "4\n");
    link(realTty, sys + "/class/tty/ttyS0");
    link("../../../serial8250", realTty + "/device");
    write(dev + "/ttyS0", "");

    // None of these are ports: a slot the driver found nothing in, a virtual
    // terminal, and a device with no node.
    const QString phantomTty = platform + "/tty/ttyS1";
    write(phantomTty + "/type", "0\n");
    link(phantomTty, sys + "/class/tty/ttyS1");
    link("../../../serial8250", phantomTty + "/device");
    write(dev + "/ttyS1", "");
    const QString virtualTty = sys + "/devices/virtual/tty/tty0";
    write(virtualTty + "/dev", "4:0\n");
    link(virtualTty, sys + "/class/tty/tty0");
    write(dev + "/tty0", "");
    const QString noNode = platform + "/tty/ttyS2";
    write(noNode + "/type", "4\n");
    link(noNode, sys + "/class/tty/ttyS2");
    link("../../../serial8250", noNode + "/device");

    const QList<QSerialPortInfo> ports = QtPrivate::SerialPortScanner::scan(sys, dev);
    CHECK(ports.size() == 3);
    if (ports.size() != 3)
        return;

    const QSerialPortInfo &plain = ports.at(0);
    CHECK(plain.portName() == "ttyS0" && plain.systemLocation() == dev + "/ttyS0" && !plain.isNull());
    CHECK(!plain.hasVendorIdentifier() && !plain.hasProductIdentifier() && plain.vendorIdentifier() == 0);
    CHECK(plain.description().isEmpty() && plain.manufacturer().isEmpty() && plain.serialNumber().isEmpty());

    const QSerialPortInfo &card = ports.at(1);
    CHECK(card.portName() == "ttyS4" && card.vendorIdentifier() == 0x8086 && card.productIdentifier() == 0x1c3d);
    CHECK(card.hasVendorIdentifier() && card.hasProductIdentifier() && card.description().isEmpty());

    QSerialPortInfo adapter = ports.at(2);
    CHECK(adapter.portName() == "ttyUSB0" && adapter.systemLocation() == dev + "/ttyUSB0");
    CHECK(adapter.vendorIdentifier() == 0x0403 && adapter.productIdentifier() == 0x6001);
    CHECK(adapter.manufacturer() == "FTDI" && adapter.description() == "FT232R USB UART");
    CHECK(adapter.serialNumber() == "A50285BI");

    QSerialPortInfo other;
    CHECK(other.isNull());
    other.swap(adapter);
    CHECK(adapter.isNull() && other.portName() == "ttyUSB0");

    QSerialPort port(other);
    CHECK(port.portName() == dev + "/ttyUSB0");

    CHECK(QtPrivate::SerialPortScanner::scan(dir.path() + "/none", dev).isEmpty());
}

static void testSystemPorts() {
    const QList<qint32> rates = QSerialPortInfo::standardBaudRates();
    CHECK(rates.contains(9600) && rates.contains(115200) && !rates.contains(0));

    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports)
        CHECK(!info.isNull() && QFileInfo::exists(info.systemLocation()));
    CHECK(QSerialPortInfo("nt-no-such-port").isNull());
    CHECK(QSerialPortInfo(QSerialPort("nt-no-such-port")).isNull());
}

static void testMetaObject() {
    QSerialPort port;
    const QMetaObject *mo = port.metaObject();
    CHECK(std::string(mo->className()) == "QSerialPort");
    for (const char *name : {"baudRate", "dataBits", "parity", "stopBits", "flowControl", "dataTerminalReady",
                             "requestToSend", "error", "breakEnabled"}) {
        const QMetaProperty property = mo->property(mo->indexOfProperty(name));
        CHECK(property.isValid() && property.hasNotifySignal());
    }

    CHECK(port.setProperty("baudRate", 57600) && port.baudRate() == 57600);
    CHECK(port.property("baudRate").toInt() == 57600);
    const QMetaProperty parity = mo->property(mo->indexOfProperty("parity"));
    CHECK(parity.isEnumType() && std::string(parity.enumerator().name()) == "Parity");
    CHECK(std::string(parity.enumerator().valueToKey(QSerialPort::MarkParity)) == "MarkParity");

    const QMetaEnum errors = mo->enumerator(mo->indexOfEnumerator("SerialPortError"));
    CHECK(errors.isValid() && std::string(errors.valueToKey(QSerialPort::NotOpenError)) == "NotOpenError");
    const QMetaEnum directions = mo->enumerator(mo->indexOfEnumerator("Directions"));
    CHECK(directions.isValid() && directions.isFlag());
    for (const char *name : {"BaudRate", "DataBits", "StopBits", "FlowControl", "PinoutSignals"})
        CHECK(mo->indexOfEnumerator(name) >= 0);

    // The error property resets through clearError().
    port.close();
    CHECK(port.error() == QSerialPort::NotOpenError);
    CHECK(mo->property(mo->indexOfProperty("error")).reset(&port) && port.error() == QSerialPort::NoError);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testClosedPort();
    testOpenFailures();
    testExclusiveAccess();
    testEventDrivenIo();
    testBlockingIo();
    testSettingsReachTheDevice();
    testTermiosFlags();
    testOpenModes();
    testBuffers();
    testLinesAndBreak();
    testDeviceRemoved();
    testLifetime();
    testPortList();
    testSystemPorts();
    testMetaObject();
    if (g_failures == 0)
        std::puts("all serial port tests passed");
    return g_failures == 0 ? 0 : 1;
}
