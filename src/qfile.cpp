// MIT License - see LICENSE
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>

#include <algorithm>
#include <climits>
#include <ctime>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

/*!
    \class QFileDevice
    \brief The QFileDevice class is the base class of devices that read and write files.

    QFileDevice implements the file-descriptor based reading, writing and
    seeking shared by QFile, QSaveFile and QTemporaryFile. It is not
    instantiated directly. Data is read and written straight through the
    operating system, without an additional buffer.

    The last failure is available from error() and errorString().

    \differences
    map() and unmap() use \c mmap(2), and a mapping is removed only by
    unmap() or the destructor. flush() calls \c fsync() on the descriptor instead
    of flushing a user-space buffer. The class is POSIX-only. Permission and
    Permissions are the QFileInfo types. setFileTime() can change only the
    access and modification times, and fileTime() reports the birth time only
    where the kernel and file system provide it (\c statx() on Linux).
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
    Destroys the file device, removing the mappings that map() created.
    Subclasses close the file in their own destructors; a descriptor that is
    still open here is closed unless it was opened with \c DontCloseHandle.
*/
QFileDevice::~QFileDevice() {
    for (const auto &m : m_maps)
        ::munmap(m.second.first, m.second.second);
    if (m_fd >= 0 && m_closeHandle) {
        if (m_stdio)
            std::fclose(m_stdio);
        else
            ::close(m_fd);
    }
    m_fd = -1;
}

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
    m_closeHandle = true;
    m_stdio = nullptr;
    m_error = NoError;
    QIODevice::open(mode);
    return true;
}

/*!
    Adopts the open descriptor \a fd, or the stream \a fh if it is not
    \c nullptr, in \a mode. With \c AutoCloseHandle in \a handleFlags,
    close() closes it too. Subclasses call this from their open() overloads.

    \sa openPath()
*/
bool QFileDevice::openHandle(int fd, std::FILE *fh, OpenMode mode, FileHandleFlags handleFlags) {
    if (isOpen()) {
        setFileError(OpenError, QString::fromUtf8("file already open"));
        return false;
    }
    if (fh) {
        // Data the stream still buffers would be written after ours; push it out first.
        std::fflush(fh);
        fd = ::fileno(fh);
    }
    if (fd < 0) {
        setFileError(OpenError, QString::fromUtf8("invalid file handle"));
        return false;
    }
    m_fd = fd;
    m_stdio = fh;
    m_closeHandle = handleFlags.testFlag(AutoCloseHandle);
    m_error = NoError;
    QIODevice::open(mode);
    return true;
}

/*!
    \since 4.4
    Maps \a size bytes of the file, starting at \a offset, into memory and
    returns the address, or \c nullptr if it fails. The file must be open,
    and the range must lie within it. The memory is writable if the file is
    open for writing or \a flags contains \c MapPrivateOption. The mapping
    stays valid after close() until unmap() is called or the object is
    destroyed.

    \sa unmap()
*/
uchar *QFileDevice::map(qint64 offset, qint64 size, MemoryMapFlags flags) {
    if (m_fd < 0) {
        setFileError(PermissionsError, QString::fromUtf8("the file is not open"));
        return nullptr;
    }
    if (offset < 0 || size <= 0 || offset > this->size() - size) {
        setFileError(UnspecifiedError, QString::fromUtf8("the range is not in the file"));
        return nullptr;
    }
    const qint64 page = ::sysconf(_SC_PAGESIZE);
    const qint64 start = offset - offset % page; // mmap() needs a page-aligned offset
    const auto delta = static_cast<std::size_t>(offset - start);
    const std::size_t length = static_cast<std::size_t>(size) + delta;
    const bool priv = flags.testFlag(MapPrivateOption);
    int prot = PROT_READ;
    if (priv || openMode().testFlag(WriteOnly))
        prot |= PROT_WRITE;
    void *base = ::mmap(nullptr, length, prot, priv ? MAP_PRIVATE : MAP_SHARED, m_fd, static_cast<off_t>(start));
    if (base == MAP_FAILED) {
        setFileError(errno == EACCES ? PermissionsError : ResourceError, QString::fromUtf8(std::strerror(errno)));
        return nullptr;
    }
    uchar *address = static_cast<uchar *>(base) + delta;
    m_maps[address] = {base, length};
    m_error = NoError;
    return address;
}

