// MIT License - see LICENSE
#include "qdbustransport_p.h"

#include <QtCore/qbytearray.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qrandom.h>
#include <QtCore/qstringlist.h>

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>

namespace QDBusTransport {

namespace {

QByteArray localBytes(const QString &s) { return s.toLocal8Bit(); }

QByteArray environment(const char *name) {
    const char *value = std::getenv(name); // NOLINT(concurrency-mt-unsafe): read once while connecting
    return value ? QByteArray(value) : QByteArray();
}

bool isUnreserved(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
           c == '/' || c == '\\' || c == '.' || c == '*';
}

bool unescape(const QByteArray &in, QString *out) {
    QByteArray bytes;
    for (qsizetype i = 0; i < in.size(); ++i) {
        const char c = in.at(i);
        if (c == '%') {
            if (i + 2 >= in.size())
                return false;
            bool ok = false;
            const int v = in.mid(i + 1, 2).toInt(&ok, 16);
            if (!ok)
                return false;
            bytes.append(static_cast<char>(v));
            i += 2;
        } else {
            bytes.append(c);
        }
    }
    *out = QString::fromUtf8(bytes);
    return true;
}

bool fillSockaddr(const Address &a, sockaddr_un *sa, socklen_t *len, QDBusError *error) {
    std::memset(sa, 0, sizeof *sa);
    sa->sun_family = AF_UNIX;
    QByteArray name;
    bool abstract = false;
    if (a.params.contains(QStringLiteral("path"))) {
        name = localBytes(a.params.value(QStringLiteral("path")));
    } else if (a.params.contains(QStringLiteral("abstract"))) {
#ifdef __linux__
        name = a.params.value(QStringLiteral("abstract")).toUtf8();
        abstract = true;
#else
        *error = QDBusError(QDBusError::NotSupported, QStringLiteral("Abstract sockets are not supported here"));
        return false;
#endif
    } else {
        *error = QDBusError(QDBusError::BadAddress, QStringLiteral("A unix address needs a path or an abstract name"));
        return false;
    }
    const std::size_t offset = abstract ? 1 : 0;
    if (name.isEmpty() || static_cast<std::size_t>(name.size()) + offset >= sizeof sa->sun_path) {
        *error = QDBusError(QDBusError::BadAddress, QStringLiteral("The socket name is too long"));
        return false;
    }
    std::memcpy(sa->sun_path + offset, name.constData(), static_cast<std::size_t>(name.size()));
    *len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + offset + static_cast<std::size_t>(name.size()) +
                                  (abstract ? 0 : 1));
    return true;
}

bool setNonBlocking(int fd, bool on) {
    const int flags = ::fcntl(fd, F_GETFL);
    if (flags < 0)
        return false;
    return ::fcntl(fd, F_SETFL, on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)) == 0;
}

using Clock = std::chrono::steady_clock;

int remainingMs(Clock::time_point deadline) {
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
    return left < 0 ? 0 : static_cast<int>(left);
}

bool writeAll(int fd, const QByteArray &data, Clock::time_point deadline) {
    qsizetype done = 0;
    while (done < data.size()) {
        const ssize_t n =
            ::send(fd, data.constData() + done, static_cast<std::size_t>(data.size() - done), MSG_NOSIGNAL);
        if (n > 0) {
            done += n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            pollfd p{fd, POLLOUT, 0};
            if (::poll(&p, 1, remainingMs(deadline)) <= 0)
                return false;
            continue;
        }
        return false;
    }
    return true;
}

// Reads one line that ends in CRLF, a byte at a time so as not to read past it.
bool readLine(int fd, QByteArray *line, Clock::time_point deadline) {
    line->clear();
    for (;;) {
        char c;
        const ssize_t n = ::recv(fd, &c, 1, 0);
        if (n == 1) {
            line->append(c);
            if (line->endsWith("\r\n")) {
                line->chop(2);
                return true;
            }
            if (line->size() > 16384)
                return false;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            pollfd p{fd, POLLIN, 0};
            if (::poll(&p, 1, remainingMs(deadline)) <= 0)
                return false;
            continue;
        }
        return false;
    }
}

