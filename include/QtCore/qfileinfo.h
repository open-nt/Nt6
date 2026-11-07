// MIT License - see LICENSE
#pragma once

#include <QtCore/qdatetime.h>
#include <QtCore/qflags.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

#include <utility>

class QDir;
class QFile;

class QFileInfo {
public:
    QFileInfo() = default;
    enum Permission {
        ReadOwner = 0x4000, WriteOwner = 0x2000, ExeOwner = 0x1000,
        ReadUser = 0x0400, WriteUser = 0x0200, ExeUser = 0x0100,
        ReadGroup = 0x0040, WriteGroup = 0x0020, ExeGroup = 0x0010,
        ReadOther = 0x0004, WriteOther = 0x0002, ExeOther = 0x0001
    };
    Q_DECLARE_FLAGS(Permissions, Permission)

    explicit QFileInfo(const QString &file) : m_file(file) {}
    explicit QFileInfo(const QFile &file);
    QFileInfo(const QDir &dir, const QString &file);

    void setFile(const QString &file) { m_file = file; m_cached = false; }
    void setFile(const QFile &file);
    void setFile(const QDir &dir, const QString &file);
    void refresh() { m_cached = false; }
    void setCaching(bool enable) { m_caching = enable; m_cached = false; }
    bool caching() const { return m_caching; }
    bool makeAbsolute();
    void swap(QFileInfo &other) noexcept { std::swap(m_file, other.m_file); std::swap(m_stat, other.m_stat); std::swap(m_cached, other.m_cached); std::swap(m_caching, other.m_caching); }
    bool exists() const;
    static bool exists(const QString &file);

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
    QString owner() const;
    QString group() const;
    uint ownerId() const;
    uint groupId() const;
    Permissions permissions() const;
    bool permission(Permissions permissions) const { return (this->permissions() & permissions) == permissions; }

    bool isReadable() const;
    bool isWritable() const;
    bool isExecutable() const;
    bool isHidden() const;
    bool isRelative() const { return !isAbsolute(); }
    bool isAbsolute() const { return m_file.startsWith(QChar(u'/')); }
    bool isFile() const;
    bool isDir() const;
    bool isSymLink() const;
    bool isRoot() const { return absoluteFilePath() == QString("/"); }
    qint64 size() const;
    QDateTime lastModified() const;
    QDateTime lastRead() const;
    QDateTime metadataChangeTime() const;

    friend bool operator==(const QFileInfo &a, const QFileInfo &b) { return a.absoluteFilePath() == b.absoluteFilePath(); }
    friend bool operator!=(const QFileInfo &a, const QFileInfo &b) { return !(a == b); }

private:
    struct Stat {
        bool exists = false;
        bool link = false;
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
