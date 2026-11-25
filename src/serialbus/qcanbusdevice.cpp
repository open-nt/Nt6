// MIT License - see LICENSE
#include <QtSerialBus/qcanbusdevice.h>

#include "qcanbus_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qeventloop.h>
#include <QtCore/qmap.h>
#include <QtCore/qtimer.h>

class QCanBusDevicePrivate {
public:
    QCanBusDevice::CanBusDeviceState state = QCanBusDevice::UnconnectedState;
    QCanBusDevice::CanBusError error = QCanBusDevice::NoError;
    QString errorText;
    QList<QCanBusFrame> incoming;
    QList<QCanBusFrame> outgoing;
    QMap<int, QVariant> configuration;
    std::function<void()> resetter;
    std::function<QCanBusDevice::CanBusStatus()> statusGetter;
    bool waitingForWritten = false;
    bool waitingForReceived = false;
};

/*!
    \class QCanBusDevice
    \inmodule NtSerialBus
    \brief The QCanBusDevice class sends and receives the frames of a CAN bus.
    \since 6.0

    Make one with QCanBus::createDevice(), set it up, and connect it:

    \code
    QString error;
    QCanBusDevice *device = QCanBus::instance()->createDevice(QStringLiteral("socketcan"),
                                                              QStringLiteral("can0"), &error);
    if (!device) {
        qWarning() << error;
        return;
    }
    device->setConfigurationParameter(QCanBusDevice::BitRateKey, 500000);
    QObject::connect(device, &QCanBusDevice::framesReceived, [device] {
        while (device->framesAvailable())
            qDebug() << device->readFrame().toString();
    });
    if (!device->connectDevice())
        qWarning() << device->errorString();
    device->writeFrame(QCanBusFrame(0x123, QByteArray::fromHex("cafe")));
    \endcode

    The backends of Nt are \c socketcan (Linux SocketCAN), \c virtualcan (a bus inside the process)
    and, on Zephyr, \c zephyrcan (the CAN drivers of Zephyr); see \l{Using Nt SerialBus}.

    \differences
    waitForFramesWritten() and waitForFramesReceived() run a local event loop, as in Qt, but may
    not be nested. The base class stores configuration parameters without checking their values;
    each backend checks those it knows when it connects.
    \enddifferences

    \sa QCanBus, QCanBusFrame
*/

/*!
    \enum QCanBusDevice::CanBusError

    This enum describes the errors of a device.

    \value NoError No error.
    \value ReadError Reading from the bus failed.
    \value WriteError Writing to the bus failed, or the frame was not valid for it.
    \value ConnectionError Connecting or disconnecting failed.
    \value ConfigurationError A configuration parameter could not be applied.
    \value UnknownError Another error.
    \value OperationError The operation is not possible in the state of the device.
    \value TimeoutError An operation timed out.
*/

/*!
    \enum QCanBusDevice::CanBusDeviceState

    This enum describes the state of a device.

    \value UnconnectedState Not connected.
    \value ConnectingState Being connected.
    \value ConnectedState Connected: frames can be sent and received.
    \value ClosingState Being disconnected.
*/

/*!
    \enum QCanBusDevice::CanBusStatus

    This enum describes the state of the controller on the bus.

    \value Unknown Not known.
    \value Good Error active: the controller works normally.
    \value Warning The error counters passed the warning level.
    \value Error Error passive.
    \value BusOff The controller is off the bus.
*/

/*!
    \enum QCanBusDevice::ConfigurationKey

    This enum names the configuration parameters of a device.

    \value RawFilterKey A QList<QCanBusDevice::Filter> of the frames to receive; empty for all.
    \value ErrorFilterKey The QCanBusFrame::FrameErrors whose error frames are received.
    \value LoopbackKey A bool: whether frames that are sent also reach the other applications of
           the machine (SocketCAN), or are looped back inside the controller (Zephyr).
    \value ReceiveOwnKey A bool: whether frames that are sent are also received by this device.
    \value BitRateKey The bit rate, in bits a second.
    \value CanFdKey A bool: whether CAN FD frames can be sent and received.
    \value DataBitRateKey The data bit rate of CAN FD, in bits a second.
    \value ProtocolKey The protocol of a backend that supports several.
    \value UserKey The first key for the parameters of a backend.
*/

/*!
    \enum QCanBusDevice::Direction

    This enum describes the queues of a device.

    \value Input The frames received.
    \value Output The frames to send.
    \value AllDirections Both.
*/