int connectOne(const Address &a, Clock::time_point deadline, QDBusError *error) {
    Address target = a;
    if (a.transport == QLatin1String("unix") && a.params.value(QStringLiteral("runtime")) == QLatin1String("yes")) {
        const QByteArray dir = environment("XDG_RUNTIME_DIR");
        if (dir.isEmpty()) {
            *error = QDBusError(QDBusError::BadAddress, QStringLiteral("XDG_RUNTIME_DIR is not set"));
            return -1;
        }
        target.params.insert(QStringLiteral("path"), QString::fromLocal8Bit(dir) + QStringLiteral("/bus"));
    }
    if (target.transport != QLatin1String("unix")) {
        *error = QDBusError(QDBusError::NotSupported,
                            QStringLiteral("The D-Bus transport \"%1\" is not supported").arg(a.transport));
        return -1;
    }
    sockaddr_un sa;
    socklen_t len = 0;
    if (!fillSockaddr(target, &sa, &len, error))
        return -1;
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        *error = QDBusError(QDBusError::NoServer, QString::fromLocal8Bit(std::strerror(errno)));
        return -1;
    }
    setNonBlocking(fd, true);
    int rc;
    do {
        rc = ::connect(fd, reinterpret_cast<const sockaddr *>(&sa), len);
    } while (rc < 0 && errno == EINTR);
    if (rc < 0 && errno == EINPROGRESS) {
        pollfd p{fd, POLLOUT, 0};
        int err = ETIMEDOUT;
        if (::poll(&p, 1, remainingMs(deadline)) > 0) {
            socklen_t errLen = sizeof err;
            ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errLen);
        }
        rc = err ? -1 : 0;
        errno = err;
    }
    if (rc < 0) {
        *error = QDBusError(
            QDBusError::NoServer,
            QStringLiteral("Failed to connect to %1: %2")
                .arg(target.params.value(QStringLiteral("path"), target.params.value(QStringLiteral("abstract"))),
                     QString::fromLocal8Bit(std::strerror(errno))));
        ::close(fd);
        return -1;
    }
    return fd;
}

bool clientAuth(int fd, Clock::time_point deadline, bool *unixFds, QDBusError *error) {
    auto fail = [error](const QString &why) {
        *error = QDBusError(QDBusError::AccessDenied, QStringLiteral("Authentication failed: %1").arg(why));
        return false;
    };
    const QByteArray uid = QByteArray::number(static_cast<qlonglong>(::getuid())).toHex();
    if (!writeAll(fd, QByteArray(1, '\0') + "AUTH EXTERNAL " + uid + "\r\n", deadline))
        return fail(QStringLiteral("the server closed the connection"));
    QByteArray line;
    if (!readLine(fd, &line, deadline))
        return fail(QStringLiteral("no answer from the server"));
    if (line.startsWith("DATA")) {
        // Some servers ask for the identity in a second step.
        if (!writeAll(fd, "DATA\r\n", deadline) || !readLine(fd, &line, deadline))
            return fail(QStringLiteral("no answer from the server"));
    }
    if (!line.startsWith("OK ")) {
        if (!writeAll(fd, "AUTH ANONYMOUS\r\n", deadline) || !readLine(fd, &line, deadline) || !line.startsWith("OK "))
            return fail(QString::fromLatin1(line));
    }
    if (!writeAll(fd, "NEGOTIATE_UNIX_FD\r\n", deadline) || !readLine(fd, &line, deadline))
        return fail(QStringLiteral("no answer from the server"));
    *unixFds = line == "AGREE_UNIX_FD";
    if (!writeAll(fd, "BEGIN\r\n", deadline))
        return fail(QStringLiteral("the server closed the connection"));
    return true;
}

} // namespace

