// MIT License - see LICENSE
#include "qdbusconnection_p.h"
#include "qdbusmarshaller_p.h"

#include <QtDBus/qdbusabstractinterface.h>
#include <QtDBus/qdbusmetatype.h>

/*!
    \class QDBusAbstractInterface
    \brief The QDBusAbstractInterface class is the base class for all D-Bus interfaces in the
    Nt D-Bus binding, allowing access to remote interfaces.

    Subclass it to write a proxy class for one remote interface: methods call
    callWithArgumentList() or asyncCall(), and signals that the class declares are emitted when
    the remote object emits the D-Bus signal of the same name:

    \snippet dbus/clock.cpp proxy

    QDBusInterface is a ready-made subclass for interfaces known only at run time.

    \differences
    property() and setProperty() hide those of QObject and read and write the remote
    properties; call them through a QDBusAbstractInterface pointer or reference. A subclass
    relays every signal of the remote interface that it declares, whether connected or not.
    There is no \c qdbusxml2cpp to generate subclasses.
    \enddifferences

    \sa QDBusInterface, QDBusConnection
*/

class QDBusAbstractInterfacePrivate {
public:
    QDBusConnection connection{QString()};
    std::weak_ptr<QDBusConnectionPrivate> weakConnection; // instead, for QDBusConnectionInterface
    bool weak = false;
    QString service;
    QString path;
    QString interface;
    mutable std::mutex mutex;
    mutable QDBusError lastError;
    int timeout = -1;
    bool interactive = false;
    uint hook = 0;

    QDBusConnection conn() const {
        if (!weak)
            return connection;
        const auto d = weakConnection.lock();
        return d ? QDBusConnectionPrivate::wrap(d) : QDBusConnection(QString());
    }
    void setError(const QDBusError &e) const {
        std::lock_guard<std::mutex> lock(mutex);
        lastError = e;
    }
};

/*!
    Constructs an interface to \a interface of the object at \a path in \a service, reached
    through \a connection, with the given \a parent.
*/
QDBusAbstractInterface::QDBusAbstractInterface(const QString &service, const QString &path, const char *interface,
                                               const QDBusConnection &connection, QObject *parent)
    : QDBusAbstractInterface(service, path, QString::fromLatin1(interface), connection, parent, true) {}

QDBusAbstractInterface::QDBusAbstractInterface(const QString &service, const QString &path, const QString &interface,
                                               const QDBusConnection &connection, QObject *parent, bool relaySignals)
    : QObject(parent), d(new QDBusAbstractInterfacePrivate) {
    d->service = service;
    d->path = path;
    d->interface = interface;
    const auto p = QDBusConnectionPrivate::get(connection);
    if (!relaySignals && p && p->mode() == QDBusConnectionPrivate::BusMode &&
        service == QLatin1String("org.freedesktop.DBus")) {
        // The connection owns its bus interface, which must not keep it alive.
        d->weak = true;
        d->weakConnection = p;
    } else {
        d->connection = connection;
    }
    if (relaySignals && p && p->isConnected())
        d->hook = p->addHook(service, path, interface, QString(), {}, QString(), this,
                             [this](const QDBusMessage &m) { relaySignal(m); });
}

/*!
    Destroys the interface.
*/
QDBusAbstractInterface::~QDBusAbstractInterface() {
    if (d->hook)
        if (const auto p = QDBusConnectionPrivate::get(d->conn()))
            p->removeHook(d->hook);
}

void QDBusAbstractInterface::relaySignal(const QDBusMessage &message) {
    const QMetaObject *mo = metaObject();
    const QByteArray name = message.member().toLatin1();
    const QByteArray signature = message.signature().toLatin1();
    for (int i = QDBusAbstractInterface::staticMetaObject.methodCount(); i < mo->methodCount(); ++i) {
        const QMetaMethod m = mo->method(i);
        if (m.methodType() != QMetaMethod::Signal || name != m.name())
            continue;
        const QDBusMethodInfo info = QDBusMethodInfo::parse(m);
        if (!info.valid || !signature.startsWith(info.inSignature))
            continue;
        QDBusError error;
        info.invoke(this, message, nullptr, &error);
        return;
    }
}

/*!
    Returns \c true if the connection is open and the remote service exists.
*/
bool QDBusAbstractInterface::isValid() const {
    const QDBusConnection c = d->conn();
    if (!c.isConnected()) {
        d->setError(c.lastError());
        return false;
    }
    const auto p = QDBusConnectionPrivate::get(c);
    if (d->service.isEmpty() || p->mode() != QDBusConnectionPrivate::BusMode)
        return true;
    QDBusMessage m =
        QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                                       QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameHasOwner"));
    m << d->service;
    const QDBusMessage reply = c.call(m);
    const bool ok = reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty() &&
                    reply.arguments().first().toBool();
    if (!ok)
        d->setError(QDBusError(QDBusError::ServiceUnknown,
                               QStringLiteral("The name %1 was not provided by any .service files").arg(d->service)));
    return ok;
}

/*!
    Returns the connection that the interface uses.
*/
QDBusConnection QDBusAbstractInterface::connection() const { return d->conn(); }

/*!
    Returns the name of the remote service.
*/
QString QDBusAbstractInterface::service() const { return d->service; }

/*!
    Returns the path of the remote object.
*/
QString QDBusAbstractInterface::path() const { return d->path; }

/*!
    Returns the name of the remote interface.
*/
QString QDBusAbstractInterface::interface() const { return d->interface; }

/*!
    Returns the error of the last call that failed.
*/
QDBusError QDBusAbstractInterface::lastError() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    return d->lastError;
}