/*!
    \class QCanBusDevice::Filter
    \inmodule NtSerialBus
    \brief The QCanBusDevice::Filter struct selects the frames that a device receives.
    \since 6.0

    A frame passes a filter when the bits of its identifier that frameIdMask has set equal those of
    frameId, its type is \a type (or \a type is \c InvalidFrame, for any type), and its format is
    one that format allows. A frame passes a list of filters when it passes any of them.

    \code
    QCanBusDevice::Filter filter;
    filter.frameId = 0x100;
    filter.frameIdMask = 0x700;   // 0x100 to 0x1FF
    filter.format = QCanBusDevice::Filter::MatchBaseFormat;
    device->setConfigurationParameter(QCanBusDevice::RawFilterKey,
                                      QVariant::fromValue(QList<QCanBusDevice::Filter>{filter}));
    \endcode

    \differences
    None.
    \enddifferences
*/

/*!
    \enum QCanBusDevice::Filter::FormatFilter

    This enum describes the formats that a filter lets through.

    \value MatchBaseFormat Frames with an 11-bit identifier.
    \value MatchExtendedFormat Frames with a 29-bit identifier.
    \value MatchBaseAndExtendedFormat Both.
*/

/*!
    \variable QCanBusDevice::Filter::frameId

    The identifier that the bits of frameIdMask must match.
*/

/*!
    \variable QCanBusDevice::Filter::frameIdMask

    The bits of the identifier that are compared; 0 matches every identifier.
*/

/*!
    \variable QCanBusDevice::Filter::type

    The type of frame that passes, or \c InvalidFrame for every type.
*/

/*!
    \variable QCanBusDevice::Filter::format

    The formats of identifier that pass.
*/

/*!
    Constructs a device with the given \a parent.
*/
QCanBusDevice::QCanBusDevice(QObject *parent) : QObject(parent), d(std::make_unique<QCanBusDevicePrivate>()) {}

/*!
    Destroys the device.
*/
QCanBusDevice::~QCanBusDevice() = default;

/*!
    Sets the configuration parameter \a key to \a value; an invalid \a value removes it. A backend
    applies the parameters when it connects, and those it can at once.
*/
void QCanBusDevice::setConfigurationParameter(ConfigurationKey key, const QVariant &value) {
    if (value.isValid())
        d->configuration.insert(int(key), value);
    else
        d->configuration.remove(int(key));
}

/*!
    Returns the configuration parameter \a key, or an invalid QVariant if it is not set.
*/
QVariant QCanBusDevice::configurationParameter(ConfigurationKey key) const { return d->configuration.value(int(key)); }

/*!
    Returns the keys of the configuration parameters that are set.
*/
QList<QCanBusDevice::ConfigurationKey> QCanBusDevice::configurationKeys() const {
    QList<ConfigurationKey> keys;
    for (auto it = d->configuration.cbegin(); it != d->configuration.cend(); ++it)
        keys.append(ConfigurationKey(it.key()));
    return keys;
}

/*!
    \fn bool QCanBusDevice::writeFrame(const QCanBusFrame &frame)

    Sends \a frame. Returns \c false, with error() set to \c WriteError, if the device is not
    connected or the frame is not valid for it; framesWritten() is emitted once it is on the bus.
*/

/*!
    Returns the oldest frame received and removes it, or an invalid frame if there is none or the
    device is not connected.
*/
QCanBusFrame QCanBusDevice::readFrame() {
    if (d->state != ConnectedState) {
        qWarning("QCanBusDevice::readFrame: the device is not connected");
        return QCanBusFrame(QCanBusFrame::InvalidFrame);
    }
    if (d->incoming.isEmpty())
        return QCanBusFrame(QCanBusFrame::InvalidFrame);
    return d->incoming.takeFirst();
}

/*!
    Returns all the frames received, oldest first, and removes them.
*/
QList<QCanBusFrame> QCanBusDevice::readAllFrames() {
    if (d->state != ConnectedState) {
        qWarning("QCanBusDevice::readAllFrames: the device is not connected");
        return {};
    }
    return std::exchange(d->incoming, QList<QCanBusFrame>());
}

/*!
    Returns the number of frames received and not read.
*/
qint64 QCanBusDevice::framesAvailable() const { return d->incoming.size(); }

/*!
    Returns the number of frames waiting to be sent.
*/
qint64 QCanBusDevice::framesToWrite() const { return d->outgoing.size(); }

