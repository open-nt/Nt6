// MIT License - see LICENSE
#include <QtCore/qlockfile.h>

#include <QtCore/qdebug.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <climits>
#include <csignal>
#include <limits>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace {

constexpr int kInitialSleepMs = 10;
constexpr int kMaxSleepMs = 500;

std::atomic<unsigned> g_tempCounter{0};

QByteArray localHostName() {
    char name[HOST_NAME_MAX + 1] = {};
    if (::gethostname(name, sizeof name - 1) != 0)
        return QByteArray();
    return QByteArray(name);
}

QByteArray processPath(pid_t pid) {
    const QByteArray link = "/proc/" + QByteArray::number(static_cast<qint64>(pid)) + "/exe";
    char buf[PATH_MAX];
    const ssize_t n = ::readlink(link.constData(), buf, sizeof buf);
    return n > 0 ? QByteArray(buf, static_cast<qsizetype>(n)) : QByteArray();
}

QByteArray baseName(const QByteArray &path) {
    const qsizetype slash = path.lastIndexOf('/');
    return slash < 0 ? path : path.mid(slash + 1);
}

bool isProcessRunning(qint64 pid, const QByteArray &appname) {
    if (pid <= 0)
        return false;
    if (::kill(static_cast<pid_t>(pid), 0) == -1 && errno == ESRCH)
        return false;
    // The pid may have been reused by another program; this can only be told if the application path was recorded.
    if (appname.startsWith('/')) {
        const QByteArray running = baseName(processPath(static_cast<pid_t>(pid)));
        if (!running.isEmpty() && running != baseName(appname)) {
            char resolved[PATH_MAX];
            if (!::realpath(appname.constData(), resolved) || baseName(QByteArray(resolved)) != running)
                return false;
        }
    }
    return true;
}

} // namespace

struct QLockFile::Private {
    explicit Private(const QString &name) : fileName(name), encoded(name.toUtf8()) {}

    QString fileName;
    QByteArray encoded;
    int staleTime = 30000;
    bool locked = false;
    LockError error = NoError;
    dev_t dev = 0;
    ino_t ino = 0;

    QByteArray contents() const {
        QByteArray appname = processPath(::getpid());
        return QByteArray::number(static_cast<qint64>(::getpid())) + '\n' + appname + '\n' + localHostName() + '\n';
    }

    static LockError errorFor(int e) {
        switch (e) {
        case EEXIST:
            return LockFailedError;
        case EACCES:
        case EROFS:
        case EPERM:
            return PermissionError;
        default:
            return UnknownError;
        }
    }

