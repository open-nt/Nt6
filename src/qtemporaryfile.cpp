// MIT License - see LICENSE

#include <QtCore/qtemporaryfile.h>

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qtemporarydir.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

/*!
    \class QTemporaryFile
    \brief The QTemporaryFile class is an I/O device that operates on temporary files.

    QTemporaryFile creates a uniquely named file that is safe to use from
    several processes. The file is created by open() and, by default, removed
    when the object is destroyed.

    The file name is built from a template. The six characters \c XXXXXX in
    the last path component are replaced by random characters. A template
    without \c XXXXXX gets \c{.XXXXXX} appended. The default template is
    \c{qt_temp.XXXXXX} in QDir::tempPath(); a relative template is resolved
    against the current directory.

    \code
    QTemporaryFile file;
    if (file.open()) {
        file.write("scratch data");
        const QString name = file.fileName();
    }
    \endcode

    \differences
    open() always opens the file for reading and writing, whatever mode
    was requested; only the \c Append, \c Truncate, \c Text and \c Unbuffered
    flags are honored. createNativeFile() copies resources only; a file that
    is reached through a custom file engine does not exist in Nt.

    On Zephyr the default directory, \c{/tmp}, does not exist, so give a
    template with an absolute path on a mounted file system. See \l{Nt on
    Zephyr}.
    \enddifferences

    \sa QTemporaryDir, QFile, QSaveFile
*/

/*!
    \class QTemporaryDir
    \brief The QTemporaryDir class creates a unique directory for temporary use.

    QTemporaryDir creates a uniquely named directory in its constructor and,
    by default, removes it with all of its contents when the object is
    destroyed. Check isValid() before using path().

    The directory name is built from a template. The six characters
    \c XXXXXX in the last path component are replaced by random characters. A
    template without \c XXXXXX gets \c{-XXXXXX} appended. The default template
    is \c{qt_temp-XXXXXX} in QDir::tempPath(). The directory is created with
    mode \c 0700.

    \code
    QTemporaryDir dir;
    if (dir.isValid()) {
        QFile file(dir.filePath("output.txt"));
        file.open(QIODevice::WriteOnly);
    }
    \endcode

    A QTemporaryDir can be moved but not copied.

    \differences
    path() is always an absolute path, even for a relative template.
    \enddifferences

    \sa QTemporaryFile, QDir
*/

namespace {

const char kPlaceholder[] = "XXXXXX";

// Splits a template into a mutable path and the length of the part after the
// last XXXXXX in the file name, appending `fallbackSuffix` when there is none.
std::string expandTemplate(const QString &templ, const char *fallbackSuffix, std::size_t &suffixLen) {
    std::string path = templ.toStdString();
    const std::size_t nameStart = path.rfind('/') == std::string::npos ? 0 : path.rfind('/') + 1;
    std::size_t pos = path.rfind(kPlaceholder);
    if (pos == std::string::npos || pos < nameStart) {
        path += fallbackSuffix;
        pos = path.size() - 6;
    }
    suffixLen = path.size() - pos - 6;
    return path;
}

QString defaultFileTemplate() { return QDir::tempPath() + QString::fromUtf8("/qt_temp.XXXXXX"); }

} // namespace

/*!
    Constructs a QTemporaryFile using the default template, which is
    \c{qt_temp.XXXXXX} in QDir::tempPath(). The file is created by open().

    \sa setFileTemplate()
*/
QTemporaryFile::QTemporaryFile() : m_template(defaultFileTemplate()) {}

/*!
    Constructs a QTemporaryFile with the given \a templateName. The file is
    created by open().

    \sa setFileTemplate()
*/
QTemporaryFile::QTemporaryFile(const QString &templateName) : m_template(templateName) {}

/*!
    Constructs a QTemporaryFile with the given \a parent, using the default
    template.
*/
QTemporaryFile::QTemporaryFile(QObject *parent) : QFile(parent), m_template(defaultFileTemplate()) {}

/*!
    Constructs a QTemporaryFile with the given \a parent, using
    \a templateName as the file name template.
*/
QTemporaryFile::QTemporaryFile(const QString &templateName, QObject *parent) : QFile(parent), m_template(templateName) {}

