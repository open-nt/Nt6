// MIT License - see LICENSE
#pragma once

#include <QtCore/qfileinfo.h>
#include <QtCore/qflags.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

#include <utility>

class QDir {
public:
    enum Filter {
        Dirs = 0x001,
        Files = 0x002,
        Drives = 0x004,
        NoSymLinks = 0x008,
        AllEntries = Dirs | Files | Drives,
        TypeMask = 0x00f,
        Readable = 0x010,
        Writable = 0x020,
        Executable = 0x040,
        PermissionMask = 0x070,
        Modified = 0x080,
        Hidden = 0x100,
        System = 0x200,
        AccessMask = 0x3F0,
        AllDirs = 0x400,
        CaseSensitive = 0x800,
        NoDot = 0x2000,
        NoDotDot = 0x4000,
        NoDotAndDotDot = NoDot | NoDotDot,
        NoFilter = -1
    };
    Q_DECLARE_FLAGS(Filters, Filter)

    enum SortFlag {
        Name = 0x00,
        Time = 0x01,
        Size = 0x02,
        Unsorted = 0x03,
        SortByMask = 0x03,
        DirsFirst = 0x04,
        Reversed = 0x08,
        IgnoreCase = 0x10,
        DirsLast = 0x20,
        LocaleAware = 0x40,
        Type = 0x80,
        NoSort = -1
    };
    Q_DECLARE_FLAGS(SortFlags, SortFlag)

    explicit QDir(const QString &path = QString());
    QDir(const QString &path, const QString &nameFilter, SortFlags sort = SortFlags(SortFlag(Name | IgnoreCase)), Filters filter = AllEntries);

    void setPath(const QString &path);
    void swap(QDir &other) noexcept { std::swap(m_path, other.m_path); std::swap(m_nameFilters, other.m_nameFilters); std::swap(m_filter, other.m_filter); std::swap(m_sort, other.m_sort); }
    QString path() const { return m_path; }
    QString absolutePath() const;
    QString canonicalPath() const;
    QString dirName() const;
    QString filePath(const QString &fileName) const;
    QString absoluteFilePath(const QString &fileName) const;
    QString relativeFilePath(const QString &fileName) const;
    bool cd(const QString &dirName);
    bool cdUp();

    QStringList nameFilters() const { return m_nameFilters; }
    void setNameFilters(const QStringList &nameFilters) { m_nameFilters = nameFilters; }
    Filters filter() const { return m_filter; }
    void setFilter(Filters filter) { m_filter = filter; }
    SortFlags sorting() const { return m_sort; }
    void setSorting(SortFlags sort) { m_sort = sort; }

    qsizetype count() const { return entryList().size(); }
    bool isEmpty(Filters filters = Filters(Filter(AllEntries | NoDotAndDotDot))) const;
    QString operator[](qsizetype i) const { return entryList().at(i); }
    QStringList entryList(Filters filters = NoFilter, SortFlags sort = NoSort) const;
    QStringList entryList(const QStringList &nameFilters, Filters filters = NoFilter, SortFlags sort = NoSort) const;
    QFileInfoList entryInfoList(Filters filters = NoFilter, SortFlags sort = NoSort) const;
    QFileInfoList entryInfoList(const QStringList &nameFilters, Filters filters = NoFilter, SortFlags sort = NoSort) const;

    bool mkdir(const QString &dirName) const;
    bool rmdir(const QString &dirName) const;
    bool mkpath(const QString &dirPath) const;
    bool rmpath(const QString &dirPath) const;
    bool removeRecursively();
    bool remove(const QString &fileName);
    bool rename(const QString &oldName, const QString &newName);
    bool exists() const;
    bool exists(const QString &name) const;
    bool isReadable() const;
    bool isRoot() const { return absolutePath() == rootPath(); }
    bool isAbsolute() const { return isAbsolutePath(m_path); }
    bool isRelative() const { return !isAbsolute(); }
    bool makeAbsolute();
    void refresh() const {}

    static QString cleanPath(const QString &path);
    static bool isAbsolutePath(const QString &path) { return path.startsWith(QChar(u'/')); }
    static bool isRelativePath(const QString &path) { return !isAbsolutePath(path); }
    static QChar separator() noexcept { return QChar(u'/'); }
    static QChar listSeparator() noexcept { return QChar(u':'); }
    static QString toNativeSeparators(const QString &path) { return path; }
    static QString fromNativeSeparators(const QString &path) { return path; }
    static bool setCurrent(const QString &path);
    static QDir current() { return QDir(currentPath()); }
    static QString currentPath();
    static QDir home() { return QDir(homePath()); }
    static QString homePath();
    static QDir temp() { return QDir(tempPath()); }
    static QString tempPath();
    static QDir root() { return QDir(rootPath()); }
    static QString rootPath() { return QString("/"); }
    static QFileInfoList drives();
    static bool match(const QString &filter, const QString &fileName);
    static bool match(const QStringList &filters, const QString &fileName);

    friend bool operator==(const QDir &a, const QDir &b) { return a.absolutePath() == b.absolutePath(); }
    friend bool operator!=(const QDir &a, const QDir &b) { return !(a == b); }

private:
    QString m_path;
    QStringList m_nameFilters = QStringList(QString("*"));
    Filters m_filter = AllEntries;
    SortFlags m_sort = SortFlags(SortFlag(Name | IgnoreCase));
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QDir::Filters)
Q_DECLARE_OPERATORS_FOR_FLAGS(QDir::SortFlags)
