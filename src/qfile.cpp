// MIT License - see LICENSE
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QtCore/qfile.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

/*!
    \class QFileDevice
    \brief The QFileDevice class is the base class of devices that read and write files.

    QFileDevice implements the file-descriptor based reading, writing and
    seeking shared by QFile, QSaveFile and QTemporaryFile. It is not
    instantiated directly. Data is read and written straight through the
    operating system, without an additional buffer.

    The last failure is available from error() and errorString().

    \differences
    There are no permission or timestamp functions (\c permissions(), \c
    setPermissions(), \c fileTime(), \c setFileTime() and the \c Permission
    and \c FileTime enums), no memory mapping (\c map() and \c unmap()), and
    no \c FileHandleFlags. flush() calls \c fsync() on the descriptor instead
    of flushing a user-space buffer. The class is POSIX-only.
    \enddifferences

    \sa QFile, QSaveFile, QTemporaryFile, QIODevice
*/

/*!
    Constructs a file device with no parent. The device is not open.
*/
QFileDevice::QFileDevice() = default;
/*!
    Constructs a file device with the given \a parent. The device is not open.
*/
QFileDevice::QFileDevice(QObject *parent) : QIODevice(parent) {}
/*!
    Destroys the file device. It does not close the file; subclasses do so in
    their own destructors.
*/
QFileDevice::~QFileDevice() = default;

/*!
    Sets the file error to \a e and the error string to \a text.

    \sa error(), setErrorString()
*/
void QFileDevice::setFileError(FileError e, const QString &text) {
    m_error = e;
    setErrorString(text);
}

/*!
    Opens the file at \a path in the given \a mode and stores its descriptor.
    Returns \c true on success. Otherwise sets error() and errorString() and
    returns \c false.

    \c Append and \c Truncate imply \c WriteOnly. The file is created if it
    does not exist, unless \c ExistingOnly is given. \c NewOnly fails if the
    file already exists. Opening a device that is already open fails with
    \c OpenError.

    Subclasses call this function from their open() reimplementations.

    \sa close()
*/
bool QFileDevice::openPath(const QString &path, OpenMode mode) {
    if (mode & (Append | Truncate))
        mode |= WriteOnly;
    if (isOpen()) {
        setFileError(OpenError, QString::fromUtf8("file already open"));
        return false;
    }
    if (!(mode & (ReadOnly | WriteOnly))) {
        setFileError(OpenError, QString::fromUtf8("no open mode given"));
        return false;
    }
    int flags = mode.testFlag(ReadOnly) ? (mode.testFlag(WriteOnly) ? O_RDWR : O_RDONLY) : O_WRONLY;
    if (mode.testFlag(WriteOnly)) {
        if (!mode.testFlag(ExistingOnly))
            flags |= O_CREAT;
        if (mode.testFlag(NewOnly))
            flags |= O_EXCL | O_CREAT;
        if (mode.testFlag(Append))
            flags |= O_APPEND;
        if (mode.testFlag(Truncate) || (!mode.testFlag(ReadOnly) && !mode.testFlag(Append)))
            flags |= O_TRUNC;
    }
    const std::string p = path.toUtf8().toStdString();
    const int fd = ::open(p.c_str(), flags | O_CLOEXEC, 0666);
    if (fd < 0) {
        setFileError(OpenError, QString::fromUtf8(std::strerror(errno)));
        return false;
    }
    m_fd = fd;
    m_error = NoError;
    QIODevice::open(mode);
    return true;
}

/*!
    \reimp

    Closes the file and its descriptor. Does nothing if the file is not open.
*/
void QFileDevice::close() {
    if (!isOpen())
        return;
    QIODevice::close();
    if (m_fd >= 0)
        ::close(m_fd);
    m_fd = -1;
}

/*!
    \reimp

    Returns the current file position, or \c 0 if the file is not open.
*/
qint64 QFileDevice::pos() const {
    if (m_fd < 0)
        return 0;
    return static_cast<qint64>(::lseek(m_fd, 0, SEEK_CUR));
}

/*!
    \reimp

    Returns the size of the file in bytes. If the file is not open, the size
    of the file at fileName() is returned. Returns \c 0 if the size cannot be
    determined.
*/
qint64 QFileDevice::size() const {
    struct stat st;
    if (m_fd >= 0 && ::fstat(m_fd, &st) == 0)
        return static_cast<qint64>(st.st_size);
    if (!m_name.isEmpty() && ::stat(m_name.toUtf8().constData(), &st) == 0)
        return static_cast<qint64>(st.st_size);
    return 0;
}