/*!
    Destroys the temporary file object, closing the file. The file is deleted
    if autoRemove() is \c true.

    \sa autoRemove()
*/
QTemporaryFile::~QTemporaryFile() {
    close();
    if (m_autoRemove && !m_name.isEmpty())
        ::unlink(m_name.toUtf8().constData());
}

/*!
    \reimp

    Creates the file from the template and opens it for reading and writing.
    Only the \c Append, \c Truncate, \c Text and \c Unbuffered flags of
    \a mode are used. Returns \c true on success; otherwise returns \c false
    and sets error() to \c OpenError. If the file was already created by an
    earlier call, it is reopened under the same name.

    \sa open()
*/
bool QTemporaryFile::open(OpenMode mode) {
    if (isOpen()) {
        setFileError(OpenError, QString::fromUtf8("file already open"));
        return false;
    }
    const OpenMode keep = mode & (Append | Truncate | Text | Unbuffered);
    const OpenMode effective = OpenMode(ReadWrite) | keep;
    if (!m_name.isEmpty())
        return openPath(m_name, effective);

    std::size_t suffixLen = 0;
    std::string path = expandTemplate(m_template.isEmpty() ? defaultFileTemplate() : m_template, ".XXXXXX", suffixLen);
    std::vector<char> buf(path.begin(), path.end());
    buf.push_back('\0');
    const int fd = ::mkostemps(buf.data(), static_cast<int>(suffixLen), O_CLOEXEC);
    if (fd < 0) {
        setFileError(OpenError, QString::fromUtf8(std::strerror(errno)));
        return false;
    }
    m_name = QString::fromUtf8(buf.data());
    m_fd = fd;
    m_error = NoError;
    // Only records the open mode; the file is already open.
    QIODevice::open(effective); // NOLINT(bugprone-parent-virtual-call)
    return true;
}

/*!
    Renames the temporary file to \a newName, keeping it open, and returns
    \c true if successful. Unlike QFile::rename(), it never copies the file,
    so it fails if \a newName is on another file system. Whether the file is
    removed on destruction still depends on autoRemove().

    \sa setAutoRemove()
*/
bool QTemporaryFile::rename(const QString &newName) {
    if (m_name.isEmpty() || newName.isEmpty())
        return false;
    if (::rename(m_name.toUtf8().constData(), newName.toUtf8().constData()) != 0) {
        setFileError(RenameError, QString::fromUtf8(std::strerror(errno)));
        return false;
    }
    m_name = newName;
    return true;
}

/*!
    Returns a temporary copy of \a file if it is a resource, for code that
    needs a file on disk; returns \c nullptr if \a file is a native file
    already, has no name or cannot be read. The copy is open for reading and
    writing at position 0, and the caller owns it. \a file is left as it
    was: open at the same position, or closed.

    \code
    QFile icon(":/icons/open.svg");
    std::unique_ptr<QTemporaryFile> copy(QTemporaryFile::createNativeFile(icon));
    const QString path = copy ? copy->fileName() : icon.fileName();
    \endcode

    \sa QFileInfo::isNativePath()
*/
QTemporaryFile *QTemporaryFile::createNativeFile(QFile &file) {
    if (file.fileName().isEmpty() || QFileInfo(file.fileName()).isNativePath())
        return nullptr;
    const bool wasOpen = file.isOpen();
    const qint64 oldPos = wasOpen ? file.pos() : 0;
    if (!wasOpen && !file.open(QIODevice::ReadOnly))
        return nullptr;
    auto copy = std::make_unique<QTemporaryFile>();
    bool ok = copy->open() && file.seek(0);
    if (ok) {
        const QByteArray data = file.readAll();
        ok = copy->write(data) == data.size() && copy->seek(0);
    }
    if (wasOpen)
        file.seek(oldPos);
    else
        file.close();
    return ok ? copy.release() : nullptr;
}

/*!
    \overload
    Returns a temporary copy of the file \a fileName if it is a resource, or
    \c nullptr if it is a native file already.
*/
QTemporaryFile *QTemporaryFile::createNativeFile(const QString &fileName) {
    QFile file(fileName);
    return createNativeFile(file);
}

