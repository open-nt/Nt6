// MIT License - see LICENSE
#include <QtCore/qdir.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qpointer.h>
#include <QtCore/qsocketnotifier.h>
#include <QtNetwork/qlocalsocket.h>

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

/*!
    \class QLocalSocket
    \brief The QLocalSocket class provides a local stream socket for talking to
    another process on the same machine.

    Connect to a QLocalServer by name, then read and write as with any
    QIODevice:

    \code
    QLocalSocket socket;
    socket.connectToServer("my-service");
    socket.write("hello");
    \endcode

    A relative name is placed in QDir::tempPath(); a name starting with \c /
    is used as the socket path.

    \differences
    Connecting either succeeds or fails before connectToServer() returns, so
    the socket never stays in ConnectingState. Linux and other POSIX systems
    only, using Unix domain sockets.
    \enddifferences

    \sa QLocalServer
*/

/*!
    \enum QLocalSocket::LocalSocketError

    Describes an error on a local socket.

    \value ConnectionRefusedError The server refused the connection.
    \value PeerClosedError The other end closed the connection.
    \value ServerNotFoundError No server has that name.
    \value SocketAccessError The process may not use that socket.
    \value SocketResourceError The system ran out of resources.
    \value SocketTimeoutError The operation timed out.
    \value DatagramTooLargeError The data was too large to send.
    \value ConnectionError A connection error occurred.
    \value UnsupportedSocketOperationError The operation is not supported.
    \value OperationError The operation is not valid in the current state.
    \value UnknownSocketError An unidentified error occurred.
*/

/*!
    \enum QLocalSocket::LocalSocketState

    Describes the state of a local socket.

    \value UnconnectedState The socket is not connected.
    \value ConnectingState The socket is connecting.
    \value ConnectedState The socket is connected.
    \value ClosingState The socket is closing and flushing pending writes.
*/

/*!
    Constructs a local socket with the given \a parent.
*/
QLocalSocket::QLocalSocket(QObject *parent) : QIODevice(parent) {}

/*!
    Destroys the socket, closing the connection if necessary.
*/
QLocalSocket::~QLocalSocket() { closeDescriptor(); }

void QLocalSocket::setState(LocalSocketState state) {
    if (m_state == state)
        return;
    m_state = state;
    stateChanged(state);
}

void QLocalSocket::setError(LocalSocketError error, const QString &text) {
    m_error = error;
    setErrorString(text);
    errorOccurred(error);
}

void QLocalSocket::adopt(int fd) {
    m_fd = fd;
    m_rn = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    m_wn = new QSocketNotifier(fd, QSocketNotifier::Write, this);
    m_wn->setEnabled(false);
    QObject::connect(m_rn, &QSocketNotifier::activated, this, [this] { receive(); });
    QObject::connect(m_wn, &QSocketNotifier::activated, this, [this] { send(); });
}

void QLocalSocket::closeDescriptor() {
    for (QSocketNotifier *n : {m_rn, m_wn}) {
        if (n) {
            n->setEnabled(false);
            n->deleteLater();
        }
    }
    m_rn = m_wn = nullptr;
    if (m_fd >= 0)
        ::close(m_fd);
    m_fd = -1;
    m_wbuf.clear();
}

void QLocalSocket::updateNotifiers() {
    if (m_wn)
        m_wn->setEnabled(!m_wbuf.isEmpty());
    if (m_rn)
        m_rn->setEnabled(m_readLimit <= 0 || m_rbuf.size() - m_rpos < m_readLimit);
}

void QLocalSocket::receive() {
    if (m_fd < 0)
        return;
    bool got = false, peerClosed = false;
    for (;;) {
        qint64 room = 65536;
        if (m_readLimit > 0) {
            room = qMin(room, m_readLimit - (m_rbuf.size() - m_rpos));
            if (room <= 0)
                break;
        }
        char chunk[65536];
        const ssize_t n = ::recv(m_fd, chunk, static_cast<std::size_t>(room), 0);
        if (n > 0) {
            m_rbuf.append(chunk, n);
            got = true;
        } else if (n == 0) {
            peerClosed = true;
            break;
        } else {
            if (errno == EINTR)
                continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                peerClosed = true;
            break;
        }
    }
    updateNotifiers();
    QPointer<QLocalSocket> guard(this); // a slot may delete the socket
    if (got)
        readyRead();
    if (peerClosed && guard && m_fd >= 0) {
        setError(PeerClosedError, QString("The remote socket closed the connection"));
        if (guard)
            finishClosing();
    }
}

qint64 QLocalSocket::send() {
    if (m_fd < 0 || m_wbuf.isEmpty()) {
        updateNotifiers();
        return 0;
    }
    qint64 sent = 0;
    while (sent < m_wbuf.size()) {
        const ssize_t n = ::send(m_fd, m_wbuf.constData() + sent, static_cast<std::size_t>(m_wbuf.size() - sent), MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            m_wbuf.clear();
            setError(PeerClosedError, QString::fromUtf8(std::strerror(errno)));
            finishClosing();
            return -1;
        }
        sent += n;
    }
    m_wbuf = m_wbuf.mid(sent);
    updateNotifiers();
    QPointer<QLocalSocket> guard(this);
    if (sent > 0)
        bytesWritten(sent);
    if (guard && m_state == ClosingState && m_wbuf.isEmpty())
        finishClosing();
    return sent;
}

