// MIT License - see LICENSE
#include <QtCore/qdiriterator.h>

#include <QtCore/qset.h>

#include <vector>

struct QDirIterator::Private {
    struct Item {
        QFileInfo info;
        bool listed = false;
        bool descend = false;
    };
    struct Level {
        std::vector<Item> items;
        std::size_t index = 0;
        QString canonical;
    };

    QDir dir;
    QStringList nameFilters;
    QDir::Filters filters;
    IteratorFlags flags;
    std::vector<Level> stack;
    QString pendingDescent;
    QFileInfo current;
    QFileInfo upcoming;
    bool hasUpcoming = false;

    Private(const QDir &d, const QStringList &names, QDir::Filters f, IteratorFlags fl)
        : dir(d), nameFilters(names), filters(f), flags(fl) {
        if (static_cast<int>(filters) == QDir::NoFilter)
            filters = QDir::AllEntries;
        if (nameFilters.isEmpty())
            nameFilters = QStringList(QString::fromUtf8("*"));
        pushDirectory(dir.path());
        advance();
    }

    // Returns false, entering nothing, if \a path is a link to one of the directories being traversed.
    bool pushDirectory(const QString &path) {
        QString canonical;
        if (flags.testFlag(FollowSymlinks)) {
            canonical = QFileInfo(path).canonicalFilePath();
            for (const Level &ancestor : stack)
                if (ancestor.canonical == canonical)
                    return false;
        }
        const QDir listing(path);
        // Directories are listed whatever their name so that they can be entered; only the entries that
        // pass the real filters are returned.
        const QFileInfoList all =
            listing.entryInfoList(QStringList(), filters | QDir::AllDirs, QDir::SortFlags(QDir::Unsorted));
        const QFileInfoList wanted = listing.entryInfoList(nameFilters, filters, QDir::SortFlags(QDir::Unsorted));
        QSet<QString> wantedNames;
        for (const QFileInfo &fi : wanted)
            wantedNames.insert(fi.fileName());
        Level level;
        level.canonical = canonical;
        for (const QFileInfo &fi : all) {
            Item item;
            item.info = fi;
            const QString name = fi.fileName();
            item.listed = wantedNames.contains(name);
            item.descend = flags.testFlag(Subdirectories) && fi.isDir() && name != QString::fromUtf8(".") &&
                           name != QString::fromUtf8("..") && (flags.testFlag(FollowSymlinks) || !fi.isSymLink());
            if (item.listed || item.descend)
                level.items.push_back(item);
        }
        stack.push_back(std::move(level));
        return true;
    }

    // Finds the next entry to return; the entries of a directory follow the directory itself.
    void advance() {
        hasUpcoming = false;
        if (!pendingDescent.isEmpty()) {
            pushDirectory(pendingDescent);
            pendingDescent.clear();
        }
        while (!stack.empty()) {
            Level &level = stack.back();
            if (level.index >= level.items.size()) {
                stack.pop_back();
                continue;
            }
            const Item item = level.items[level.index++];
            if (item.listed) {
                upcoming = item.info;
                hasUpcoming = true;
                if (item.descend)
                    pendingDescent = item.info.filePath();
                return;
            }
            pushDirectory(item.info.filePath());
        }
    }
};

/*!
    \class QDirIterator
    \brief The QDirIterator class provides an iterator for directory entries.

    You can use QDirIterator to navigate entries of a directory one at a time.
    It is similar to QDir::entryList() and QDir::entryInfoList(), but because
    it lists entries one at a time instead of all at once, it scales better
    and is more suitable for large directory structures. It also supports
    listing directory contents recursively, and following symbolic links.

    \code
    QDirIterator it("/etc", QStringList() << "*.conf", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        qDebug() << it.next();
    \endcode

    The path of an entry is the path of its directory joined with its name.
    Entries come in the order the file system returns them, which is not
    sorted. With Subdirectories, a directory is returned before the entries it
    contains, and a directory is entered even if its name does not match the
    name filters or QDir::Dirs is not set. Hidden directories are entered only
    with QDir::Hidden. The \c{.} and \c{..} entries are returned unless
    QDir::NoDotAndDotDot is set, but are never entered.

    \differences
    The entries of one directory are read when the iterator enters it, so the
    directory is not read lazily entry by entry. Entries are not returned in
    exactly the order of Qt on every file system, because that depends on the
    file system. There is no QDirListing-based API.
    \enddifferences

    \sa QDir, QFileInfo
*/

