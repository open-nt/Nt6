// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>
#include <QtDBus/qdbusconnection.h>

#include <memory>

class QDBusServiceWatcherPrivate;

class QDBusServiceWatcher : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList watchedServices READ watchedServices WRITE setWatchedServices)
    Q_PROPERTY(WatchMode watchMode READ watchMode WRITE setWatchMode)
public:
    enum WatchModeFlag { WatchForRegistration = 0x01, WatchForUnregistration = 0x02, WatchForOwnerChange = 0x03 };
    Q_DECLARE_FLAGS(WatchMode, WatchModeFlag)
    Q_FLAG(WatchMode)

    explicit QDBusServiceWatcher(QObject *parent = nullptr);
    QDBusServiceWatcher(const QString &service, const QDBusConnection &connection,
                        WatchMode watchMode = WatchForOwnerChange, QObject *parent = nullptr);
    ~QDBusServiceWatcher() override;

    QStringList watchedServices() const;
    void setWatchedServices(const QStringList &services);
    void addWatchedService(const QString &newService);
    bool removeWatchedService(const QString &service);

    WatchMode watchMode() const;
    void setWatchMode(WatchMode mode);

    QDBusConnection connection() const;
    void setConnection(const QDBusConnection &connection);

signals:
    /*!
        This signal is emitted when the watched \a service gets an owner.
    */
    void serviceRegistered(const QString &service) { emitSignal(&QDBusServiceWatcher::serviceRegistered, service); }
    /*!
        This signal is emitted when the watched \a service loses its owner.
    */
    void serviceUnregistered(const QString &service) { emitSignal(&QDBusServiceWatcher::serviceUnregistered, service); }
    /*!
        This signal is emitted when the owner of the watched \a service changes from \a oldOwner
        to \a newOwner.
    */
    void serviceOwnerChanged(const QString &service, const QString &oldOwner, const QString &newOwner) {
        emitSignal(&QDBusServiceWatcher::serviceOwnerChanged, service, oldOwner, newOwner);
    }

private:
    std::unique_ptr<QDBusServiceWatcherPrivate> d;
    void hookAll();
    void handle(const QDBusMessage &message);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QDBusServiceWatcher::WatchMode)
