// MIT License - see LICENSE

#include <QtCore/qsavefile.h>

#include <QtCore/qfileinfo.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

/*!
    \class QSaveFile
    \brief The QSaveFile class provides an interface for safely writing to files.

    QSaveFile writes to a temporary file next to the target and renames it
    over the target when commit() is called, so readers never see a partially
    written file. Call commit() when all data has been written. If a write
    fails, or cancelWriting() is called, commit() fails and the target is left
    untouched.

    \code
    QSaveFile file("config.txt");
    if (file.open(QIODevice::WriteOnly)) {
        file.write("key=value\n");
        if (!file.commit()) {
            // handle file.errorString()
        }
    }
    \endcode

    The temporary file is named \c{<target>.XXXXXX} and is removed when the
    object is destroyed without a commit. The permissions of an existing
    target are preserved; a new target gets \c 0666 masked by the umask.

    \differences
    close() commits instead of aborting, so a call to the base-class close()
    still finishes the save; prefer commit(). Only write modes are
    supported: \c Append is rejected and existing content is never copied
    into the temporary file.
    \enddifferences

    \sa QFile, QTemporaryFile, QFileDevice
*/

/*!
    Constructs a QSaveFile that will write to the file named \a name, with no
    parent.

    \sa setFileName()
*/
QSaveFile::QSaveFile(const QString &name) : m_target(name) { m_name = name; }

/*!
    Constructs a QSaveFile with no file name and the given \a parent.
*/
QSaveFile::QSaveFile(QObject *parent) : QFileDevice(parent) {}

/*!
    Constructs a QSaveFile that will write to the file named \a name, with the
    given \a parent.
*/
QSaveFile::QSaveFile(const QString &name, QObject *parent) : QFileDevice(parent), m_target(name) { m_name = name; }

/*!
    Destroys the object. If the file is still open, it is closed and the
    temporary file is removed without replacing the target.

    \sa commit(), cancelWriting()
*/
QSaveFile::~QSaveFile() {
    if (isOpen()) {
        QFileDevice::close();
        if (!m_direct && !m_temp.isEmpty())
            ::unlink(m_temp.toUtf8().constData());
    }
}

/*!
    \reimp

    Opens a temporary file next to the target for writing with the given
    \a mode. Returns \c true on success; otherwise returns \c false and sets
    error(). The open fails if \a mode does not include \c WriteOnly or
    includes \c Append, if no file name is set, if \c NewOnly is given and the
    target exists, if \c ExistingOnly is given and it does not, or if an
    existing target is not writable. If the temporary file cannot be created
    and the direct write fallback is enabled, the target is opened and
    truncated instead.

    \sa commit(), setDirectWriteFallback()
*/
bool QSaveFile::open(OpenMode mode) {
    if (isOpen()) {
        setFileError(OpenError, QString::fromUtf8("file already open"));
        return false;
    }
    if (m_target.isEmpty()) {
        setFileError(OpenError, QString::fromUtf8("no file name specified"));
        return false;
    }
    if (!(mode & WriteOnly) || (mode & Append)) {
        setFileError(OpenError, QString::fromUtf8("QSaveFile only supports writing without Append"));
        return false;
    }
    m_writeError = false;
    m_direct = false;
    m_temp.clear();

    const QByteArray target = m_target.toUtf8();
    struct stat st;
    const bool exists = ::stat(target.constData(), &st) == 0;
    if (mode & NewOnly) {
        if (exists) {
            setFileError(OpenError, QString::fromUtf8("file exists"));
            return false;
        }
    }
    if ((mode & ExistingOnly) && !exists) {
        setFileError(OpenError, QString::fromUtf8("file does not exist"));
        return false;
    }
    if (exists && ::access(target.constData(), W_OK) != 0) {
        m_writeError = true;
        setFileError(WriteError, QString::fromUtf8("existing file is not writable"));
        return false;
    }

    std::string tmpl = std::string(target.constData(), static_cast<std::size_t>(target.size())) + ".XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    const int fd = ::mkostemp(buf.data(), O_CLOEXEC);
    if (fd < 0) {
        const int err = errno;
        if (m_directFallback) {
            if (openPath(m_target, (mode & ~OpenMode(NewOnly | ExistingOnly)) | Truncate)) {
                m_direct = true;
                return true;
            }
            return false;
        }
        setFileError(OpenError, QString::fromUtf8(std::strerror(err)));
        return false;
    }

    mode_t perms;
    if (exists) {
        perms = st.st_mode & 07777;
    } else {
        const mode_t mask = ::umask(0);
        ::umask(mask);
        perms = 0666 & ~mask;
    }
    ::fchmod(fd, perms);

    m_temp = QString::fromUtf8(buf.data());
    m_fd = fd;
    m_error = NoError;
    QIODevice::open(mode & ~OpenMode(NewOnly | ExistingOnly | Truncate));
    return true;
}

/*!
    \reimp

    Writes \a maxSize bytes from \a data to the temporary file. If the write
    is short or fails, commit() will fail.
*/
qint64 QSaveFile::writeData(const char *data, qint64 maxSize) {
    const qint64 n = QFileDevice::writeData(data, maxSize);
    if (n != maxSize)
        m_writeError = true;
    return n;
}

/*!
    Syncs the temporary file to disk, closes it and renames it over the
    target. Returns \c true if the target was replaced; otherwise returns
    \c false, removes the temporary file and leaves the target untouched.
    Returns \c false if the file is not open, a write failed, or
    cancelWriting() was called. When writing directly to the target (see
    setDirectWriteFallback()), the file is flushed and closed instead.

    \sa cancelWriting(), open()
*/
bool QSaveFile::commit() {
    if (!isOpen())
        return false;
    bool ok = !m_writeError;
    if (m_direct) {
        if (ok)
            ok = flush();
        QFileDevice::close();
        m_direct = false;
        return ok;
    }
    if (ok && ::fsync(m_fd) != 0 && errno != EINVAL) {
        setFileError(WriteError, QString::fromUtf8(std::strerror(errno)));
        ok = false;
    }
    QFileDevice::close();
    const QByteArray temp = m_temp.toUtf8();
    if (ok && ::rename(temp.constData(), m_target.toUtf8().constData()) != 0) {
        setFileError(RenameError, QString::fromUtf8(std::strerror(errno)));
        ok = false;
    }
    if (ok) {
        const QString dir = QFileInfo(m_target).absolutePath();
        const int dfd = ::open(dir.toUtf8().constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dfd >= 0) {
            ::fsync(dfd);
            ::close(dfd);
        }
    } else {
        ::unlink(temp.constData());
    }
    m_temp.clear();
    return ok;
}

/*!
    Cancels writing the file: sets error() to \c AbortError and makes a later
    commit() fail without touching the target. Does nothing if the file is not
    open.

    \sa commit()
*/
void QSaveFile::cancelWriting() {
    if (!isOpen())
        return;
    setFileError(AbortError, QString::fromUtf8("Writing canceled by application"));
    m_writeError = true;
}

void QSaveFile::close() { commit(); }
