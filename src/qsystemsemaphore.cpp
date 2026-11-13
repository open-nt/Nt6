// MIT License - see LICENSE
#include <QtCore/qsystemsemaphore.h>

#include <QtCore/qcryptographichash.h>
#include <QtCore/qlogging.h>

#include "qipckey_p.h"

#include <cerrno>

#ifndef _WIN32
#  include <fcntl.h>
#  include <semaphore.h>
#endif

/*!
    \class QSystemSemaphore
    \brief The QSystemSemaphore class provides a general counting system semaphore.

    A system semaphore is a semaphore that several processes share, found by its key. Like a
    QSemaphore it guards a number of resources: acquire() takes one, waiting until there is one,
    and release() gives resources back.

    \code
    QSystemSemaphore printers(u"org.example.printers"_s, 3, QSystemSemaphore::Create);
    printers.acquire();
    usePrinter();
    printers.release();
    \endcode

    The process that sets the system up passes \c Create, which makes the semaphore anew with the
    given count; the others pass \c Open and share it. A semaphore outlives a process that
    crashes, with whatever count it had; creating it with \c Create resets it.

    \differences
    The semaphore is a POSIX named semaphore, so the class works on Linux and macOS. A process
    cannot find a semaphore that another library made with a System V key. The semaphore is
    removed from the system when the object that created it is destroyed: processes that have it
    open keep using it, but a process that opens the key afterwards gets a new one. There is no
    \c QNativeIpcKey, and so no constructor that takes one and no \c nativeIpcKey().

    The class is not built for Zephyr, where \c QT_NO_SYSTEMSEMAPHORE is defined instead. See \l{Nt on
    Zephyr}.
    \enddifferences

    \sa QSharedMemory, QSemaphore
*/

/*!
    \enum QSystemSemaphore::AccessMode

    How the semaphore of a key is taken up.

    \value Open Use the semaphore if it exists, and create it with the initial count if it does not.
    \value Create Create the semaphore with the initial count, replacing one that a crashed process may have left behind.
*/

/*!
    \enum QSystemSemaphore::SystemSemaphoreError

    The errors that error() reports.

    \value NoError The last operation succeeded.
    \value PermissionDenied The process may not use the semaphore.
    \value KeyError The key is empty or cannot be used.
    \value AlreadyExists The semaphore exists already.
    \value NotFound There is no semaphore with the key.
    \value OutOfResources The system has no room for another semaphore.
    \value UnknownError Something else went wrong; errorString() has the text of the system.
*/

struct QSystemSemaphore::Private {
    QString key;
    QByteArray name;
    SystemSemaphoreError error = NoError;
    QString errorString;
#ifndef _WIN32
    sem_t *handle = SEM_FAILED;
#endif
    bool created = false;

    void fail(const char *function, int code) {
        switch (code) {
        case EACCES:
        case EPERM:
            error = PermissionDenied;
            break;
        case EEXIST:
            error = AlreadyExists;
            break;
        case ENOENT:
            error = NotFound;
            break;
        case EMFILE:
        case ENFILE:
        case ENOMEM:
        case ENOSPC:
            error = OutOfResources;
            break;
        case EINVAL:
        case ENAMETOOLONG:
            error = KeyError;
            break;
        default:
            error = UnknownError;
        }
        errorString = QString("QSystemSemaphore::") + QString::fromUtf8(function) + QString(": ") + qt_error_string(code);
    }

    void close() {
#ifndef _WIN32
        if (handle != SEM_FAILED) {
            sem_close(handle);
            handle = SEM_FAILED;
            if (created)
                sem_unlink(name.constData());
        }
#endif
        created = false;
    }

    void open(int initialValue, AccessMode mode) {
        error = NoError;
        errorString.clear();
        if (name.isEmpty()) {
            error = KeyError;
            errorString = QString("QSystemSemaphore::handle: the key is empty");
            return;
        }
#ifdef _WIN32
        Q_UNUSED(initialValue)
        Q_UNUSED(mode)
        error = UnknownError;
        errorString = QString("QSystemSemaphore::handle: not supported on this platform");
#else
        const auto value = static_cast<unsigned>(initialValue < 0 ? 0 : initialValue);
        if (mode == Create)
            sem_unlink(name.constData());
        handle = sem_open(name.constData(), O_CREAT | O_EXCL, 0600, value);
        created = handle != SEM_FAILED;
        if (!created && errno == EEXIST && mode == Open)
            handle = sem_open(name.constData(), 0);
        if (handle == SEM_FAILED)
            fail("handle", errno);
#endif
    }
};

/*!
    Constructs a semaphore for \a key. With the \a mode \c Open the semaphore of the system is
    used if it exists and created with the count \a initialValue if not; with \c Create it is
    always created with that count. A negative \a initialValue counts as 0.

    If the semaphore cannot be opened, error() says why and acquire() and release() fail.

    \sa setKey()
*/
QSystemSemaphore::QSystemSemaphore(const QString &key, int initialValue, AccessMode mode)
    : d(std::make_unique<Private>()) {
    setKey(key, initialValue, mode);
}

