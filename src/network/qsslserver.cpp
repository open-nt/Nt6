// MIT License - see LICENSE
#include <QtCore/qpointer.h>
#include <QtCore/qtimer.h>
#include <QtNetwork/qsslserver.h>

#include <unistd.h>

/*!
    \class QSslServer
    \brief The QSslServer class accepts TLS connections.

    Set a configuration holding a certificate and key, then use it like
    QTcpServer. newConnection() is emitted only for clients that finish the
    handshake, and nextPendingConnection() returns a socket that is already
    encrypted:

    \code
    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    config.setLocalCertificate(certificate);
    config.setPrivateKey(key);

    QSslServer server;
    server.setSslConfiguration(config);
    server.listen(QHostAddress::Any, 8443);
    QObject::connect(&server, &QSslServer::newConnection, [&] {
        auto *client = static_cast<QSslSocket *>(server.nextPendingConnection());
        client->write("hello\n");
    });
    \endcode

    A client that does not finish its handshake within handshakeTimeout() is
    dropped, so a slow client cannot hold connections open indefinitely.

    \differences
    The \c alertSent, \c alertReceived, \c peerVerifyError,
    \c preSharedKeyAuthenticationRequired and \c handshakeInterruptedOnError
    signals are absent.
    \enddifferences

    \sa QSslSocket, QSslConfiguration
*/

/*!
    Constructs a server with the given \a parent.
*/
QSslServer::QSslServer(QObject *parent) : QTcpServer(parent) {
    m_configuration = QSslConfiguration::defaultConfiguration();
}

/*!
    Destroys the server.
*/
QSslServer::~QSslServer() = default;

/*!
    Sets the configuration for new connections to \a sslConfiguration.

    \sa sslConfiguration()
*/
void QSslServer::setSslConfiguration(const QSslConfiguration &sslConfiguration) { m_configuration = sslConfiguration; }

/*!
    Returns the configuration for new connections.

    \sa setSslConfiguration()
*/
QSslConfiguration QSslServer::sslConfiguration() const { return m_configuration; }

/*!
    Sets how long a client has to finish its handshake to \a timeout
    milliseconds.

    \sa handshakeTimeout()
*/
void QSslServer::setHandshakeTimeout(int timeout) { m_timeout = timeout; }

/*!
    Returns how long a client has to finish its handshake, in milliseconds. The
    default is 5000.

    \sa setHandshakeTimeout()
*/
int QSslServer::handshakeTimeout() const { return m_timeout; }

/*!
    \reimp
*/
void QSslServer::incomingConnection(qintptr socket) {
    auto *s = new QSslSocket(this);
    if (!s->setSocketDescriptor(socket)) {
        ::close(static_cast<int>(socket));
        delete s;
        return;
    }
    s->setSslConfiguration(m_configuration);

    auto *timer = new QTimer(s);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, s, [this, s] {
        errorOccurred(s, QAbstractSocket::SocketTimeoutError);
        s->abort();
        s->deleteLater();
    });

    QObject::connect(s, &QSslSocket::sslErrors, this,
                     [this, s](const QList<QSslError> &errors) { sslErrors(s, errors); });
    QObject::connect(s, &QAbstractSocket::errorOccurred, this, [this, s](QAbstractSocket::SocketError error) {
        if (s->isEncrypted())
            return; // the application owns the socket by now
        errorOccurred(s, error);
        s->deleteLater();
    });
    QObject::connect(s, &QSslSocket::encrypted, this, [this, s, timer] {
        timer->stop();
        addPendingConnection(s);
        newConnection();
    });

    timer->start(m_timeout);
    startedEncryptionHandshake(s);
    s->startServerEncryption();
}