/*!
    Constructs a QDirIterator that can iterate over the entries of \a dir,
    using the name filters and the filters of \a dir. You can pass options via
    \a flags to decide how the directory should be iterated.

    By default, \a flags is NoIteratorFlags, which provides the same behavior
    as in QDir::entryList().

    The sorting of \a dir is ignored.
*/
QDirIterator::QDirIterator(const QDir &dir, IteratorFlags flags)
    : d(std::make_unique<Private>(dir, dir.nameFilters(), dir.filter(), flags)) {}

/*!
    \overload

    Constructs a QDirIterator that can iterate over \a path, with no name
    filter and QDir::NoFilter as filter. You can pass options via \a flags to
    decide how the directory should be iterated.
*/
QDirIterator::QDirIterator(const QString &path, IteratorFlags flags)
    : d(std::make_unique<Private>(QDir(path), QStringList(), QDir::NoFilter, flags)) {}

/*!
    \overload

    Constructs a QDirIterator that can iterate over \a path. You can pass
    options via \a flags to decide how the directory should be iterated and
    \a filters to filter entries.
*/
QDirIterator::QDirIterator(const QString &path, QDir::Filters filters, IteratorFlags flags)
    : d(std::make_unique<Private>(QDir(path), QStringList(), filters, flags)) {}

/*!
    \overload

    Constructs a QDirIterator that can iterate over \a path, with the name
    filters \a nameFilters and the filters \a filters. You can pass options
    via \a flags to decide how the directory should be iterated.
*/
QDirIterator::QDirIterator(const QString &path, const QStringList &nameFilters, QDir::Filters filters,
                           IteratorFlags flags)
    : d(std::make_unique<Private>(QDir(path), nameFilters, filters, flags)) {}

/*!
    Destroys the QDirIterator.
*/
QDirIterator::~QDirIterator() = default;

/*!
    Advances the iterator to the next entry, and returns the file path of this
    new entry. If hasNext() returns \c false, this function does nothing, and
    returns an empty QString.

    You can call fileName() or filePath() to get the current entry file name
    or path, or fileInfo() to get a QFileInfo for the current entry.

    \sa hasNext(), fileName(), filePath(), fileInfo()
*/
QString QDirIterator::next() { return nextFileInfo().filePath(); }

/*!
    Advances the iterator to the next entry, and returns the file info of this
    new entry. If hasNext() returns \c false, this function does nothing, and
    returns an empty QFileInfo.

    \since 6.3
    \sa hasNext(), next(), fileInfo()
*/
QFileInfo QDirIterator::nextFileInfo() {
    if (!d->hasUpcoming)
        return QFileInfo();
    d->current = d->upcoming;
    d->advance();
    return d->current;
}

/*!
    Returns \c true if there is at least one more entry in the directory;
    otherwise, \c false is returned.

    \sa next(), nextFileInfo()
*/
bool QDirIterator::hasNext() const { return d->hasUpcoming; }

/*!
    Returns the file name for the current directory entry, without the path
    prepended. If the current entry is invalid (i.e. no entry was returned by
    next() yet), an empty string is returned.

    \sa filePath(), fileInfo()
*/
QString QDirIterator::fileName() const { return d->current.fileName(); }

/*!
    Returns the full file path for the current directory entry. If the current
    entry is invalid (i.e. no entry was returned by next() yet), an empty
    string is returned.

    \sa fileInfo(), fileName()
*/
QString QDirIterator::filePath() const { return d->current.filePath(); }

/*!
    Returns a QFileInfo for the current directory entry. If the current entry
    is invalid (i.e. no entry was returned by next() yet), a default
    constructed QFileInfo is returned.

    \sa filePath(), fileName()
*/
QFileInfo QDirIterator::fileInfo() const { return d->current; }

/*!
    Returns the base directory of the iterator.
*/
QString QDirIterator::path() const { return d->dir.path(); }
