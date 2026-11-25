// MIT License - see LICENSE
#include <QtSerialBus/qcanbus.h>
#include <QtSerialBus/qcanbusdevice.h>
#include <QtSerialBus/qcanbusfactory.h>

#include "qcanbus_p.h"

#include <QtCore/qmap.h>

#include <mutex>

/*!
    \module NtSerialBus
    \title Nt SerialBus C++ Classes
    \brief Classes for the CAN bus.
    \cmaketarget Nt::SerialBus

    Nt SerialBus has the CAN part of Qt Serial Bus: QCanBus makes a QCanBusDevice for an interface,
    which sends and receives QCanBusFrame objects. Linux uses SocketCAN and Zephyr its CAN drivers;
    a virtual bus inside the process serves tests and simulations.

    See \l{Using Nt SerialBus} for a walk through.
*/

namespace QtSerialBusPrivate {

namespace {

struct Registry {
    std::mutex mutex;
    QMap<QString, QCanBusFactory *> factories;
    bool builtIn = false;
};

Registry &registry() {
    static Registry r;
    return r;
}

QMap<QString, QCanBusFactory *> factories() {
    Registry &r = registry();
    const std::lock_guard<std::mutex> lock(r.mutex);
    if (!r.builtIn) {
        r.builtIn = true;
        r.factories.insert(QStringLiteral("virtualcan"), virtualCanFactory());
#if NT_SERIALBUS_SOCKETCAN
        r.factories.insert(QStringLiteral("socketcan"), socketCanFactory());
#endif
#if NT_SERIALBUS_ZEPHYR
        r.factories.insert(QStringLiteral("zephyrcan"), zephyrCanFactory());
#endif
    }
    return r.factories;
}

} // namespace

bool passesFilters(const QCanBusFrame &frame, const QVariant &value) {
    const QList<QCanBusDevice::Filter> filters = value.value<QList<QCanBusDevice::Filter>>();
    if (filters.isEmpty())
        return true;
    for (const QCanBusDevice::Filter &f : filters) {
        if ((frame.frameId() & f.frameIdMask) != (f.frameId & f.frameIdMask))
            continue;
        if (f.type != QCanBusFrame::InvalidFrame && f.type != frame.frameType())
            continue;
        const bool extended = frame.hasExtendedFrameFormat();
        if (extended && !(f.format & QCanBusDevice::Filter::MatchExtendedFormat))
            continue;
        if (!extended && !(f.format & QCanBusDevice::Filter::MatchBaseFormat))
            continue;
        return true;
    }
    return false;
}

} // namespace QtSerialBusPrivate

/*!
    \class QCanBusFactory
    \inmodule NtSerialBus
    \brief The QCanBusFactory class makes the devices of a CAN backend.
    \since 6.0

    Reimplement it for a backend of your own, such as a CAN controller on SPI, and register it with
    QCanBus::registerFactory().

    \differences
    Backends are not loaded as plugins; they are registered from code.
    \enddifferences
*/

/*!
    Constructs a factory.
*/
QCanBusFactory::QCanBusFactory() = default;

/*!
    Destroys the factory.
*/
QCanBusFactory::~QCanBusFactory() = default;

/*!
    \fn QCanBusDevice *QCanBusFactory::createDevice(const QString &interfaceName, QString *errorMessage) const

    Returns a new device for the interface \a interfaceName, or \c nullptr with \a errorMessage set.
*/

/*!
    \fn QList<QCanBusDeviceInfo> QCanBusFactory::availableDevices(QString *errorMessage) const

    Returns the interfaces of the backend; on failure an empty list, with \a errorMessage set.
*/

/*!
    \class QCanBus
    \inmodule NtSerialBus
    \brief The QCanBus class makes the devices of the CAN backends.
    \since 6.0

    \code
    for (const QCanBusDeviceInfo &info : QCanBus::instance()->availableDevices())
        qDebug() << info.plugin() << info.name();
    QCanBusDevice *device = QCanBus::instance()->createDevice(QStringLiteral("socketcan"),
                                                              QStringLiteral("can0"));
    \endcode

    \differences
    The backends are built in rather than plugins: \c socketcan on Linux, \c zephyrcan on Zephyr
    and \c virtualcan everywhere. registerFactory() adds a backend of your own; it is an Nt
    addition.
    \enddifferences

    \sa QCanBusDevice, {Using Nt SerialBus}
*/

QCanBus::QCanBus(QObject *parent) : QObject(parent) {}

/*!
    Returns the one QCanBus.
*/
QCanBus *QCanBus::instance() {
    static QCanBus bus;
    return &bus;
}

/*!
    Returns the names of the backends.
*/
QStringList QCanBus::plugins() const { return QtSerialBusPrivate::factories().keys(); }

/*!
    Returns the interfaces of the backend \a plugin. If there is no such backend, or it cannot list
    them, returns an empty list and sets \a errorMessage.
*/
QList<QCanBusDeviceInfo> QCanBus::availableDevices(const QString &plugin, QString *errorMessage) const {
    QCanBusFactory *factory = QtSerialBusPrivate::factories().value(plugin);
    if (!factory) {
        if (errorMessage)
            *errorMessage = QStringLiteral("no CAN backend is called %1").arg(plugin);
        return {};
    }
    QString error;
    const QList<QCanBusDeviceInfo> devices = factory->availableDevices(&error);
    if (errorMessage)
        *errorMessage = error;
    return devices;
}

/*!
    Returns the interfaces of every backend. Errors of the backends are joined in \a errorMessage.
*/
QList<QCanBusDeviceInfo> QCanBus::availableDevices(QString *errorMessage) const {
    QList<QCanBusDeviceInfo> all;
    QStringList errors;
    const QMap<QString, QCanBusFactory *> factories = QtSerialBusPrivate::factories();
    for (auto it = factories.cbegin(); it != factories.cend(); ++it) {
        QString error;
        all.append(it.value()->availableDevices(&error));
        if (!error.isEmpty())
            errors.append(error);
    }
    if (errorMessage)
        *errorMessage = errors.join(QStringLiteral("; "));
    return all;
}

/*!
    Returns a new device for the interface \a interfaceName of the backend \a plugin, which the
    caller owns, or \c nullptr with \a errorMessage set.
*/
QCanBusDevice *QCanBus::createDevice(const QString &plugin, const QString &interfaceName, QString *errorMessage) const {
    QCanBusFactory *factory = QtSerialBusPrivate::factories().value(plugin);
    if (!factory) {
        if (errorMessage)
            *errorMessage = QStringLiteral("no CAN backend is called %1").arg(plugin);
        return nullptr;
    }
    QString error;
    QCanBusDevice *device = factory->createDevice(interfaceName, &error);
    if (errorMessage)
        *errorMessage = error;
    return device;
}

/*!
    Registers \a factory, which QCanBus does not take ownership of, as the backend \a plugin, in
    place of any of that name. This is an Nt addition.
*/
void QCanBus::registerFactory(const QString &plugin, QCanBusFactory *factory) {
    QtSerialBusPrivate::factories(); // the built-in backends first, so that one of them can be replaced
    QtSerialBusPrivate::Registry &r = QtSerialBusPrivate::registry();
    const std::lock_guard<std::mutex> lock(r.mutex);
    if (factory)
        r.factories.insert(plugin, factory);
    else
        r.factories.remove(plugin);
}

#include "moc_qcanbus.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
