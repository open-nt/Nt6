// MIT License - see LICENSE
#include "qdbusconnection_p.h"

#include <QtDBus/qdbusconnectioninterface.h>

/*!
    \class QDBusConnectionInterface
    \brief The QDBusConnectionInterface class provides access to the D-Bus bus daemon service.

    QDBusConnection::interface() returns the one of a connection. It lists and looks up the
    names on the bus, claims and releases names, and reports names that appear and disappear:

    \code
    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    if (bus->isServiceRegistered("org.example.Clock"))
        qDebug() << "owned by" << bus->serviceOwner("org.example.Clock").value();
    \endcode

    \differences
    The object lives in the thread that does the connection's I/O, so its signals reach other
    threads through queued connections. It listens for every \c NameOwnerChanged signal of the
    bus from the moment it is created. The \c NameAcquired and \c NameLost signals are not
    available.
    \enddifferences

    \sa QDBusConnection, QDBusServiceWatcher
*/

/*!
    \enum QDBusConnectionInterface::ServiceQueueOptions

    What registerService() does when the name is taken.

    \value DontQueueService Fail.
    \value QueueService Wait in the queue for the name.
    \value ReplaceExistingService Take the name if its owner allows replacement; otherwise fail.
*/

/*!
    \enum QDBusConnectionInterface::ServiceReplacementOptions

    \value DontAllowReplacement Other connections cannot take the name away.
    \value AllowReplacement Another connection may take the name with \c ReplaceExistingService.
*/

/*!
    \enum QDBusConnectionInterface::RegisterServiceReply

    \value ServiceNotRegistered The name was not claimed.
    \value ServiceRegistered This connection owns the name.
    \value ServiceQueued This connection waits in the queue for the name.
*/

namespace {

QDBusMessage busMethod(const QString &method) {
    return QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
                                          QStringLiteral("/org/freedesktop/DBus"),
                                          QStringLiteral("org.freedesktop.DBus"), method);
}

} // namespace

QDBusConnectionInterface::QDBusConnectionInterface(const QDBusConnection &connection, QObject *parent)
    : QDBusAbstractInterface(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                             QStringLiteral("org.freedesktop.DBus"), connection, parent, false) {
    const auto p = QDBusConnectionPrivate::get(connection);
    p->addHook(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
               QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameOwnerChanged"), {}, QString(), this,
               [this](const QDBusMessage &m) {
                   const QList<QVariant> args = m.arguments();
                   if (args.size() != 3)
                       return;
                   const QString name = args.at(0).toString();
                   const QString oldOwner = args.at(1).toString();
                   const QString newOwner = args.at(2).toString();
                   if (oldOwner.isEmpty())
                       serviceRegistered(name);
                   else if (newOwner.isEmpty())
                       serviceUnregistered(name);
                   serviceOwnerChanged(name, oldOwner, newOwner);
               });
}

/*!
    Returns the names that exist on the bus, unique and well-known.
*/
QDBusReply<QStringList> QDBusConnectionInterface::registeredServiceNames() const {
    return connection().call(busMethod(QStringLiteral("ListNames")));
}

/*!
    Returns the names that the bus can start a program for.
*/
QDBusReply<QStringList> QDBusConnectionInterface::activatableServiceNames() const {
    return connection().call(busMethod(QStringLiteral("ListActivatableNames")));
}

/*!
    Returns whether a connection owns \a serviceName.
*/
QDBusReply<bool> QDBusConnectionInterface::isServiceRegistered(const QString &serviceName) const {
    QDBusMessage m = busMethod(QStringLiteral("NameHasOwner"));
    m << serviceName;
    return connection().call(m);
}

/*!
    Returns the unique name of the connection that owns \a name.
*/
QDBusReply<QString> QDBusConnectionInterface::serviceOwner(const QString &name) const {
    QDBusMessage m = busMethod(QStringLiteral("GetNameOwner"));
    m << name;
    return connection().call(m);
}

/*!
    Releases \a serviceName. The value is \c true if this connection owned it.
*/
QDBusReply<bool> QDBusConnectionInterface::unregisterService(const QString &serviceName) {
    const auto p = QDBusConnectionPrivate::get(connection());
    if (!p)
        return QDBusReply<bool>(QDBusError(QDBusError::Disconnected, QString()));
    const QDBusMessage reply = p->releaseName(serviceName);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return QDBusReply<bool>(reply);
    return QDBusReply<bool>(reply.createReply(reply.arguments().first().toUInt() == 1));
}

/*!
    Claims \a serviceName. \a qoption says what happens if the name is taken, and \a roption
    whether others may take it away later.
*/
QDBusReply<QDBusConnectionInterface::RegisterServiceReply>
QDBusConnectionInterface::registerService(const QString &serviceName, ServiceQueueOptions qoption,
                                          ServiceReplacementOptions roption) {
    const auto p = QDBusConnectionPrivate::get(connection());
    if (!p)
        return QDBusReply<RegisterServiceReply>(QDBusError(QDBusError::Disconnected, QString()));
    uint flags = 0;
    if (qoption == DontQueueService)
        flags |= 4;
    else if (qoption == ReplaceExistingService)
        flags |= 4 | 2;
    if (roption == AllowReplacement)
        flags |= 1;
    const QDBusMessage reply = p->requestName(serviceName, flags);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return QDBusReply<RegisterServiceReply>(reply);
    const uint code = reply.arguments().first().toUInt();
    const RegisterServiceReply result = code == 1 || code == 4 ? ServiceRegistered
                                        : code == 2            ? ServiceQueued
                                                               : ServiceNotRegistered;
    return QDBusReply<RegisterServiceReply>(reply.createReply(QVariant::fromValue(result)));
}

/*!
    Returns the process id of the connection that owns \a serviceName.
*/
QDBusReply<uint> QDBusConnectionInterface::servicePid(const QString &serviceName) const {
    QDBusMessage m = busMethod(QStringLiteral("GetConnectionUnixProcessID"));
    m << serviceName;
    return connection().call(m);
}

/*!
    Returns the user id of the connection that owns \a serviceName.
*/
QDBusReply<uint> QDBusConnectionInterface::serviceUid(const QString &serviceName) const {
    QDBusMessage m = busMethod(QStringLiteral("GetConnectionUnixUser"));
    m << serviceName;
    return connection().call(m);
}

/*!
    Asks the bus to start the program that provides \a name.
*/
QDBusReply<void> QDBusConnectionInterface::startService(const QString &name) {
    QDBusMessage m = busMethod(QStringLiteral("StartServiceByName"));
    m << name << 0u;
    return connection().call(m);
}

#include "moc_qdbusconnectioninterface.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
