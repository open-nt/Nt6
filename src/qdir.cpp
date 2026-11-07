// MIT License - see LICENSE

#include <QtCore/qdir.h>

#include <QtCore/qfile.h>

#include <dirent.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
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

bool isDirPath(const QString &path) {
    struct stat st;
    return ::stat(path.toUtf8().constData(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool removeTree(const QString &path) {
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

QDir::QDir(const QString &path) { setPath(path); }

QDir::QDir(const QString &path, const QString &nameFilter, SortFlags sort, Filters filter)
    : m_filter(filter), m_sort(sort) {
    setPath(path);
    m_nameFilters = splitFilterString(nameFilter);
    if (m_nameFilters.isEmpty())
        m_nameFilters = QStringList(QString::fromUtf8("*"));
}

void QDir::setPath(const QString &path) { m_path = normalizeStored(path); }

QString QDir::absolutePath() const {
    if (isAbsolute())
        return cleanPath(m_path);
    return cleanPath(currentPath() + QChar(u'/') + m_path);
}

QString QDir::canonicalPath() const { return QFileInfo(m_path).canonicalFilePath(); }

QString QDir::dirName() const {
    const QString clean = cleanPath(m_path);
    if (clean == rootPath())
        return QString::fromUtf8("");
    const qsizetype slash = clean.lastIndexOf(QChar(u'/'));
    return slash < 0 ? clean : clean.mid(slash + 1);
}

QString QDir::filePath(const QString &fileName) const {
    if (isAbsolutePath(fileName))
        return fileName;
    if (fileName.isEmpty())
        return m_path;
    return m_path.endsWith(QChar(u'/')) ? m_path + fileName : m_path + QChar(u'/') + fileName;
}

QString QDir::absoluteFilePath(const QString &fileName) const {
    if (isAbsolutePath(fileName))
        return fileName;
    const QString base = absolutePath();
    if (fileName.isEmpty())
        return base;
    return base.endsWith(QChar(u'/')) ? base + fileName : base + QChar(u'/') + fileName;
}

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

bool QDir::cd(const QString &dirName) {
    if (dirName.isEmpty() || dirName == QString::fromUtf8("."))
        return true;
    const QString target = isAbsolutePath(dirName) ? cleanPath(dirName) : cleanPath(m_path + QChar(u'/') + dirName);
    if (!isDirPath(target))
        return false;
    m_path = target;
    return true;
}

bool QDir::cdUp() { return cd(QString::fromUtf8("..")); }

bool QDir::isEmpty(Filters filters) const {
    return entryList(filters, SortFlags(Unsorted)).isEmpty();
}

QStringList QDir::entryList(Filters filters, SortFlags sort) const { return entryList(m_nameFilters, filters, sort); }

QFileInfoList QDir::entryInfoList(Filters filters, SortFlags sort) const {
    return entryInfoList(m_nameFilters, filters, sort);
}

QStringList QDir::entryList(const QStringList &nameFilters, Filters filters, SortFlags sort) const {
    QStringList names;
    for (const QFileInfo &fi : entryInfoList(nameFilters, filters, sort))
        names.append(fi.fileName());
    return names;
}

QFileInfoList QDir::entryInfoList(const QStringList &nameFilters, Filters filters, SortFlags sort) const {
    const int f = static_cast<int>(filters) == NoFilter ? static_cast<int>(m_filter) : static_cast<int>(filters);
    const int s = static_cast<int>(sort) == NoSort ? static_cast<int>(m_sort) : static_cast<int>(sort);
    const bool caseSensitive = f & CaseSensitive;

    QFileInfoList result;
    DIR *d = ::opendir(m_path.toUtf8().constData());
    if (!d)
        return result;

    std::vector<Entry> entries;
    while (const dirent *e = ::readdir(d)) {
        Entry en;
        en.name = QString::fromUtf8(e->d_name);
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
    ::closedir(d);

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

bool QDir::mkdir(const QString &dirName) const {
    return ::mkdir(filePath(dirName).toUtf8().constData(), 0777) == 0;
}

bool QDir::rmdir(const QString &dirName) const { return ::rmdir(filePath(dirName).toUtf8().constData()) == 0; }

bool QDir::mkpath(const QString &dirPath) const {
    const QString full = cleanPath(filePath(dirPath));
    if (full.isEmpty())
        return false;
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

bool QDir::rmpath(const QString &dirPath) const {
    QString cur = cleanPath(filePath(dirPath));
    if (::rmdir(cur.toUtf8().constData()) != 0)
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

bool QDir::removeRecursively() { return removeTree(m_path); }

bool QDir::remove(const QString &fileName) {
    if (fileName.isEmpty())
        return false;
    return QFile::remove(filePath(fileName));
}

bool QDir::rename(const QString &oldName, const QString &newName) {
    if (oldName.isEmpty() || newName.isEmpty())
        return false;
    return QFile::rename(filePath(oldName), filePath(newName));
}

bool QDir::exists() const { return isDirPath(m_path); }

bool QDir::exists(const QString &name) const { return QFileInfo(filePath(name)).exists(); }

bool QDir::isReadable() const { return ::access(m_path.toUtf8().constData(), R_OK | X_OK) == 0 && exists(); }

bool QDir::makeAbsolute() {
    m_path = absolutePath();
    return true;
}

QString QDir::cleanPath(const QString &path) {
    if (path.isEmpty())
        return path;
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

bool QDir::setCurrent(const QString &path) { return ::chdir(path.toUtf8().constData()) == 0; }

QString QDir::currentPath() {
    std::vector<char> buf(PATH_MAX);
    return ::getcwd(buf.data(), buf.size()) ? QString::fromUtf8(buf.data()) : QString();
}

QString QDir::homePath() {
    if (const char *home = std::getenv("HOME"); home && *home)
        return cleanPath(QString::fromUtf8(home));
    if (const passwd *pw = ::getpwuid(::getuid()); pw && pw->pw_dir && *pw->pw_dir)
        return cleanPath(QString::fromUtf8(pw->pw_dir));
    return rootPath();
}

QString QDir::tempPath() {
    if (const char *tmp = std::getenv("TMPDIR"); tmp && *tmp)
        return cleanPath(QString::fromUtf8(tmp));
    return QString::fromUtf8("/tmp");
}

QFileInfoList QDir::drives() {
    QFileInfoList list;
    list.append(QFileInfo(rootPath()));
    return list;
}

bool QDir::match(const QString &filter, const QString &fileName) { return match(splitFilterString(filter), fileName); }

bool QDir::match(const QStringList &filters, const QString &fileName) {
    for (const QString &pattern : filters)
        if (wildcard(pattern, fileName, false))
            return true;
    return false;
}
