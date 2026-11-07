// MIT License - see LICENSE
#include "qserialportengine_p.h"

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qpointer.h>
#include <QtCore/qsocketnotifier.h>
#include <QtSerialPort/qserialport.h>
#include <QtSerialPort/qserialportinfo.h>

#include <cstring>

/*!
    \module NtSerialPort
    \title Nt Serial Port C++ Classes
    \brief Classes for talking to serial devices.
    \cmaketarget Nt::SerialPort

    Nt Serial Port provides an event-driven serial port built on the Nt Core
    event loop, and a way to list the ports of the machine.
*/

/*!
    \class QSerialPort
    \brief The QSerialPort class provides access to a serial port.
    \since 6.0

    Name the port, open it, then read and write as with any QIODevice. Settings
    made before open() take effect when the port opens; settings made later
    take effect at once.

    \code
    QSerialPort port("ttyUSB0");
    port.setBaudRate(QSerialPort::Baud115200);
    if (!port.open(QIODevice::ReadWrite)) {
        qWarning() << port.errorString();
        return;
    }
    QObject::connect(&port, &QIODevice::readyRead, [&] { handle(port.readAll()); });
    port.write("AT\r\n");
    \endcode

    A new port is set to 9600 baud, 8 data bits, no parity, one stop bit and no
    flow control. Whatever the device was set to before is restored by close().

    Reads and writes do not block. Data moves while the event loop runs, or
    inside waitForReadyRead() and waitForBytesWritten() in a program without an
    event loop.

    A port is opened exclusively. Opening a port that another QSerialPort has
    open fails with \c PermissionError, in this process or any other.

    When the device goes away, for example when a USB adapter is unplugged,
    errorOccurred() is emitted with \c ResourceError. The port then stays open
    but idle, and data received earlier can still be read. Call close(), and
    open() again once the device is back.

    \differences
    Bytes received with a parity or framing error are dropped. \c OneAndHalfStop
    is not available, because POSIX has no such setting; it makes open() or
    setStopBits() fail. errorOccurred() is not emitted for \c NoError, which
    Qt does when an error is cleared. There are no bindable properties.
    waitForBytesWritten() waits for the whole buffer, not for one chunk of it.
    Exclusive access uses \c flock() and \c TIOCEXCL rather than a lock file in
    \c{/var/lock}. Ports are POSIX terminal devices; only Linux has been tested.
    \enddifferences

    \sa QSerialPortInfo
*/

/*!
    \enum QSerialPort::Direction

    Describes the direction of data transmission.

    \value Input Data received by the port.
    \value Output Data sent by the port.
    \value AllDirections Both directions.
*/

/*!
    \typedef QSerialPort::Directions

    A QFlags type holding \c Direction values.
*/

/*!
    \enum QSerialPort::BaudRate

    Common baud rates. Any positive rate can be passed to setBaudRate().

    \value Baud1200 1200 baud.
    \value Baud2400 2400 baud.
    \value Baud4800 4800 baud.
    \value Baud9600 9600 baud.
    \value Baud19200 19200 baud.
    \value Baud38400 38400 baud.
    \value Baud57600 57600 baud.
    \value Baud115200 115200 baud.
*/

/*!
    \enum QSerialPort::DataBits

    The number of data bits in a character.

    \value Data5 Five bits, mostly used by old teleprinters.
    \value Data6 Six bits.
    \value Data7 Seven bits, used for plain ASCII.
    \value Data8 Eight bits, used for almost everything else.
*/

/*!
    \enum QSerialPort::Parity

    The parity bit added to each character.

    \value NoParity No parity bit.
    \value EvenParity The number of set bits, parity included, is even.
    \value OddParity The number of set bits, parity included, is odd.
    \value SpaceParity The parity bit is always 0.
    \value MarkParity The parity bit is always 1.
*/

/*!
    \enum QSerialPort::StopBits

    The number of stop bits after each character.

    \value OneStop One stop bit.
    \value OneAndHalfStop One and a half stop bits. Not supported; see the
           class documentation.
    \value TwoStop Two stop bits.
*/

/*!
    \enum QSerialPort::FlowControl

    How the two ends hold each other back.

    \value NoFlowControl No flow control.
    \value HardwareControl The RTS and CTS lines.
    \value SoftwareControl XON and XOFF characters. These can no longer appear
           in the data.
*/

