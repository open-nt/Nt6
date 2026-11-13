// MIT License - see LICENSE
#include <QtCore/qsharedmemory.h>

#include <QtCore/qlogging.h>
#include <QtCore/qsystemsemaphore.h>

#include "qipckey_p.h"

#include <cerrno>

#ifndef _WIN32
#  include <fcntl.h>
#  include <sys/file.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

/*!
    \class QSharedMemory
    \brief The QSharedMemory class provides access to a shared memory segment.

    A shared memory segment is memory that several processes see, found by its key. One process
    creates the segment with create(); the others attach() to it. Each then reads and writes the
    same bytes through data(). As the processes run at the same time, they take the lock of the
    segment around every access:

    \code
    QSharedMemory memory(u"org.example.frame"_s);
    if (!memory.create(1024) && !memory.attach())
        return;
    memory.lock();
    std::memcpy(memory.data(), frame, 1024);
    memory.unlock();
    \endcode

    The segment is removed from the system when the last object that is attached to it detaches,
    which the destructor does. A process that crashes does not detach, and its segment stays; a
    process that then attaches to the key and detaches again removes it.

    \differences
    The segment is POSIX shared memory, so the class works on Linux and macOS, and a process
    cannot find a segment that another library made with a System V key. The native key is the
    name of the POSIX object, such as \c{/name}. There is no \c QNativeIpcKey, and so no
    constructor that takes one and no \c nativeIpcKey(). The lock is a file lock on the segment
    where the system has locks that belong to an open file, as Linux does, and a system semaphore
    elsewhere: with the file lock, lock() needs the object to be attached, and the lock of a
    process that dies is released. create(), attach() and detach() do not take the lock
    themselves.

    The class is not built for Zephyr, where \c QT_NO_SHAREDMEMORY is defined instead. See \l{Nt on Zephyr}.
    \enddifferences

    \sa QSystemSemaphore
*/

/*!
    \enum QSharedMemory::AccessMode

    What the process may do with the memory it attaches.

    \value ReadOnly The memory can only be read. Writing to it ends the process.
    \value ReadWrite The memory can be read and written.
*/

/*!
    \enum QSharedMemory::SharedMemoryError

    The errors that error() reports.

    \value NoError The last operation succeeded.
    \value PermissionDenied The process may not use the segment.
    \value InvalidSize The size given to create() is not a size a segment can have.
    \value KeyError The key is empty or cannot be used.
    \value AlreadyExists create() found a segment with the key.
    \value NotFound attach() found no segment with the key.
    \value LockError lock() or unlock() failed, or the object is attached already or not attached.
    \value OutOfResources The system has not enough memory for the segment.
    \value UnknownError Something else went wrong; errorString() has the text of the system.
*/

struct QSharedMemory::Private {
    QString key;
    QString nativeKey;
    std::unique_ptr<QSystemSemaphore> semaphore;
    void *memory = nullptr;
    qsizetype size = 0;
    int fd = -1;
    bool writable = false;
    bool locked = false;
    SharedMemoryError error = NoError;
    QString errorString;

    void succeed() {
        error = NoError;
        errorString.clear();
    }
    bool fail(SharedMemoryError code, const char *function, const QString &text) {
        error = code;
        errorString = QString("QSharedMemory::") + QString::fromUtf8(function) + QString(": ") + text;
        return false;
    }
    bool failWithErrno(const char *function, int code) {
        SharedMemoryError mapped = UnknownError;
        switch (code) {
        case EACCES:
        case EPERM:
            mapped = PermissionDenied;
            break;
        case EEXIST:
            mapped = AlreadyExists;
            break;
        case ENOENT:
            mapped = NotFound;
            break;
        case EMFILE:
        case ENFILE:
        case ENOMEM:
        case ENOSPC:
            mapped = OutOfResources;
            break;
        case EINVAL:
        case ENAMETOOLONG:
            mapped = KeyError;
            break;
        default:
            break;
        }
        return fail(mapped, function, qt_error_string(code));
    }