QList<Address> parseAddresses(const QString &addresses) {
    QList<Address> list;
    const QStringList entries = addresses.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &entry : entries) {
        const qsizetype colon = entry.indexOf(QLatin1Char(':'));
        if (colon <= 0)
            return {};
        Address a;
        a.transport = entry.left(colon);
        const QStringList pairs = entry.mid(colon + 1).split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString &pair : pairs) {
            const qsizetype eq = pair.indexOf(QLatin1Char('='));
            QString value;
            if (eq <= 0 || !unescape(pair.mid(eq + 1).toUtf8(), &value))
                return {};
            a.params.insert(pair.left(eq), value);
        }
        list.append(a);
    }
    return list;
}

QString escapeValue(const QString &value) {
    const QByteArray utf8 = value.toUtf8();
    QString out;
    for (const char c : utf8) {
        if (isUnreserved(c))
            out.append(QLatin1Char(c));
        else
            out.append(QStringLiteral("%%1").arg(static_cast<uchar>(c), 2, 16, QLatin1Char('0')));
    }
    return out;
}

QString busAddress(int busType) {
    if (busType == 0) {
        const QByteArray env = environment("DBUS_SESSION_BUS_ADDRESS");
        if (!env.isEmpty())
            return QString::fromUtf8(env);
        const QByteArray runtime = environment("XDG_RUNTIME_DIR");
        if (!runtime.isEmpty()) {
            const QString path = QString::fromLocal8Bit(runtime) + QStringLiteral("/bus");
            struct stat st;
            if (::stat(localBytes(path).constData(), &st) == 0 && S_ISSOCK(st.st_mode))
                return QStringLiteral("unix:path=") + escapeValue(path);
        }
        return QString();
    }
    if (busType == 1) {
        const QByteArray env = environment("DBUS_SYSTEM_BUS_ADDRESS");
        return env.isEmpty() ? QStringLiteral("unix:path=/var/run/dbus/system_bus_socket") : QString::fromUtf8(env);
    }
    const QByteArray starter = environment("DBUS_STARTER_ADDRESS");
    return QString::fromUtf8(starter);
}

int connectAndAuthenticate(const QString &addresses, int timeoutMs, bool *unixFds, QDBusError *error) {
    const QList<Address> list = parseAddresses(addresses);
    if (list.isEmpty()) {
        *error = QDBusError(QDBusError::BadAddress, QStringLiteral("Invalid D-Bus address: \"%1\"").arg(addresses));
        return -1;
    }
    const Clock::time_point deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    for (const Address &a : list) {
        const int fd = connectOne(a, deadline, error);
        if (fd < 0)
            continue;
        if (clientAuth(fd, deadline, unixFds, error))
            return fd;
        ::close(fd);
    }
    return -1;
}

int listen(const QString &address, QString *clientAddress, QString *unlinkPath, QDBusError *error) {
    const QList<Address> list = parseAddresses(address);
    if (list.size() != 1 || list.first().transport != QLatin1String("unix")) {
        *error = QDBusError(QDBusError::BadAddress,
                            QStringLiteral("Only a single unix: address can be listened on: \"%1\"").arg(address));
        return -1;
    }
    Address a = list.first();
    const QString dir = a.params.value(QStringLiteral("dir"), a.params.value(QStringLiteral("tmpdir")));
    if (!dir.isEmpty()) {
        a.params.clear();
        a.params.insert(QStringLiteral("path"),
                        dir + QStringLiteral("/dbus-") +
                            QString::fromLatin1(QByteArray::number(QRandomGenerator::global()->generate64(), 16)));
    }
    sockaddr_un sa;
    socklen_t len = 0;
    if (!fillSockaddr(a, &sa, &len, error))
        return -1;
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        *error = QDBusError(QDBusError::Failed, QString::fromLocal8Bit(std::strerror(errno)));
        return -1;
    }
    if (::bind(fd, reinterpret_cast<const sockaddr *>(&sa), len) < 0 || ::listen(fd, 30) < 0) {
        *error = QDBusError(errno == EADDRINUSE ? QDBusError::AddressInUse : QDBusError::Failed,
                            QString::fromLocal8Bit(std::strerror(errno)));
        ::close(fd);
        return -1;
    }
    setNonBlocking(fd, true);
    if (a.params.contains(QStringLiteral("path"))) {
        *unlinkPath = a.params.value(QStringLiteral("path"));
        *clientAddress = QStringLiteral("unix:path=") + escapeValue(*unlinkPath);
    } else {
        unlinkPath->clear();
        *clientAddress = QStringLiteral("unix:abstract=") + escapeValue(a.params.value(QStringLiteral("abstract")));
    }
    return fd;
}

