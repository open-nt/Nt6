// MIT License - see LICENSE

#include <QtCore/qfileinfo.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>

#include "qdir_p.h"
#include "qresource_p.h"

#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <vector>

/*
    Differences from Qt:
    - permissions() returns QFileInfo::Permissions (same bit values as Qt's
      QFileDevice::Permission) because QFileDevice has no permission enum yet.
    - absoluteFilePath() and absolutePath() are lexically cleaned (no trailing
      slash, "." and ".." resolved) without touching the file system.
    - isReadable()/isWritable()/isExecutable() use access(2), i.e. the real
      rather than the effective user ID.
*/

namespace {

qint64 toMs(const timespec &ts) { return static_cast<qint64>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000; }

QString lastComponent(const QString &path) {
    const qsizetype slash = path.lastIndexOf(QChar(u'/'));
    return slash < 0 ? path : path.mid(slash + 1);
}

QString emptyString() { return QString::fromUtf8(""); }

} // namespace

/*!
    \class QFileInfo
    \brief The QFileInfo class provides information about a file's name, location, type and metadata.

    A QFileInfo holds a path, relative or absolute, and queries the file
    system for information about it. A path that starts with a colon, such
    as \c{:/icons/open.png}, refers to a \l{The Nt Resource System}{resource}:
    resources are read-only, have no owner, and record only their
    modification time. The results are cached until refresh()
    is called or the path changes; use setCaching() to turn caching off.

    \code
    QFileInfo fi("/usr/lib/libz.so.1.3");
    fi.suffix();         // "3"
    fi.completeSuffix(); // "so.1.3"
    fi.baseName();       // "libz"
    \endcode

    \differences
    Permissions are reported as QFileInfo::Permissions, with the same values
    as Qt's \c QFileDevice::Permission, because QFileDevice has no permission
    enum yet. fileTime() is a template over the QFileDevice::FileTime enum,
    because \c qfileinfo.h cannot include \c qfile.h. isShortcut(),
    isAlias(), isJunction() and isBundle() are always \c false. isReadable(),
    isWritable() and isExecutable() use \c access(2), which tests against the
    real rather than the effective user ID. absoluteFilePath() and
    absolutePath() are lexically cleaned without touching the file system.
    \enddifferences

    \sa QDir, QFile
*/

/*!
    \enum QFileInfo::Permission

    Describes the permissions of a file. The owner and user values are
    identical and are both set from the owner permission bits.

    \value ReadOwner The file is readable by the owner.
    \value WriteOwner The file is writable by the owner.
    \value ExeOwner The file is executable by the owner.
    \value ReadUser The file is readable by the user.
    \value WriteUser The file is writable by the user.
    \value ExeUser The file is executable by the user.
    \value ReadGroup The file is readable by the group.
    \value WriteGroup The file is writable by the group.
    \value ExeGroup The file is executable by the group.
    \value ReadOther The file is readable by anyone.
    \value WriteOther The file is writable by anyone.
    \value ExeOther The file is executable by anyone.

    \sa permissions()
*/

/*!
    \typedef QFileInfo::Permissions

    A QFlags of Permission values.
*/

/*!
    Constructs a QFileInfo for the file that \a file refers to.

    \sa setFile()
*/
QFileInfo::QFileInfo(const QFile &file) : m_file(file.fileName()) {}

/*!
    Constructs a QFileInfo for \a file, interpreted relative to \a dir. If
    \a file is absolute, \a dir is ignored.

    \sa setFile()
*/
QFileInfo::QFileInfo(const QDir &dir, const QString &file) { setFile(dir, file); }

/*!
    \overload

    Sets the file to the one that \a file refers to.
*/
void QFileInfo::setFile(const QFile &file) { setFile(file.fileName()); }

/*!
    \overload

    Sets the file to \a file, interpreted relative to \a dir. If \a file is
    absolute, \a dir is ignored.
*/
void QFileInfo::setFile(const QDir &dir, const QString &file) {
    if (QDir::isAbsolutePath(file))
        setFile(file);
    else
        setFile(dir.filePath(file));
}