    void setNames(const QString &newKey, const QString &newNativeKey) {
        key = newKey;
        nativeKey = newNativeKey;
        semaphore.reset();
    }
    // The semaphore that is the lock of the segment; it starts with one resource.
    QSystemSemaphore *lockSemaphore() {
        if (!semaphore)
            semaphore = std::make_unique<QSystemSemaphore>(QString("qipc_sharedmemory_lock_") + nativeKey, 1);
        return semaphore.get();
    }

#ifndef _WIN32
    // Maps the segment behind fd, and holds a shared lock on it for as long as it is attached: the last one to
    // detach finds that it can lock the segment exclusively.
    bool map(const char *function, AccessMode mode) {
        struct stat info = {};
        if (fstat(fd, &info) != 0 || flock(fd, LOCK_SH) != 0) {
            const int code = errno;
            ::close(fd);
            fd = -1;
            return failWithErrno(function, code);
        }
        const int protection = mode == ReadOnly ? PROT_READ : PROT_READ | PROT_WRITE;
        void *address = mmap(nullptr, static_cast<std::size_t>(info.st_size), protection, MAP_SHARED, fd, 0);
        if (address == MAP_FAILED) {
            const int code = errno;
            ::close(fd);
            fd = -1;
            return failWithErrno(function, code);
        }
        memory = address;
        size = static_cast<qsizetype>(info.st_size);
        succeed();
        return true;
    }
#endif
};

/*!
    Constructs a shared memory object with the given \a parent and no key.

    \sa setKey()
*/
QSharedMemory::QSharedMemory(QObject *parent) : QObject(parent), d(std::make_unique<Private>()) {}

/*!
    Constructs a shared memory object with the given \a parent for the segment \a key. Nothing is
    created or attached yet.

    \sa create(), attach()
*/
QSharedMemory::QSharedMemory(const QString &key, QObject *parent) : QSharedMemory(parent) { setKey(key); }

/*!
    \since 6.6
    Constructs a shared memory object for the native key \a key, with the given \a parent.

    \sa setNativeKey()
*/
QSharedMemory::QSharedMemory(const QNativeIpcKey &key, QObject *parent) : QSharedMemory(parent) { setNativeKey(key); }

/*!
    Destroys the object, after it has detached from the segment. The segment is removed from the
    system if no other object is attached to it.
*/
QSharedMemory::~QSharedMemory() {
    if (d->locked)
        unlock();
    detach();
}

/*!
    Sets the key of the object to \a key, after detaching from the segment it is attached to.
    The key can be any text; the name of the segment in the system is made from it.

    \sa key(), nativeKey()
*/
void QSharedMemory::setKey(const QString &key) {
    if (d->locked)
        unlock();
    detach();
    d->setNames(key, QString::fromLatin1(QtPrivate::ipcName("qipc_sharedmemory_", key)));
    d->succeed();
}

/*!
    Returns the key that was set with setKey(), or an empty string if a native key was set.

    \sa setKey()
*/
QString QSharedMemory::key() const { return d->key; }

/*!
    Sets the name of the segment in the system to \a key, after detaching from the segment the
    object is attached to. Use this to share memory with a program that does not use this class.

    \sa nativeKey()
*/
void QSharedMemory::setNativeKey(const QString &key) {
    if (d->locked)
        unlock();
    detach();
    d->setNames(QString(), key);
    d->succeed();
}

/*!
    \since 6.6
    \overload
    Sets the native key to \a key. A key of a type other than \c Type::PosixRealtime is a key error.

    \sa nativeIpcKey(), platformSafeKey()
*/
void QSharedMemory::setNativeKey(const QNativeIpcKey &key) {
    setNativeKey(isKeyTypeSupported(key.type()) ? key.nativeKey() : QString());
    if (!isKeyTypeSupported(key.type()))
        d->fail(KeyError, "setNativeKey", QStringLiteral("the type of the key is not supported"));
}