void QLocalSocket::finishClosing() {
    closeDescriptor();
    QPointer<QLocalSocket> guard(this);
    readChannelFinished();
    if (!guard)
        return;
    setState(UnconnectedState);
    if (guard)
        disconnected();
}

/*!
    Connects to the server set with setServerName(), opening the socket with
    \a openMode.

    \sa setServerName(), connected()
*/
void QLocalSocket::connectToServer(OpenMode openMode) {
    if (m_state != UnconnectedState) {
        setError(OperationError, QString("Trying to connect while connection is in progress"));
        return;
    }
    m_error = UnknownSocketError;
    m_rbuf.clear();
    m_rpos = 0;
    if (m_name.isEmpty()) {
        setError(ServerNotFoundError, QString("Invalid name"));
        return;
    }
    m_fullName = m_name.startsWith(QChar(u'/')) ? m_name : QDir::tempPath() + QChar(u'/') + m_name;
    sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    const QByteArray path = m_fullName.toUtf8();
    if (path.size() >= static_cast<qsizetype>(sizeof addr.sun_path)) {
        setError(ServerNotFoundError, QString("Socket path is too long"));
        return;
    }
    std::memcpy(addr.sun_path, path.constData(), static_cast<std::size_t>(path.size()));

    setState(ConnectingState);
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        setError(SocketResourceError, QString::fromUtf8(std::strerror(errno)));
        setState(UnconnectedState);
        return;
    }
    int rc;
    do {
        rc = ::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof addr);
    } while (rc != 0 && errno == EINTR);
    if (rc != 0) {
        const int err = errno;
        ::close(fd);
        const LocalSocketError e = err == ENOENT ? ServerNotFoundError
                                   : err == EACCES || err == EPERM ? SocketAccessError
                                   : err == EAGAIN || err == ECONNREFUSED ? ConnectionRefusedError
                                                                          : ConnectionError;
        setError(e, QString::fromUtf8(std::strerror(err)));
        setState(UnconnectedState);
        return;
    }
    adopt(fd);
    QIODevice::open(openMode);
    setState(ConnectedState);
    connected();
}

/*!
    \overload

    Sets the server name to \a name and connects, opening the socket with
    \a openMode.
*/
void QLocalSocket::connectToServer(const QString &name, OpenMode openMode) {
    setServerName(name);
    connectToServer(openMode);
}

/*!
    Sets the name of the server to connect to, to \a name.

    \sa serverName()
*/
void QLocalSocket::setServerName(const QString &name) {
    if (m_state == UnconnectedState)
        m_name = name;
}

/*!
    Returns the server name given to connectToServer().

    \sa fullServerName()
*/
QString QLocalSocket::serverName() const { return m_name; }

/*!
    Returns the path of the socket connected to.

    \sa serverName()
*/
QString QLocalSocket::fullServerName() const { return m_fullName; }

/*!
    Disconnects after pending data has been written.

    \sa abort(), disconnected()
*/
void QLocalSocket::disconnectFromServer() {
    if (m_state != ConnectedState)
        return;
    if (m_wbuf.isEmpty()) {
        finishClosing();
    } else {
        setState(ClosingState);
        send();
    }
}

/*!
    Closes the connection at once, discarding pending writes.

    \sa disconnectFromServer()
*/
void QLocalSocket::abort() {
    if (m_state == UnconnectedState) {
        QIODevice::close();
        return;
    }
    m_wbuf.clear();
    close();
}

/*!
    Writes as much pending data as possible without blocking. Returns \c true
    if any data was written.

    \sa waitForBytesWritten()
*/
bool QLocalSocket::flush() { return send() > 0; }

/*!
    Returns \c true if the socket is connected.

    \sa state()
*/
bool QLocalSocket::isValid() const { return m_state == ConnectedState; }

/*!
    \reimp
*/
bool QLocalSocket::isSequential() const { return true; }

/*!
    \reimp
*/
qint64 QLocalSocket::bytesAvailable() const { return QIODevice::bytesAvailable() + (m_rbuf.size() - m_rpos); }

/*!
    \reimp
*/
qint64 QLocalSocket::bytesToWrite() const { return m_wbuf.size(); }

/*!
    \reimp
*/
bool QLocalSocket::canReadLine() const { return QIODevice::canReadLine() || m_rbuf.indexOf('\n', m_rpos) != -1; }

/*!
    \reimp
*/
void QLocalSocket::close() {
    QIODevice::close();
    m_rbuf.clear();
    m_rpos = 0;
    if (m_state != UnconnectedState) {
        m_wbuf.clear();
        finishClosing();
    }
}

/*!
    Returns the last error.

    \sa errorOccurred()
*/
QLocalSocket::LocalSocketError QLocalSocket::error() const { return m_error; }

