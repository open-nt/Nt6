// MIT License - see LICENSE
#include "qdbusconnection_p.h"

#include <QtDBus/qdbusservicewatcher.h>

/*!
    \class QDBusServiceWatcher
    \brief The QDBusServiceWatcher class allows the user to watch for a bus service change.

    A watcher reports when the names it watches get or lose an owner:

    \code
    QDBusServiceWatcher watcher("org.example.Clock", QDBusConnection::sessionBus(),
                                QDBusServiceWatcher::WatchForUnregistration);
    QObject::connect(&watcher, &QDBusServiceWatcher::serviceUnregistered,
                     [](const QString &name) { qDebug() << name << "went away"; });
    \endcode

    A name that ends in \c{.*} watches every name below it, such as \c{org.example.*}.

    \differences
    None.
    \enddifferences

    \sa QDBusConnectionInterface
*/

/*!
    \enum QDBusServiceWatcher::WatchModeFlag

    \value WatchForRegistration Report names that get an owner.
    \value WatchForUnregistration Report names that lose their owner.
    \value WatchForOwnerChange Report every change of owner.
*/

class QDBusServiceWatcherPrivate {
public:
    QStringList services;
    QDBusServiceWatcher::WatchMode mode = QDBusServiceWatcher::WatchForOwnerChange;
    QDBusConnection connection{QString()};
    QList<uint> hooks;
};

/*!
    Constructs a watcher with the given \a parent that watches nothing yet.
*/
QDBusServiceWatcher::QDBusServiceWatcher(QObject *parent) : QObject(parent), d(new QDBusServiceWatcherPrivate) {}

/*!
    Constructs a watcher with the given \a parent that watches \a service on \a connection for
    the changes \a watchMode selects.
*/
QDBusServiceWatcher::QDBusServiceWatcher(const QString &service, const QDBusConnection &connection, WatchMode watchMode,
                                         QObject *parent)
    : QObject(parent), d(new QDBusServiceWatcherPrivate) {
    d->services = QStringList{service};
    d->mode = watchMode;
    d->connection = connection;
    hookAll();
}

/*!
    Destroys the watcher.
*/
QDBusServiceWatcher::~QDBusServiceWatcher() {
    if (const auto p = QDBusConnectionPrivate::get(d->connection))
        for (uint id : d->hooks)
            p->removeHook(id);
}

void QDBusServiceWatcher::hookAll() {
    const auto p = QDBusConnectionPrivate::get(d->connection);
    if (p) {
        for (uint id : d->hooks)
            p->removeHook(id);
    }
    d->hooks.clear();
    if (!p || !p->isConnected() || p->mode() != QDBusConnectionPrivate::BusMode)
        return;
    for (const QString &service : d->services) {
        QStringList args;
        QString ns;
        if (service.endsWith(QStringLiteral(".*")))
            ns = service.chopped(2);
        else if (service.endsWith(QLatin1Char('*')))
            ns = service.chopped(1);
        else
            args.append(service);
        d->hooks.append(p->addHook(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                                   QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameOwnerChanged"), args, ns,
                                   this, [this](const QDBusMessage &m) { handle(m); }));
    }
}

void QDBusServiceWatcher::handle(const QDBusMessage &message) {
    const QList<QVariant> args = message.arguments();
    if (args.size() != 3)
        return;
    const QString name = args.at(0).toString();
    const QString oldOwner = args.at(1).toString();
    const QString newOwner = args.at(2).toString();
    if (oldOwner.isEmpty() && (d->mode & WatchForRegistration))
        serviceRegistered(name);
    else if (newOwner.isEmpty() && (d->mode & WatchForUnregistration))
        serviceUnregistered(name);
    if ((d->mode & WatchForOwnerChange) == WatchForOwnerChange)
        serviceOwnerChanged(name, oldOwner, newOwner);
}

/*!
    Returns the names that are watched.
*/
QStringList QDBusServiceWatcher::watchedServices() const { return d->services; }

/*!
    Watches \a services instead of the names watched so far.
*/
void QDBusServiceWatcher::setWatchedServices(const QStringList &services) {
    d->services = services;
    hookAll();
}

/*!
    Adds \a newService to the names that are watched.
*/
void QDBusServiceWatcher::addWatchedService(const QString &newService) {
    if (d->services.contains(newService))
        return;
    d->services.append(newService);
    hookAll();
}

/*!
    Stops watching \a service. Returns \c true if it was watched.
*/
bool QDBusServiceWatcher::removeWatchedService(const QString &service) {
    if (!d->services.removeOne(service))
        return false;
    hookAll();
    return true;
}

/*!
    Returns the changes that are reported.
*/
QDBusServiceWatcher::WatchMode QDBusServiceWatcher::watchMode() const { return d->mode; }

/*!
    Reports the changes that \a mode selects.
*/
void QDBusServiceWatcher::setWatchMode(WatchMode mode) { d->mode = mode; }

/*!
    Returns the connection whose bus is watched.
*/
QDBusConnection QDBusServiceWatcher::connection() const { return d->connection; }

/*!
    Watches the bus of \a connection instead.
*/
void QDBusServiceWatcher::setConnection(const QDBusConnection &connection) {
    d->connection = connection;
    hookAll();
}

#include "moc_qdbusservicewatcher.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