/*!
    \since 6.6
    Returns the native key as a QNativeIpcKey of the type \c Type::PosixRealtime.

    \sa nativeKey()
*/
QNativeIpcKey QSharedMemory::nativeIpcKey() const {
    return QNativeIpcKey(nativeKey(), QNativeIpcKey::Type::PosixRealtime);
}

/*!
    \since 6.6
    Returns the native key that setKey() would use for \a key: a name that the system accepts, made from the
    letters and digits of \a key and a hash of it, of the type \a type.

    \sa legacyNativeKey()
*/
QNativeIpcKey QSharedMemory::platformSafeKey(const QString &key, QNativeIpcKey::Type type) {
    return QNativeIpcKey(QString::fromLatin1(QtPrivate::ipcName("qipc_sharedmemory_", key)), type);
}

/*!
    \since 6.6
    Returns the native key that setKey() gives \a key, of the type \a type. Nt makes the same names as
    platformSafeKey() for every key, so this returns the same name.
*/
QNativeIpcKey QSharedMemory::legacyNativeKey(const QString &key, QNativeIpcKey::Type type) {
    return platformSafeKey(key, type);
}

/*!
    Returns the name that the segment has in the system.

    \sa setNativeKey()
*/
QString QSharedMemory::nativeKey() const { return d->nativeKey; }

/*!
    Creates a segment of \a size bytes for the key and attaches to it with the access \a mode.
    The memory starts as zeros. Returns \c true on success. It fails, with error() set, if the
    object is attached already, the key is empty, \a size is not positive, or a segment with the
    key exists: then attach() to that one.

    \sa attach(), error()
*/
bool QSharedMemory::create(qsizetype size, AccessMode mode) {
    if (d->memory)
        return d->fail(LockError, "create", QString("the object is attached already"));
    if (d->nativeKey.isEmpty())
        return d->fail(KeyError, "create", QString("the key is empty"));
    if (size <= 0)
        return d->fail(InvalidSize, "create", QString("the size is not positive"));
#ifdef _WIN32
    Q_UNUSED(mode)
    return d->fail(UnknownError, "create", QString("not supported on this platform"));
#else
    const QByteArray name = d->nativeKey.toUtf8();
    d->fd = shm_open(name.constData(), O_CREAT | O_EXCL | O_RDWR, 0600);
    if (d->fd < 0)
        return d->failWithErrno("create", errno);
    d->writable = true;
    if (ftruncate(d->fd, static_cast<off_t>(size)) != 0) {
        const int code = errno;
        ::close(d->fd);
        d->fd = -1;
        shm_unlink(name.constData());
        return d->fail(code == EINVAL || code == EFBIG ? InvalidSize : OutOfResources, "create", qt_error_string(code));
    }
    if (!d->map("create", mode)) {
        shm_unlink(name.constData());
        return false;
    }
    return true;
#endif
}

/*!
    Returns the size of the attached segment in bytes, or 0 if the object is not attached.

    \sa create()
*/
qsizetype QSharedMemory::size() const { return d->size; }

/*!
    Attaches to the segment of the key, which another object created, with the access \a mode.
    Returns \c true on success. It fails, with error() set, if the object is attached already or
    there is no such segment.

    \sa create(), detach(), isAttached()
*/
bool QSharedMemory::attach(AccessMode mode) {
    if (d->memory)
        return d->fail(LockError, "attach", QString("the object is attached already"));
    if (d->nativeKey.isEmpty())
        return d->fail(KeyError, "attach", QString("the key is empty"));
#ifdef _WIN32
    Q_UNUSED(mode)
    return d->fail(UnknownError, "attach", QString("not supported on this platform"));
#else
    // Also for reading the segment is opened for writing if it can be, as the lock needs that.
    const QByteArray name = d->nativeKey.toUtf8();
    d->fd = shm_open(name.constData(), O_RDWR, 0600);
    d->writable = d->fd >= 0;
    if (d->fd < 0 && mode == ReadOnly && (errno == EACCES || errno == EPERM))
        d->fd = shm_open(name.constData(), O_RDONLY, 0600);
    if (d->fd < 0)
        return d->failWithErrno("attach", errno);
    return d->map("attach", mode);
#endif
}