/*!
    \enum QSerialPort::PinoutSignal

    The lines of a serial connection.

    \value NoSignal No line is active.
    \value DataTerminalReadySignal DTR.
    \value DataCarrierDetectSignal DCD.
    \value DataSetReadySignal DSR.
    \value RingIndicatorSignal RI.
    \value RequestToSendSignal RTS.
    \value ClearToSendSignal CTS.
    \value SecondaryTransmittedDataSignal STD.
    \value SecondaryReceivedDataSignal SRD.
*/

/*!
    \typedef QSerialPort::PinoutSignals

    A QFlags type holding \c PinoutSignal values.
*/

/*!
    \enum QSerialPort::SerialPortError

    Describes an error on a serial port.

    \value NoError No error occurred.
    \value DeviceNotFoundError The device does not exist.
    \value PermissionError The process may not open the device, or something
           else has it open.
    \value OpenError The device could not be opened, or is already open in
           this object.
    \value WriteError Writing to the device failed.
    \value ReadError Reading from the device failed.
    \value ResourceError The device went away, or the system ran out of
           resources.
    \value UnsupportedOperationError The device or the system cannot do this.
    \value UnknownError An unidentified error occurred.
    \value TimeoutError A wait timed out.
    \value NotOpenError The operation needs an open port.
*/

/*!
    \typedef QSerialPort::Handle

    The native type of a port, a file descriptor.
*/

namespace {
const qint64 kChunk = 65536; // also how far a read buffer may lag before it is compacted
}

class QSerialPortPrivate {
public:
    explicit QSerialPortPrivate(QSerialPort *port) : q(port) {}

    void setError(QSerialPort::SerialPortError code, const QString &text);
    void setEngineError(QtPrivate::SerialOperation operation) {
        setError(engine.error(operation), engine.errorString());
    }
    void setNotOpen() { setError(QSerialPort::NotOpenError, QString("The device is not open")); }
    void fail(QtPrivate::SerialOperation operation);

    bool applySettings(const QtPrivate::SerialSettings &next);
    bool setPin(QSerialPort::PinoutSignal pin, bool on, bool *changed);

    void startNotifiers();
    void stopNotifiers();
    void updateNotifiers();

    qint64 buffered() const { return rbuf.size() - rpos; }
    bool readBufferFull() const { return readLimit > 0 && buffered() >= readLimit; }

    bool receive();
    bool send();
    bool waitFor(int msecs, bool forRead);

    QSerialPort *q;
    QtPrivate::SerialEngine engine;
    QtPrivate::SerialSettings settings;
    QString location;
    QSerialPort::SerialPortError error = QSerialPort::NoError;
    QByteArray rbuf;
    qint64 rpos = 0;
    QByteArray wbuf;
    qint64 readLimit = 0;
    QSocketNotifier *readNotifier = nullptr;
    QSocketNotifier *writeNotifier = nullptr;
    bool failed = false; // the device stopped working; only close() helps
    bool breakEnabled = false;
    bool emittingReadyRead = false;
};

void QSerialPortPrivate::setError(QSerialPort::SerialPortError code, const QString &text) {
    error = code;
    q->setErrorString(text);
    if (code != QSerialPort::NoError)
        q->errorOccurred(code);
}

void QSerialPortPrivate::fail(QtPrivate::SerialOperation operation) {
    failed = true;
    wbuf.clear();
    updateNotifiers();
    setEngineError(operation);
}

bool QSerialPortPrivate::applySettings(const QtPrivate::SerialSettings &next) {
    if (q->isOpen() && !engine.apply(next)) {
        setEngineError(QtPrivate::SerialOperation::Control);
        return false;
    }
    settings = next;
    return true;
}

bool QSerialPortPrivate::setPin(QSerialPort::PinoutSignal pin, bool on, bool *changed) {
    if (!q->isOpen()) {
        setNotOpen();
        return false;
    }
    QSerialPort::PinoutSignals before = QSerialPort::NoSignal;
    if (!engine.pins(&before) || !engine.setPins(pin, on)) {
        setEngineError(QtPrivate::SerialOperation::Control);
        return false;
    }
    *changed = before.testFlag(pin) != on;
    return true;
}

void QSerialPortPrivate::startNotifiers() {
    const int fd = engine.handle();
    if (q->isReadable()) {
        readNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, q);
        QObject::connect(readNotifier, &QSocketNotifier::activated, q, [this] { receive(); });
    }
    writeNotifier = new QSocketNotifier(fd, QSocketNotifier::Write, q);
    writeNotifier->setEnabled(false);
    QObject::connect(writeNotifier, &QSocketNotifier::activated, q, [this] { send(); });
}

