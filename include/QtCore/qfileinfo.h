// MIT License - see LICENSE
#pragma once

#include <QtCore/qdatetime.h>
#include <QtCore/qflags.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

#include <type_traits>
#include <utility>

// The std::filesystem overloads of QFileInfo, QDir and QFile. Zephyr has no file system library.
#if __has_include(<filesystem>) && !defined(__ZEPHYR__)
#include <filesystem>
#define NT_HAS_STD_FILESYSTEM 1

namespace QtPrivate {
// True for std::filesystem::path only, so that the overloads that take one do not catch strings.
template <typename T> using IsFilesystemPath = std::is_same<T, std::filesystem::path>;
template <typename T> using ForFilesystemPath = std::enable_if_t<IsFilesystemPath<T>::value, bool>;

inline std::filesystem::path toFilesystemPath(const QString &path) {
    return std::filesystem::path(path.toStdU16String());
}
inline QString fromFilesystemPath(const std::filesystem::path &path) {
    return QString::fromStdU16String(path.u16string());
}
} // namespace QtPrivate
#endif

class QDir;
class QFile;

class QFileInfo {
public:
    /*!
        Constructs an empty QFileInfo. It refers to no file, and exists()
        returns \c false.
    */
    QFileInfo() = default;
    enum Permission {
        ReadOwner = 0x4000, WriteOwner = 0x2000, ExeOwner = 0x1000,
        ReadUser = 0x0400, WriteUser = 0x0200, ExeUser = 0x0100,
        ReadGroup = 0x0040, WriteGroup = 0x0020, ExeGroup = 0x0010,
        ReadOther = 0x0004, WriteOther = 0x0002, ExeOther = 0x0001
    };
    Q_DECLARE_FLAGS(Permissions, Permission)

    /*!
        Constructs a QFileInfo for the path \a file. The file does not have to
        exist.

        \sa setFile()
    */
    explicit QFileInfo(const QString &file) : m_file(file) {}
#ifdef NT_HAS_STD_FILESYSTEM
    /*!
        \since 6.0
        Constructs a QFileInfo for the file at \a file.
    */
    template <typename T, QtPrivate::ForFilesystemPath<T> = true>
    explicit QFileInfo(const T &file) : m_file(QtPrivate::fromFilesystemPath(file)) {}
#endif
    explicit QFileInfo(const QFile &file);
    QFileInfo(const QDir &dir, const QString &file);

    /*!
        Sets the file to the path \a file and clears the cached information.

        \sa refresh()
    */
    void setFile(const QString &file) { m_file = file; m_cached = false; }
#ifdef NT_HAS_STD_FILESYSTEM
    /*!
        \since 6.0
        \overload
        Makes the QFileInfo refer to \a file.
    */
    template <typename T, QtPrivate::ForFilesystemPath<T> = true> void setFile(const T &file) {
        setFile(QtPrivate::fromFilesystemPath(file));
    }
#endif
    void setFile(const QFile &file);
    void setFile(const QDir &dir, const QString &file);
    /*!
        Clears the cached information, so the next query reads the file system
        again.

        \sa setCaching()
    */
    void refresh() { m_cached = false; }
    void stat();
    /*!
        Enables or disables caching of file information according to
        \a enable, and clears what is cached. Caching is on by default. With
        caching off, every query reads the file system.

        \sa caching(), refresh()
    */
    void setCaching(bool enable) { m_caching = enable; m_cached = false; }
    /*!
        Returns \c true if file information is cached; otherwise returns
        \c false.

        \sa setCaching()
    */
    bool caching() const { return m_caching; }
    bool makeAbsolute();
    /*!
        Exchanges this QFileInfo with \a other.
    */
    void swap(QFileInfo &other) noexcept { std::swap(m_file, other.m_file); std::swap(m_stat, other.m_stat); std::swap(m_cached, other.m_cached); std::swap(m_caching, other.m_caching); }
    bool exists() const;
    static bool exists(const QString &file);