/*!
    Resets the controller, for example to bring it back after it went bus off, if the backend can.
    Otherwise sets \c OperationError.

    \sa hasBusStatus()
*/
void QCanBusDevice::resetController() {
    if (d->resetter)
        d->resetter();
    else
        setError(QStringLiteral("this backend cannot reset its controller"), OperationError);
}

/*!
    Returns \c true if the backend can tell the state of the controller with busStatus().
*/
bool QCanBusDevice::hasBusStatus() const { return bool(d->statusGetter); }

/*!
    Returns the state of the controller on the bus, or \c Unknown if the backend cannot tell.
*/
QCanBusDevice::CanBusStatus QCanBusDevice::busStatus() {
    return d->statusGetter ? d->statusGetter() : CanBusStatus::Unknown;
}

/*!
    Throws away the frames of the queues that \a direction names. The device has to be connected.
*/
void QCanBusDevice::clear(QCanBusDevice::Directions direction) {
    if (d->state != ConnectedState) {
        setError(QStringLiteral("cannot clear the queues of a device that is not connected"), OperationError);
        return;
    }
    clearError();
    if (direction & Input)
        d->incoming.clear();
    if (direction & Output)
        d->outgoing.clear();
}

namespace {

// Runs an event loop until signal is emitted, an error occurs, or msecs pass (-1 for no limit).
template <typename Signal> bool waitFor(QCanBusDevice *device, Signal signal, int msecs) {
    QEventLoop loop;
    bool done = false;
    const QMetaObject::Connection a = QObject::connect(device, signal, &loop, [&] {
        done = true;
        loop.quit();
    });
    const QMetaObject::Connection b = QObject::connect(device, &QCanBusDevice::errorOccurred, &loop,
                                                       [&](QCanBusDevice::CanBusError) { loop.quit(); });
    const QMetaObject::Connection c = QObject::connect(device, &QCanBusDevice::stateChanged, &loop,
                                                       [&](QCanBusDevice::CanBusDeviceState) { loop.quit(); });
    QTimer timer;
    if (msecs >= 0) {
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, [&] { loop.quit(); });
        timer.start(msecs);
    }
    loop.exec();
    QObject::disconnect(a);
    QObject::disconnect(b);
    QObject::disconnect(c);
    return done;
}

} // namespace

/*!
    Waits until framesWritten() is emitted, an error occurs or \a msecs milliseconds pass (-1 for
    no limit). Returns \c true if frames were written, at once if there are none to write.
*/
bool QCanBusDevice::waitForFramesWritten(int msecs) {
    if (d->state != ConnectedState || d->waitingForWritten)
        return false;
    if (framesToWrite() == 0)
        return true;
    d->waitingForWritten = true;
    const bool done = waitFor(this, &QCanBusDevice::framesWritten, msecs);
    d->waitingForWritten = false;
    return done;
}

/*!
    Waits until framesReceived() is emitted, an error occurs or \a msecs milliseconds pass (-1 for
    no limit). Returns \c true if frames were received.
*/
bool QCanBusDevice::waitForFramesReceived(int msecs) {
    if (d->state != ConnectedState || d->waitingForReceived)
        return false;
    d->waitingForReceived = true;
    const bool done = waitFor(this, &QCanBusDevice::framesReceived, msecs);
    d->waitingForReceived = false;
    return done;
}

/*!
    Connects the device to the bus, applying its configuration. Returns \c false and sets error()
    if it fails or the device is not unconnected.
*/
bool QCanBusDevice::connectDevice() {
    if (d->state != UnconnectedState) {
        setError(QStringLiteral("the device is already connected"), ConnectionError);
        return false;
    }
    clearError();
    setState(ConnectingState);
    if (!open()) {
        setState(UnconnectedState);
        return false;
    }
    if (d->state == ConnectingState)
        setState(ConnectedState);
    return true;
}

/*!
    Disconnects the device from the bus. Frames that were not read or sent are thrown away.
*/
void QCanBusDevice::disconnectDevice() {
    if (d->state == UnconnectedState || d->state == ClosingState) {
        qWarning("QCanBusDevice::disconnectDevice: the device is not connected");
        return;
    }
    setState(ClosingState);
    close();
    d->incoming.clear();
    d->outgoing.clear();
    if (d->state == ClosingState)
        setState(UnconnectedState);
}

/*!
    Returns the state of the device.
*/
QCanBusDevice::CanBusDeviceState QCanBusDevice::state() const { return d->state; }

/*!
    Returns the last error.
*/
QCanBusDevice::CanBusError QCanBusDevice::error() const { return d->error; }