int accept(int listenFd) {
    for (;;) {
        const int fd = ::accept4(listenFd, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
        if (fd < 0 && errno == EINTR)
            continue;
        return fd;
    }
}

void closeFd(int fd) {
    if (fd >= 0)
        ::close(fd);
}

long peerUid(int fd) {
#ifdef SO_PEERCRED
    ucred cred{};
    socklen_t len = sizeof cred;
    if (::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) == 0)
        return static_cast<long>(cred.uid);
#else
    uid_t uid;
    gid_t gid;
    if (::getpeereid(fd, &uid, &gid) == 0)
        return static_cast<long>(uid);
#endif
    return -1;
}

long currentUid() { return static_cast<long>(::getuid()); }

ServerAuth::ServerAuth(int fd, const QByteArray &guid, bool allowAnonymous)
    : m_fd(fd), m_guid(guid), m_allowAnonymous(allowAnonymous) {}

bool ServerAuth::checkExternal(const QByteArray &hexIdentity) {
    const long uid = peerUid(m_fd);
    if (uid < 0 || uid != currentUid())
        return false;
    if (hexIdentity.isEmpty())
        return true;
    return QByteArray::fromHex(hexIdentity) == QByteArray::number(static_cast<qlonglong>(uid));
}

QByteArray ServerAuth::handleLine(const QByteArray &line) {
    QByteArray mechanisms = m_allowAnonymous ? "REJECTED EXTERNAL ANONYMOUS\r\n" : "REJECTED EXTERNAL\r\n";
    const QList<QByteArray> words = line.split(' ');
    const QByteArray command = words.value(0);
    if (m_authenticated) {
        if (command == "BEGIN") {
            m_state = Authenticated;
            return QByteArray();
        }
        if (command == "NEGOTIATE_UNIX_FD") {
            m_unixFds = true;
            return "AGREE_UNIX_FD\r\n";
        }
        if (command == "CANCEL" || command == "ERROR") {
            m_authenticated = false;
            return mechanisms;
        }
        return "ERROR\r\n";
    }
    if (m_waitingForData) {
        m_waitingForData = false;
        if (command == "DATA" && checkExternal(words.value(1))) {
            m_authenticated = true;
            return "OK " + m_guid + "\r\n";
        }
        return mechanisms;
    }
    if (command == "AUTH") {
        const QByteArray mechanism = words.value(1);
        if (mechanism == "EXTERNAL") {
            if (words.size() < 3) {
                m_waitingForData = true;
                return "DATA\r\n";
            }
            if (checkExternal(words.value(2))) {
                m_authenticated = true;
                return "OK " + m_guid + "\r\n";
            }
        } else if (mechanism == "ANONYMOUS" && m_allowAnonymous) {
            m_authenticated = true;
            return "OK " + m_guid + "\r\n";
        }
        return mechanisms;
    }
    if (command == "BEGIN") {
        m_state = Failed;
        return QByteArray();
    }
    if (command == "CANCEL" || command == "ERROR")
        return mechanisms;
    return "ERROR\r\n";
}

