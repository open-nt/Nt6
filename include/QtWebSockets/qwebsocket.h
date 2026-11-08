// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qurl.h>
#include <QtNetwork/qabstractsocket.h>
#include <QtNetwork/qhostaddress.h>
#include <QtNetwork/qnetworkrequest.h>
#include <QtWebSockets/qwebsockethandshakeoptions.h>
#include <QtWebSockets/qwebsocketprotocol.h>
#ifndef QT_NO_SSL
#include <QtNetwork/qsslconfiguration.h>
#include <QtNetwork/qsslerror.h>
#endif

#include <memory>

class QMaskGenerator;
class QWebSocketPrivate;

class QWebSocket : public QObject {
    Q_OBJECT
public:
    explicit QWebSocket(const QString &origin = QString(),
                        QWebSocketProtocol::Version version = QWebSocketProtocol::VersionLatest,
                        QObject *parent = nullptr);
    ~QWebSocket() override;

    void abort();
    QAbstractSocket::SocketError error() const;
    QString errorString() const;
    bool flush();
    bool isValid() const;
    QHostAddress localAddress() const;
    quint16 localPort() const;
    QHostAddress peerAddress() const;
    QString peerName() const;
    quint16 peerPort() const;
    QAbstractSocket::SocketState state() const;
    QWebSocketProtocol::Version version() const;
    QString resourceName() const;
    QUrl requestUrl() const;
    QNetworkRequest request() const;
    QWebSocketHandshakeOptions handshakeOptions() const;
    QString origin() const;
    QString subprotocol() const;
    QWebSocketProtocol::CloseCode closeCode() const;
    QString closeReason() const;
    qint64 bytesToWrite() const;

    void setMaskGenerator(const QMaskGenerator *maskGenerator);
    const QMaskGenerator *maskGenerator() const;

    qint64 sendTextMessage(const QString &message);
    qint64 sendBinaryMessage(const QByteArray &data);

#ifndef QT_NO_SSL
    void ignoreSslErrors(const QList<QSslError> &errors);
    void setSslConfiguration(const QSslConfiguration &sslConfiguration);
    QSslConfiguration sslConfiguration() const;
#endif

    void setMaxAllowedIncomingFrameSize(quint64 maxAllowedIncomingFrameSize);
    quint64 maxAllowedIncomingFrameSize() const;
    void setMaxAllowedIncomingMessageSize(quint64 maxAllowedIncomingMessageSize);
    quint64 maxAllowedIncomingMessageSize() const;
    static quint64 maxIncomingMessageSize();
    static quint64 maxIncomingFrameSize();
    void setOutgoingFrameSize(quint64 outgoingFrameSize);
    quint64 outgoingFrameSize() const;
    static quint64 maxOutgoingFrameSize();

public slots:
    void close(QWebSocketProtocol::CloseCode closeCode = QWebSocketProtocol::CloseCodeNormal,
               const QString &reason = QString());
    void open(const QUrl &url);
    void open(const QNetworkRequest &request);
    void open(const QUrl &url, const QWebSocketHandshakeOptions &options);
    void open(const QNetworkRequest &request, const QWebSocketHandshakeOptions &options);
    void ping(const QByteArray &payload = QByteArray());
#ifndef QT_NO_SSL
    void ignoreSslErrors();
#endif

signals:
    /*!
        This signal is emitted when the closing handshake begins, either
        because close() was called or because the peer sent a close frame.

        \sa close(), disconnected()
    */
    void aboutToClose() { emitSignal(&QWebSocket::aboutToClose); }
    /*!
        This signal is emitted when the opening handshake has succeeded and
        messages can be sent.

        \sa open(), disconnected()
    */
    void connected() { emitSignal(&QWebSocket::connected); }
    /*!
        This signal is emitted when the connection has closed. closeCode()
        and closeReason() say why.

        \sa connected(), close()
    */
    void disconnected() { emitSignal(&QWebSocket::disconnected); }
    /*!
        This signal is emitted when the state of the socket changes to \a state.

        \sa state()
    */
    void stateChanged(QAbstractSocket::SocketState state) { emitSignal(&QWebSocket::stateChanged, state); }
    /*!
        This signal is emitted when no more data will be received.
    */
    void readChannelFinished() { emitSignal(&QWebSocket::readChannelFinished); }
    /*!
        This signal is emitted for each text frame received. \a frame holds
        the complete characters of the frame, and \a isLastFrame is \c true
        for the last frame of a message. A character split between two frames
        is reported with the second one.

        \sa textMessageReceived()
    */
    void textFrameReceived(const QString &frame, bool isLastFrame) {
        emitSignal(&QWebSocket::textFrameReceived, frame, isLastFrame);
    }
    /*!
        This signal is emitted for each binary frame received, with its
        payload \a frame. \a isLastFrame is \c true for the last frame of a
        message.

        \sa binaryMessageReceived()
    */
    void binaryFrameReceived(const QByteArray &frame, bool isLastFrame) {
        emitSignal(&QWebSocket::binaryFrameReceived, frame, isLastFrame);
    }
    /*!
        This signal is emitted when a complete text \a message has been received.

        \sa textFrameReceived(), sendTextMessage()
    */
    void textMessageReceived(const QString &message) { emitSignal(&QWebSocket::textMessageReceived, message); }
    /*!
        This signal is emitted when a complete binary \a message has been received.

        \sa binaryFrameReceived(), sendBinaryMessage()
    */
    void binaryMessageReceived(const QByteArray &message) { emitSignal(&QWebSocket::binaryMessageReceived, message); }
    /*!
        This signal is emitted when an \a error occurred, for example when the
        host cannot be reached or the opening handshake fails.

        \sa error(), errorString()
    */
    void errorOccurred(QAbstractSocket::SocketError error) { emitSignal(&QWebSocket::errorOccurred, error); }
    /*!
        This signal is emitted when a pong arrives, with the \a payload of the
        ping and the round trip \a elapsedTime in milliseconds since the last
        ping() call.

        \sa ping()
    */
    void pong(quint64 elapsedTime, const QByteArray &payload) { emitSignal(&QWebSocket::pong, elapsedTime, payload); }
    /*!
        This signal is emitted when \a bytes bytes have been written to the network.

        \sa bytesToWrite()
    */
    void bytesWritten(qint64 bytes) { emitSignal(&QWebSocket::bytesWritten, bytes); }
#ifndef QT_NO_SSL
    /*!
        This signal is emitted when verifying the server's certificate found
        \a errors. Unless a connected slot calls ignoreSslErrors(), the
        connection is closed.

        \sa ignoreSslErrors()
    */
    void sslErrors(const QList<QSslError> &errors) { emitSignal(&QWebSocket::sslErrors, errors); }
    /*!
        This signal is emitted for each \a error found while verifying the
        server's certificate, before sslErrors().
    */
    void peerVerifyError(const QSslError &error) { emitSignal(&QWebSocket::peerVerifyError, error); }
#endif

private:
    friend class QWebSocketPrivate;
    friend class QWebSocketServer;
    std::unique_ptr<QWebSocketPrivate> d;
};
