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
    /*!
        Exchanges this directory with \a other.
    */
    void swap(QDir &other) noexcept { std::swap(m_path, other.m_path); std::swap(m_nameFilters, other.m_nameFilters); std::swap(m_filter, other.m_filter); std::swap(m_sort, other.m_sort); }
    /*!
        Returns the path of the directory, as set with setPath(). It may be
        relative.

        \sa absolutePath(), dirName()
    */
    QString path() const { return m_path; }
    QString absolutePath() const;
    QString canonicalPath() const;
    QString dirName() const;
    QString filePath(const QString &fileName) const;
    QString absoluteFilePath(const QString &fileName) const;
    QString relativeFilePath(const QString &fileName) const;
    bool cd(const QString &dirName);
    bool cdUp();

    /*!
        Returns the name filters used by entryList() and entryInfoList(). The
        default is a single \c * pattern.

        \sa setNameFilters()
    */
    QStringList nameFilters() const { return m_nameFilters; }
    /*!
        Sets the name filters used by entryList() and entryInfoList() to
        \a nameFilters.

        \sa nameFilters()
    */
    void setNameFilters(const QStringList &nameFilters) { m_nameFilters = nameFilters; }
    /*!
        Returns the entry filter used by entryList() and entryInfoList().

        \sa setFilter()
    */
    Filters filter() const { return m_filter; }
    /*!
        Sets the entry filter used by entryList() and entryInfoList() to
        \a filter.

        \sa filter()
    */
    void setFilter(Filters filter) { m_filter = filter; }
    /*!
        Returns the sort order used by entryList() and entryInfoList().

        \sa setSorting()
    */
    SortFlags sorting() const { return m_sort; }
    /*!
        Sets the sort order used by entryList() and entryInfoList() to
        \a sort.

        \sa sorting()
    */
    void setSorting(SortFlags sort) { m_sort = sort; }

    /*!
        Returns the number of entries that entryList() returns.
    */
    qsizetype count() const { return entryList().size(); }
    bool isEmpty(Filters filters = Filters(Filter(AllEntries | NoDotAndDotDot))) const;
    /*!
        Returns the file name at index \a i of entryList().
    */
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
    /*!
        Returns \c true if the directory is the root directory; otherwise
        returns \c false.

        \sa rootPath()
    */
    bool isRoot() const { return absolutePath() == rootPath(); }
    /*!
        Returns \c true if the directory's path is absolute; otherwise
        returns \c false.

        \sa isRelative(), makeAbsolute()
    */
    bool isAbsolute() const { return isAbsolutePath(m_path); }
    /*!
        Returns \c true if the directory's path is relative; otherwise
        returns \c false.

        \sa isAbsolute()
    */
    bool isRelative() const { return !isAbsolute(); }
    bool makeAbsolute();
    /*!
        Does nothing. Entries are read from the file system on every call, so
        there is no cache to refresh.
    */
    void refresh() const {}

    static QString cleanPath(const QString &path);
    static void setSearchPaths(const QString &prefix, const QStringList &searchPaths);
    static void addSearchPath(const QString &prefix, const QString &path);
    static QStringList searchPaths(const QString &prefix);
    /*!
        Returns \c true if \a path is absolute, which means it starts with a
        slash, or with a colon for a resource; otherwise returns \c false.

        \sa isRelativePath()
    */
    static bool isAbsolutePath(const QString &path) {
        return path.startsWith(QChar(u'/')) || path.startsWith(QChar(u':'));
    }
    /*!
        Returns \c true if \a path is relative; otherwise returns \c false.

        \sa isAbsolutePath()
    */
    static bool isRelativePath(const QString &path) { return !isAbsolutePath(path); }
    /*!
        Returns the directory separator, \c /.

        \sa listSeparator()
    */
    static QChar separator() noexcept { return QChar(u'/'); }
    /*!
        Returns the separator used in path lists such as \c PATH, \c :.

        \sa separator()
    */
    static QChar listSeparator() noexcept { return QChar(u':'); }
    /*!
        Returns \a path unchanged; native paths already use \c / on Nt.

        \sa fromNativeSeparators()
    */
    static QString toNativeSeparators(const QString &path) { return path; }
    /*!
        Returns \a path unchanged; native paths already use \c / on Nt.

        \sa toNativeSeparators()
    */
    static QString fromNativeSeparators(const QString &path) { return path; }
    static bool setCurrent(const QString &path);
    /*!
        Returns a QDir for the current working directory.

        \sa currentPath(), setCurrent()
    */
    static QDir current() { return QDir(currentPath()); }
    static QString currentPath();
    /*!
        Returns a QDir for the user's home directory.

        \sa homePath()
    */
    static QDir home() { return QDir(homePath()); }
    static QString homePath();
    /*!
        Returns a QDir for the system's temporary directory.

        \sa tempPath()
    */
    static QDir temp() { return QDir(tempPath()); }
    static QString tempPath();
    /*!
        Returns a QDir for the root directory.

        \sa rootPath()
    */
    static QDir root() { return QDir(rootPath()); }
    /*!
        Returns the path of the root directory, \c /.

        \sa root(), isRoot()
    */
    static QString rootPath() { return QString("/"); }
    static QFileInfoList drives();
    static QStringList nameFiltersFromString(const QString &nameFilter);
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
