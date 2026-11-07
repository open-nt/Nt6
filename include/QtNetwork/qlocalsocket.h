// MIT License - see LICENSE
#pragma once

#include <QtCore/qiodevice.h>
#include <QtCore/qstring.h>

class QSocketNotifier;

class QLocalSocket : public QIODevice {
    Q_OBJECT
public:
    enum LocalSocketError {
        ConnectionRefusedError = 0,
        PeerClosedError = 1,
        ServerNotFoundError = 2,
        SocketAccessError = 3,
        SocketResourceError = 4,
        SocketTimeoutError = 5,
        DatagramTooLargeError = 6,
        ConnectionError = 7,
        UnsupportedSocketOperationError = 10,
        OperationError = 19,
        UnknownSocketError = -1
    };
    enum LocalSocketState { UnconnectedState = 0, ConnectingState = 2, ConnectedState = 3, ClosingState = 6 };

    explicit QLocalSocket(QObject *parent = nullptr);
    ~QLocalSocket() override;

    void connectToServer(OpenMode openMode = ReadWrite);
    void connectToServer(const QString &name, OpenMode openMode = ReadWrite);
    void disconnectFromServer();
    void setServerName(const QString &name);
    QString serverName() const;
    QString fullServerName() const;
    void abort();
    bool flush();
    bool isValid() const;
    bool isSequential() const override;
    qint64 bytesAvailable() const override;
    qint64 bytesToWrite() const override;
    bool canReadLine() const override;
    void close() override;
    LocalSocketError error() const;
    LocalSocketState state() const;
    qint64 readBufferSize() const;
    void setReadBufferSize(qint64 size);
    bool setSocketDescriptor(qintptr socketDescriptor, LocalSocketState socketState = ConnectedState,
                             OpenMode openMode = ReadWrite);
    qintptr socketDescriptor() const;

    bool waitForConnected(int msecs = 30000);
    bool waitForDisconnected(int msecs = 30000);
    bool waitForReadyRead(int msecs = 30000) override;
    bool waitForBytesWritten(int msecs = 30000) override;

signals:
    /*!
        This signal is emitted once the connection is established.
    */
    void connected() { emitSignal(&QLocalSocket::connected); }
    /*!
        This signal is emitted when the socket has disconnected.
    */
    void disconnected() { emitSignal(&QLocalSocket::disconnected); }
    /*!
        This signal is emitted when the error \a socketError occurs.
    */
    void errorOccurred(QLocalSocket::LocalSocketError socketError) {
        emitSignal(&QLocalSocket::errorOccurred, socketError);
    }
    /*!
        This signal is emitted when the state changes to \a socketState.
    */
    void stateChanged(QLocalSocket::LocalSocketState socketState) {
        emitSignal(&QLocalSocket::stateChanged, socketState);
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    void setState(LocalSocketState state);
    void setError(LocalSocketError error, const QString &text);
    void adopt(int fd);
    void closeDescriptor();
    void receive();
    qint64 send();
    void finishClosing();
    void updateNotifiers();

    int m_fd = -1;
    LocalSocketState m_state = UnconnectedState;
    LocalSocketError m_error = UnknownSocketError;
    QString m_name;
    QString m_fullName;
    QByteArray m_rbuf;
    qint64 m_rpos = 0;
    QByteArray m_wbuf;
    qint64 m_readLimit = 0;
    QSocketNotifier *m_rn = nullptr;
    QSocketNotifier *m_wn = nullptr;
};
