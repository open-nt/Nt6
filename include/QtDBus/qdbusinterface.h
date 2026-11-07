// MIT License - see LICENSE
#pragma once

#include <QtDBus/qdbusabstractinterface.h>

#include <memory>

class QDBusInterfacePrivate;

class QDBusInterface : public QDBusAbstractInterface {
    Q_OBJECT
public:
    QDBusInterface(const QString &service, const QString &path, const QString &interface = QString(),
                   const QDBusConnection &connection = QDBusConnection::sessionBus(), QObject *parent = nullptr);
    ~QDBusInterface() override;

private:
    QList<QVariant> fitArguments(const QString &method, const QList<QVariant> &args) const override;
    QVariant fitProperty(const char *name, const QVariant &value) const override;

    std::unique_ptr<QDBusInterfacePrivate> dd;
};
