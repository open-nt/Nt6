// MIT License - see LICENSE
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QtCore/qfile.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

QFileDevice::QFileDevice() = default;
QFileDevice::QFileDevice(QObject *parent) : QIODevice(parent) {}
QFileDevice::~QFileDevice() = default;

void QFileDevice::setFileError(FileError e, const QString &text) {
    m_error = e;
    setErrorString(text);
}

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

void QFileDevice::close() {
    if (!isOpen())
        return;
    QIODevice::close();
    if (m_fd >= 0)
        ::close(m_fd);
    m_fd = -1;
}

qint64 QFileDevice::pos() const {
    if (m_fd < 0)
        return 0;
    return static_cast<qint64>(::lseek(m_fd, 0, SEEK_CUR));
}

qint64 QFileDevice::size() const {
    struct stat st;
    if (m_fd >= 0 && ::fstat(m_fd, &st) == 0)
        return static_cast<qint64>(st.st_size);
    if (!m_name.isEmpty() && ::stat(m_name.toUtf8().constData(), &st) == 0)
        return static_cast<qint64>(st.st_size);
    return 0;
}

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

bool QFileDevice::atEnd() const { return isOpen() && QIODevice::bytesAvailable() == 0 && pos() >= size(); }

qint64 QFileDevice::bytesAvailable() const { return QIODevice::bytesAvailable() + std::max<qint64>(0, size() - pos()); }

bool QFileDevice::canReadLine() const { return QIODevice::canReadLine() || !atEnd(); }

bool QFileDevice::flush() { return m_fd < 0 || ::fsync(m_fd) == 0 || errno == EINVAL; }

bool QFileDevice::resize(qint64 sz) {
    if (m_fd < 0)
        return false;
    return ::ftruncate(m_fd, static_cast<off_t>(sz)) == 0;
}

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

// Reads a chunk and seeks back past the newline to avoid one syscall per byte.
qint64 QFileDevice::readLineData(char *data, qint64 maxSize) {
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

QFile::QFile() = default;
QFile::QFile(const QString &name) { m_name = name; }
QFile::QFile(QObject *parent) : QFileDevice(parent) {}
QFile::QFile(const QString &name, QObject *parent) : QFileDevice(parent) { m_name = name; }
QFile::~QFile() { close(); }

bool QFile::open(OpenMode mode) {
    if (m_name.isEmpty()) {
        setFileError(OpenError, QString::fromUtf8("no file name specified"));
        return false;
    }
    return openPath(m_name, mode);
}

bool QFile::exists(const QString &fileName) {
    struct stat st;
    return ::stat(fileName.toUtf8().constData(), &st) == 0;
}

bool QFile::remove(const QString &fileName) { return ::unlink(fileName.toUtf8().constData()) == 0; }

bool QFile::remove() {
    if (isOpen())
        close();
    if (!remove(m_name)) {
        setFileError(RemoveError, QString::fromUtf8(std::strerror(errno)));
        return false;
    }
    return true;
}

bool QFile::rename(const QString &oldName, const QString &newName) {
    return ::rename(oldName.toUtf8().constData(), newName.toUtf8().constData()) == 0;
}

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

bool QFile::copy(const QString &newName) {
    if (isOpen())
        close();
    if (!copy(m_name, newName)) {
        setFileError(CopyError, QString::fromUtf8("copy failed"));
        return false;
    }
    return true;
}