/*!
    Sets the timeout of the calls to \a timeout milliseconds; -1 selects the default.
*/
void QDBusAbstractInterface::setTimeout(int timeout) { d->timeout = timeout; }

/*!
    Returns the timeout of the calls in milliseconds, or -1 for the default.
*/
int QDBusAbstractInterface::timeout() const { return d->timeout; }

/*!
    Sets whether the calls allow the remote side to ask the user for authorization to
    \a enable.
*/
void QDBusAbstractInterface::setInteractiveAuthorizationAllowed(bool enable) { d->interactive = enable; }

/*!
    Returns \c true if the calls allow interactive authorization.
*/
bool QDBusAbstractInterface::isInteractiveAuthorizationAllowed() const { return d->interactive; }

QList<QVariant> QDBusAbstractInterface::fitArguments(const QString &, const QList<QVariant> &args) const {
    return args;
}

QVariant QDBusAbstractInterface::fitProperty(const char *, const QVariant &value) const { return value; }

/*!
    Calls \a method with the arguments \a args and waits as \a mode says. Returns the reply,
    which is an error message if the call failed.
*/
QDBusMessage QDBusAbstractInterface::callWithArgumentList(QDBus::CallMode mode, const QString &method,
                                                          const QList<QVariant> &args) {
    QDBusMessage m = QDBusMessage::createMethodCall(d->service, d->path, d->interface, method);
    m.setArguments(fitArguments(method, args));
    m.setInteractiveAuthorizationAllowed(d->interactive);
    const QDBusMessage reply = d->conn().call(m, mode, d->timeout);
    if (reply.type() == QDBusMessage::ErrorMessage)
        d->setError(QDBusError(reply));
    else
        d->setError(QDBusError());
    return reply;
}

/*!
    Calls \a method with the arguments \a args and returns at once.
*/
QDBusPendingCall QDBusAbstractInterface::asyncCallWithArgumentList(const QString &method, const QList<QVariant> &args) {
    QDBusMessage m = QDBusMessage::createMethodCall(d->service, d->path, d->interface, method);
    m.setArguments(fitArguments(method, args));
    m.setInteractiveAuthorizationAllowed(d->interactive);
    return d->conn().asyncCall(m, d->timeout);
}

/*!
    Calls \a method with the arguments \a args. When the reply arrives, \a returnMethod of
    \a receiver is called with it, or \a errorMethod with the error. Returns \c true if the call
    was sent.

    \sa QDBusConnection::callWithCallback()
*/
bool QDBusAbstractInterface::callWithCallback(const QString &method, const QList<QVariant> &args, QObject *receiver,
                                              const char *returnMethod, const char *errorMethod) {
    QDBusMessage m = QDBusMessage::createMethodCall(d->service, d->path, d->interface, method);
    m.setArguments(fitArguments(method, args));
    return d->conn().callWithCallback(m, receiver, returnMethod, errorMethod, d->timeout);
}

/*!
    \overload
    Calls \a method with the arguments \a args and calls \a slot of \a receiver with the reply.
*/
bool QDBusAbstractInterface::callWithCallback(const QString &method, const QList<QVariant> &args, QObject *receiver,
                                              const char *slot) {
    return callWithCallback(method, args, receiver, slot, nullptr);
}

/*!
    Returns the value of the remote property \a name, or an invalid QVariant if it cannot be
    read; lastError() then says why. The property \c objectName is that of this QObject.
*/
QVariant QDBusAbstractInterface::property(const char *name) const {
    if (std::strcmp(name, "objectName") == 0)
        return QObject::property(name);
    return internalPropGet(name);
}

/*!
    Sets the remote property \a name to \a value. Returns \c true on success.
*/
bool QDBusAbstractInterface::setProperty(const char *name, const QVariant &value) {
    if (std::strcmp(name, "objectName") == 0)
        return QObject::setProperty(name, value);
    internalPropSet(name, value);
    return !lastError().isValid();
}

/*!
    Reads the remote property \a propname with \c{org.freedesktop.DBus.Properties.Get}. If this
    class declares the property, the value is converted to its type.
*/
QVariant QDBusAbstractInterface::internalPropGet(const char *propname) const {
    QDBusMessage m = QDBusMessage::createMethodCall(
        d->service, d->path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    m << d->interface << QString::fromLatin1(propname);
    const QDBusMessage reply = d->conn().call(m, QDBus::Block, d->timeout);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        d->setError(reply.type() == QDBusMessage::ErrorMessage
                        ? QDBusError(reply)
                        : QDBusError(QDBusError::InvalidSignature, QStringLiteral("Invalid reply to Get")));
        return QVariant();
    }
    d->setError(QDBusError());
    QVariant value = reply.arguments().first().value<QDBusVariant>().variant();
    const int index = metaObject()->indexOfProperty(propname);
    if (index >= 0) {
        const QMetaType type = metaObject()->property(index).metaType();
        bool ok = false;
        const QVariant converted = qDBusAdaptArgument(value, type, &ok);
        if (ok)
            value = converted;
    }
    return value;
}

/*!
    Writes \a value to the remote property \a propname with
    \c{org.freedesktop.DBus.Properties.Set}.
*/
void QDBusAbstractInterface::internalPropSet(const char *propname, const QVariant &value) {
    QDBusMessage m = QDBusMessage::createMethodCall(
        d->service, d->path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Set"));
    m << d->interface << QString::fromLatin1(propname)
      << QVariant::fromValue(QDBusVariant(fitProperty(propname, value)));
    const QDBusMessage reply = d->conn().call(m, QDBus::Block, d->timeout);
    d->setError(reply.type() == QDBusMessage::ErrorMessage ? QDBusError(reply) : QDBusError());
}

#include "moc_qdbusabstractinterface.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
