// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtNetwork/qabstractsocket.h>
#include <QtNetwork/qhostaddress.h>

#include <memory>

class QTcpSocket;

class QTcpServer : public QObject {
    Q_OBJECT
public:
    explicit QTcpServer(QObject *parent = nullptr);
    ~QTcpServer() override;

    bool listen(const QHostAddress &address = QHostAddress::Any, quint16 port = 0);
    void close();
    /*!
        Returns \c true if the server is listening for incoming connections;
        otherwise returns \c false.

        \sa listen(), close()
    */
    bool isListening() const { return m_fd >= 0; }

    /*!
        Sets the maximum number of pending accepted connections to
        \a numConnections. The server stops accepting while that many
        connections are waiting. The default is 30.

        \sa maxPendingConnections(), nextPendingConnection()
    */
    void setMaxPendingConnections(int numConnections) { m_maxPending = numConnections; }
    /*!
        Returns the maximum number of pending accepted connections.

        \sa setMaxPendingConnections()
    */
    int maxPendingConnections() const { return m_maxPending; }
    quint16 serverPort() const;
    QHostAddress serverAddress() const;
    /*!
        Returns the native operating system socket descriptor the server is
        listening on, or -1 if it is not listening.

        \sa setSocketDescriptor()
    */
    qintptr socketDescriptor() const { return m_fd; }
    bool setSocketDescriptor(qintptr socketDescriptor);

    bool waitForNewConnection(int msec = 0, bool *timedOut = nullptr);
    virtual bool hasPendingConnections() const;
    virtual QTcpSocket *nextPendingConnection();

    /*!
        Returns the type of error that last occurred in the server.

        \sa errorString()
    */
    QAbstractSocket::SocketError serverError() const { return m_error; }
    /*!
        Returns a human-readable description of the last error that
        occurred.

        \sa serverError()
    */
    QString errorString() const { return m_errorString; }
    void pauseAccepting();
    void resumeAccepting();

signals:
    /*!
        This signal is emitted every time a new connection is available.
        Call nextPendingConnection() to accept it.

        \sa hasPendingConnections(), nextPendingConnection()
    */
    void newConnection() { emitSignal(&QTcpServer::newConnection); }
    /*!
        \since 6.4
        This signal is emitted every time a connection has been added to the
        pending connections, by addPendingConnection(). For a QTcpServer
        that is when newConnection() is emitted; a QSslServer emits it once
        the handshake of the connection is done.

        \sa hasPendingConnections(), nextPendingConnection()
    */
    void pendingConnectionAvailable() { emitSignal(&QTcpServer::pendingConnectionAvailable); }
    /*!
        This signal is emitted when accepting a new connection results in an
        error. The \a socketError parameter describes the type of error.

        \sa serverError()
    */
    void acceptError(QAbstractSocket::SocketError socketError) { emitSignal(&QTcpServer::acceptError, socketError); }

protected:
    virtual void incomingConnection(qintptr handle);
    void addPendingConnection(QTcpSocket *socket);

private:
    struct Private;
    void acceptActivated();

    int m_fd = -1;
    int m_maxPending = 30;
    QAbstractSocket::SocketError m_error = QAbstractSocket::UnknownSocketError;
    QString m_errorString;
    class QSocketNotifier *m_notifier = nullptr;
    std::unique_ptr<Private> d;
};