/*!
    \reimp

    For random-access devices, this function sets the current position to
    \a offset, measured from the start of the file. Returns \c true on
    success. Returns \c false if the file is not open, \a offset is negative,
    or the position cannot be changed, in which case error() is set to
    \c PositionError.
*/
bool QFileDevice::seek(qint64 offset) {
    if (m_fd < 0 || offset < 0)
        return false;
    QIODevice::seek(offset);
    if (::lseek(m_fd, static_cast<off_t>(offset), SEEK_SET) < 0) {
        setFileError(PositionError, QString::fromUtf8(std::strerror(errno)));
        return false;
    }
    return true;
}

/*!
    \reimp

    Returns \c true if the file is open and the position is at the end of the
    file with no buffered data left; otherwise returns \c false.
*/
bool QFileDevice::atEnd() const { return isOpen() && QIODevice::bytesAvailable() == 0 && pos() >= size(); }

/*!
    \reimp

    Returns the number of bytes between the current position and the end of
    the file, plus any buffered look-ahead data.
*/
qint64 QFileDevice::bytesAvailable() const { return QIODevice::bytesAvailable() + std::max<qint64>(0, size() - pos()); }

/*!
    \reimp

    Returns \c true if the look-ahead buffer holds a complete line or the file
    is not at its end; otherwise returns \c false.
*/
bool QFileDevice::canReadLine() const { return QIODevice::canReadLine() || !atEnd(); }

/*!
    Flushes the file's pending data to disk by calling \c fsync() on the
    descriptor. Returns \c true if successful, or if the file is not open or
    does not support synchronizing; otherwise returns \c false.

    \sa close()
*/
bool QFileDevice::flush() { return m_fd < 0 || ::fsync(m_fd) == 0 || errno == EINVAL; }

/*!
    Sets the file size to \a sz bytes. Returns \c true if the file was
    resized successfully; returns \c false if the file is not open or the
    operation fails. If \a sz is larger than the file, the extra bytes read as
    zero; if it is smaller, the file is cut.

    \sa size()
*/
bool QFileDevice::resize(qint64 sz) {
    if (m_fd < 0)
        return false;
    return ::ftruncate(m_fd, static_cast<off_t>(sz)) == 0;
}

/*!
    \reimp

    Reads up to \a maxSize bytes from the file descriptor into \a data,
    retrying when interrupted. Returns the number of bytes read, or \c -1 on
    error, in which case error() is set to \c ReadError.
*/
qint64 QFileDevice::readData(char *data, qint64 maxSize) {
    if (m_fd < 0)
        return -1;
    ssize_t n;
    do {
        n = ::read(m_fd, data, static_cast<std::size_t>(maxSize));
    } while (n < 0 && errno == EINTR);
    if (n < 0) {
        setFileError(ReadError, QString::fromUtf8(std::strerror(errno)));
        return -1;
    }
    return n;
}

/*!
    \reimp

    Writes up to \a maxSize bytes from \a data to the file descriptor,
    retrying on partial writes. Returns the number of bytes written, or
    \c -1 if nothing could be written, in which case error() is set to
    \c WriteError.
*/
qint64 QFileDevice::writeData(const char *data, qint64 maxSize) {
    if (m_fd < 0)
        return -1;
    qint64 done = 0;
    while (done < maxSize) {
        const ssize_t n = ::write(m_fd, data + done, static_cast<std::size_t>(maxSize - done));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            setFileError(WriteError, QString::fromUtf8(std::strerror(errno)));
            return done > 0 ? done : -1;
        }
        done += n;
    }
    return done;
}

/*!
    \reimp

    Reads a chunk of up to \a maxSize bytes into \a data, ending at the first
    newline, and leaves the file position just after it. Returns the number
    of bytes read.
*/
qint64 QFileDevice::readLineData(char *data, qint64 maxSize) {
    // Reads a chunk and seeks back past the newline to avoid one syscall per byte.
    if (m_fd < 0 || maxSize <= 0)
        return -1;
    const qint64 want = std::min<qint64>(maxSize, 4096);
    const qint64 n = readData(data, want);
    if (n <= 0)
        return n;
    const char *nl = static_cast<const char *>(std::memchr(data, '\n', static_cast<std::size_t>(n)));
    if (!nl)
        return n;
    const qint64 keep = nl - data + 1;
    if (keep < n)
        ::lseek(m_fd, static_cast<off_t>(keep - n), SEEK_CUR);
    return keep;
}

/*!
    \class QFile
    \brief The QFile class provides an interface for reading from and writing to files.

    QFile opens a file by name and reads and writes it through the operating
    system's file descriptors. Set the name in the constructor or with
    setFileName(), then call open():

    \code
    QFile file("data.txt");
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray contents = file.readAll();
        file.close();
    }
    \endcode

    QFile also has static functions to check whether a file exists and to
    remove, rename and copy files. For text and binary data, use QTextStream
    and QDataStream on top of a QFile.

    \differences
    The constructors do not accept \c std::filesystem::path, and there are no
    \c open() overloads that take a \c FILE* or a file descriptor. The \c
    Text open flag is accepted but does not translate line endings. There is
    no \c symLinkTarget(), \c link(), \c moveToTrash(), \c readLink(),
    \c decodeName() or \c encodeName(). Static copy() refuses to overwrite an
    existing file, and rename() does not fall back to copying across
    filesystems. Permissions are not copied.
    \enddifferences

    \sa QFileDevice, QTextStream, QDataStream, QSaveFile, QTemporaryFile
*/