/*!
    Makes the path absolute. Returns \c true if the path is absolute
    afterwards; otherwise returns \c false.

    \sa isAbsolute(), absoluteFilePath()
*/
bool QFileInfo::makeAbsolute() {
    if (isAbsolute() || m_file.isEmpty())
        return isAbsolute();
    setFile(absoluteFilePath());
    return true;
}

const QFileInfo::Stat &QFileInfo::info() const {
    const QString file = ntResolveSearchPath(m_file);
    if (m_cached && m_caching)
        return m_stat;
    m_stat = Stat();
    if (NtResource::isResourcePath(file)) {
        const NtResource::Entry e = NtResource::find(file);
        m_stat.resource = true;
        m_stat.exists = e.exists;
        m_stat.mode = static_cast<unsigned>((e.dir ? S_IFDIR : S_IFREG) | 0444);
        m_stat.size = e.uncompressedSize;
        m_stat.mtimeMs = e.lastModified;
    } else if (!file.isEmpty()) {
        const QByteArray path = file.toUtf8();
        struct stat st;
        if (::stat(path.constData(), &st) == 0) {
            m_stat.exists = true;
            m_stat.mode = st.st_mode;
            m_stat.uid = st.st_uid;
            m_stat.gid = st.st_gid;
            m_stat.size = static_cast<qint64>(st.st_size);
            m_stat.mtimeMs = toMs(st.st_mtim);
            m_stat.atimeMs = toMs(st.st_atim);
            m_stat.ctimeMs = toMs(st.st_ctim);
        }
        struct stat lst;
        if (::lstat(path.constData(), &lst) == 0)
            m_stat.link = S_ISLNK(lst.st_mode);
    }
    m_cached = true;
    return m_stat;
}

/*!
    Returns \c true if the file exists; otherwise returns \c false. A
    dangling symbolic link does not exist.

    \sa isFile(), isDir()
*/
bool QFileInfo::exists() const { return info().exists; }

/*!
    \overload

    Returns \c true if the path \a file exists; otherwise returns \c false.
*/
bool QFileInfo::exists(const QString &file) { return QFileInfo(file).exists(); }

/*!
    Returns the absolute path of the file, including its name. The path is
    cleaned lexically, so \c . and \c .. components are resolved and symbolic
    links are not followed. Returns an empty string if the QFileInfo is empty.

    \sa canonicalFilePath(), filePath()
*/
QString QFileInfo::absoluteFilePath() const {
    const QString resolved = ntResolveSearchPath(m_file);
    if (resolved != m_file)
        return QFileInfo(resolved).absoluteFilePath();
    if (m_file.isEmpty())
        return QString();
    if (isAbsolute())
        return QDir::cleanPath(m_file);
    return QDir::cleanPath(QDir::currentPath() + QChar(u'/') + m_file);
}

/*!
    Returns the canonical path of the file, including its name, with symbolic
    links and redundant components resolved. Returns an empty string if the
    file does not exist.

    \sa absoluteFilePath()
*/
QString QFileInfo::canonicalFilePath() const {
    const QString file = ntResolveSearchPath(m_file);
    if (file.isEmpty())
        return QString();
    if (NtResource::isResourcePath(file))
        return exists() ? absoluteFilePath() : QString();
    char buf[PATH_MAX];
    if (!::realpath(file.toUtf8().constData(), buf))
        return QString();
    return QString::fromUtf8(buf);
}

/*!
    Returns the file name, without its directory path. For a name that uses
    a \l{QDir::setSearchPaths()}{search path} prefix, it is the name of the
    file the prefix resolves to.

    \sa path(), baseName(), suffix()
*/
QString QFileInfo::fileName() const { return lastComponent(ntResolveSearchPath(m_file)); }

/*!
    Returns the file name without its directory path and without the last
    suffix. For \c archive.tar.gz this is \c archive.tar.

    \sa baseName(), suffix()
*/
QString QFileInfo::completeBaseName() const {
    const QString name = fileName();
    const qsizetype dot = name.lastIndexOf(QChar(u'.'));
    return dot < 0 ? name : name.left(dot);
}