    // Creates the lock file with its contents in place, so that no reader ever sees it empty.
    LockError tryCreate() {
        const QByteArray data = contents();
        const qsizetype slash = encoded.lastIndexOf('/');
        const QByteArray dir = slash < 0 ? QByteArray(".") : encoded.left(slash == 0 ? 1 : slash);
        const QByteArray tmp = dir + "/.nt-lock-" + QByteArray::number(static_cast<qint64>(::getpid())) + '-' +
                               QByteArray::number(g_tempCounter.fetch_add(1)) + ".tmp";
        int fd = ::open(tmp.constData(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
        if (fd < 0)
            return errorFor(errno);
        bool ok = true;
        std::size_t done = 0;
        while (done < static_cast<std::size_t>(data.size())) {
            const ssize_t n = ::write(fd, data.constData() + done, static_cast<std::size_t>(data.size()) - done);
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0) {
                ok = false;
                break;
            }
            done += static_cast<std::size_t>(n);
        }
        const int writeError = ok ? 0 : errno;
        struct stat st;
        const bool statOk = ok && ::fstat(fd, &st) == 0;
        ::close(fd);
        if (!ok || !statOk) {
            ::unlink(tmp.constData());
            return errorFor(writeError ? writeError : errno);
        }
        LockError result = NoError;
        if (::link(tmp.constData(), encoded.constData()) != 0) {
            const int linkError = errno;
            result = errorFor(linkError);
            if (linkError != EEXIST && linkError != EACCES && linkError != EROFS && linkError != ENOENT) {
                // No hard links on this file system: create the lock file in place.
                int lockFd = ::open(encoded.constData(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
                if (lockFd < 0) {
                    result = errorFor(errno);
                } else {
                    const ssize_t n = ::write(lockFd, data.constData(), static_cast<std::size_t>(data.size()));
                    struct stat lst;
                    if (n == data.size() && ::fstat(lockFd, &lst) == 0) {
                        st = lst;
                        result = NoError;
                    } else {
                        ::unlink(encoded.constData());
                        result = UnknownError;
                    }
                    ::close(lockFd);
                }
            }
        }
        ::unlink(tmp.constData());
        if (result == NoError) {
            dev = st.st_dev;
            ino = st.st_ino;
        }
        return result;
    }

    bool readInfo(qint64 *pid, QString *hostname, QString *appname) const {
        const int fd = ::open(encoded.constData(), O_RDONLY | O_CLOEXEC);
        if (fd < 0)
            return false;
        char buf[4096];
        ssize_t n;
        do {
            n = ::read(fd, buf, sizeof buf);
        } while (n < 0 && errno == EINTR);
        ::close(fd);
        if (n <= 0)
            return false;
        const QByteArray data(buf, static_cast<qsizetype>(n));
        const QList<QByteArray> lines = data.split('\n');
        if (lines.size() < 3)
            return false;
        bool ok = false;
        const qint64 p = lines.at(0).toLongLong(&ok);
        if (!ok || p <= 0)
            return false;
        if (pid)
            *pid = p;
        if (appname)
            *appname = QString::fromUtf8(lines.at(1));
        if (hostname)
            *hostname = QString::fromUtf8(lines.at(2));
        return true;
    }

    bool isApparentlyStale() const {
        qint64 pid = 0;
        QString host;
        QString app;
        if (readInfo(&pid, &host, &app)) {
            if (host.isEmpty() || host.toUtf8() == localHostName()) {
                if (!isProcessRunning(pid, app.toUtf8()))
                    return true;
            }
        }
        struct stat st;
        if (::stat(encoded.constData(), &st) != 0)
            return false;
        const auto now =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        const long long modified = static_cast<long long>(st.st_mtim.tv_sec) * 1000 + st.st_mtim.tv_nsec / 1000000;
        const long long age = now - modified;
        return staleTime > 0 && (age > staleTime || -age > staleTime);
    }

    bool removeStale() {
        const int fd = ::open(encoded.constData(), O_WRONLY | O_CLOEXEC);
        if (fd < 0)
            return false;
        ::close(fd);
        if (::unlink(encoded.constData()) != 0) {
            qWarning("QLockFile: Failed to remove stale lock file %s: %s", encoded.constData(), std::strerror(errno));
            return false;
        }
        return true;
    }
};

/*!
    \class QLockFile
    \brief The QLockFile class provides locking between processes using a file.

    A lock file can be used to prevent multiple processes from accessing a
    same resource. For instance, a configuration file on disk, or a socket,
    a port, a region of shared memory...

    Serialization is only guaranteed if all processes that access the shared
    resource use QLockFile, with the same file name.

    QLockFile supports two use cases: protecting a resource for a short
    period of time (a few seconds at most) and for a long time.

    \code
    QLockFile lockFile("/tmp/app.lock");
    if (lockFile.tryLock(2000)) {
        useResource();
        lockFile.unlock();
    } else if (lockFile.error() == QLockFile::LockFailedError) {
        qint64 pid;
        QString host, app;
        lockFile.getLockInfo(&pid, &host, &app);
    }
    \endcode

    The lock file is created with the process id, application path and host
    name of the owner in it, so that other processes can find out who holds
    the lock. If the owning process dies without unlocking, the lock file is
    detected as stale: on the same host, because its process no longer exists
    (or the pid belongs to another program), and on any host once the file is
    older than staleLockTime(). A stale lock is then removed, with a second
    lock file named \c{.rmlock} making sure that two processes do not remove
    it at the same time.

    The file is created atomically: it is written under a temporary name in
    the same directory and then hard linked to the lock file name, so that
    other processes never see a partial file. If the file system does not
    support hard links, the lock file is created directly.

    \differences
    Calling lock() or tryLock() on a lock that this object already holds
    prints a warning and fails with \c LockFailedError instead of waiting.
    While waiting, the polling interval starts at 10 ms and doubles up to
    half a second. An unlock() that finds the lock file replaced by another
    process leaves the other file alone. The application path in the lock
    file is the executable path from \c /proc, and a lock file whose
    application name is not an absolute path is not checked for a reused
    pid. The lock is not re-created if the file is removed by another process.
    \enddifferences

    \sa QFile, QSaveFile
*/

/*!
    Constructs a new lock file object. The object is created in an unlocked
    state. When calling lock() or tryLock(), a lock file named \a fileName
    will be created, if it doesn't already exist.

    \sa lock(), unlock()
*/
QLockFile::QLockFile(const QString &fileName) : d(std::make_unique<Private>(fileName)) {}

/*!
    Destroys the lock file object. If the lock was acquired, this will
    release the lock, by deleting the lock file.
*/
QLockFile::~QLockFile() { unlock(); }

/*!
    Creates the lock file. If another process (or thread) has created the
    lock file already, this function will block until that process (or
    thread) releases it. Calling this function multiple times on the same lock
    from the same thread without unlocking first is not allowed.

    Returns \c true if the lock was acquired, \c false if it could not be
    acquired due to an unrecoverable error, such as no permissions in the
    parent directory.

    \sa unlock(), tryLock(), error()
*/
bool QLockFile::lock() { return tryLock(-1); }

/*!
    Attempts to create the lock file. This function returns \c true if the
    lock was obtained; otherwise it returns \c false. If another process (or
    thread) has created the lock file already, this function will wait for at
    most \a timeout milliseconds for the lock file to become available.

    Note: Passing a negative number as the \a timeout is equivalent to
    calling lock(), i.e. this function will wait forever until the lock file
    can be locked if \a timeout is negative.

    If the lock was obtained, it must be released with unlock() before another
    process (or thread) can successfully lock it.

    Calling this function multiple times on the same lock from the same
    thread without unlocking first is not allowed; it fails.

    \sa lock(), unlock()
*/
bool QLockFile::tryLock(int timeout) {
    if (d->locked) {
        qWarning("QLockFile::lock(\"%s\"): already locked by this object", d->encoded.constData());
        d->error = LockFailedError;
        return false;
    }
    const auto start = std::chrono::steady_clock::now();
    int sleepMs = kInitialSleepMs;
    for (;;) {
        d->error = d->tryCreate();
        switch (d->error) {
        case NoError:
            d->locked = true;
            return true;
        case PermissionError:
        case UnknownError:
            return false;
        case LockFailedError:
            if (d->isApparentlyStale()) {
                QLockFile removeLock(d->fileName + QString::fromUtf8(".rmlock"));
                if (removeLock.tryLock() && d->isApparentlyStale() && d->removeStale())
                    continue;
            }
            break;
        }
        if (timeout >= 0) {
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeout)
                return false;
            sleepMs = static_cast<int>(std::min<long long>(sleepMs, timeout - elapsed));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        sleepMs = std::min(sleepMs * 2, kMaxSleepMs);
    }
}

/*!
    Releases the lock, by deleting the lock file. Calling this function
    without having successfully obtained the lock is a no-op.

    \sa lock(), tryLock()
*/
void QLockFile::unlock() {
    if (!d->locked)
        return;
    d->locked = false;
    struct stat st;
    // Do not delete a file that some other process created after ours was removed as stale.
    if (::stat(d->encoded.constData(), &st) != 0 || st.st_dev != d->dev || st.st_ino != d->ino)
        return;
    if (::unlink(d->encoded.constData()) != 0 && errno != ENOENT)
        d->error = Private::errorFor(errno);
}

/*!
    Sets the time in milliseconds after which a lock file is considered
    stale to \a staleLockTime. The default value is 30000, i.e. 30 seconds. If
    your application typically keeps the file locked for more than 30 seconds
    (for instance while saving megabytes of data for 2 minutes), you should
    set a bigger value using setStaleLockTime().

    The value is used by lock() and tryLock() in order to
    determine when an existing lock file is considered stale, i.e. left over
    by a crashed process. This is done by checking the modification time of
    the lock file, in addition to checking whether the process that created
    it still exists on the same host. A value of 0 or less disables the check
    of the modification time.

    \sa staleLockTime()
*/
void QLockFile::setStaleLockTime(int staleLockTime) { d->staleTime = staleLockTime; }

/*!
    Returns the time in milliseconds after which a lock file is considered
    stale.

    \sa setStaleLockTime()
*/
int QLockFile::staleLockTime() const { return d->staleTime; }

/*!
    \overload
    \since 6.2
    Sets the time after which a lock file is considered stale to \a value.

    \sa staleLockTimeAsDuration()
*/
void QLockFile::setStaleLockTime(std::chrono::milliseconds value) {
    const auto limit = static_cast<std::chrono::milliseconds::rep>(std::numeric_limits<int>::max());
    setStaleLockTime(static_cast<int>(std::min(value.count(), limit)));
}

/*!
    \since 6.2
    Returns the time after which a lock file is considered stale, as a
    duration.

    \sa staleLockTime(), setStaleLockTime()
*/
std::chrono::milliseconds QLockFile::staleLockTimeAsDuration() const { return std::chrono::milliseconds(d->staleTime); }

/*!
    \since 6.6
    Returns the name of the lock file, as given to the constructor.
*/
QString QLockFile::fileName() const { return d->fileName; }

/*!
    \overload
    \since 6.2
    Tries to create the lock file like tryLock(int), waiting for at most
    \a timeout. A negative \a timeout waits for as long as it takes.
*/
bool QLockFile::tryLock(std::chrono::milliseconds timeout) {
    const auto limit = static_cast<std::chrono::milliseconds::rep>(std::numeric_limits<int>::max());
    return tryLock(timeout.count() < 0 ? -1 : static_cast<int>(std::min(timeout.count(), limit)));
}

/*!
    Returns \c true if the lock was acquired by this QLockFile instance,
    otherwise returns \c false.

    \sa lock(), unlock(), tryLock()
*/
bool QLockFile::isLocked() const { return d->locked; }

/*!
    Retrieves information about the current owner of the lock file.

    If tryLock() returns \c false, and error() returns \c LockFailedError,
    this function can be called to find out more information about the
    existing lock file: \a pid is the process id of the process that created
    it, \a hostname the host it ran on and \a appname the application it
    ran. Returns \c true if the information could be retrieved, in which case
    the outputs that are not \c nullptr are set; otherwise returns \c false.

    \sa tryLock(), error()
*/
bool QLockFile::getLockInfo(qint64 *pid, QString *hostname, QString *appname) const {
    return d->readInfo(pid, hostname, appname);
}

/*!
    Attempts to forcefully remove an existing lock file.

    Calling this is not recommended when protecting a short-lived operation:
    QLockFile already takes care of removing lock files after a crash, based
    on a time-out and a check that the process that created the lock file is
    still running. Also, this function is not guaranteed to remove the lock
    file if it is held by another process; it only deletes the file. The
    lock file must not be locked by this object.

    Returns \c true if the lock file was removed.

    \sa setStaleLockTime(), getLockInfo()
*/
bool QLockFile::removeStaleLockFile() {
    if (d->locked) {
        qWarning("QLockFile::removeStaleLockFile can only be called when not holding the lock");
        return false;
    }
    return d->removeStale();
}

/*!
    Returns the lock file error status.

    If tryLock() returns \c false, this function can be called to find out the
    reason why the locking failed.

    \sa tryLock()
*/
QLockFile::LockError QLockFile::error() const { return d->error; }