/*!
    \since 4.4
    Removes the mapping at \a address, which map() returned. Returns \c true
    if successful.

    \sa map()
*/
bool QFileDevice::unmap(uchar *address) {
    const auto it = m_maps.find(address);
    if (it == m_maps.end()) {
        setFileError(PermissionsError, QString::fromUtf8("the address was not mapped by this file"));
        return false;
    }
    ::munmap(it->second.first, it->second.second);
    m_maps.erase(it);
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
    if (m_fd >= 0 && m_closeHandle) {
        if (m_stdio)
            std::fclose(m_stdio);
        else
            ::close(m_fd);
    }
    m_fd = -1;
    m_stdio = nullptr;
    m_closeHandle = true;
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

namespace {

mode_t toMode(QFileDevice::Permissions p) {
    mode_t m = 0;
    // The owner is the user on POSIX systems, so either flag sets the owner bit, as in Qt.
    if (p & (QFileDevice::ReadOwner | QFileDevice::ReadUser))
        m |= S_IRUSR;
    if (p & (QFileDevice::WriteOwner | QFileDevice::WriteUser))
        m |= S_IWUSR;
    if (p & (QFileDevice::ExeOwner | QFileDevice::ExeUser))
        m |= S_IXUSR;
    if (p & QFileDevice::ReadGroup)
        m |= S_IRGRP;
    if (p & QFileDevice::WriteGroup)
        m |= S_IWGRP;
    if (p & QFileDevice::ExeGroup)
        m |= S_IXGRP;
    if (p & QFileDevice::ReadOther)
        m |= S_IROTH;
    if (p & QFileDevice::WriteOther)
        m |= S_IWOTH;
    if (p & QFileDevice::ExeOther)
        m |= S_IXOTH;
    return m;
}

QFileDevice::Permissions fromMode(mode_t m) {
    QFileDevice::Permissions p;
    if (m & S_IRUSR)
        p |= QFileDevice::ReadOwner | QFileDevice::ReadUser;
    if (m & S_IWUSR)
        p |= QFileDevice::WriteOwner | QFileDevice::WriteUser;
    if (m & S_IXUSR)
        p |= QFileDevice::ExeOwner | QFileDevice::ExeUser;
    if (m & S_IRGRP)
        p |= QFileDevice::ReadGroup;
    if (m & S_IWGRP)
        p |= QFileDevice::WriteGroup;
    if (m & S_IXGRP)
        p |= QFileDevice::ExeGroup;
    if (m & S_IROTH)
        p |= QFileDevice::ReadOther;
    if (m & S_IWOTH)
        p |= QFileDevice::WriteOther;
    if (m & S_IXOTH)
        p |= QFileDevice::ExeOther;
    return p;
}

qint64 toMSecs(const struct timespec &t) { return static_cast<qint64>(t.tv_sec) * 1000 + t.tv_nsec / 1000000; }

// Reads the stat of the open descriptor \a fd, or of \a path when fd is negative.
bool statFile(int fd, const QString &path, struct stat *st) {
    if (fd >= 0)
        return ::fstat(fd, st) == 0;
    return !path.isEmpty() && ::stat(path.toUtf8().constData(), st) == 0;
}

QDateTime birthTime(int fd, const QString &path) {
#if defined(__linux__) && defined(STATX_BTIME)
    struct statx sx;
    const QByteArray p = path.toUtf8();
    const int r = fd >= 0 ? ::statx(fd, "", AT_EMPTY_PATH, STATX_BTIME, &sx)
                          : ::statx(AT_FDCWD, p.constData(), 0, STATX_BTIME, &sx);
    if (r == 0 && (sx.stx_mask & STATX_BTIME))
        return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(sx.stx_btime.tv_sec) * 1000 +
                                              sx.stx_btime.tv_nsec / 1000000);
#else
    Q_UNUSED(fd)
    Q_UNUSED(path)
#endif
    return QDateTime();
}

} // namespace

/*!
    Returns the permissions of the file: of the open descriptor, or of the
    file named fileName() if the device is not open. Returns no permissions if
    the file does not exist.

    \sa setPermissions(), QFileInfo::permissions()
*/
QFileDevice::Permissions QFileDevice::permissions() const {
    struct stat st;
    return statFile(m_fd, m_name, &st) ? fromMode(st.st_mode) : Permissions();
}