/*!
    Destroys the object and closes the semaphore. Resources that were acquired are not released.
*/
QSystemSemaphore::~QSystemSemaphore() { d->close(); }

/*!
    Closes the semaphore and takes up the one for \a key instead, with \a initialValue and \a mode
    as in the constructor.

    \sa key()
*/
void QSystemSemaphore::setKey(const QString &key, int initialValue, AccessMode mode) {
    d->close();
    d->key = key;
    d->name = QtPrivate::ipcName("qipc_systemsem_", key);
    d->open(initialValue, mode);
}

/*!
    \since 6.6
    Constructs a semaphore for the native key \a key, with \a initialValue and \a mode as for the constructor
    that takes a key.

    \sa setNativeKey()
*/
QSystemSemaphore::QSystemSemaphore(const QNativeIpcKey &key, int initialValue, AccessMode mode)
    : d(std::make_unique<Private>()) {
    setNativeKey(key, initialValue, mode);
}

/*!
    \since 6.6
    Makes the object the semaphore of the system named by the native key \a key, with \a initialValue and
    \a mode as for setKey(). A name without a leading slash gets one. A key of a type other than
    \c Type::PosixRealtime is a key error.

    \sa nativeIpcKey(), platformSafeKey()
*/
void QSystemSemaphore::setNativeKey(const QNativeIpcKey &key, int initialValue, AccessMode mode) {
    d->close();
    d->key.clear();
    d->name.clear();
    if (isKeyTypeSupported(key.type()) && !key.isEmpty()) {
        d->name = key.nativeKey().toUtf8();
        if (!d->name.startsWith('/'))
            d->name.prepend('/');
    }
    d->open(initialValue, mode);
    if (!isKeyTypeSupported(key.type()))
        d->errorString = QStringLiteral("QSystemSemaphore::setNativeKey: the type of the key is not supported");
}

/*!
    \since 6.6
    Returns the name of the semaphore of the system as a QNativeIpcKey of the type \c Type::PosixRealtime.
*/
QNativeIpcKey QSystemSemaphore::nativeIpcKey() const {
    return QNativeIpcKey(QString::fromUtf8(d->name), QNativeIpcKey::Type::PosixRealtime);
}

/*!
    \since 6.6
    Returns the native key that setKey() would use for \a key, of the type \a type.

    \sa legacyNativeKey()
*/
QNativeIpcKey QSystemSemaphore::platformSafeKey(const QString &key, QNativeIpcKey::Type type) {
    return QNativeIpcKey(QString::fromLatin1(QtPrivate::ipcName("qipc_systemsem_", key)), type);
}

/*!
    \since 6.6
    Returns the native key that setKey() gives \a key, of the type \a type: the same as platformSafeKey().
*/
QNativeIpcKey QSystemSemaphore::legacyNativeKey(const QString &key, QNativeIpcKey::Type type) {
    return platformSafeKey(key, type);
}

/*!
    Returns the key of the semaphore.

    \sa setKey()
*/
QString QSystemSemaphore::key() const { return d->key; }

/*!
    Takes one resource, waiting until one is free. Returns \c true when it has it, and \c false if
    the semaphore is not open or the system reports an error.

    \sa release()
*/
bool QSystemSemaphore::acquire() {
#ifdef _WIN32
    return false;
#else
    if (d->handle == SEM_FAILED)
        return false;
    int result = 0;
    do {
        result = sem_wait(d->handle);
    } while (result != 0 && errno == EINTR);
    if (result != 0) {
        d->fail("acquire", errno);
        return false;
    }
    d->error = NoError;
    d->errorString.clear();
    return true;
#endif
}

/*!
    Gives \a n resources back, which need not have been acquired by this object. Returns \c true
    on success; releasing no resources succeeds, and a negative \a n fails.

    \sa acquire()
*/
bool QSystemSemaphore::release(int n) {
#ifdef _WIN32
    Q_UNUSED(n)
    return false;
#else
    if (d->handle == SEM_FAILED || n < 0)
        return false;
    for (int i = 0; i < n; ++i) {
        if (sem_post(d->handle) != 0) {
            d->fail("release", errno);
            return false;
        }
    }
    d->error = NoError;
    d->errorString.clear();
    return true;
#endif
}

/*!
    Returns what went wrong in the last operation, or \c NoError.

    \sa errorString()
*/
QSystemSemaphore::SystemSemaphoreError QSystemSemaphore::error() const { return d->error; }

/*!
    Returns a text for the error of the last operation, or an empty string if it succeeded.

    \sa error()
*/
QString QSystemSemaphore::errorString() const { return d->errorString; }