/*!
    Returns the file name without its directory path, up to the first dot.
    For \c archive.tar.gz this is \c archive.

    \sa completeBaseName(), completeSuffix()
*/
QString QFileInfo::baseName() const {
    const QString name = fileName();
    const qsizetype dot = name.indexOf(QChar(u'.'));
    return dot < 0 ? name : name.left(dot);
}

/*!
    Returns the part of the file name after the last dot, or an empty string
    if there is no dot. For \c archive.tar.gz this is \c gz.

    \sa completeSuffix(), completeBaseName()
*/
QString QFileInfo::suffix() const {
    const QString name = fileName();
    const qsizetype dot = name.lastIndexOf(QChar(u'.'));
    return dot < 0 ? emptyString() : name.mid(dot + 1);
}

/*!
    Returns the part of the file name after the first dot, or an empty string
    if there is no dot. For \c archive.tar.gz this is \c tar.gz.

    \sa suffix(), baseName()
*/
QString QFileInfo::completeSuffix() const {
    const QString name = fileName();
    const qsizetype dot = name.indexOf(QChar(u'.'));
    return dot < 0 ? emptyString() : name.mid(dot + 1);
}

/*!
    Returns the path of the file's directory, without the file name. The path
    is relative if the file's path is relative, and is \c . if it contains no
    directory part.

    \sa absolutePath(), fileName(), dir()
*/
QString QFileInfo::path() const {
    const qsizetype slash = m_file.lastIndexOf(QChar(u'/'));
    if (slash < 0)
        return QString::fromUtf8(".");
    QString dir = m_file.left(slash);
    while (dir.endsWith(QChar(u'/')))
        dir.chop(1);
    return dir.isEmpty() ? QString::fromUtf8("/") : dir;
}

/*!
    Returns the absolute path of the file's directory, without the file name.
    The path is cleaned lexically.

    \sa path(), canonicalPath(), absoluteDir()
*/
QString QFileInfo::absolutePath() const {
    QString abs = absoluteFilePath();
    if (abs.isEmpty())
        return QString::fromUtf8(".");
    if (m_file.endsWith(QChar(u'/')))
        return abs;
    const qsizetype slash = abs.lastIndexOf(QChar(u'/'));
    return slash <= 0 ? QString::fromUtf8("/") : abs.left(slash);
}

/*!
    Returns the canonical path of the file's directory, without the file name.
    Returns an empty string if the file does not exist.

    \sa canonicalFilePath(), absolutePath()
*/
QString QFileInfo::canonicalPath() const {
    const QString canon = canonicalFilePath();
    if (canon.isEmpty())
        return QString();
    const qsizetype slash = canon.lastIndexOf(QChar(u'/'));
    return slash <= 0 ? QString::fromUtf8("/") : canon.left(slash);
}

/*!
    Returns the file's directory as a QDir, built from path().

    \sa absoluteDir(), path()
*/
QDir QFileInfo::dir() const { return QDir(path()); }

/*!
    Returns the file's directory as a QDir, built from absolutePath().

    \sa dir(), absolutePath()
*/
QDir QFileInfo::absoluteDir() const { return QDir(absolutePath()); }

/*!
    Returns the absolute path that the symbolic link points to, or an empty
    string if the file is not a symbolic link. The target does not have to
    exist.

    \sa isSymLink()
*/
QString QFileInfo::symLinkTarget() const {
    const QString file = ntResolveSearchPath(m_file);
    if (file.isEmpty())
        return QString();
    std::vector<char> buf(256);
    const QByteArray path = file.toUtf8();
    for (;;) {
        const ssize_t n = ::readlink(path.constData(), buf.data(), buf.size());
        if (n < 0)
            return QString();
        if (static_cast<std::size_t>(n) < buf.size()) {
            const QString target = QString::fromUtf8(buf.data(), static_cast<qsizetype>(n));
            if (QDir::isAbsolutePath(target))
                return QDir::cleanPath(target);
            return QDir::cleanPath(absolutePath() + QChar(u'/') + target);
        }
        buf.resize(buf.size() * 2);
    }
}

