// MIT License - see LICENSE
#pragma once

// The state of a QWebSocket: the opening handshake of a client, and for both sides the framing,
// fragmentation, control frames and closing handshake of RFC 6455.

#include <QtCore/qbytearray.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qpointer.h>
#include <QtCore/qstring.h>
#include <QtNetwork/qabstractsocket.h>
#include <QtNetwork/qnetworkrequest.h>
#include <QtWebSockets/qwebsocket.h>
#include <QtWebSockets/qwebsockethandshakeoptions.h>
#include <QtWebSockets/qwebsocketprotocol.h>
#ifndef QT_NO_SSL
#include <QtNetwork/qsslconfiguration.h>
#include <QtNetwork/qsslerror.h>
#endif

class QMaskGenerator;
class QTimer;

class QWebSocketPrivate {
public:
    enum OpCode : quint8 {
        Continuation = 0x0,
        Text = 0x1,
        Binary = 0x2,
        Close = 0x8,
        Ping = 0x9,
        Pong = 0xA,
    };

    // How long to wait for the peer's part of the closing handshake.
    static constexpr int CloseTimeout = 5000;

    QWebSocketPrivate(QWebSocket *q, const QString &origin, QWebSocketProtocol::Version version);
    ~QWebSocketPrivate();

    // A server-side socket whose opening handshake has been answered. pending holds the bytes that
    // arrived after the request; they are processed from the event loop.
    static QWebSocket *createServerSide(QAbstractSocket *socket, const QNetworkRequest &request, const QString &origin,
                                        const QString &subprotocol, const QByteArray &pending, QObject *parent);

    void open(const QNetworkRequest &request, const QWebSocketHandshakeOptions &options);
    void close(QWebSocketProtocol::CloseCode code, const QString &reason);
    void abort();
    qint64 sendMessage(OpCode opcode, const QByteArray &data);
    void ping(const QByteArray &payload);

    QWebSocket *q;
    QPointer<QAbstractSocket> socket;
    bool isServer = false;
    QAbstractSocket::SocketState state = QAbstractSocket::UnconnectedState;
    QAbstractSocket::SocketError error = QAbstractSocket::UnknownSocketError;
    QString errorString;

    QString origin;
    QWebSocketProtocol::Version version;
    QNetworkRequest request;
    QWebSocketHandshakeOptions options;
    QString subprotocol;
    QWebSocketProtocol::CloseCode closeCode = QWebSocketProtocol::CloseCodeNormal;
    QString closeReason;

    const QMaskGenerator *maskGenerator = nullptr;
    quint64 maxFrameSize;
    quint64 maxMessageSize;
    quint64 outgoingFrameSize;

#ifndef QT_NO_SSL
    QSslConfiguration sslConfiguration;
    bool hasSslConfiguration = false;
    QList<QSslError> ignoredSslErrors;
#endif

private:
    void attach(QAbstractSocket *s);
    void setState(QAbstractSocket::SocketState s);
    void fail(QAbstractSocket::SocketError e, const QString &message);
    void sendHandshake();
    bool processHandshake();
    void readyRead();
    void processFrames();
    bool processFrame(bool fin, quint8 opcode, const QByteArray &payload);
    bool handleText(const QByteArray &payload, bool fin);
    void handleClose(const QByteArray &payload);
    void failConnection(QWebSocketProtocol::CloseCode code, const QString &reason);
    void writeFrame(quint8 opcode, const char *data, qsizetype size, bool fin);
    void sendClose(QWebSocketProtocol::CloseCode code, const QString &reason);
    void closeTcp();
    void socketDisconnected();
    quint32 nextMask();

    QByteArray m_key;    // the client's Sec-WebSocket-Key
    QByteArray m_buffer; // received bytes not processed yet
    qsizetype m_offset = 0;
    bool m_handshakeDone = false;
    bool m_tcpConnected = false;
    bool m_closeSent = false;
    bool m_closeReceived = false;
    bool m_processing = false;
    bool m_failed = false; // no more frames are read

    bool m_inMessage = false;
    quint8 m_messageOpcode = 0;
    quint64 m_messageSize = 0;
    QByteArray m_binaryMessage;
    QString m_textMessage;
    QByteArray m_textCarry; // the start of a character split between frames

    QElapsedTimer m_pingTimer;
    QTimer *m_closeTimer = nullptr;
};