    /*!
        Returns the path of the file as it was given, relative or absolute,
        including the file name.

        \sa absoluteFilePath(), fileName()
    */
    QString filePath() const { return m_file; }
    QString absoluteFilePath() const;
    QString canonicalFilePath() const;
    QString fileName() const;
    QString baseName() const;
    QString completeBaseName() const;
    QString suffix() const;
    QString completeSuffix() const;
    QString path() const;
    QString absolutePath() const;
    QString canonicalPath() const;
    QDir dir() const;
    QDir absoluteDir() const;
    QString symLinkTarget() const;
    QString readSymLink() const;
#ifdef NT_HAS_STD_FILESYSTEM
    /*!
        \since 6.0
        Returns filePath() as a \c std::filesystem::path.
    */
    std::filesystem::path filesystemFilePath() const { return QtPrivate::toFilesystemPath(filePath()); }
    /*!
        \since 6.0
        Returns absoluteFilePath() as a \c std::filesystem::path.
    */
    std::filesystem::path filesystemAbsoluteFilePath() const { return QtPrivate::toFilesystemPath(absoluteFilePath()); }
    /*!
        \since 6.0
        Returns canonicalFilePath() as a \c std::filesystem::path.
    */
    std::filesystem::path filesystemCanonicalFilePath() const {
        return QtPrivate::toFilesystemPath(canonicalFilePath());
    }
    /*!
        \since 6.0
        Returns path() as a \c std::filesystem::path.
    */
    std::filesystem::path filesystemPath() const { return QtPrivate::toFilesystemPath(path()); }
    /*!
        \since 6.0
        Returns absolutePath() as a \c std::filesystem::path.
    */
    std::filesystem::path filesystemAbsolutePath() const { return QtPrivate::toFilesystemPath(absolutePath()); }
    /*!
        \since 6.0
        Returns canonicalPath() as a \c std::filesystem::path.
    */
    std::filesystem::path filesystemCanonicalPath() const { return QtPrivate::toFilesystemPath(canonicalPath()); }
    /*!
        \since 6.0
        Returns symLinkTarget() as a \c std::filesystem::path.
    */
    std::filesystem::path filesystemSymLinkTarget() const { return QtPrivate::toFilesystemPath(symLinkTarget()); }
    /*!
        \since 6.6
        Returns readSymLink() as a \c std::filesystem::path.
    */
    std::filesystem::path filesystemReadSymLink() const { return QtPrivate::toFilesystemPath(readSymLink()); }
#endif
    QString owner() const;
    QString group() const;
    uint ownerId() const;
    uint groupId() const;
    Permissions permissions() const;
    /*!
        Returns \c true if the file has all of the given \a permissions;
        otherwise returns \c false.

        \sa permissions()
    */
    bool permission(Permissions permissions) const { return (this->permissions() & permissions) == permissions; }

    bool isReadable() const;
    bool isWritable() const;
    bool isExecutable() const;
    bool isHidden() const;
    /*!
        Returns \c true if the file's path is relative; otherwise returns
        \c false.

        \sa isAbsolute(), makeAbsolute()
    */
    bool isRelative() const { return !isAbsolute(); }
    /*!
        Returns \c true if the file's path is absolute, which means it starts
        with a slash, or with a colon for a resource; otherwise returns
        \c false.

        \sa isRelative()
    */
    bool isAbsolute() const { return m_file.startsWith(QChar(u'/')) || m_file.startsWith(QChar(u':')); }
    bool isFile() const;
    bool isDir() const;
    bool isOther() const;
    bool isSymLink() const;
    /*!
        \since 6.4
        Returns \c true if the file is a symbolic link. Same as isSymLink() on
        POSIX systems.
    */
    bool isSymbolicLink() const { return isSymLink(); }
    /*!
        \since 6.4
        Returns \c false: Windows shortcuts (\c .lnk files) are ordinary
        files on the platforms Nt supports.
    */
    bool isShortcut() const { return false; }
    /*!
        \since 6.4
        Returns \c false: macOS aliases are not recognized.
    */
    bool isAlias() const { return false; }
    /*!
        \since 5.15
        Returns \c false: NTFS junctions exist only on Windows.
    */
    bool isJunction() const { return false; }
    /*!
        Returns \c false: macOS bundles are not recognized.
    */
    bool isBundle() const { return false; }
    bool isNativePath() const;
    /*!
        Returns \c true if the file is the root directory, or the root of the
        resources (\c{:/}); otherwise returns \c false.
    */
    bool isRoot() const { const QString a = absoluteFilePath(); return a == QString("/") || a == QString(":/"); }
    qint64 size() const;
    QDateTime lastModified() const;
    QDateTime lastRead() const;
    QDateTime metadataChangeTime() const;
    QDateTime birthTime() const;
    /*!
        \since 6.6
        Returns the time stamp \a time of the file, one of the
        QFileDevice::FileTime values: the same as lastRead(), birthTime(),
        metadataChangeTime() or lastModified().

        \code
        QDateTime changed = QFileInfo(path).fileTime(QFile::FileModificationTime);
        \endcode
    */
    template <typename FileTime, typename = std::enable_if_t<std::is_enum_v<FileTime>>>
    QDateTime fileTime(FileTime time) const {
        return fileTimeByIndex(static_cast<int>(time));
    }

    friend bool operator==(const QFileInfo &a, const QFileInfo &b) { return a.absoluteFilePath() == b.absoluteFilePath(); }
    friend bool operator!=(const QFileInfo &a, const QFileInfo &b) { return !(a == b); }

private:
    QDateTime fileTimeByIndex(int time) const;
    struct Stat {
        bool exists = false;
        bool link = false;
        bool resource = false;
        unsigned mode = 0;
        unsigned uid = 0;
        unsigned gid = 0;
        qint64 size = 0;
        qint64 mtimeMs = 0;
        qint64 atimeMs = 0;
        qint64 ctimeMs = 0;
    };
    const Stat &info() const;

    QString m_file;
    mutable Stat m_stat;
    mutable bool m_cached = false;
    bool m_caching = true;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QFileInfo::Permissions)

using QFileInfoList = QList<QFileInfo>;