/*!
    Returns the name of the file's owner, or an empty string if the file does
    not exist or the user ID has no name.

    \sa ownerId(), group()
*/
QString QFileInfo::owner() const {
    const Stat &s = info();
    if (!s.exists || s.resource)
        return QString();
    std::vector<char> buf(1024);
    passwd pw;
    passwd *res = nullptr;
    while (::getpwuid_r(s.uid, &pw, buf.data(), buf.size(), &res) == ERANGE && buf.size() < (1u << 20))
        buf.resize(buf.size() * 2);
    return res ? QString::fromUtf8(res->pw_name) : QString();
}

/*!
    Returns the name of the file's group, or an empty string if the file does
    not exist or the group ID has no name.

    \sa groupId(), owner()
*/
QString QFileInfo::group() const {
    const Stat &s = info();
    if (!s.exists || s.resource)
        return QString();
    std::vector<char> buf(1024);
    ::group gr;
    ::group *res = nullptr;
    while (::getgrgid_r(s.gid, &gr, buf.data(), buf.size(), &res) == ERANGE && buf.size() < (1u << 20))
        buf.resize(buf.size() * 2);
    return res ? QString::fromUtf8(res->gr_name) : QString();
}

/*!
    Returns the numeric ID of the file's owner, or \c{(uint)-2} if the file
    does not exist.

    \sa owner(), groupId()
*/
uint QFileInfo::ownerId() const {
    const Stat &s = info();
    return s.exists && !s.resource ? s.uid : static_cast<uint>(-2);
}

/*!
    Returns the numeric ID of the file's group, or \c{(uint)-2} if the file
    does not exist.

    \sa group(), ownerId()
*/
uint QFileInfo::groupId() const {
    const Stat &s = info();
    return s.exists && !s.resource ? s.gid : static_cast<uint>(-2);
}

/*!
    Returns the permissions of the file, or an empty set if the file does not
    exist.

    \sa permission(), isReadable()
*/
QFileInfo::Permissions QFileInfo::permissions() const {
    const Stat &s = info();
    if (!s.exists)
        return Permissions();
    // Qt reports the owner bits twice: as the Owner and the User permissions.
    Permissions p;
    if (s.mode & S_IRUSR) p |= ReadOwner | ReadUser;
    if (s.mode & S_IWUSR) p |= WriteOwner | WriteUser;
    if (s.mode & S_IXUSR) p |= ExeOwner | ExeUser;
    if (s.mode & S_IRGRP) p |= ReadGroup;
    if (s.mode & S_IWGRP) p |= WriteGroup;
    if (s.mode & S_IXGRP) p |= ExeGroup;
    if (s.mode & S_IROTH) p |= ReadOther;
    if (s.mode & S_IWOTH) p |= WriteOther;
    if (s.mode & S_IXOTH) p |= ExeOther;
    return p;
}

/*!
    Returns \c true if the process may read the file; otherwise returns
    \c false. The check uses \c access(2).

    \sa isWritable(), isExecutable(), permissions()
*/
bool QFileInfo::isReadable() const {
    const QString file = ntResolveSearchPath(m_file);
    if (NtResource::isResourcePath(file))
        return exists();
    return !file.isEmpty() && ::access(file.toUtf8().constData(), R_OK) == 0;
}

/*!
    Returns \c true if the process may write the file; otherwise returns
    \c false. The check uses \c access(2).

    \sa isReadable(), isExecutable(), permissions()
*/
bool QFileInfo::isWritable() const {
    const QString file = ntResolveSearchPath(m_file);
    return !file.isEmpty() && !NtResource::isResourcePath(file) && ::access(file.toUtf8().constData(), W_OK) == 0;
}