/*!
    Returns the socket's state.

    \sa stateChanged()
*/
QLocalSocket::LocalSocketState QLocalSocket::state() const { return m_state; }

/*!
    Returns the read buffer limit in bytes; \c 0 means unlimited.

    \sa setReadBufferSize()
*/
qint64 QLocalSocket::readBufferSize() const { return m_readLimit; }

/*!
    Limits the read buffer to \a size bytes; \c 0 means unlimited. When it is
    full, the sender is held back until data is read.

    \sa readBufferSize()
*/
void QLocalSocket::setReadBufferSize(qint64 size) {
    m_readLimit = size;
    updateNotifiers();
}

/*!
    Adopts the connected descriptor \a socketDescriptor, with the state
    \a socketState and open mode \a openMode. Returns \c false if the descriptor
    is invalid.

    \sa socketDescriptor()
*/
bool QLocalSocket::setSocketDescriptor(qintptr socketDescriptor, LocalSocketState socketState, OpenMode openMode) {
    if (socketDescriptor < 0)
        return false;
    closeDescriptor();
    const int fd = static_cast<int>(socketDescriptor);
    ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);
    adopt(fd);
    QIODevice::open(openMode);
    setState(socketState);
    return true;
}

/*!
    Returns the native descriptor, or \c -1 if there is none.

    \sa setSocketDescriptor()
*/
qintptr QLocalSocket::socketDescriptor() const { return m_fd; }

/*!
    Returns \c true if the socket is connected. Connecting finishes inside
    connectToServer(), so this never waits; \a msecs is ignored.

    \sa connectToServer()
*/
bool QLocalSocket::waitForConnected(int msecs) {
    Q_UNUSED(msecs)
    return m_state == ConnectedState;
}

/*!
    Waits up to \a msecs milliseconds for the socket to disconnect. A negative
    value waits forever. Returns \c true if it disconnected.

    \sa disconnectFromServer()
*/
bool QLocalSocket::waitForDisconnected(int msecs) {
    QElapsedTimer t;
    t.start();
    while (m_state != UnconnectedState) {
        const qint64 left = msecs < 0 ? -1 : msecs - t.elapsed();
        if (msecs >= 0 && left <= 0)
            return false;
        pollfd p = {m_fd, static_cast<short>(POLLIN | (m_wbuf.isEmpty() ? 0 : POLLOUT)), 0};
        if (::poll(&p, 1, static_cast<int>(left)) < 0 && errno != EINTR)
            return false;
        if (p.revents & POLLOUT)
            send();
        if (m_fd >= 0 && (p.revents & (POLLIN | POLLHUP | POLLERR)))
            receive();
    }
    return true;
}

/*!
    Waits up to \a msecs milliseconds for data to read, handling it as the
    event loop would. A negative value waits forever. Returns \c true if data
    arrived.

    \sa readyRead()
*/
bool QLocalSocket::waitForReadyRead(int msecs) {
    if (bytesAvailable() > 0)
        return true;
    if (m_fd < 0)
        return false;
    pollfd p = {m_fd, POLLIN, 0};
    int rc;
    do {
        rc = ::poll(&p, 1, msecs);
    } while (rc < 0 && errno == EINTR);
    if (rc <= 0) {
        if (rc == 0)
            setError(SocketTimeoutError, QString("Socket operation timed out"));
        return false;
    }
    const qint64 before = m_rbuf.size();
    receive();
    return m_rbuf.size() > before;
}

/*!
    Waits up to \a msecs milliseconds until pending data has been written. A
    negative value waits forever. Returns \c true if all of it was written.

    \sa flush()
*/
bool QLocalSocket::waitForBytesWritten(int msecs) {
    QElapsedTimer t;
    t.start();
    while (!m_wbuf.isEmpty() && m_fd >= 0) {
        const qint64 left = msecs < 0 ? -1 : msecs - t.elapsed();
        if (msecs >= 0 && left <= 0) {
            setError(SocketTimeoutError, QString("Socket operation timed out"));
            return false;
        }
        pollfd p = {m_fd, POLLOUT, 0};
        if (::poll(&p, 1, static_cast<int>(left)) < 0 && errno != EINTR)
            return false;
        if (p.revents & POLLOUT)
            send();
    }
    return m_wbuf.isEmpty() && m_fd >= 0;
}

/*!
    \reimp
*/
qint64 QLocalSocket::readData(char *data, qint64 maxSize) {
    const qint64 n = qMin(maxSize, m_rbuf.size() - m_rpos);
    if (n > 0) {
        std::memcpy(data, m_rbuf.constData() + m_rpos, static_cast<std::size_t>(n));
        m_rpos += n;
        if (m_rpos == m_rbuf.size()) {
            m_rbuf.clear();
            m_rpos = 0;
        }
        updateNotifiers();
        return n;
    }
    return m_state == UnconnectedState ? -1 : 0;
}

/*!
    \reimp
*/
qint64 QLocalSocket::writeData(const char *data, qint64 maxSize) {
    if (m_state != ConnectedState || m_fd < 0)
        return -1;
    m_wbuf.append(data, maxSize);
    updateNotifiers();
    return maxSize;
}