void QSerialPortPrivate::stopNotifiers() {
    for (QSocketNotifier *notifier : {readNotifier, writeNotifier}) {
        if (notifier) {
            notifier->setEnabled(false);
            notifier->deleteLater();
        }
    }
    readNotifier = writeNotifier = nullptr;
}

void QSerialPortPrivate::updateNotifiers() {
    if (readNotifier)
        readNotifier->setEnabled(!failed && !readBufferFull());
    if (writeNotifier)
        writeNotifier->setEnabled(!failed && !wbuf.isEmpty());
}

// Moves what the device has into the read buffer. Returns true if anything
// arrived. The port may have been deleted when this returns.
bool QSerialPortPrivate::receive() {
    if (failed || engine.handle() < 0)
        return false;
    bool got = false;
    bool broken = false;
    while (!readBufferFull()) {
        char chunk[kChunk];
        const qint64 room = readLimit > 0 ? qMin<qint64>(kChunk, readLimit - buffered()) : kChunk;
        const qint64 n = engine.read(chunk, room);
        if (n <= 0) {
            broken = n < 0;
            break;
        }
        rbuf.append(chunk, n);
        got = true;
    }
    updateNotifiers();
    QPointer<QSerialPort> guard(q); // a slot may delete the port
    if (got && !emittingReadyRead) {
        emittingReadyRead = true;
        q->readyRead();
        if (guard)
            emittingReadyRead = false;
    }
    if (broken && guard)
        fail(QtPrivate::SerialOperation::Read);
    return got;
}

// Hands as much of the write buffer to the device as it takes. The port may
// have been deleted when this returns.
bool QSerialPortPrivate::send() {
    if (failed || engine.handle() < 0)
        return false;
    qint64 sent = 0;
    bool broken = false;
    while (sent < wbuf.size()) {
        const qint64 n = engine.write(wbuf.constData() + sent, wbuf.size() - sent);
        if (n <= 0) {
            broken = n < 0;
            break;
        }
        sent += n;
    }
    wbuf.remove(0, sent);
    updateNotifiers();
    QPointer<QSerialPort> guard(q);
    if (sent > 0)
        q->bytesWritten(sent);
    if (broken && guard)
        fail(QtPrivate::SerialOperation::Write);
    return sent > 0;
}

// Does the work of the event loop until new data has arrived (forRead) or the
// write buffer has emptied.
bool QSerialPortPrivate::waitFor(int msecs, bool forRead) {
    QPointer<QSerialPort> guard(q);
    QElapsedTimer timer;
    timer.start();
    for (;;) {
        const int left = msecs < 0 ? -1 : static_cast<int>(qMax<qint64>(0, msecs - timer.elapsed()));
        bool readable = false;
        bool writable = false;
        const bool wantRead = q->isReadable() && !readBufferFull();
        const int rc = engine.wait(wantRead, !wbuf.isEmpty(), left, &readable, &writable);
        if (rc == 0) {
            setError(QSerialPort::TimeoutError, QString("The operation timed out"));
            return false;
        }
        if (rc < 0) {
            fail(QtPrivate::SerialOperation::Control);
            return false;
        }
        if (writable)
            send();
        if (!guard || !q->isOpen())
            return false;
        const bool got = readable && receive();
        if (forRead && got)
            return true;
        if (!guard || !q->isOpen() || failed)
            return false;
        if (!forRead && wbuf.isEmpty())
            return true;
    }
}

/*!
    Constructs a closed port with the given \a parent. Set the port to open
    with setPortName() or setPort().
*/
QSerialPort::QSerialPort(QObject *parent) : QIODevice(parent), d(new QSerialPortPrivate(this)) {}

/*!
    Constructs a closed port for the port called \a name, with the given
    \a parent.

    \sa setPortName()
*/
QSerialPort::QSerialPort(const QString &name, QObject *parent) : QSerialPort(parent) { setPortName(name); }

/*!
    Constructs a closed port for the port described by \a info, with the given
    \a parent.

    \sa setPort()
*/
QSerialPort::QSerialPort(const QSerialPortInfo &info, QObject *parent) : QSerialPort(parent) { setPort(info); }

/*!
    Destroys the port, closing it if necessary.
*/
QSerialPort::~QSerialPort() {
    if (isOpen())
        QSerialPort::close();
}

/*!
    Sets the name of the port to \a name, which takes effect the next time the
    port opens. The name is either a device in \c{/dev}, such as \c ttyUSB0, or
    a path.

    \sa portName(), setPort()
*/
void QSerialPort::setPortName(const QString &name) { d->location = QtPrivate::serialSystemLocation(name); }