/*!
    Constructs a QFile with no file name and no parent.

    \sa setFileName()
*/
QFile::QFile() = default;
/*!
    \overload

    Constructs a new file object to represent the file with the given
    \a name.
*/
QFile::QFile(const QString &name) { m_name = name; }
/*!
    \overload

    Constructs a QFile with no file name and the given \a parent.
*/
QFile::QFile(QObject *parent) : QFileDevice(parent) {}
/*!
    \overload

    Constructs a new file object with the given \a parent to represent the
    file with the given \a name.
*/
QFile::QFile(const QString &name, QObject *parent) : QFileDevice(parent) { m_name = name; }
/*!
    Destroys the file object, closing it if necessary.
*/
QFile::~QFile() { close(); }

/*!
    \reimp

    Opens the file named by fileName() using the given \a mode. Returns
    \c true if successful; otherwise returns \c false and sets error() to
    \c OpenError. It fails if no file name has been set, the file is already
    open, or the operating system refuses to open it.

    \sa QFileDevice::openPath(), close(), setFileName()
*/
bool QFile::open(OpenMode mode) {
    if (m_name.isEmpty()) {
        setFileError(OpenError, QString::fromUtf8("no file name specified"));
        return false;
    }
    return openPath(m_name, mode);
}

/*!
    Returns \c true if the file specified by \a fileName exists; otherwise
    returns \c false.

    \sa remove()
*/
bool QFile::exists(const QString &fileName) {
    struct stat st;
    return ::stat(fileName.toUtf8().constData(), &st) == 0;
}

/*!
    \overload

    Removes the file specified by \a fileName. Returns \c true if successful;
    otherwise returns \c false. The file is not moved to a trash.

    \sa remove(), exists()
*/
bool QFile::remove(const QString &fileName) { return ::unlink(fileName.toUtf8().constData()) == 0; }

/*!
    Removes the file specified by fileName(), closing it first if it is open.
    Returns \c true if successful; otherwise returns \c false and sets
    error() to \c RemoveError.

    \sa exists()
*/
bool QFile::remove() {
    if (isOpen())
        close();
    if (!remove(m_name)) {
        setFileError(RemoveError, QString::fromUtf8(std::strerror(errno)));
        return false;
    }
    return true;
}

/*!
    \overload

    Renames the file \a oldName to \a newName. Returns \c true if successful;
    otherwise returns \c false. An existing file at \a newName is replaced.

    \sa rename()
*/
bool QFile::rename(const QString &oldName, const QString &newName) {
    return ::rename(oldName.toUtf8().constData(), newName.toUtf8().constData()) == 0;
}

/*!
    Renames the file currently specified by fileName() to \a newName,
    closing it first if it is open. Returns \c true if successful; otherwise
    returns \c false and sets error() to \c RenameError. On success,
    fileName() becomes \a newName.

    \sa exists()
*/
bool QFile::rename(const QString &newName) {
    if (isOpen())
        close();
    if (!rename(m_name, newName)) {
        setFileError(RenameError, QString::fromUtf8(std::strerror(errno)));
        return false;
    }
    m_name = newName;
    return true;
}

/*!
    \overload

    Copies the file \a fileName to \a newName. Returns \c true if successful;
    otherwise returns \c false. The copy fails if a file named \a newName
    already exists.

    \sa copy()
*/
bool QFile::copy(const QString &fileName, const QString &newName) {
    if (exists(newName))
        return false;
    QFile in(fileName), out(newName);
    if (!in.open(ReadOnly) || !out.open(WriteOnly))
        return false;
    char buf[65536];
    for (;;) {
        const qint64 n = in.read(buf, sizeof buf);
        if (n < 0)
            return false;
        if (n == 0)
            return true;
        if (out.write(buf, n) != n)
            return false;
    }
}

/*!
    Copies the file currently specified by fileName() to a file called
    \a newName, closing it first if it is open. Returns \c true if
    successful; otherwise returns \c false and sets error() to
    \c CopyError. The copy fails if a file named \a newName already exists.

    \sa rename(), remove()
*/
bool QFile::copy(const QString &newName) {
    if (isOpen())
        close();
    if (!copy(m_name, newName)) {
        setFileError(CopyError, QString::fromUtf8("copy failed"));
        return false;
    }
    return true;
}

#include "moc_qfile.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