/*!
    Sets the permissions of the file to \a permissions. Returns \c true if
    successful; otherwise sets error() to \c PermissionsError and returns
    \c false. The owner bits are set if either the Owner or the User flag is
    given.

    \sa permissions()
*/
bool QFileDevice::setPermissions(Permissions permissions) {
    const int r =
        m_fd >= 0 ? ::fchmod(m_fd, toMode(permissions)) : ::chmod(m_name.toUtf8().constData(), toMode(permissions));
    if (r != 0) {
        setFileError(PermissionsError, QString::fromUtf8(std::strerror(errno)));
        return false;
    }
    return true;
}

/*!
    \since 5.10
    Returns the time stamp \a time of the file, of the open descriptor or of
    the file named fileName(). Returns an invalid QDateTime if the file does
    not exist or the time is not available, such as a birth time on a file
    system that does not record it.

    \sa setFileTime(), QFileInfo::lastModified()
*/
QDateTime QFileDevice::fileTime(FileTime time) const {
    if (time == FileBirthTime)
        return birthTime(m_fd, m_name);
    struct stat st;
    if (!statFile(m_fd, m_name, &st))
        return QDateTime();
    switch (time) {
    case FileAccessTime:
        return QDateTime::fromMSecsSinceEpoch(toMSecs(st.st_atim));
    case FileMetadataChangeTime:
        return QDateTime::fromMSecsSinceEpoch(toMSecs(st.st_ctim));
    case FileModificationTime:
    case FileBirthTime:
        break;
    }
    return QDateTime::fromMSecsSinceEpoch(toMSecs(st.st_mtim));
}

/*!
    \since 5.10
    Sets the time stamp \a fileTime of the file to \a newDate. Returns
    \c true if successful; otherwise sets error() to \c UnspecifiedError and
    returns \c false. Only FileAccessTime and FileModificationTime can be
    changed, and \a newDate must be valid.

    \sa fileTime()
*/
bool QFileDevice::setFileTime(const QDateTime &newDate, FileTime fileTime) {
    if (!newDate.isValid() || (fileTime != FileAccessTime && fileTime != FileModificationTime)) {
        setFileError(UnspecifiedError, QString::fromUtf8("Setting this file time is not supported"));
        return false;
    }
    const qint64 ms = newDate.toMSecsSinceEpoch();
    struct timespec times[2];
    times[0].tv_sec = times[1].tv_sec = 0;
    times[0].tv_nsec = times[1].tv_nsec = UTIME_OMIT;
    struct timespec &t = times[fileTime == FileAccessTime ? 0 : 1];
    qint64 secs = ms / 1000, rest = ms % 1000;
    if (rest < 0) {
        rest += 1000;
        --secs;
    }
    t.tv_sec = static_cast<time_t>(secs);
    t.tv_nsec = static_cast<long>(rest * 1000000);
    const int r = m_fd >= 0 ? ::futimens(m_fd, times) : ::utimensat(AT_FDCWD, m_name.toUtf8().constData(), times, 0);
    if (r != 0) {
        setFileError(UnspecifiedError, QString::fromUtf8(std::strerror(errno)));
        return false;
    }
    return true;
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
    The constructors do not accept \c std::filesystem::path. The \c Text open
    flag is accepted but does not translate line endings. moveToTrash()
    follows the freedesktop.org trash specification, so it works on Linux and
    other XDG desktops. Static copy() refuses to overwrite an
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
    return openPath(QtPrivate::resolveSearchPath(m_name), mode);
}

/*!
    \overload
    Opens the existing C stream \a fh in \a mode, which must match how the
    stream was opened. close() also closes the stream if \a handleFlags has
    \c AutoCloseHandle. Reads and writes go to the stream's descriptor, so
    do not mix them with \c stdio calls on \a fh.

    \code
    QFile out;
    out.open(stdout, QIODevice::WriteOnly);
    \endcode
*/
bool QFile::open(std::FILE *fh, OpenMode mode, FileHandleFlags handleFlags) {
    if (!fh) {
        setFileError(OpenError, QString::fromUtf8("invalid file handle"));
        return false;
    }
    return openHandle(-1, fh, mode, handleFlags);
}

/*!
    \overload
    Opens the existing file descriptor \a fd in \a mode. close() also
    closes the descriptor if \a handleFlags has \c AutoCloseHandle.
*/
bool QFile::open(int fd, OpenMode mode, FileHandleFlags handleFlags) {
    return openHandle(fd, nullptr, mode, handleFlags);
}