/*!
    Returns the name of the port. A device in \c{/dev} is returned without that
    prefix, however it was set.

    \sa setPortName()
*/
QString QSerialPort::portName() const { return QtPrivate::serialPortName(d->location); }

/*!
    Sets the port to the one described by \a info.

    \sa setPortName(), QSerialPortInfo::availablePorts()
*/
void QSerialPort::setPort(const QSerialPortInfo &info) { setPortName(info.systemLocation()); }

/*!
    Opens the port with \a mode and applies its settings. Returns \c true if
    successful; otherwise sets an error and returns \c false.

    The mode must be \c ReadOnly, \c WriteOnly or \c ReadWrite. Any other flag
    fails with \c UnsupportedOperationError.

    \sa close(), error()
*/
bool QSerialPort::open(OpenMode mode) {
    if (isOpen()) {
        d->setError(OpenError, QString("The device is already open"));
        return false;
    }
    if ((mode & ~OpenMode(ReadWrite)) != OpenMode(NotOpen) || (mode & ReadWrite) == OpenMode(NotOpen)) {
        d->setError(UnsupportedOperationError, QString("Unsupported open mode"));
        return false;
    }
    clearError();
    if (!d->engine.open(d->location, mode, d->settings)) {
        d->setEngineError(QtPrivate::SerialOperation::Open);
        return false;
    }
    QIODevice::open(mode);
    d->failed = false;
    d->startNotifiers();
    return true;
}

/*!
    Closes the port and restores the settings the device had before open().
    Data still in the write buffer is discarded, so call waitForBytesWritten()
    first if it matters. Sets \c NotOpenError if the port is not open.

    \sa open()
*/
void QSerialPort::close() {
    if (!isOpen()) {
        d->setNotOpen();
        return;
    }
    QIODevice::close();
    d->stopNotifiers();
    if (d->breakEnabled)
        d->engine.setBreak(false);
    d->breakEnabled = false;
    d->engine.close();
    d->rbuf.clear();
    d->rpos = 0;
    d->wbuf.clear();
}

/*!
    Sets the baud rate to \a baudRate for the given \a directions. Returns
    \c true if successful.

    Any positive rate is accepted, and a rate that is not among
    QSerialPortInfo::standardBaudRates() is passed to the driver as it is. If
    the port is open and the driver or the system refuses it, the function
    returns \c false; if the port is closed, open() does. Most hardware
    needs the same rate in both directions.

    \sa baudRate()
*/
bool QSerialPort::setBaudRate(qint32 baudRate, Directions directions) {
    if (baudRate <= 0 || !(directions & AllDirections)) {
        d->setError(UnsupportedOperationError, QString("Invalid baud rate value"));
        return false;
    }
    QtPrivate::SerialSettings next = d->settings;
    if (directions & Input)
        next.inputBaudRate = baudRate;
    if (directions & Output)
        next.outputBaudRate = baudRate;
    if (next.inputBaudRate == d->settings.inputBaudRate && next.outputBaudRate == d->settings.outputBaudRate)
        return true;
    if (!d->applySettings(next))
        return false;
    baudRateChanged(baudRate, directions);
    return true;
}

/*!
    Returns the baud rate for the given \a directions. For \c AllDirections, it
    returns \c -1 if the two directions differ.

    \sa setBaudRate()
*/
qint32 QSerialPort::baudRate(Directions directions) const {
    if (directions == AllDirections)
        return d->settings.inputBaudRate == d->settings.outputBaudRate ? d->settings.inputBaudRate : -1;
    return directions & Input ? d->settings.inputBaudRate : d->settings.outputBaudRate;
}

/*!
    Sets the number of data bits to \a dataBits. Returns \c true if successful.

    \sa dataBits()
*/
bool QSerialPort::setDataBits(DataBits dataBits) {
    if (d->settings.dataBits == dataBits)
        return true;
    QtPrivate::SerialSettings next = d->settings;
    next.dataBits = dataBits;
    if (!d->applySettings(next))
        return false;
    dataBitsChanged(dataBits);
    return true;
}

/*!
    Returns the number of data bits.

    \sa setDataBits()
*/
QSerialPort::DataBits QSerialPort::dataBits() const { return d->settings.dataBits; }

