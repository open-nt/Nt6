// MIT License - see LICENSE
#include <QtDBus/qdbusunixfiledescriptor.h>

#include <fcntl.h>
#include <unistd.h>

/*!
    \class QDBusUnixFileDescriptor
    \brief The QDBusUnixFileDescriptor class holds one Unix file descriptor.

    A file descriptor can travel in a message to a process on the same machine, if both ends of
    the connection support it; see QDBusConnection::connectionCapabilities(). The object owns
    the descriptor it holds and closes it when the last copy is destroyed:

    \code
    QFile log("/var/log/app.log");
    log.open(QIODevice::ReadOnly);
    iface.call("Watch", QVariant::fromValue(QDBusUnixFileDescriptor(log.handle())));
    \endcode

    \differences
    None.
    \enddifferences

    \sa QDBusConnection::connectionCapabilities()
*/

struct QDBusUnixFileDescriptor::Handle {
    explicit Handle(int f) : fd(f) {}
    ~Handle() {
        if (fd >= 0)
            ::close(fd);
    }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    int fd;
};

/*!
    Constructs an object that holds no file descriptor.
*/
QDBusUnixFileDescriptor::QDBusUnixFileDescriptor() = default;

/*!
    Constructs an object that holds a duplicate of \a fileDescriptor. The caller keeps its own
    descriptor.
*/
QDBusUnixFileDescriptor::QDBusUnixFileDescriptor(int fileDescriptor) { setFileDescriptor(fileDescriptor); }

/*!
    Constructs a copy of \a other that shares its descriptor.
*/
QDBusUnixFileDescriptor::QDBusUnixFileDescriptor(const QDBusUnixFileDescriptor &other) = default;

/*!
    Constructs an object that takes the descriptor of \a other.
*/
QDBusUnixFileDescriptor::QDBusUnixFileDescriptor(QDBusUnixFileDescriptor &&other) noexcept = default;

/*!
    Makes this object share the descriptor of \a other and returns a reference to it.
*/
QDBusUnixFileDescriptor &QDBusUnixFileDescriptor::operator=(const QDBusUnixFileDescriptor &other) = default;

/*!
    Moves the descriptor of \a other into this object and returns a reference to it.
*/
QDBusUnixFileDescriptor &QDBusUnixFileDescriptor::operator=(QDBusUnixFileDescriptor &&other) noexcept = default;

/*!
    Destroys the object. The descriptor is closed when the last object that shares it is
    destroyed.
*/
QDBusUnixFileDescriptor::~QDBusUnixFileDescriptor() = default;

/*!
    Returns \c true if the object holds a file descriptor.
*/
bool QDBusUnixFileDescriptor::isValid() const { return d && d->fd >= 0; }

/*!
    Returns the descriptor, or -1 if there is none. The object still owns it.
*/
int QDBusUnixFileDescriptor::fileDescriptor() const { return d ? d->fd : -1; }

/*!
    Makes the object hold a duplicate of \a fileDescriptor, which the caller keeps.
*/
void QDBusUnixFileDescriptor::setFileDescriptor(int fileDescriptor) {
    d.reset();
    if (fileDescriptor < 0)
        return;
    const int copy = ::fcntl(fileDescriptor, F_DUPFD_CLOEXEC, 3);
    if (copy >= 0)
        d = std::make_shared<Handle>(copy);
}

/*!
    Makes the object hold \a fileDescriptor itself, which it now owns and closes.
*/
void QDBusUnixFileDescriptor::giveFileDescriptor(int fileDescriptor) {
    d.reset();
    if (fileDescriptor >= 0)
        d = std::make_shared<Handle>(fileDescriptor);
}

/*!
    Returns the descriptor and gives up ownership of it; the object no longer holds one. If
    other objects share it, the caller gets a duplicate.
*/
int QDBusUnixFileDescriptor::takeFileDescriptor() {
    if (!d)
        return -1;
    int fd;
    if (d.use_count() > 1) {
        fd = ::fcntl(d->fd, F_DUPFD_CLOEXEC, 3);
    } else {
        fd = d->fd;
        d->fd = -1;
    }
    d.reset();
    return fd;
}

/*!
    Swaps this object with \a other.
*/
void QDBusUnixFileDescriptor::swap(QDBusUnixFileDescriptor &other) noexcept { d.swap(other.d); }

/*!
    Returns \c true if this system can pass file descriptors over D-Bus.
*/
bool QDBusUnixFileDescriptor::isSupported() { return true; }
