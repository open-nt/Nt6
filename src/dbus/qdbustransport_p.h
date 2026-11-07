// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qstring.h>
#include <QtDBus/qdbuserror.h>
#include <QtDBus/qdbusunixfiledescriptor.h>

// The socket layer of D-Bus: addresses, connecting, listening, authentication and passing file
// descriptors. Everything that depends on the operating system is here.
namespace QDBusTransport {

struct Address {
    QString transport;
    QMap<QString, QString> params;
};

// Parses a semicolon-separated list of addresses. Returns an empty list if any is malformed.
QList<Address> parseAddresses(const QString &addresses);
QString escapeValue(const QString &value);

// The address of the bus of the given kind (QDBusConnection::BusType), or an empty string.
QString busAddress(int busType);

// Connects to the first address that works and authenticates as a client with EXTERNAL, falling
// back to ANONYMOUS. Returns a non-blocking descriptor, or -1 with `error` set.
int connectAndAuthenticate(const QString &addresses, int timeoutMs, bool *unixFds, QDBusError *error);

// Listens on `address`. Returns the descriptor and the address clients use, or -1.
int listen(const QString &address, QString *clientAddress, QString *unlinkPath, QDBusError *error);
int accept(int listenFd);
void closeFd(int fd);

// The user id at the other end of a Unix socket, or -1.
long peerUid(int fd);
long currentUid();

// The server's side of the authentication, fed with the bytes the client sends.
class ServerAuth {
public:
    enum State { Waiting, Authenticated, Failed };
    ServerAuth(int fd, const QByteArray &guid, bool allowAnonymous);
    // Consumes `data`. Returns the reply to send, and leaves in `rest` what follows BEGIN.
    QByteArray feed(const QByteArray &data, QByteArray *rest);
    State state() const { return m_state; }
    bool unixFds() const { return m_unixFds; }

private:
    QByteArray handleLine(const QByteArray &line);
    bool checkExternal(const QByteArray &hexIdentity);

    int m_fd;
    QByteArray m_guid;
    bool m_allowAnonymous;
    bool m_sawNul = false;
    bool m_waitingForData = false;
    bool m_authenticated = false;
    bool m_unixFds = false;
    State m_state = Waiting;
    QByteArray m_line;
};

// Sends bytes[offset...] and the descriptors (with the first byte only). Returns the number of
// bytes written, 0 if the socket would block, or -1 on error.
qint64 send(int fd, const QByteArray &bytes, qsizetype offset, const QList<QDBusUnixFileDescriptor> &fds);
// Reads into `buffer` and appends received descriptors to `fds`. Returns the byte count, 0 on
// end of file, -2 if it would block or -1 on error.
qint64 receive(int fd, char *buffer, qsizetype size, QList<QDBusUnixFileDescriptor> &fds);

QByteArray machineId();
QByteArray randomGuid();

} // namespace QDBusTransport
