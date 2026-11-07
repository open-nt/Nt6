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
    bool isListening() const { return m_fd >= 0; }

    void setMaxPendingConnections(int numConnections) { m_maxPending = numConnections; }
    int maxPendingConnections() const { return m_maxPending; }
    quint16 serverPort() const;
    QHostAddress serverAddress() const;
    qintptr socketDescriptor() const { return m_fd; }
    bool setSocketDescriptor(qintptr socketDescriptor);

    bool waitForNewConnection(int msec = 0, bool *timedOut = nullptr);
    virtual bool hasPendingConnections() const;
    virtual QTcpSocket *nextPendingConnection();

    QAbstractSocket::SocketError serverError() const { return m_error; }
    QString errorString() const { return m_errorString; }
    void pauseAccepting();
    void resumeAccepting();

signals:
    void newConnection() { emitSignal(&QTcpServer::newConnection); }
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
