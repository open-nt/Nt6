// MIT License - see LICENSE

#include <QtCore/qfileinfo.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>

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
    - birthTime() is not provided; isReadable()/isWritable()/isExecutable()
      use access(2), i.e. the real rather than the effective user ID.
*/

namespace {

qint64 toMs(const timespec &ts) { return static_cast<qint64>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000; }

QString lastComponent(const QString &path) {
    const qsizetype slash = path.lastIndexOf(QChar(u'/'));
    return slash < 0 ? path : path.mid(slash + 1);
}

QString emptyString() { return QString::fromUtf8(""); }

} // namespace

QFileInfo::QFileInfo(const QFile &file) : m_file(file.fileName()) {}

QFileInfo::QFileInfo(const QDir &dir, const QString &file) { setFile(dir, file); }

void QFileInfo::setFile(const QFile &file) { setFile(file.fileName()); }

void QFileInfo::setFile(const QDir &dir, const QString &file) {
    if (QDir::isAbsolutePath(file))
        setFile(file);
    else
        setFile(dir.filePath(file));
}

bool QFileInfo::makeAbsolute() {
    if (isAbsolute() || m_file.isEmpty())
        return isAbsolute();
    setFile(absoluteFilePath());
    return true;
}

const QFileInfo::Stat &QFileInfo::info() const {
    if (m_cached && m_caching)
        return m_stat;
    m_stat = Stat();
    if (!m_file.isEmpty()) {
        const QByteArray path = m_file.toUtf8();
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

bool QFileInfo::exists() const { return info().exists; }

bool QFileInfo::exists(const QString &file) { return QFileInfo(file).exists(); }

QString QFileInfo::absoluteFilePath() const {
    if (m_file.isEmpty())
        return QString();
    if (isAbsolute())
        return QDir::cleanPath(m_file);
    return QDir::cleanPath(QDir::currentPath() + QChar(u'/') + m_file);
}

QString QFileInfo::canonicalFilePath() const {
    if (m_file.isEmpty())
        return QString();
    char buf[PATH_MAX];
    if (!::realpath(m_file.toUtf8().constData(), buf))
        return QString();
    return QString::fromUtf8(buf);
}

QString QFileInfo::fileName() const { return lastComponent(m_file); }

QString QFileInfo::completeBaseName() const {
    const QString name = fileName();
    const qsizetype dot = name.lastIndexOf(QChar(u'.'));
    return dot < 0 ? name : name.left(dot);
}

QString QFileInfo::baseName() const {
    const QString name = fileName();
    const qsizetype dot = name.indexOf(QChar(u'.'));
    return dot < 0 ? name : name.left(dot);
}

QString QFileInfo::suffix() const {
    const QString name = fileName();
    const qsizetype dot = name.lastIndexOf(QChar(u'.'));
    return dot < 0 ? emptyString() : name.mid(dot + 1);
}

QString QFileInfo::completeSuffix() const {
    const QString name = fileName();
    const qsizetype dot = name.indexOf(QChar(u'.'));
    return dot < 0 ? emptyString() : name.mid(dot + 1);
}

QString QFileInfo::path() const {
    const qsizetype slash = m_file.lastIndexOf(QChar(u'/'));
    if (slash < 0)
        return QString::fromUtf8(".");
    QString dir = m_file.left(slash);
    while (dir.endsWith(QChar(u'/')))
        dir.chop(1);
    return dir.isEmpty() ? QString::fromUtf8("/") : dir;
}

QString QFileInfo::absolutePath() const {
    const QString abs = absoluteFilePath();
    if (abs.isEmpty())
        return QString::fromUtf8(".");
    if (m_file.endsWith(QChar(u'/')))
        return abs;
    const qsizetype slash = abs.lastIndexOf(QChar(u'/'));
    return slash <= 0 ? QString::fromUtf8("/") : abs.left(slash);
}

QString QFileInfo::canonicalPath() const {
    const QString canon = canonicalFilePath();
    if (canon.isEmpty())
        return QString();
    const qsizetype slash = canon.lastIndexOf(QChar(u'/'));
    return slash <= 0 ? QString::fromUtf8("/") : canon.left(slash);
}

QDir QFileInfo::dir() const { return QDir(path()); }

QDir QFileInfo::absoluteDir() const { return QDir(absolutePath()); }

QString QFileInfo::symLinkTarget() const {
    if (m_file.isEmpty())
        return QString();
    std::vector<char> buf(256);
    const QByteArray path = m_file.toUtf8();
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

QString QFileInfo::owner() const {
    const Stat &s = info();
    if (!s.exists)
        return QString();
    std::vector<char> buf(1024);
    passwd pw;
    passwd *res = nullptr;
    while (::getpwuid_r(s.uid, &pw, buf.data(), buf.size(), &res) == ERANGE && buf.size() < (1u << 20))
        buf.resize(buf.size() * 2);
    return res ? QString::fromUtf8(res->pw_name) : QString();
}

QString QFileInfo::group() const {
    const Stat &s = info();
    if (!s.exists)
        return QString();
    std::vector<char> buf(1024);
    ::group gr;
    ::group *res = nullptr;
    while (::getgrgid_r(s.gid, &gr, buf.data(), buf.size(), &res) == ERANGE && buf.size() < (1u << 20))
        buf.resize(buf.size() * 2);
    return res ? QString::fromUtf8(res->gr_name) : QString();
}

uint QFileInfo::ownerId() const {
    const Stat &s = info();
    return s.exists ? s.uid : static_cast<uint>(-2);
}

uint QFileInfo::groupId() const {
    const Stat &s = info();
    return s.exists ? s.gid : static_cast<uint>(-2);
}

QFileInfo::Permissions QFileInfo::permissions() const {
    const Stat &s = info();
    if (!s.exists)
        return Permissions();
    // Qt reports the owner bits twice: as the Owner and the User permissions.
    int p = 0;
    if (s.mode & S_IRUSR) p |= ReadOwner | ReadUser;
    if (s.mode & S_IWUSR) p |= WriteOwner | WriteUser;
    if (s.mode & S_IXUSR) p |= ExeOwner | ExeUser;
    if (s.mode & S_IRGRP) p |= ReadGroup;
    if (s.mode & S_IWGRP) p |= WriteGroup;
    if (s.mode & S_IXGRP) p |= ExeGroup;
    if (s.mode & S_IROTH) p |= ReadOther;
    if (s.mode & S_IWOTH) p |= WriteOther;
    if (s.mode & S_IXOTH) p |= ExeOther;
    return Permissions(static_cast<Permission>(p));
}

bool QFileInfo::isReadable() const { return !m_file.isEmpty() && ::access(m_file.toUtf8().constData(), R_OK) == 0; }

bool QFileInfo::isWritable() const { return !m_file.isEmpty() && ::access(m_file.toUtf8().constData(), W_OK) == 0; }

bool QFileInfo::isExecutable() const { return !m_file.isEmpty() && ::access(m_file.toUtf8().constData(), X_OK) == 0; }

bool QFileInfo::isHidden() const { return fileName().startsWith(QChar(u'.')); }

bool QFileInfo::isFile() const {
    const Stat &s = info();
    return s.exists && S_ISREG(s.mode);
}

bool QFileInfo::isDir() const {
    const Stat &s = info();
    return s.exists && S_ISDIR(s.mode);
}

bool QFileInfo::isSymLink() const { return info().link; }

qint64 QFileInfo::size() const {
    const Stat &s = info();
    return s.exists ? s.size : 0;
}

QDateTime QFileInfo::lastModified() const {
    const Stat &s = info();
    return s.exists ? QDateTime::fromMSecsSinceEpoch(s.mtimeMs) : QDateTime();
}

QDateTime QFileInfo::lastRead() const {
    const Stat &s = info();
    return s.exists ? QDateTime::fromMSecsSinceEpoch(s.atimeMs) : QDateTime();
}

QDateTime QFileInfo::metadataChangeTime() const {
    const Stat &s = info();
    return s.exists ? QDateTime::fromMSecsSinceEpoch(s.ctimeMs) : QDateTime();
}