/*!
    Sets the parity to \a parity. Returns \c true if successful.

    \sa parity()
*/
bool QSerialPort::setParity(Parity parity) {
    if (d->settings.parity == parity)
        return true;
    QtPrivate::SerialSettings next = d->settings;
    next.parity = parity;
    if (!d->applySettings(next))
        return false;
    parityChanged(parity);
    return true;
}

/*!
    Returns the parity.

    \sa setParity()
*/
QSerialPort::Parity QSerialPort::parity() const { return d->settings.parity; }

/*!
    Sets the number of stop bits to \a stopBits. Returns \c true if successful.

    \sa stopBits()
*/
bool QSerialPort::setStopBits(StopBits stopBits) {
    if (d->settings.stopBits == stopBits)
        return true;
    QtPrivate::SerialSettings next = d->settings;
    next.stopBits = stopBits;
    if (!d->applySettings(next))
        return false;
    stopBitsChanged(stopBits);
    return true;
}

/*!
    Returns the number of stop bits.

    \sa setStopBits()
*/
QSerialPort::StopBits QSerialPort::stopBits() const { return d->settings.stopBits; }

/*!
    Sets the flow control to \a flowControl. Returns \c true if successful.

    \sa flowControl()
*/
bool QSerialPort::setFlowControl(FlowControl flowControl) {
    if (d->settings.flowControl == flowControl)
        return true;
    QtPrivate::SerialSettings next = d->settings;
    next.flowControl = flowControl;
    if (!d->applySettings(next))
        return false;
    flowControlChanged(flowControl);
    return true;
}

/*!
    Returns the flow control.

    \sa setFlowControl()
*/
QSerialPort::FlowControl QSerialPort::flowControl() const { return d->settings.flowControl; }

/*!
    Sets the DTR line to \a set. Returns \c true if successful. The port must be
    open.

    \sa isDataTerminalReady()
*/
bool QSerialPort::setDataTerminalReady(bool set) {
    bool changed = false;
    if (!d->setPin(DataTerminalReadySignal, set, &changed))
        return false;
    if (changed)
        dataTerminalReadyChanged(set);
    return true;
}

/*!
    Returns the state of the DTR line.

    \sa setDataTerminalReady(), pinoutSignals()
*/
bool QSerialPort::isDataTerminalReady() { return pinoutSignals().testFlag(DataTerminalReadySignal); }

/*!
    Sets the RTS line to \a set. Returns \c true if successful. The port must be
    open and must not use \c HardwareControl, which drives this line itself.

    \sa isRequestToSend()
*/
bool QSerialPort::setRequestToSend(bool set) {
    if (d->settings.flowControl == HardwareControl) {
        d->setError(UnsupportedOperationError, QString("RTS is driven by hardware flow control"));
        return false;
    }
    bool changed = false;
    if (!d->setPin(RequestToSendSignal, set, &changed))
        return false;
    if (changed)
        requestToSendChanged(set);
    return true;
}

/*!
    Returns the state of the RTS line.

    \sa setRequestToSend(), pinoutSignals()
*/
bool QSerialPort::isRequestToSend() { return pinoutSignals().testFlag(RequestToSendSignal); }

/*!
    Returns the lines that are active. It reads them from the device, so it is
    not const. Returns \c NoSignal and sets an error if the port is closed or
    the device has no modem lines.

    \sa isDataTerminalReady(), isRequestToSend()
*/
QSerialPort::PinoutSignals QSerialPort::pinoutSignals() {
    if (!isOpen()) {
        d->setNotOpen();
        return NoSignal;
    }
    PinoutSignals pins = NoSignal;
    if (!d->engine.pins(&pins)) {
        d->setEngineError(QtPrivate::SerialOperation::Control);
        return NoSignal;
    }
    return pins;
}

/*!
    Holds the transmit line in the break state if \a set is \c true, and
    releases it otherwise. Returns \c true if successful. The port must be open.

    \sa isBreakEnabled()
*/
bool QSerialPort::setBreakEnabled(bool set) {
    if (!isOpen()) {
        d->setNotOpen();
        return false;
    }
    if (!d->engine.setBreak(set)) {
        d->setEngineError(QtPrivate::SerialOperation::Control);
        return false;
    }
    if (d->breakEnabled != set) {
        d->breakEnabled = set;
        breakEnabledChanged(set);
    }
    return true;
}

/*!
    Returns \c true if the transmit line is in the break state; otherwise
    returns \c false.

    \sa setBreakEnabled()
*/
bool QSerialPort::isBreakEnabled() const { return d->breakEnabled; }

