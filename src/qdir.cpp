// MIT License - see LICENSE

#include <QtCore/qdir.h>

#include <QtCore/qfile.h>
#include <QtCore/qlogging.h>

#include "qdir_p.h"
#include "qresource_p.h"

#include <dirent.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

/*
    Differences from Qt:
    - cleanPath() drops a trailing slash ("a/b/" becomes "a/b").
    - Name filters are case-insensitive unless QDir::CaseSensitive is set, and
      QDir::match() is always case-insensitive. Wildcards are *, ? and
      [set] / [!set] with ranges; a backslash escapes the next character.
    - QDir::Time and QDir::Size sort newest / largest first, as Qt does.
    - QDir::Modified is accepted but ignored, QDir::Drives lists nothing, and
      QDir::drives() returns only the root directory.
    - cd() resolves ".." lexically and never follows symbolic links to do so.
    - The QDir path is stored with duplicate and trailing slashes removed but
      "." and ".." components are kept as given.
*/

namespace {

bool foldedEqual(char16_t a, char16_t b, bool cs) {
    return a == b || (!cs && QChar(a).toLower() == QChar(b).toLower());
}

// Matches the class starting at p[pi] == '['. Returns the index after ']' or
// npos when unterminated; `matched` tells whether c is in the class.
std::size_t matchClass(const std::u16string &p, std::size_t pi, char16_t c, bool cs, bool &matched) {
    std::size_t j = pi + 1;
    bool negate = false;
    if (j < p.size() && (p[j] == u'!' || p[j] == u'^')) {
        negate = true;
        ++j;
    }
    bool hit = false;
    bool first = true;
    for (;; first = false) {
        if (j >= p.size())
            return std::u16string::npos;
        char16_t lo = p[j];
        if (lo == u']' && !first)
            break;
        if (lo == u'\\' && j + 1 < p.size())
            lo = p[++j];
        if (j + 2 < p.size() && p[j + 1] == u'-' && p[j + 2] != u']') {
            const char16_t hi = p[j + 2];
            if ((c >= lo && c <= hi) ||
                (!cs && QChar(c).toLower().unicode() >= QChar(lo).toLower().unicode() &&
                 QChar(c).toLower().unicode() <= QChar(hi).toLower().unicode()))
                hit = true;
            j += 3;
        } else {
            if (foldedEqual(c, lo, cs))
                hit = true;
            ++j;
        }
    }
    matched = hit != negate;
    return j + 1;
}

bool wildcardMatch(const std::u16string &p, std::size_t pi, const std::u16string &s, std::size_t si, bool cs) {
    while (pi < p.size()) {
        const char16_t ch = p[pi];
        if (ch == u'*') {
            while (pi < p.size() && p[pi] == u'*')
                ++pi;
            if (pi == p.size())
                return true;
            for (std::size_t k = si; k <= s.size(); ++k)
                if (wildcardMatch(p, pi, s, k, cs))
                    return true;
            return false;
        }
        if (si >= s.size())
            return false;
        if (ch == u'?') {
            ++pi;
        } else if (ch == u'[') {
            bool ok = false;
            const std::size_t next = matchClass(p, pi, s[si], cs, ok);
            if (next == std::u16string::npos) {
                if (s[si] != u'[')
                    return false;
                ++pi;
            } else {
                if (!ok)
                    return false;
                pi = next;
            }
        } else {
            std::size_t lit = pi;
            if (ch == u'\\' && pi + 1 < p.size())
                lit = pi + 1;
            if (!foldedEqual(s[si], p[lit], cs))
                return false;
            pi = lit + 1;
        }
        ++si;
    }
    return si == s.size();
}

bool wildcard(const QString &pattern, const QString &name, bool cs) {
    return wildcardMatch(pattern.u16string(), 0, name.u16string(), 0, cs);
}

QStringList splitFilterString(const QString &filter) {
    QStringList out;
    QString cur;
    for (qsizetype i = 0; i < filter.size(); ++i) {
        const QChar c = filter.at(i);
        if (c == QChar(u' ') || c == QChar(u';')) {
            if (!cur.isEmpty())
                out.append(cur);
            cur.clear();
        } else {
            cur.append(c);
        }
    }
    if (!cur.isEmpty())
        out.append(cur);
    return out;
}

QString normalizeStored(const QString &path) {
    if (path.isEmpty())
        return QString::fromUtf8(".");
    QString out;
    for (qsizetype i = 0; i < path.size(); ++i) {
        const QChar c = path.at(i);
        if (c == QChar(u'/') && !out.isEmpty() && out.endsWith(QChar(u'/')))
            continue;
        out.append(c);
    }
    if (out.size() > 1 && out.endsWith(QChar(u'/')))
        out.chop(1);
    return out;
}

// The search paths of QDir::setSearchPaths(), by prefix.
struct SearchPaths {
    std::mutex mutex;
    std::map<QString, QStringList> paths;
};

SearchPaths &searchPathTable() {
    static SearchPaths *table = new SearchPaths; // never freed, so it outlives static destructors
    return *table;
}

bool isSearchPathPrefix(const QString &prefix) {
    if (prefix.size() < 2) {
        qWarning("QDir::setSearchPaths: Prefix must be longer than 1 character");
        return false;
    }
    for (const QChar c : prefix) {
        if (!c.isLetterOrNumber()) {
            qWarning("QDir::setSearchPaths: Prefix can only contain letters or numbers");
            return false;
        }
    }
    return true;
}

QString resolveSearchPath(const QString &path, int depth) {
    const qsizetype colon = path.indexOf(QChar(u':'));
    // A one-letter prefix is never registered, and ":/" is a resource.
    if (colon < 2 || depth > 8)
        return path;
    QStringList candidates;
    {
        SearchPaths &table = searchPathTable();
        const std::lock_guard<std::mutex> lock(table.mutex);
        const auto it = table.paths.find(path.left(colon));
        if (it == table.paths.end())
            return path;
        candidates = it->second;
    }
    const QString rest = path.mid(colon + 1);
    QString first;
    for (const QString &dir : candidates) {
        // A search path can itself use a prefix or name a resource directory.
        const QString candidate = resolveSearchPath(QDir::cleanPath(dir + QChar(u'/') + rest), depth + 1);
        if (first.isEmpty())
            first = candidate;
        // Tested without resolving prefixes again, so that prefixes naming each other end.
        struct stat st;
        if (NtResource::isResourcePath(candidate) ? NtResource::find(candidate).exists
                                                  : ::stat(candidate.toUtf8().constData(), &st) == 0)
            return candidate;
    }
    return first.isEmpty() ? path : first;
}

bool isDirPath(const QString &rawPath) {
    const QString path = ntResolveSearchPath(rawPath);
    if (NtResource::isResourcePath(path))
        return NtResource::find(path).dir;
    struct stat st;
    return ::stat(path.toUtf8().constData(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool removeTree(const QString &path) {
    if (NtResource::isResourcePath(path))
        return false;
    struct stat st;
    const QByteArray native = path.toUtf8();
    if (::lstat(native.constData(), &st) != 0)
        return errno == ENOENT;
    if (!S_ISDIR(st.st_mode))
        return ::unlink(native.constData()) == 0;
    bool ok = true;
    if (DIR *d = ::opendir(native.constData())) {
        std::vector<QString> names;
        while (const dirent *e = ::readdir(d)) {
            const QString n = QString::fromUtf8(e->d_name);
            if (n != QString::fromUtf8(".") && n != QString::fromUtf8(".."))
                names.push_back(n);
        }
        ::closedir(d);
        for (const QString &n : names)
            ok = removeTree(path + QChar(u'/') + n) && ok;
    } else {
        ok = false;
    }
    return ::rmdir(native.constData()) == 0 && ok;
}

struct Entry {
    QString name;
    QFileInfo info;
    bool isDir = false;
};

} // namespace

QString ntResolveSearchPath(const QString &path) { return resolveSearchPath(path, 0); }

/*!
    \class QDir
    \brief The QDir class provides access to directory structures and their contents.

    A QDir holds a directory path together with name filters, an entry
    filter and a sort order. Use it to list a directory, to create, remove
    and rename entries, and to convert between relative and absolute paths.
    Paths always use \c / as the separator.

    A path that starts with a colon, such as \c{:/icons}, refers to a
    directory of \l{The Nt Resource System}{resources}. It can be listed,
    entered and iterated with QDirIterator like any other directory, but
    nothing can be created, removed or renamed in it.

    setSearchPaths() gives a name prefix a list of directories, so that a
    path such as \c{icons:open.png} names the first such file in them.

    \code
    QDir dir("/var/log");
    dir.setNameFilters(QStringList() << "*.log");
    const QStringList logs = dir.entryList(QDir::Files, QDir::Time);
    \endcode

    \differences
    Only POSIX paths are supported, so toNativeSeparators() and
    fromNativeSeparators() return their argument unchanged. A path that uses a search path prefix keeps
    its prefix in path() and the paths of its entries; it is resolved each
    time the file system is used. Drives is accepted but
    lists nothing, drives() returns only the root directory, and Modified is
    ignored. Name filters are case-insensitive unless CaseSensitive is set,
    and match() is always case-insensitive. LocaleAware is ignored when
    sorting. cd()
    resolves \c .. lexically and never follows symbolic links to do so.
    \enddifferences

    \sa QFileInfo, QStandardPaths
*/

/*!
    \enum QDir::Filter

    Selects which entries entryList() and entryInfoList() return.

    \value Dirs List directories that match the name filters.
    \value Files List files.
    \value Drives Accepted for compatibility; nothing is listed for it.
    \value NoSymLinks Do not list symbolic links.
    \value AllEntries List directories, files and drives.
    \value TypeMask A mask for the type bits Dirs, Files, Drives and NoSymLinks.
    \value Readable List entries the process can read. Applies only together with a permission filter.
    \value Writable List entries the process can write. Applies only together with a permission filter.
    \value Executable List entries the process can execute. Applies only together with a permission filter.
    \value PermissionMask A mask for Readable, Writable and Executable.
    \value Modified Accepted for compatibility; ignored.
    \value Hidden List hidden entries, whose names start with a dot.
    \value System List entries that are neither files nor directories, such as devices and broken symbolic links.
    \value AccessMask A mask for the access bits Readable, Writable, Executable, Modified, Hidden and System.
    \value AllDirs List all directories, ignoring the name filters.
    \value CaseSensitive Match the name filters case-sensitively.
    \value NoDot Do not list the \c . entry.
    \value NoDotDot Do not list the \c .. entry.
    \value NoDotAndDotDot Do not list \c{.} and \c{..}.
    \value NoFilter Use the filter set with setFilter(). Pass this to entryList() to keep the QDir's own filter.

    \sa filter(), setFilter()
*/

/*!
    \typedef QDir::Filters

    A QFlags of Filter values.
*/

/*!
    \enum QDir::SortFlag

    Selects the order of the entries returned by entryList() and
    entryInfoList().

    \value Name Sort by name.
    \value Time Sort by modification time, newest first.
    \value Size Sort by size, largest first.
    \value Unsorted Return the entries in directory order.
    \value SortByMask A mask for Name, Time, Size and Unsorted.
    \value DirsFirst Put directories before files.
    \value Reversed Reverse the sort order.
    \value IgnoreCase Compare names case-insensitively.
    \value DirsLast Put directories after files. Ignored if DirsFirst is set.
    \value LocaleAware Accepted for compatibility; ignored.
    \value Type Sort by file suffix, then by name.
    \value NoSort Use the order set with setSorting(). Pass this to entryList() to keep the QDir's own order.

    \sa sorting(), setSorting()
*/

/*!
    \typedef QDir::SortFlags

    A QFlags of SortFlag values.
*/

/*!
    Constructs a QDir for \a path. An empty \a path refers to the current
    directory (\c .).

    \sa setPath()
*/
QDir::QDir(const QString &path) { setPath(path); }

/*!
    Constructs a QDir for \a path that lists entries matching \a nameFilter,
    which holds patterns separated by spaces or semicolons, with the given
    \a sort order and \a filter.

    \sa setNameFilters(), setSorting(), setFilter()
*/
QDir::QDir(const QString &path, const QString &nameFilter, SortFlags sort, Filters filter)
    : m_filter(filter), m_sort(sort) {
    setPath(path);
    m_nameFilters = splitFilterString(nameFilter);
    if (m_nameFilters.isEmpty())
        m_nameFilters = QStringList(QString::fromUtf8("*"));
}

/*!
    Sets the path of the directory to \a path. Duplicate and trailing slashes
    are removed. The directory does not have to exist.

    \sa path(), absolutePath()
*/
void QDir::setPath(const QString &path) { m_path = normalizeStored(path); }

/*!
    Returns the absolute path of the directory, with \c . and \c .. components
    resolved lexically. Symbolic links are not resolved.

    \sa canonicalPath(), path(), cleanPath()
*/
QString QDir::absolutePath() const {
    const QString resolved = ntResolveSearchPath(m_path);
    if (resolved != m_path)
        return QDir(resolved).absolutePath();
    if (isAbsolute())
        return cleanPath(m_path);
    return cleanPath(currentPath() + QChar(u'/') + m_path);
}

/*!
    Returns the canonical path of the directory, with symbolic links resolved.
    Returns an empty string if the directory does not exist.

    \sa absolutePath(), QFileInfo::canonicalFilePath()
*/
QString QDir::canonicalPath() const { return QFileInfo(m_path).canonicalFilePath(); }

/*!
    Returns the name of the directory, which is the last component of its
    cleaned path. Returns an empty string for the root directory.

    \sa path()
*/
QString QDir::dirName() const {
    const QString clean = cleanPath(m_path);
    if (clean == rootPath())
        return QString::fromUtf8("");
    const qsizetype slash = clean.lastIndexOf(QChar(u'/'));
    return slash < 0 ? clean : clean.mid(slash + 1);
}

/*!
    Returns \a fileName prefixed with the path of the directory. If
    \a fileName is absolute, it is returned unchanged. The file does not have
    to exist.

    \sa absoluteFilePath(), relativeFilePath()
*/
QString QDir::filePath(const QString &fileName) const {
    if (isAbsolutePath(fileName))
        return fileName;
    if (fileName.isEmpty())
        return m_path;
    return m_path.endsWith(QChar(u'/')) ? m_path + fileName : m_path + QChar(u'/') + fileName;
}

/*!
    Returns \a fileName prefixed with the absolute path of the directory. If
    \a fileName is absolute, it is returned unchanged. The file does not have
    to exist.

    \sa filePath(), relativeFilePath(), absolutePath()
*/
QString QDir::absoluteFilePath(const QString &fileName) const {
    if (isAbsolutePath(fileName))
        return fileName;
    QString base = absolutePath();
    if (fileName.isEmpty())
        return base;
    return base.endsWith(QChar(u'/')) ? base + fileName : base + QChar(u'/') + fileName;
}

/*!
    Returns the path of \a fileName relative to the directory, using \c ..
    components where needed. Returns \c . if \a fileName is the directory
    itself.

    \sa filePath(), absoluteFilePath()
*/
QString QDir::relativeFilePath(const QString &fileName) const {
    const QString dir = absolutePath();
    const QString file = isAbsolutePath(fileName) ? cleanPath(fileName) : cleanPath(dir + QChar(u'/') + fileName);
    if (file == dir)
        return QString::fromUtf8(".");
    const QStringList d = dir.split(QChar(u'/'), Qt::SkipEmptyParts);
    const QStringList f = file.split(QChar(u'/'), Qt::SkipEmptyParts);
    std::size_t common = 0;
    while (common < static_cast<std::size_t>(d.size()) && common < static_cast<std::size_t>(f.size()) &&
           d[static_cast<qsizetype>(common)] == f[static_cast<qsizetype>(common)])
        ++common;
    QStringList parts;
    for (std::size_t i = common; i < static_cast<std::size_t>(d.size()); ++i)
        parts.append(QString::fromUtf8(".."));
    for (std::size_t i = common; i < static_cast<std::size_t>(f.size()); ++i)
        parts.append(f[static_cast<qsizetype>(i)]);
    QString out;
    for (qsizetype i = 0; i < parts.size(); ++i) {
        if (i)
            out.append(QChar(u'/'));
        out.append(parts[i]);
    }
    return out;
}

/*!
    Changes the directory to \a dirName and returns \c true. Returns \c false,
    leaving the path unchanged, if \a dirName is not an existing directory.
    \c .. is resolved lexically.

    \sa cdUp(), setPath()
*/
bool QDir::cd(const QString &dirName) {
    if (dirName.isEmpty() || dirName == QString::fromUtf8("."))
        return true;
    const QString target = isAbsolutePath(dirName) ? cleanPath(dirName) : cleanPath(m_path + QChar(u'/') + dirName);
    if (!isDirPath(target))
        return false;
    m_path = target;
    return true;
}

/*!
    Changes to the parent directory. Returns \c true on success; otherwise
    returns \c false.

    \sa cd()
*/
bool QDir::cdUp() { return cd(QString::fromUtf8("..")); }

/*!
    Returns \c true if the directory contains no entries that pass
    \a filters, using the directory's name filters; otherwise returns
    \c false. The default filters ignore \c{.} and \c{..}.

    \sa entryList()
*/
bool QDir::isEmpty(Filters filters) const {
    return entryList(filters, SortFlags(Unsorted)).isEmpty();
}

/*!
    Returns the names of the entries in the directory that match the
    directory's name filters and the given \a filters, sorted by \a sort. If
    \a filters is NoFilter, the directory's own filter is used; if \a sort is
    NoSort, its own sort order is used.

    \sa entryInfoList(), setNameFilters(), setFilter(), setSorting()
*/
QStringList QDir::entryList(Filters filters, SortFlags sort) const { return entryList(m_nameFilters, filters, sort); }

/*!
    Returns a QFileInfo for each entry in the directory that matches the
    directory's name filters and the given \a filters, sorted by \a sort. If
    \a filters is NoFilter, the directory's own filter is used; if \a sort is
    NoSort, its own sort order is used.

    \sa entryList(), setNameFilters(), setFilter(), setSorting()
*/
QFileInfoList QDir::entryInfoList(Filters filters, SortFlags sort) const {
    return entryInfoList(m_nameFilters, filters, sort);
}

/*!
    \overload

    Returns the entries that match \a nameFilters instead of the directory's
    own name filters.
*/
QStringList QDir::entryList(const QStringList &nameFilters, Filters filters, SortFlags sort) const {
    QStringList names;
    for (const QFileInfo &fi : entryInfoList(nameFilters, filters, sort))
        names.append(fi.fileName());
    return names;
}

/*!
    \overload

    Returns the entries that match \a nameFilters instead of the directory's
    own name filters.
*/
QFileInfoList QDir::entryInfoList(const QStringList &nameFilters, Filters filters, SortFlags sort) const {
    const int f = static_cast<int>(filters) == NoFilter ? static_cast<int>(m_filter) : static_cast<int>(filters);
    const int s = static_cast<int>(sort) == NoSort ? static_cast<int>(m_sort) : static_cast<int>(sort);
    const bool caseSensitive = f & CaseSensitive;

    QFileInfoList result;
    QStringList names;
    const QString path = ntResolveSearchPath(m_path);
    if (NtResource::isResourcePath(path)) {
        // A resource directory has no "." and ".." entries.
        const NtResource::Entry dir = NtResource::find(path, QLocale(), true);
        if (!dir.dir)
            return result;
        names = dir.children;
    } else {
        DIR *d = ::opendir(path.toUtf8().constData());
        if (!d)
            return result;
        while (const dirent *e = ::readdir(d))
            names.append(QString::fromUtf8(e->d_name));
        ::closedir(d);
    }

    std::vector<Entry> entries;
    for (const QString &name : names) {
        Entry en;
        en.name = name;
        const bool dot = en.name == QString::fromUtf8(".");
        const bool dotdot = en.name == QString::fromUtf8("..");
        if ((dot && (f & NoDot)) || (dotdot && (f & NoDotDot)))
            continue;
        en.info = QFileInfo(filePath(en.name));
        en.isDir = en.info.isDir();
        if ((f & NoSymLinks) && en.info.isSymLink())
            continue;
        if (en.isDir) {
            if (!(f & (Dirs | AllDirs)))
                continue;
        } else if (en.info.isFile()) {
            if (!(f & Files))
                continue;
        } else if (!(f & System)) {
            continue;
        }
        if (!dot && !dotdot && en.name.startsWith(QChar(u'.')) && !(f & Hidden))
            continue;
        if (f & PermissionMask) {
            if ((f & Readable) && !en.info.isReadable())
                continue;
            if ((f & Writable) && !en.info.isWritable())
                continue;
            if ((f & Executable) && !en.info.isExecutable())
                continue;
        }
        if (!nameFilters.isEmpty() && !((f & AllDirs) && en.isDir)) {
            bool ok = false;
            for (const QString &pattern : nameFilters) {
                if (wildcard(pattern, en.name, caseSensitive)) {
                    ok = true;
                    break;
                }
            }
            if (!ok)
                continue;
        }
        entries.push_back(std::move(en));
    }

    const int by = s & SortByMask;
    const bool ignoreCase = s & IgnoreCase;
    const bool reversed = s & Reversed;
    const Qt::CaseSensitivity cs = ignoreCase ? Qt::CaseInsensitive : Qt::CaseSensitive;
    const bool dirsFirst = s & DirsFirst;
    const bool dirsLast = (s & DirsLast) && !dirsFirst;

    auto less = [&](const Entry &a, const Entry &b) {
        if ((dirsFirst || dirsLast) && a.isDir != b.isDir)
            return dirsFirst ? a.isDir : b.isDir;
        if (by == Unsorted && !(s & Type))
            return false;
        int r = 0;
        if (s & Type) {
            r = a.info.suffix().compare(b.info.suffix(), cs);
        } else if (by == Time) {
            const qint64 ta = a.info.lastModified().toMSecsSinceEpoch(), tb = b.info.lastModified().toMSecsSinceEpoch();
            r = ta == tb ? 0 : (ta > tb ? -1 : 1);
        } else if (by == Size) {
            const qint64 sa = a.info.size(), sb = b.info.size();
            r = sa == sb ? 0 : (sa > sb ? -1 : 1);
        }
        if (r == 0)
            r = a.name.compare(b.name, cs);
        return reversed ? r > 0 : r < 0;
    };
    std::stable_sort(entries.begin(), entries.end(), less);

    for (Entry &en : entries)
        result.append(en.info);
    return result;
}

/*!
    Creates the sub-directory \a dirName and returns \c true on success. The
    parent must already exist.

    \sa mkpath(), rmdir()
*/
bool QDir::mkdir(const QString &dirName) const {
    if (NtResource::isResourcePath(filePath(dirName)))
        return false;
    return ::mkdir(filePath(dirName).toUtf8().constData(), 0777) == 0;
}

/*!
    Removes the empty sub-directory \a dirName and returns \c true on
    success.

    \sa rmpath(), removeRecursively(), mkdir()
*/
bool QDir::rmdir(const QString &dirName) const {
    const QString path = filePath(dirName);
    return !NtResource::isResourcePath(path) && ::rmdir(path.toUtf8().constData()) == 0;
}

/*!
    Creates \a dirPath and any missing parent directories. Returns \c true if
    the directory exists afterwards; otherwise returns \c false.

    \sa mkdir(), rmpath()
*/
bool QDir::mkpath(const QString &dirPath) const {
    const QString full = cleanPath(filePath(dirPath));
    if (full.isEmpty())
        return false;
    if (NtResource::isResourcePath(full))
        return isDirPath(full);
    QString cur = isAbsolutePath(full) ? QString::fromUtf8("/") : QString();
    for (const QString &part : full.split(QChar(u'/'), Qt::SkipEmptyParts)) {
        if (!cur.isEmpty() && !cur.endsWith(QChar(u'/')))
            cur.append(QChar(u'/'));
        cur.append(part);
        if (isDirPath(cur))
            continue;
        if (::mkdir(cur.toUtf8().constData(), 0777) != 0 && !(errno == EEXIST && isDirPath(cur)))
            return false;
    }
    return true;
}

/*!
    Removes the directory \a dirPath, then each parent directory that becomes
    empty. Returns \c true if \a dirPath itself was removed.

    \sa rmdir(), mkpath()
*/
bool QDir::rmpath(const QString &dirPath) const {
    QString cur = cleanPath(filePath(dirPath));
    if (NtResource::isResourcePath(cur) || ::rmdir(cur.toUtf8().constData()) != 0)
        return false;
    for (;;) {
        const qsizetype slash = cur.lastIndexOf(QChar(u'/'));
        if (slash <= 0)
            break;
        cur.truncate(slash);
        if (::rmdir(cur.toUtf8().constData()) != 0)
            break;
    }
    return true;
}

/*!
    Removes the directory, including all its contents. Symbolic links are
    removed, not followed. Returns \c true on success; a directory that does
    not exist also counts as success.

    \sa rmdir()
*/
bool QDir::removeRecursively() { return removeTree(m_path); }

/*!
    Removes the file \a fileName, relative to the directory. Returns \c true
    on success; otherwise returns \c false.

    \sa rename(), QFile::remove()
*/
bool QDir::remove(const QString &fileName) {
    if (fileName.isEmpty())
        return false;
    return QFile::remove(filePath(fileName));
}

/*!
    Renames the file or directory \a oldName to \a newName, both relative to
    the directory. Returns \c true on success; otherwise returns \c false.

    \sa remove(), QFile::rename()
*/
bool QDir::rename(const QString &oldName, const QString &newName) {
    if (oldName.isEmpty() || newName.isEmpty())
        return false;
    return QFile::rename(filePath(oldName), filePath(newName));
}

/*!
    Returns \c true if the directory exists; otherwise returns \c false.

    \sa isReadable()
*/
bool QDir::exists() const { return isDirPath(m_path); }

/*!
    \overload

    Returns \c true if the file or directory \a name, relative to the
    directory, exists; otherwise returns \c false.
*/
bool QDir::exists(const QString &name) const { return QFileInfo(filePath(name)).exists(); }

/*!
    Returns \c true if the directory exists and the process may list and enter
    it; otherwise returns \c false.

    \sa exists()
*/
bool QDir::isReadable() const {
    const QString path = ntResolveSearchPath(m_path);
    if (NtResource::isResourcePath(path))
        return exists();
    return ::access(path.toUtf8().constData(), R_OK | X_OK) == 0 && exists();
}

/*!
    Converts the directory's path to an absolute path. Always returns
    \c true.

    \sa isAbsolute(), absolutePath()
*/
bool QDir::makeAbsolute() {
    m_path = absolutePath();
    return true;
}

/*!
    Sets or replaces the search paths for file names that start with
    \a prefix to \a searchPaths. An empty list removes the prefix.

    A file name such as \c{icons:open.png} is then looked up in each search
    path in turn, and refers to the first file that exists. If none exists,
    it refers to the file in the first search path, so that files can be
    created there. Search paths can be resource directories, such as
    \c{:/icons}, and can use other prefixes. QFile, QFileInfo, QDir and
    QDirIterator all resolve prefixes:

    \code
    QDir::setSearchPaths("icons", QStringList() << QDir::homePath() + "/.app/icons" << ":/icons");
    QFile icon("icons:open.png");   // the user's icon, or the built-in one
    \endcode

    The \a prefix must be at least two characters long, so that it cannot be
    confused with a drive letter, and may contain only letters and digits;
    otherwise a warning is printed and nothing changes.

    \since 4.3
    \sa addSearchPath(), searchPaths()
*/
void QDir::setSearchPaths(const QString &prefix, const QStringList &searchPaths) {
    if (!isSearchPathPrefix(prefix))
        return;
    SearchPaths &table = searchPathTable();
    const std::lock_guard<std::mutex> lock(table.mutex);
    if (searchPaths.isEmpty()) {
        table.paths.erase(prefix);
        return;
    }
    QStringList cleaned;
    for (const QString &dir : searchPaths)
        cleaned.append(cleanPath(dir));
    table.paths[prefix] = cleaned;
}

/*!
    Adds \a path to the end of the search paths for \a prefix.

    \since 4.3
    \sa setSearchPaths()
*/
void QDir::addSearchPath(const QString &prefix, const QString &path) {
    if (path.isEmpty() || !isSearchPathPrefix(prefix))
        return;
    SearchPaths &table = searchPathTable();
    const std::lock_guard<std::mutex> lock(table.mutex);
    table.paths[prefix].append(cleanPath(path));
}

/*!
    Returns the search paths for \a prefix, or an empty list if it has none.

    \since 4.3
    \sa setSearchPaths(), addSearchPath()
*/
QStringList QDir::searchPaths(const QString &prefix) {
    SearchPaths &table = searchPathTable();
    const std::lock_guard<std::mutex> lock(table.mutex);
    const auto it = table.paths.find(prefix);
    return it == table.paths.end() ? QStringList() : it->second;
}

/*!
    Returns \a path with redundant separators and \c . components removed, and
    \c .. components resolved lexically. A trailing slash is dropped. An empty
    \a path is returned as is; a path that cleans to nothing becomes \c{.}.
    A resource path is cleaned to one that starts with \c{:/}.
*/
QString QDir::cleanPath(const QString &path) {
    if (path.isEmpty())
        return path;
    // A resource path is always taken from the root of the resources.
    if (NtResource::isResourcePath(path))
        return QChar(u':') + cleanPath(QChar(u'/') + path.mid(1));
    const bool absolute = path.startsWith(QChar(u'/'));
    QStringList stack;
    const QString dotdot = QString::fromUtf8("..");
    for (const QString &comp : path.split(QChar(u'/'), Qt::SkipEmptyParts)) {
        if (comp == QString::fromUtf8("."))
            continue;
        if (comp == dotdot) {
            if (!stack.isEmpty() && stack.last() != dotdot)
                stack.removeLast();
            else if (!absolute)
                stack.append(comp);
            continue;
        }
        stack.append(comp);
    }
    QString out = absolute ? QString::fromUtf8("/") : QString();
    for (qsizetype i = 0; i < stack.size(); ++i) {
        if (i)
            out.append(QChar(u'/'));
        out.append(stack[i]);
    }
    return out.isEmpty() ? QString::fromUtf8(".") : out;
}

/*!
    Sets the process's current working directory to \a path. Returns \c true
    on success; otherwise returns \c false.

    \sa currentPath()
*/
bool QDir::setCurrent(const QString &path) { return ::chdir(path.toUtf8().constData()) == 0; }

/*!
    Returns the absolute path of the process's current working directory.

    \sa setCurrent(), current()
*/
QString QDir::currentPath() {
    std::vector<char> buf(PATH_MAX);
    return ::getcwd(buf.data(), buf.size()) ? QString::fromUtf8(buf.data()) : QString();
}

/*!
    Returns the path of the user's home directory. It is taken from \c HOME,
    then from the password database, and falls back to the root directory.

    \sa home()
*/
QString QDir::homePath() {
    if (const char *home = std::getenv("HOME"); home && *home)
        return cleanPath(QString::fromUtf8(home));
    if (const passwd *pw = ::getpwuid(::getuid()); pw && pw->pw_dir && *pw->pw_dir)
        return cleanPath(QString::fromUtf8(pw->pw_dir));
    return rootPath();
}

/*!
    Returns the path of the system's temporary directory. It is taken from
    \c TMPDIR and falls back to \c /tmp.

    \sa temp()
*/
QString QDir::tempPath() {
    if (const char *tmp = std::getenv("TMPDIR"); tmp && *tmp)
        return cleanPath(QString::fromUtf8(tmp));
    return QString::fromUtf8("/tmp");
}

/*!
    Returns the root directories of the system. On Nt this is always the list
    containing only \c /.

    \sa root()
*/
QFileInfoList QDir::drives() {
    QFileInfoList list;
    list.append(QFileInfo(rootPath()));
    return list;
}

/*!
    Returns \c true if \a fileName matches the wildcard patterns in
    \a filter, which are separated by spaces or semicolons; otherwise returns
    \c false. Matching is case-insensitive.

    \sa entryList()
*/
bool QDir::match(const QString &filter, const QString &fileName) { return match(splitFilterString(filter), fileName); }

/*!
    Returns the name filters in \a nameFilter, which separates them with
    spaces or semicolons. Leading and trailing spaces of each filter are
    removed and empty filters are skipped.

    \sa setNameFilters()
*/
QStringList QDir::nameFiltersFromString(const QString &nameFilter) { return splitFilterString(nameFilter); }

/*!
    \overload

    Returns \c true if \a fileName matches any of the wildcard patterns in
    \a filters; otherwise returns \c false. Matching is case-insensitive.
*/
bool QDir::match(const QStringList &filters, const QString &fileName) {
    for (const QString &pattern : filters)
        if (wildcard(pattern, fileName, false))
            return true;
    return false;
}
