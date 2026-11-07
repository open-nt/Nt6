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

/*
    Differences from Qt:
    - close() commits instead of aborting, so a base-class close() call still
      finishes the save; prefer commit().
    - The temporary file is named "<target>.XXXXXX" and, like Qt, is removed
      when the object is destroyed or cancelWriting()/a write error precedes
      commit(). Permissions of an existing target are preserved; a new target
      gets 0666 masked by the umask.
    - Only write modes are supported; Append is rejected and existing content
      is never copied into the temporary file.
*/

QSaveFile::QSaveFile(const QString &name) : m_target(name) { m_name = name; }

QSaveFile::QSaveFile(QObject *parent) : QFileDevice(parent) {}

QSaveFile::QSaveFile(const QString &name, QObject *parent) : QFileDevice(parent), m_target(name) { m_name = name; }

QSaveFile::~QSaveFile() {
    if (isOpen()) {
        QFileDevice::close();
        if (!m_direct && !m_temp.isEmpty())
            ::unlink(m_temp.toUtf8().constData());
    }
}

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

qint64 QSaveFile::writeData(const char *data, qint64 maxSize) {
    const qint64 n = QFileDevice::writeData(data, maxSize);
    if (n != maxSize)
        m_writeError = true;
    return n;
}

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

void QSaveFile::cancelWriting() {
    if (!isOpen())
        return;
    setFileError(AbortError, QString::fromUtf8("Writing canceled by application"));
    m_writeError = true;
}

void QSaveFile::close() { commit(); }