/*!
    Writes as much of the write buffer to the device as possible without
    blocking. Returns \c true if any data was written.

    \sa waitForBytesWritten()
*/
bool QSerialPort::flush() { return d->send(); }

/*!
    Discards the data waiting in the given \a directions, in this object and in
    the driver. Returns \c true if successful. The port must be open.

    \sa flush()
*/
bool QSerialPort::clear(Directions directions) {
    if (!isOpen()) {
        d->setNotOpen();
        return false;
    }
    if (directions & Input) {
        d->rbuf.clear();
        d->rpos = 0;
        QIODevice::seek(0); // also drops what peek() has looked at
    }
    if (directions & Output)
        d->wbuf.clear();
    d->updateNotifiers();
    if (!d->engine.discard(directions)) {
        d->setEngineError(QtPrivate::SerialOperation::Control);
        return false;
    }
    return true;
}

/*!
    Returns the last error.

    \sa errorOccurred(), errorString(), clearError()
*/
QSerialPort::SerialPortError QSerialPort::error() const { return d->error; }

/*!
    Sets the error to \c NoError.

    \sa error()
*/
void QSerialPort::clearError() { d->setError(NoError, QString()); }

/*!
    Returns the read buffer limit in bytes; \c 0 means unlimited.

    \sa setReadBufferSize()
*/
qint64 QSerialPort::readBufferSize() const { return d->readLimit; }

/*!
    Limits the read buffer to \a size bytes; \c 0, the default, means unlimited.
    When the buffer is full, data waits in the driver, which can lose it unless
    flow control holds the sender back.

    \sa readBufferSize()
*/
void QSerialPort::setReadBufferSize(qint64 size) {
    d->readLimit = qMax<qint64>(0, size);
    d->updateNotifiers();
}

/*!
    Returns the native descriptor, or \c -1 if the port is closed. Changing the
    device through it behind the object's back is at the caller's risk.
*/
QSerialPort::Handle QSerialPort::handle() const { return d->engine.handle(); }

/*!
    \reimp
*/
bool QSerialPort::isSequential() const { return true; }

/*!
    \reimp
*/
qint64 QSerialPort::bytesAvailable() const { return QIODevice::bytesAvailable() + d->buffered(); }

/*!
    \reimp
*/
qint64 QSerialPort::bytesToWrite() const { return d->wbuf.size(); }

/*!
    \reimp
*/
bool QSerialPort::canReadLine() const { return QIODevice::canReadLine() || d->rbuf.indexOf('\n', d->rpos) != -1; }

/*!
    Waits up to \a msecs milliseconds for new data, doing the work of the event
    loop meanwhile. A negative value waits forever. Returns \c true if data
    arrived, or if the read buffer is already full. A timeout sets
    \c TimeoutError.

    \sa readyRead(), waitForBytesWritten()
*/
bool QSerialPort::waitForReadyRead(int msecs) {
    if (!isOpen()) {
        d->setNotOpen();
        return false;
    }
    if (!isReadable() || d->failed)
        return false;
    return d->readBufferFull() || d->waitFor(msecs, true);
}

/*!
    Waits up to \a msecs milliseconds until the whole write buffer has been
    passed to the driver, doing the work of the event loop meanwhile. A negative
    value waits forever. Returns \c true if it has, and \c false if there was
    nothing to write. A timeout sets \c TimeoutError.

    The driver sends the data at its own pace, so it may not have left the
    machine yet.

    \sa bytesWritten(), waitForReadyRead(), flush()
*/
bool QSerialPort::waitForBytesWritten(int msecs) {
    if (!isOpen()) {
        d->setNotOpen();
        return false;
    }
    return !d->wbuf.isEmpty() && d->waitFor(msecs, false);
}

/*!
    \reimp
*/
qint64 QSerialPort::readData(char *data, qint64 maxSize) {
    const qint64 n = qMin(maxSize, d->buffered());
    if (n <= 0)
        return 0;
    std::memcpy(data, d->rbuf.constData() + d->rpos, static_cast<std::size_t>(n));
    d->rpos += n;
    if (d->rpos == d->rbuf.size()) {
        d->rbuf.clear();
        d->rpos = 0;
    } else if (d->rpos >= kChunk) {
        d->rbuf.remove(0, d->rpos);
        d->rpos = 0;
    }
    d->updateNotifiers();
    return n;
}

/*!
    \reimp
*/
qint64 QSerialPort::writeData(const char *data, qint64 maxSize) {
    if (d->failed)
        return -1;
    d->wbuf.append(data, maxSize);
    d->updateNotifiers();
    return maxSize;
}