/*!
    Constructs a QTemporaryDir that creates a directory using the default
    template, \c{qt_temp-XXXXXX} in QDir::tempPath(). Use isValid() to check
    whether the directory was created.
*/
QTemporaryDir::QTemporaryDir() { create(QDir::tempPath() + QString::fromUtf8("/qt_temp-XXXXXX")); }

/*!
    Constructs a QTemporaryDir that creates a directory from
    \a templatePath. If \a templatePath is empty, the default template is
    used. Use isValid() to check whether the directory was created.
*/
QTemporaryDir::QTemporaryDir(const QString &templatePath) {
    create(templatePath.isEmpty() ? QDir::tempPath() + QString::fromUtf8("/qt_temp-XXXXXX") : templatePath);
}

/*!
    Move-constructs a QTemporaryDir from \a other. The directory and its
    auto-remove setting are transferred; \a other becomes invalid and no
    longer removes anything.
*/
QTemporaryDir::QTemporaryDir(QTemporaryDir &&other) noexcept
    : m_path(std::move(other.m_path)), m_error(std::move(other.m_error)), m_autoRemove(other.m_autoRemove) {
    other.m_path.clear();
    other.m_autoRemove = false;
}

/*!
    Move-assigns \a other to this object and returns a reference to it. The
    directory currently held, if any and if auto-remove is enabled, is
    removed first.
*/
QTemporaryDir &QTemporaryDir::operator=(QTemporaryDir &&other) noexcept {
    if (this != &other) {
        if (m_autoRemove && isValid())
            remove();
        m_path = std::move(other.m_path);
        m_error = std::move(other.m_error);
        m_autoRemove = other.m_autoRemove;
        other.m_path.clear();
        other.m_autoRemove = false;
    }
    return *this;
}

/*!
    Destroys the object. If autoRemove() is \c true and the directory is
    valid, it is removed with all its contents.

    \sa remove()
*/
QTemporaryDir::~QTemporaryDir() {
    if (m_autoRemove && isValid())
        remove();
}

void QTemporaryDir::create(const QString &templatePath) {
    m_path.clear();
    m_error.clear();
    std::size_t suffixLen = 0;
    const std::string path = expandTemplate(templatePath, "-XXXXXX", suffixLen);
    std::vector<char> buf(path.begin(), path.end());
    buf.push_back('\0');

    bool ok = false;
    if (suffixLen == 0) {
        ok = ::mkdtemp(buf.data()) != nullptr;
    } else {
        // mkdtemp() needs the placeholder at the very end, so fill it in by hand.
        static const char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::random_device rd;
        const std::size_t pos = path.size() - suffixLen - 6;
        for (int attempt = 0; attempt < 100 && !ok; ++attempt) {
            for (std::size_t i = 0; i < 6; ++i)
                buf[pos + i] = alphabet[rd() % (sizeof alphabet - 1)];
            ok = ::mkdir(buf.data(), 0700) == 0;
            if (!ok && errno != EEXIST)
                break;
        }
    }
    if (!ok) {
        m_error = QString::fromUtf8("Failed to create a temporary directory: ") + QString::fromUtf8(std::strerror(errno));
        return;
    }
    m_path = QFileInfo(QString::fromUtf8(buf.data())).absoluteFilePath();
}

/*!
    Removes the directory and all its contents recursively. Returns \c true
    if successful; returns \c false if the directory is not valid or the
    removal fails.

    \sa isValid(), autoRemove()
*/
bool QTemporaryDir::remove() {
    if (!isValid())
        return false;
    return QDir(m_path).removeRecursively();
}

/*!
    Returns the path to \a fileName inside the temporary directory. The file
    is not created. Returns an empty string if the directory is not valid or
    \a fileName is an absolute path.

    \sa path()
*/
QString QTemporaryDir::filePath(const QString &fileName) const {
    if (!isValid() || QDir::isAbsolutePath(fileName))
        return QString();
    return m_path + QChar(u'/') + fileName;
}

#include "moc_qtemporaryfile.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