/*!
    Returns \c true if the file specified by \a fileName exists; otherwise
    returns \c false.

    \sa remove()
*/
bool QFile::exists(const QString &fileName) {
    struct stat st;
    return ::stat(QtPrivate::resolveSearchPath(fileName).toUtf8().constData(), &st) == 0;
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
    \reimp

    Returns the permissions of the file named fileName(), or of the open
    descriptor.
*/
QFile::Permissions QFile::permissions() const { return QFileDevice::permissions(); }

/*!
    \reimp

    Sets the permissions of the file to \a permissions.
*/
bool QFile::setPermissions(Permissions permissions) { return QFileDevice::setPermissions(permissions); }

/*!
    \overload

    Returns the permissions of the file \a fileName, or no permissions if the
    file does not exist.
*/
QFile::Permissions QFile::permissions(const QString &fileName) {
    struct stat st;
    return statFile(-1, fileName, &st) ? fromMode(st.st_mode) : Permissions();
}

/*!
    \overload

    Sets the permissions of the file \a fileName to \a permissions. Returns
    \c true if successful; otherwise returns \c false.
*/
bool QFile::setPermissions(const QString &fileName, Permissions permissions) {
    return ::chmod(fileName.toUtf8().constData(), toMode(permissions)) == 0;
}

/*!
    \since 4.2
    Returns the absolute path that the symbolic link fileName() points to, or
    an empty string if the file is not a symbolic link.

    \sa link(), QFileInfo::symLinkTarget()
*/
QString QFile::symLinkTarget() const { return QFileInfo(m_name).symLinkTarget(); }

/*!
    \overload

    Returns the absolute path that the symbolic link \a fileName points to,
    or an empty string if it is not a symbolic link.
*/
QString QFile::symLinkTarget(const QString &fileName) { return QFileInfo(fileName).symLinkTarget(); }

/*!
    Creates a symbolic link named \a linkName that points to fileName().
    Returns \c true if successful; otherwise sets error() to
    \c RenameError and returns \c false. An existing \a linkName is not
    replaced.

    \sa symLinkTarget()
*/
bool QFile::link(const QString &linkName) {
    if (m_name.isEmpty()) {
        setFileError(RenameError, QString::fromUtf8("No file name specified"));
        return false;
    }
    if (!link(m_name, linkName)) {
        setFileError(RenameError, QString::fromUtf8(std::strerror(errno)));
        return false;
    }
    return true;
}

/*!
    \overload

    Creates a symbolic link named \a linkName that points to \a fileName.
    The link stores \a fileName as given, so a relative name is resolved
    relative to the directory of the link. Returns \c true if successful;
    otherwise returns \c false.
*/
bool QFile::link(const QString &fileName, const QString &linkName) {
    return ::symlink(fileName.toUtf8().constData(), linkName.toUtf8().constData()) == 0;
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

namespace {

// The trash directory for `path` (absolute) as the freedesktop.org trash specification defines
// it: the home trash if the file is on the same file system as it, or $topdir/.Trash-$uid on the
// top directory of the file's mount. Returns false if neither can be created.
bool trashFor(const std::string &path, std::string &trash, bool &relativePaths) {
    auto ensureDir = [](const std::string &dir) {
        if (::mkdir(dir.c_str(), 0700) == 0)
            return true;
        struct stat st;
        return errno == EEXIST && ::lstat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    };
    struct stat fileSt;
    if (::lstat(path.c_str(), &fileSt) != 0)
        return false;
    std::string dataHome;
    if (const char *xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg == '/')
        dataHome = xdg;
    else if (const char *home = std::getenv("HOME"); home && *home)
        dataHome = std::string(home) + "/.local/share";
    if (!dataHome.empty()) {
        // Create the parents too, since a fresh home may not have ~/.local/share yet.
        std::string partial;
        for (std::size_t at = 1; at != std::string::npos;) {
            const std::size_t next = dataHome.find('/', at);
            partial = dataHome.substr(0, next);
            ::mkdir(partial.c_str(), 0700);
            at = next == std::string::npos ? next : next + 1;
        }
        struct stat homeSt;
        if (::stat(dataHome.c_str(), &homeSt) == 0 && homeSt.st_dev == fileSt.st_dev) {
            trash = dataHome + "/Trash";
            relativePaths = false;
            return ensureDir(trash) && ensureDir(trash + "/files") && ensureDir(trash + "/info");
        }
    }
    // Climb to the top directory of the mount that holds the file.
    std::string top = path.substr(0, path.rfind('/'));
    while (!top.empty()) {
        const std::string parent = top.substr(0, top.rfind('/'));
        struct stat st;
        if (::stat(parent.empty() ? "/" : parent.c_str(), &st) != 0 || st.st_dev != fileSt.st_dev)
            break;
        top = parent;
    }
    trash = (top.empty() ? std::string() : top) + "/.Trash-" + std::to_string(::getuid());
    relativePaths = true;
    return ensureDir(trash) && ensureDir(trash + "/files") && ensureDir(trash + "/info");
}

} // namespace

/*!
    \since 5.15
    Moves the file named by fileName() to the trash, closing it first, and
    sets fileName() to its path in the trash. Returns \c true if successful;
    otherwise returns \c false and sets error() to \c RenameError.

    \sa remove()
*/
bool QFile::moveToTrash() {
    if (isOpen())
        close();
    QString inTrash;
    if (!moveToTrash(m_name, &inTrash)) {
        setFileError(RenameError, QString::fromUtf8("unable to move the file to the trash"));
        return false;
    }
    m_name = inTrash;
    return true;
}

/*!
    \since 5.15
    \overload
    Moves the file or directory \a fileName to the trash of the desktop, as
    the freedesktop.org trash specification describes, so that the user can
    restore it. Returns \c true if successful and stores the new path in
    \c{*pathInTrash} if \a pathInTrash is not \c nullptr.
*/
bool QFile::moveToTrash(const QString &fileName, QString *pathInTrash) {
    if (fileName.isEmpty())
        return false;
    const std::string path = QFileInfo(fileName).absoluteFilePath().toStdString();
    std::string trash;
    bool relative = false;
    if (path == "/" || !trashFor(path, trash, relative))
        return false;
    const std::string base = path.substr(path.rfind('/') + 1);
    std::string recorded = path;
    if (relative) {
        const std::string top = trash.substr(0, trash.rfind('/'));
        recorded = path.substr(top.size() + 1);
    }
    char date[32];
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    ::localtime_r(&now, &local);
    std::strftime(date, sizeof date, "%Y-%m-%dT%H:%M:%S", &local);
    const QByteArray encoded = QByteArray::fromStdString(recorded).toPercentEncoding("/");
    const std::string info = "[Trash Info]\nPath=" + encoded.toStdString() + "\nDeletionDate=" + date + "\n";
    // Reserve a unique name by creating its .trashinfo file exclusively, then move the file.
    for (int n = 1; n < 10000; ++n) {
        const std::string name = n == 1 ? base : base + "." + std::to_string(n);
        std::string infoPath = trash;
        infoPath.append("/info/").append(name).append(".trashinfo");
        const int fd = ::open(infoPath.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd < 0) {
            if (errno == EEXIST)
                continue;
            return false;
        }
        const bool written = ::write(fd, info.data(), info.size()) == static_cast<ssize_t>(info.size());
        ::close(fd);
        std::string target = trash;
        target.append("/files/").append(name);
        struct stat st;
        if (!written || ::lstat(target.c_str(), &st) == 0 || ::rename(path.c_str(), target.c_str()) != 0) {
            const bool taken = written && ::lstat(target.c_str(), &st) == 0;
            ::unlink(infoPath.c_str());
            if (taken)
                continue;
            return false;
        }
        if (pathInTrash)
            *pathInTrash = QString::fromStdString(target);
        return true;
    }
    return false;
}

/*!
    \since 6.6
    Returns the target of the symbolic link named by fileName() exactly as it
    is stored, without making it absolute, or an empty string if the file is
    not a symbolic link.

    \sa symLinkTarget()
*/
QString QFile::readLink() const { return readLink(m_name); }

/*!
    \since 6.6
    \overload
    Returns the stored target of the symbolic link \a fileName, or an empty
    string if it is not a symbolic link.
*/
QString QFile::readLink(const QString &fileName) {
    std::vector<char> buf(PATH_MAX);
    const ssize_t n = ::readlink(fileName.toUtf8().constData(), buf.data(), buf.size());
    if (n < 0)
        return QString();
    return QString::fromUtf8(buf.data(), static_cast<qsizetype>(n));
}

#include "moc_qfile.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
