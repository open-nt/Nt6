// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtDBus/qdbusconnection.h>
#include <QtDBus/qdbuserror.h>

#include <memory>

class QDBusServerPrivate;

class QDBusServer : public QObject {
    Q_OBJECT
public:
    explicit QDBusServer(const QString &address, QObject *parent = nullptr);
    explicit QDBusServer(QObject *parent = nullptr);
    ~QDBusServer() override;

    bool isConnected() const;
    QDBusError lastError() const;
    QString address() const;
    void setAnonymousAuthenticationAllowed(bool value);

signals:
    /*!
        This signal is emitted when a client has connected and authenticated. \a connection is
        the server's side of the connection.
    */
    void newConnection(const QDBusConnection &connection) { emitSignal(&QDBusServer::newConnection, connection); }

private:
    std::shared_ptr<QDBusServerPrivate> d;
};
