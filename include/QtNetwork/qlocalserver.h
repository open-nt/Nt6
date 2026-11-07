// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtNetwork/qlocalsocket.h>

class QLocalServer : public QObject {
    Q_OBJECT
public:
    enum SocketOption {
        NoOptions = 0x0,
        UserAccessOption = 0x01,
        GroupAccessOption = 0x02,
        OtherAccessOption = 0x04,
        WorldAccessOption = 0x07
    };
    Q_DECLARE_FLAGS(SocketOptions, SocketOption)

    explicit QLocalServer(QObject *parent = nullptr);
    ~QLocalServer() override;

    bool listen(const QString &name);
    void close();
    bool isListening() const;
    QString serverName() const;
    QString fullServerName() const;
    QString errorString() const;
    QLocalSocket::LocalSocketError serverError() const;
    virtual bool hasPendingConnections() const;
    virtual QLocalSocket *nextPendingConnection();
    int maxPendingConnections() const;
    void setMaxPendingConnections(int numConnections);
    bool waitForNewConnection(int msec = 0, bool *timedOut = nullptr);
    SocketOptions socketOptions() const;
    void setSocketOptions(SocketOptions options);
    qintptr socketDescriptor() const;

    static bool removeServer(const QString &name);

signals:
    /*!
        This signal is emitted each time a new connection is available.
    */
    void newConnection() { emitSignal(&QLocalServer::newConnection); }

protected:
    virtual void incomingConnection(quintptr socketDescriptor);

private:
    void acceptPending();
    void updateNotifier();
    void fail(QLocalSocket::LocalSocketError error, const QString &text);

    int m_fd = -1;
    QString m_name;
    QString m_fullName;
    QString m_errorString;
    QLocalSocket::LocalSocketError m_error = QLocalSocket::UnknownSocketError;
    QList<QLocalSocket *> m_pending;
    int m_maxPending = 30;
    SocketOptions m_options = NoOptions;
    class QSocketNotifier *m_notifier = nullptr;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QLocalServer::SocketOptions)
