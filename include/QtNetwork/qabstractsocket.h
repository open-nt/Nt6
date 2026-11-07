// MIT License - see LICENSE
#pragma once

#include <QtCore/qiodevice.h>
#include <QtCore/qflags.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <memory>

class QHostAddress;

class QAbstractSocket : public QIODevice {
    Q_OBJECT
public:
    enum SocketType { TcpSocket, UdpSocket, SctpSocket, UnknownSocketType = -1 };
    enum NetworkLayerProtocol { IPv4Protocol, IPv6Protocol, AnyIPProtocol, UnknownNetworkLayerProtocol = -1 };
    enum SocketError {
        ConnectionRefusedError,
        RemoteHostClosedError,
        HostNotFoundError,
        SocketAccessError,
        SocketResourceError,
        SocketTimeoutError,
        DatagramTooLargeError,
        NetworkError,
        AddressInUseError,
        SocketAddressNotAvailableError,
        UnsupportedSocketOperationError,
        UnfinishedSocketOperationError,
        ProxyAuthenticationRequiredError,
        SslHandshakeFailedError,
        ProxyConnectionRefusedError,
        ProxyConnectionClosedError,
        ProxyConnectionTimeoutError,
        ProxyNotFoundError,
        ProxyProtocolError,
        OperationError,
        SslInternalError,
        SslInvalidUserDataError,
        TemporaryError,
        UnknownSocketError = -1
    };
    enum SocketState {
        UnconnectedState,
        HostLookupState,
        ConnectingState,
        ConnectedState,
        BoundState,
        ListeningState,
        ClosingState
    };
    enum SocketOption {
        LowDelayOption,
        KeepAliveOption,
        MulticastTtlOption,
        MulticastLoopbackOption,
        TypeOfServiceOption,
        SendBufferSizeSocketOption,
        ReceiveBufferSizeSocketOption,
        PathMtuSocketOption
    };
    enum BindFlag { DefaultForPlatform = 0x0, ShareAddress = 0x1, DontShareAddress = 0x2, ReuseAddressHint = 0x4 };
    Q_DECLARE_FLAGS(BindMode, BindFlag)

    QAbstractSocket(SocketType socketType, QObject *parent);
    ~QAbstractSocket() override;

    // Host names are resolved synchronously.
    virtual void connectToHost(const QString &hostName, quint16 port, OpenMode mode = ReadWrite,
                               NetworkLayerProtocol protocol = AnyIPProtocol);
    virtual void connectToHost(const QHostAddress &address, quint16 port, OpenMode mode = ReadWrite);
    virtual void disconnectFromHost();

    bool bind(const QHostAddress &address, quint16 port = 0, BindMode mode = DefaultForPlatform);
    bool bind(quint16 port = 0, BindMode mode = DefaultForPlatform);

    bool isValid() const { return m_fd >= 0; }
    qint64 bytesAvailable() const override;
    qint64 bytesToWrite() const override;
    bool canReadLine() const override;
    bool isSequential() const override { return true; }
    void close() override;
    bool flush();
    void abort();

    quint16 localPort() const;
    QHostAddress localAddress() const;
    quint16 peerPort() const;
    QHostAddress peerAddress() const;
    QString peerName() const;
    qint64 readBufferSize() const { return m_readBufferSize; }
    void setReadBufferSize(qint64 size);

    virtual qintptr socketDescriptor() const { return m_fd; }
    virtual bool setSocketDescriptor(qintptr socketDescriptor, SocketState state = ConnectedState,
                                     OpenMode openMode = ReadWrite);
    // Qt passes a QVariant; plain int values cover every option until QVariant exists.
    virtual void setSocketOption(SocketOption option, int value);
    virtual int socketOption(SocketOption option);

    SocketType socketType() const { return m_type; }
    SocketState state() const { return m_state; }
    SocketError error() const { return m_error; }

    virtual bool waitForConnected(int msecs = 30000);
    bool waitForReadyRead(int msecs = 30000) override;
    bool waitForBytesWritten(int msecs = 30000) override;
    virtual bool waitForDisconnected(int msecs = 30000);

signals:
    void hostFound() { emitSignal(&QAbstractSocket::hostFound); }
    void connected() { emitSignal(&QAbstractSocket::connected); }
    void disconnected() { emitSignal(&QAbstractSocket::disconnected); }
    void stateChanged(QAbstractSocket::SocketState state) { emitSignal(&QAbstractSocket::stateChanged, state); }
    void errorOccurred(QAbstractSocket::SocketError error) { emitSignal(&QAbstractSocket::errorOccurred, error); }

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;
    void setSocketState(SocketState state);
    void setSocketError(SocketError error, const QString &text);
    void setSocketErrorFromErrno(int err);
    // Creates the OS socket if needed; family is AF_INET or AF_INET6.
    bool createSocket(int family);
    int socketFamily() const { return m_family; }
    virtual void readActivated();

private:
    struct Private;
    void connectNext();
    void socketConnected();
    void writeActivated();
    qint64 flushWrites();
    void finishDisconnect();
    void remoteClosed();
    void closeSocket();
    bool adoptSocket(int fd, int family);
    void updateAddresses();
    void updateNotifiers();
    short pollOnce(short events, int timeoutMs);

    SocketType m_type;
    SocketState m_state = UnconnectedState;
    SocketError m_error = UnknownSocketError;
    int m_fd = -1;
    int m_family = 0;
    QByteArray m_rbuf;
    qint64 m_rpos = 0;
    QByteArray m_wbuf;
    qint64 m_readBufferSize = 0;
    bool m_readyReadEmitted = false;
    class QSocketNotifier *m_rn = nullptr;
    class QSocketNotifier *m_wn = nullptr;
    std::unique_ptr<Private> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QAbstractSocket::BindMode)

#include <QtNetwork/qhostaddress.h>