/*!
    Returns \c true if the process may execute the file, or search it if it
    is a directory; otherwise returns \c false. The check uses \c access(2).

    \sa isReadable(), isWritable(), permissions()
*/
bool QFileInfo::isExecutable() const {
    const QString file = ntResolveSearchPath(m_file);
    return !file.isEmpty() && !NtResource::isResourcePath(file) && ::access(file.toUtf8().constData(), X_OK) == 0;
}

/*!
    Returns \c true if the file name starts with a dot; otherwise returns
    \c false.
*/
bool QFileInfo::isHidden() const { return fileName().startsWith(QChar(u'.')); }

/*!
    Returns \c true if the path refers to a regular file, following symbolic
    links; otherwise returns \c false.

    \sa isDir(), isSymLink()
*/
bool QFileInfo::isFile() const {
    const Stat &s = info();
    return s.exists && S_ISREG(s.mode);
}

/*!
    Returns \c true if the path refers to a directory, following symbolic
    links; otherwise returns \c false.

    \sa isFile(), isSymLink()
*/
bool QFileInfo::isDir() const {
    const Stat &s = info();
    return s.exists && S_ISDIR(s.mode);
}

/*!
    Returns \c true if the path is a symbolic link, even a dangling one;
    otherwise returns \c false.

    \sa symLinkTarget()
*/
bool QFileInfo::isSymLink() const { return info().link; }

/*!
    \since 5.14
    Returns \c true if the path refers to a file of the native file system;
    returns \c false for an empty path and for a resource (a path that
    starts with \c{:/}, or a \l{QDir::setSearchPaths()}{search path} that
    leads to one).

    \sa QTemporaryFile::createNativeFile()
*/
bool QFileInfo::isNativePath() const {
    const QString file = ntResolveSearchPath(m_file);
    return !file.isEmpty() && !NtResource::isResourcePath(file);
}

/*!
    \since 5.10
    Returns when the file was created, or an invalid QDateTime if the file
    system does not record it (Linux reports it through \c statx(2)).

    \sa lastModified(), metadataChangeTime()
*/
QDateTime QFileInfo::birthTime() const {
    return m_file.isEmpty() ? QDateTime() : QFile(m_file).fileTime(QFileDevice::FileBirthTime);
}

QDateTime QFileInfo::fileTimeByIndex(int time) const {
    switch (static_cast<QFileDevice::FileTime>(time)) {
    case QFileDevice::FileAccessTime:
        return lastRead();
    case QFileDevice::FileBirthTime:
        return birthTime();
    case QFileDevice::FileMetadataChangeTime:
        return metadataChangeTime();
    case QFileDevice::FileModificationTime:
        return lastModified();
    }
    return QDateTime();
}

/*!
    Returns the size of the file in bytes, or 0 if it does not exist.
*/
qint64 QFileInfo::size() const {
    const Stat &s = info();
    return s.exists ? s.size : 0;
}

/*!
    Returns the time the file's contents were last modified, with millisecond
    resolution. Returns an invalid QDateTime if the file does not exist.

    \sa lastRead(), metadataChangeTime()
*/
QDateTime QFileInfo::lastModified() const {
    const Stat &s = info();
    return s.exists && (s.mtimeMs != 0 || !s.resource) ? QDateTime::fromMSecsSinceEpoch(s.mtimeMs) : QDateTime();
}

/*!
    Returns the time the file was last accessed, with millisecond resolution.
    Returns an invalid QDateTime if the file does not exist.

    \sa lastModified(), metadataChangeTime()
*/
QDateTime QFileInfo::lastRead() const {
    const Stat &s = info();
    return s.exists && !s.resource ? QDateTime::fromMSecsSinceEpoch(s.atimeMs) : QDateTime();
}

/*!
    Returns the time the file's metadata, such as its permissions, last
    changed, with millisecond resolution. Returns an invalid QDateTime if the
    file does not exist.

    \sa lastModified(), lastRead()
*/
QDateTime QFileInfo::metadataChangeTime() const {
    const Stat &s = info();
    return s.exists && !s.resource ? QDateTime::fromMSecsSinceEpoch(s.ctimeMs) : QDateTime();
}