/*!
    Returns \c true if the object is attached to a segment.

    \sa attach(), detach()
*/
bool QSharedMemory::isAttached() const { return d->memory != nullptr; }

/*!
    Detaches from the segment, after which data() is \c nullptr. If no other object is attached,
    the segment is removed from the system. Returns \c false if the object was not attached.

    \sa attach()
*/
bool QSharedMemory::detach() {
    if (!d->memory)
        return d->fail(LockError, "detach", QString("the object is not attached"));
    if (d->locked)
        unlock();
#ifndef _WIN32
    munmap(d->memory, static_cast<std::size_t>(d->size));
    // Every attached object holds a shared lock; the exclusive one is free only for the last.
    if (flock(d->fd, LOCK_EX | LOCK_NB) == 0)
        shm_unlink(d->nativeKey.toUtf8().constData());
    ::close(d->fd);
#endif
    d->fd = -1;
    d->memory = nullptr;
    d->size = 0;
    d->succeed();
    return true;
}

/*!
    Returns a pointer to the first byte of the attached segment, or \c nullptr if the object is
    not attached. Take the lock() before using the memory.

    \sa constData(), size()
*/
void *QSharedMemory::data() { return d->memory; }

/*!
    Returns a pointer to the first byte of the attached segment that cannot be written through,
    or \c nullptr if the object is not attached.

    \sa data()
*/
const void *QSharedMemory::constData() const { return d->memory; }

/*!
    \overload
*/
const void *QSharedMemory::data() const { return d->memory; }

/*!
    Takes the lock of the segment, waiting until no other object has it. Returns \c true when
    this object has the lock. It fails, with error() set, if this object has the lock already,
    is not attached, has no key, or the lock of the system cannot be used.

    \sa unlock()
*/
bool QSharedMemory::lock() {
    if (d->locked)
        return d->fail(LockError, "lock", QString("the object has the lock already"));
    if (d->nativeKey.isEmpty())
        return d->fail(KeyError, "lock", QString("the key is empty"));
#ifdef F_OFD_SETLKW
    if (!d->memory)
        return d->fail(LockError, "lock", QString("the object is not attached"));
    // The lock belongs to the open segment and not to the process, so two objects of one process exclude each
    // other as well. A segment that could only be opened for reading takes a lock that keeps the writers out.
    struct flock request = {};
    request.l_type = d->writable ? F_WRLCK : F_RDLCK;
    request.l_whence = SEEK_SET;
    int result = 0;
    do {
        result = fcntl(d->fd, F_OFD_SETLKW, &request);
    } while (result != 0 && errno == EINTR);
    if (result != 0)
        return d->fail(LockError, "lock", qt_error_string(errno));
#else
    if (!d->lockSemaphore()->acquire())
        return d->fail(LockError, "lock", d->lockSemaphore()->errorString());
#endif
    d->locked = true;
    d->succeed();
    return true;
}

/*!
    Gives the lock of the segment back. Returns \c false if this object does not have it.

    \sa lock()
*/
bool QSharedMemory::unlock() {
    if (!d->locked)
        return false;
    d->locked = false;
#ifdef F_OFD_SETLKW
    struct flock request = {};
    request.l_type = F_UNLCK;
    request.l_whence = SEEK_SET;
    if (fcntl(d->fd, F_OFD_SETLK, &request) != 0)
        return d->fail(LockError, "unlock", qt_error_string(errno));
#else
    if (!d->lockSemaphore()->release())
        return d->fail(LockError, "unlock", d->lockSemaphore()->errorString());
#endif
    d->succeed();
    return true;
}

/*!
    Returns what went wrong in the last operation, or \c NoError.

    \sa errorString()
*/
QSharedMemory::SharedMemoryError QSharedMemory::error() const { return d->error; }

/*!
    Returns a text for the error of the last operation, or an empty string if it succeeded.

    \sa error()
*/
QString QSharedMemory::errorString() const { return d->errorString; }

#include "moc_qsharedmemory.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