QByteArray ServerAuth::feed(const QByteArray &data, QByteArray *rest) {
    QByteArray reply;
    qsizetype i = 0;
    if (!m_sawNul && !data.isEmpty()) {
        if (data.at(0) != '\0') {
            m_state = Failed;
            return reply;
        }
        m_sawNul = true;
        i = 1;
    }
    for (; i < data.size() && m_state == Waiting; ++i) {
        m_line.append(data.at(i));
        if (m_line.endsWith("\r\n")) {
            m_line.chop(2);
            reply += handleLine(m_line);
            m_line.clear();
        } else if (m_line.size() > 16384) {
            m_state = Failed;
        }
    }
    if (m_state == Authenticated && rest)
        *rest = data.mid(i);
    return reply;
}

qint64 send(int fd, const QByteArray &bytes, qsizetype offset, const QList<QDBusUnixFileDescriptor> &fds) {
    iovec iov;
    iov.iov_base = const_cast<char *>(bytes.constData() + offset);
    iov.iov_len = static_cast<std::size_t>(bytes.size() - offset);
    msghdr msg{};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    QByteArray control;
    if (!fds.isEmpty()) {
        // The descriptors travel with the first byte only.
        iov.iov_len = 1;
        control.resize(static_cast<qsizetype>(CMSG_SPACE(sizeof(int) * static_cast<std::size_t>(fds.size()))));
        control.fill('\0');
        msg.msg_control = control.data();
        msg.msg_controllen = static_cast<decltype(msg.msg_controllen)>(control.size());
        cmsghdr *cm = CMSG_FIRSTHDR(&msg);
        cm->cmsg_level = SOL_SOCKET;
        cm->cmsg_type = SCM_RIGHTS;
        cm->cmsg_len = CMSG_LEN(sizeof(int) * static_cast<std::size_t>(fds.size()));
        auto *data = reinterpret_cast<unsigned char *>(CMSG_DATA(cm));
        for (qsizetype i = 0; i < fds.size(); ++i) {
            const int f = fds.at(i).fileDescriptor();
            std::memcpy(data + static_cast<std::size_t>(i) * sizeof(int), &f, sizeof(int));
        }
    }
    for (;;) {
        const ssize_t n = ::sendmsg(fd, &msg, MSG_NOSIGNAL);
        if (n >= 0)
            return n;
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        return -1;
    }
}

qint64 receive(int fd, char *buffer, qsizetype size, QList<QDBusUnixFileDescriptor> &fds) {
    iovec iov;
    iov.iov_base = buffer;
    iov.iov_len = static_cast<std::size_t>(size);
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int) * 253)];
    msghdr msg{};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof control;
    ssize_t n;
    do {
        n = ::recvmsg(fd, &msg, MSG_CMSG_CLOEXEC);
    } while (n < 0 && errno == EINTR);
    if (n < 0)
        return errno == EAGAIN || errno == EWOULDBLOCK ? -2 : -1;
    for (cmsghdr *cm = CMSG_FIRSTHDR(&msg); cm; cm = CMSG_NXTHDR(&msg, cm)) {
        if (cm->cmsg_level != SOL_SOCKET || cm->cmsg_type != SCM_RIGHTS)
            continue;
        const std::size_t count = (cm->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        const auto *data = reinterpret_cast<const unsigned char *>(CMSG_DATA(cm));
        for (std::size_t i = 0; i < count; ++i) {
            int f;
            std::memcpy(&f, data + i * sizeof(int), sizeof(int));
            QDBusUnixFileDescriptor holder;
            holder.giveFileDescriptor(f);
            fds.append(holder);
        }
    }
    if (msg.msg_flags & MSG_CTRUNC)
        return -1;
    return n;
}

QByteArray machineId() {
    for (const char *path : {"/etc/machine-id", "/var/lib/dbus/machine-id"}) {
        QFile f(QString::fromLatin1(path));
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray id = f.readAll().trimmed();
            if (id.size() == 32)
                return id;
        }
    }
    return QByteArray();
}

QByteArray randomGuid() {
    QByteArray bytes(16, '\0');
    for (qsizetype i = 0; i < bytes.size(); i += 8) {
        const quint64 r = QRandomGenerator::global()->generate64();
        std::memcpy(bytes.data() + i, &r, 8);
    }
    return bytes.toHex();
}

} // namespace QDBusTransport
