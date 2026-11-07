// MIT License - see LICENSE
#pragma once

#include <QtCore/qdatetime.h>
#include <QtCore/qflags.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

class QDir;
class QFile;

class QFileInfo {
public:
    QFileInfo() = default;
    explicit QFileInfo(const QString &file) : m_file(file) {}
    explicit QFileInfo(const QFile &file);
    QFileInfo(const QDir &dir, const QString &file);

    void setFile(const QString &file) { m_file = file; m_cached = false; }
    void refresh() { m_cached = false; }
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
    };
    const Stat &info() const;

    QString m_file;
    mutable Stat m_stat;
    mutable bool m_cached = false;
};

using QFileInfoList = QList<QFileInfo>;
