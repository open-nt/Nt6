// MIT License - see LICENSE
#pragma once

// One HTTP/1.1 connection of a QAbstractHttpServer: parses the requests that arrive on the socket,
// hands each one to the server, and writes the responses in order. Requests are answered one at a
// time; pipelined requests wait in the buffer until the previous response is complete.

#include <QtCore/qbytearray.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtHttpServer/qhttpserverrequest.h>
#include <QtHttpServer/qhttpserverresponder.h>
#include <QtNetwork/qhttpheaders.h>

class QAbstractHttpServer;
class QAbstractSocket;
class QIODevice;
class QLocalSocket;

class QHttpServerStream : public QObject {
public:
    using StatusCode = QHttpServerResponder::StatusCode;

    // Limits on what a client may send. Requests above them are refused with 414, 431 or 413.
    static constexpr qsizetype MaxRequestLineSize = 8 * 1024;
    static constexpr qsizetype MaxHeaderSize = 64 * 1024;
    static constexpr qsizetype MaxHeaderCount = 100;
    static constexpr qint64 MaxBodySize = 64 * 1024 * 1024;

    QHttpServerStream(QAbstractHttpServer *server, QIODevice *socket, bool encrypted);
    ~QHttpServerStream() override;

    // Used by QHttpServerResponder.
    bool canRespond() const;
    bool isCanceled() const { return m_canceled; }
    void respond(StatusCode status, const QHttpHeaders &headers, const QByteArray &body);
    void respondWithDevice(StatusCode status, const QHttpHeaders &headers, QIODevice *device);
    void beginChunked(StatusCode status, const QHttpHeaders &headers, const QList<QByteArray> &trailerNames);
    void writeChunk(const QByteArray &data);
    void endChunked(const QByteArray &data, const QHttpHeaders &trailers);
    void responderDestroyed();

    static QByteArray reasonPhrase(StatusCode status);

private:
    enum class Parse { RequestLine, Headers, Body, ChunkSize, ChunkData, ChunkDataEnd, Trailers, Failed };
    enum class Response { Idle, Waiting, Device, Chunked };

    void readFromSocket();
    void process();
    void scheduleProcess();
    bool parseStep();
    bool takeLine(QByteArray *line, qsizetype limit, bool *tooLong);
    bool parseRequestLine(const QByteArray &line);
    bool parseHeaderLine(const QByteArray &line);
    bool headersComplete();
    void dispatch();
    void upgradeToWebSocket(const QHttpServerRequest &request);
    void fail(StatusCode status);
    void resetRequest();

    QByteArray head(StatusCode status, const QHttpHeaders &headers, qint64 contentLength, bool chunked);
    void send(const QByteArray &data);
    void finishResponse();
    void pumpDevice();
    void finishDevice();
    void closeConnection();
    void socketDisconnected();

    QAbstractHttpServer *m_server;
    QIODevice *m_socket;
    QAbstractSocket *m_tcpSocket; // m_socket, if it is a TCP socket
    QLocalSocket *m_localSocket;  // m_socket, if it is a local socket
    bool m_encrypted;
    bool m_canceled = false;
    bool m_closing = false;
    bool m_processing = false;
    bool m_processScheduled = false;

    // Input.
    QByteArray m_buffer;
    qsizetype m_offset = 0;
    Parse m_parse = Parse::RequestLine;
    qsizetype m_headerBytes = 0;
    qint64 m_remaining = 0;
    QByteArray m_method;
    QByteArray m_target;
    int m_minorVersion = 1;
    QHttpHeaders m_headers;
    QByteArray m_body;

    // Output.
    Response m_response = Response::Idle;
    bool m_headRequest = false;
    bool m_http10 = false;
    bool m_closeAfterResponse = false;
    bool m_chunkedOutput = false;
    QPointer<QIODevice> m_device;
    qint64 m_deviceRemaining = -1; // -1: read the device until it ends
    bool m_deviceEnded = false;
};
