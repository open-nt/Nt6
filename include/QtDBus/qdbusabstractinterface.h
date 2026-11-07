// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>
#include <QtDBus/qdbusconnection.h>
#include <QtDBus/qdbuserror.h>
#include <QtDBus/qdbusmessage.h>
#include <QtDBus/qdbuspendingcall.h>

#include <memory>
#include <utility>

class QDBusAbstractInterfacePrivate;

class QDBusAbstractInterface : public QObject {
    Q_OBJECT
public:
    ~QDBusAbstractInterface() override;

    bool isValid() const;
    QDBusConnection connection() const;
    QString service() const;
    QString path() const;
    QString interface() const;
    QDBusError lastError() const;
    void setTimeout(int timeout);
    int timeout() const;
    void setInteractiveAuthorizationAllowed(bool enable);
    bool isInteractiveAuthorizationAllowed() const;

    QVariant property(const char *name) const;
    bool setProperty(const char *name, const QVariant &value);

    /*!
        Calls \a method on the remote object with the arguments \a args and blocks for the reply.
    */
    template <typename... Args> QDBusMessage call(const QString &method, Args &&...args) {
        return callWithArgumentList(QDBus::AutoDetect, method, {QVariant::fromValue(std::forward<Args>(args))...});
    }
    /*!
        \overload
        Calls \a method on the remote object with the arguments \a args, waiting as \a mode says.
    */
    template <typename... Args> QDBusMessage call(QDBus::CallMode mode, const QString &method, Args &&...args) {
        return callWithArgumentList(mode, method, {QVariant::fromValue(std::forward<Args>(args))...});
    }
    /*!
        Calls \a method on the remote object with the arguments \a args and returns at once.
    */
    template <typename... Args> QDBusPendingCall asyncCall(const QString &method, Args &&...args) {
        return asyncCallWithArgumentList(method, {QVariant::fromValue(std::forward<Args>(args))...});
    }

    QDBusMessage callWithArgumentList(QDBus::CallMode mode, const QString &method, const QList<QVariant> &args);
    QDBusPendingCall asyncCallWithArgumentList(const QString &method, const QList<QVariant> &args);
    bool callWithCallback(const QString &method, const QList<QVariant> &args, QObject *receiver,
                          const char *returnMethod, const char *errorMethod);
    bool callWithCallback(const QString &method, const QList<QVariant> &args, QObject *receiver, const char *slot);

protected:
    QDBusAbstractInterface(const QString &service, const QString &path, const char *interface,
                           const QDBusConnection &connection, QObject *parent);

    QVariant internalPropGet(const char *propname) const;
    void internalPropSet(const char *propname, const QVariant &value);

private:
    friend class QDBusInterface;
    friend class QDBusConnectionInterface;
    QDBusAbstractInterface(const QString &service, const QString &path, const QString &interface,
                           const QDBusConnection &connection, QObject *parent, bool relaySignals);
    virtual QList<QVariant> fitArguments(const QString &method, const QList<QVariant> &args) const;
    virtual QVariant fitProperty(const char *name, const QVariant &value) const;
    void relaySignal(const QDBusMessage &message);

    std::unique_ptr<QDBusAbstractInterfacePrivate> d;
};
