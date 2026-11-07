// MIT License - see LICENSE
#pragma once

#include <QtCore/qstringlist.h>
#include <QtDBus/qdbusabstractinterface.h>
#include <QtDBus/qdbusreply.h>

class QDBusConnectionPrivate;

class QDBusConnectionInterface : public QDBusAbstractInterface {
    Q_OBJECT
public:
    enum ServiceQueueOptions { DontQueueService, QueueService, ReplaceExistingService };
    Q_ENUM(ServiceQueueOptions)
    enum ServiceReplacementOptions { DontAllowReplacement, AllowReplacement };
    Q_ENUM(ServiceReplacementOptions)
    enum RegisterServiceReply { ServiceNotRegistered = 0, ServiceRegistered, ServiceQueued };
    Q_ENUM(RegisterServiceReply)

    QDBusReply<QStringList> registeredServiceNames() const;
    QDBusReply<QStringList> activatableServiceNames() const;
    QDBusReply<bool> isServiceRegistered(const QString &serviceName) const;
    QDBusReply<QString> serviceOwner(const QString &name) const;
    QDBusReply<bool> unregisterService(const QString &serviceName);
    QDBusReply<QDBusConnectionInterface::RegisterServiceReply>
    registerService(const QString &serviceName, ServiceQueueOptions qoption = DontQueueService,
                    ServiceReplacementOptions roption = DontAllowReplacement);
    QDBusReply<uint> servicePid(const QString &serviceName) const;
    QDBusReply<uint> serviceUid(const QString &serviceName) const;
    QDBusReply<void> startService(const QString &name);

signals:
    /*!
        This signal is emitted when \a service gets its first owner.
    */
    void serviceRegistered(const QString &service) {
        emitSignal(&QDBusConnectionInterface::serviceRegistered, service);
    }
    /*!
        This signal is emitted when \a service loses its last owner.
    */
    void serviceUnregistered(const QString &service) {
        emitSignal(&QDBusConnectionInterface::serviceUnregistered, service);
    }
    /*!
        This signal is emitted when the owner of \a name changes from \a oldOwner to \a newOwner.
    */
    void serviceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner) {
        emitSignal(&QDBusConnectionInterface::serviceOwnerChanged, name, oldOwner, newOwner);
    }
    /*!
        This signal is emitted when QDBusConnection::callWithCallback() fails to deliver the
        result of \a call, with the \a error that occurred.
    */
    void callWithCallbackFailed(const QDBusError &error, const QDBusMessage &call) {
        emitSignal(&QDBusConnectionInterface::callWithCallbackFailed, error, call);
    }

private:
    friend class QDBusConnectionPrivate;
    explicit QDBusConnectionInterface(const QDBusConnection &connection, QObject *parent);
};

Q_DECLARE_METATYPE(QDBusConnectionInterface::RegisterServiceReply)