/*!
    Returns a description of the last error.
*/
QString QCanBusDevice::errorString() const { return d->errorText; }

/*!
    \fn QString QCanBusDevice::interpretErrorFrame(const QCanBusFrame &errorFrame)

    Returns a description of the errors that \a errorFrame reports, or an empty string if it is
    not an error frame.
*/

/*!
    Returns a description of the interface the device opens. The default has no name.
*/
QCanBusDeviceInfo QCanBusDevice::deviceInfo() const { return createDeviceInfo(QString(), QString(), false, false); }

/*!
    Sets the state to \a newState and emits stateChanged() if it changed.
*/
void QCanBusDevice::setState(QCanBusDevice::CanBusDeviceState newState) {
    if (d->state == newState)
        return;
    d->state = newState;
    emit stateChanged(newState);
}

/*!
    Sets the error to \a errorId, described by \a errorText, and emits errorOccurred().
*/
void QCanBusDevice::setError(const QString &errorText, QCanBusDevice::CanBusError errorId) {
    d->error = errorId;
    d->errorText = errorText;
    emit errorOccurred(errorId);
}

/*!
    Clears the error.
*/
void QCanBusDevice::clearError() {
    d->error = NoError;
    d->errorText.clear();
}

/*!
    Adds \a newFrames to the frames received and emits framesReceived(). A backend calls it.
*/
void QCanBusDevice::enqueueReceivedFrames(const QList<QCanBusFrame> &newFrames) {
    if (newFrames.isEmpty())
        return;
    d->incoming.append(newFrames);
    emit framesReceived();
}

/*!
    Adds \a newFrame to the frames to send. A backend calls it.
*/
void QCanBusDevice::enqueueOutgoingFrame(const QCanBusFrame &newFrame) { d->outgoing.append(newFrame); }

/*!
    Removes the oldest frame to send and returns it, or an invalid frame if there is none.
*/
QCanBusFrame QCanBusDevice::dequeueOutgoingFrame() {
    return d->outgoing.isEmpty() ? QCanBusFrame(QCanBusFrame::InvalidFrame) : d->outgoing.takeFirst();
}

/*!
    Returns \c true if there are frames to send.
*/
bool QCanBusDevice::hasOutgoingFrames() const { return !d->outgoing.isEmpty(); }

/*!
    Sets the function that resetController() calls to \a resetter.
*/
void QCanBusDevice::setResetControllerFunction(std::function<void()> resetter) { d->resetter = std::move(resetter); }

/*!
    Sets the function that busStatus() calls to \a busStatusGetter.
*/
void QCanBusDevice::setCanBusStatusGetter(std::function<CanBusStatus()> busStatusGetter) {
    d->statusGetter = std::move(busStatusGetter);
}

/*!
    \fn bool QCanBusDevice::open()

    Opens the interface and applies the configuration. Returns \c false, with an error set, if it
    fails. connectDevice() calls it.
*/

/*!
    \fn void QCanBusDevice::close()

    Closes the interface. disconnectDevice() calls it.
*/

/*!
    Returns an info for the interface \a name of the backend \a plugin, virtual if \a isVirtual is
    \c true and capable of CAN FD if \a isFlexibleDataRateCapable is \c true.
*/
QCanBusDeviceInfo QCanBusDevice::createDeviceInfo(const QString &plugin, const QString &name, bool isVirtual,
                                                  bool isFlexibleDataRateCapable) {
    return createDeviceInfo(plugin, name, QString(), QString(), QString(), 0, isVirtual, isFlexibleDataRateCapable);
}

/*!
    Returns an info for the interface \a name of the backend \a plugin, with the given
    \a serialNumber, \a description, \a alias and \a channel, virtual if \a isVirtual is \c true
    and capable of CAN FD if \a isFlexibleDataRateCapable is \c true.
*/
QCanBusDeviceInfo QCanBusDevice::createDeviceInfo(const QString &plugin, const QString &name,
                                                  const QString &serialNumber, const QString &description,
                                                  const QString &alias, int channel, bool isVirtual,
                                                  bool isFlexibleDataRateCapable) {
    auto *info = new QCanBusDeviceInfoPrivate;
    info->plugin = plugin;
    info->name = name;
    info->serialNumber = serialNumber;
    info->description = description;
    info->alias = alias;
    info->channel = channel;
    info->isVirtual = isVirtual;
    info->hasFlexibleDataRate = isFlexibleDataRateCapable;
    return QCanBusDeviceInfo(info);
}

#include "moc_qcanbusdevice.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
