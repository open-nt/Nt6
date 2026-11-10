// MIT License - see LICENSE
#include <QtCore/qdirlisting.h>

#include <QtCore/qdir.h>
#include <QtCore/qregularexpression.h>

#include <vector>

/*!
    \class QDirListing
    \brief The QDirListing class provides an STL-style iterator for directory entries.
    \since 6.8

    A QDirListing walks the entries of a directory, and of its subdirectories if asked to, in a
    range-based \c for loop:

    \code
    using F = QDirListing::IteratorFlag;
    for (const auto &entry : QDirListing(u"/etc"_s, {u"*.conf"_s}, F::FilesOnly | F::Recursive))
        qDebug() << entry.filePath() << entry.size();
    \endcode

    By default the listing has files, directories and other entries, and leaves out hidden entries
    and the \c . and \c .. directories; the flags change that. The order of the entries is not
    specified.

    The iterators are input iterators: a listing is read from the start to the end once, and
    begin() can be called once for each QDirListing. Copies of an iterator move together.

    \differences
    A directory is read whole when the listing enters it and not entry by entry, so entries that
    are created in it afterwards are not listed. \c DirEntry::bundleName() is always empty. There
    are no \c DirEntry functions that take no time zone.
    \enddifferences

    \sa QDirIterator, QDir::entryList()
*/

/*!
    \enum QDirListing::IteratorFlag

    The flags choose which entries are listed. Combine them with \c{|}.

    \value Default Files, directories and other entries are listed, apart from hidden ones and the \c . and \c .. directories. Symbolic links are listed as links, whatever they point to.
    \value ExcludeFiles Do not list regular files. With ResolveSymlinks, symbolic links to files are not listed either.
    \value ExcludeDirs Do not list directories. With ResolveSymlinks, symbolic links to directories are not listed either.
    \value ExcludeSpecial Do not list entries that are neither regular files nor directories, such as devices, sockets and, without ResolveSymlinks, symbolic links.
    \value ExcludeOther The name of ExcludeSpecial since Qt 6.10.
    \value ResolveSymlinks Treat a symbolic link as what it points to when the other flags are applied. Links whose target does not exist are not listed.
    \value FilesOnly List only regular files, and with ResolveSymlinks the symbolic links to them.
    \value DirsOnly List only directories, and with ResolveSymlinks the symbolic links to them.
    \value IncludeHidden Also list hidden entries, whose names start with a dot.
    \value IncludeDotAndDotDot Also list the \c . and \c .. directories.
    \value CaseSensitive Match the name filters case-sensitively.
    \value Recursive Also list the entries of all subdirectories. Subdirectories are entered whether or not the other flags and the name filters list them, apart from hidden ones, which need IncludeHidden.
    \value FollowDirSymlinks With Recursive, also enter symbolic links to directories. A link to a directory that is being listed is not entered again.
*/

/*!
    \class QDirListing::DirEntry
    \brief The DirEntry class describes one entry of a QDirListing.
    \since 6.8

    Dereferencing a QDirListing::const_iterator gives a DirEntry, which answers the questions a
    QFileInfo does, for the entry the iterator is at.

    \differences
    The entry holds a QFileInfo, so it stays valid when it is copied out of the loop. bundleName()
    is always empty.
    \enddifferences

    \sa QFileInfo
*/

/*!
    \class QDirListing::const_iterator
    \brief The const_iterator class is the input iterator of a QDirListing.
    \since 6.8

    An iterator is made by QDirListing::begin() and compared with QDirListing::end(). It can be
    moved forward and read, which is what a range-based \c for loop needs.

    \differences
    None.
    \enddifferences

    \sa QDirListing::sentinel
*/

/*!
    \class QDirListing::sentinel
    \brief The sentinel class marks the end of a QDirListing.
    \since 6.8

    QDirListing::end() returns a sentinel. A QDirListing::const_iterator is equal to it once it has
    passed the last entry.

    \differences
    None.
    \enddifferences
*/

struct QDirListing::Private {
    struct Level {
        QFileInfoList entries;
        qsizetype index = 0;
        QString canonical;
    };

    QString path;
    QStringList nameFilters;
    IteratorFlags flags;
    std::vector<QRegularExpression> filters;
    std::vector<Level> stack;
    bool started = false;

    bool has(IteratorFlag flag) const { return flags.testFlag(flag); }

    // Returns false, entering nothing, for a link to one of the directories that are being listed.
    bool enter(const QString &directory) {
        Level level;
        if (has(IteratorFlag::FollowDirSymlinks)) {
            level.canonical = QFileInfo(directory).canonicalFilePath();
            for (const Level &ancestor : stack) {
                if (ancestor.canonical == level.canonical)
                    return false;
            }
        }
        const QDir::Filters everything(QDir::AllEntries | QDir::Hidden | QDir::System);
        level.entries = QDir(directory).entryInfoList(QStringList(), everything, QDir::SortFlags(QDir::Unsorted));
        stack.push_back(std::move(level));
        return true;
    }

    bool listed(const QFileInfo &info, bool dots) const {
        const QString name = info.fileName();
        if (dots) {
            if (!has(IteratorFlag::IncludeDotAndDotDot))
                return false;
        } else if (!has(IteratorFlag::IncludeHidden) && name.startsWith(QChar(u'.'))) {
            return false;
        }
        const bool link = info.isSymLink();
        if (link && has(IteratorFlag::ResolveSymlinks) && !info.exists())
            return false;
        // A link that is not resolved is neither a file nor a directory.
        const bool asIs = link && !has(IteratorFlag::ResolveSymlinks);
        const bool dir = !asIs && info.isDir(), file = !asIs && info.isFile();
        if ((dir && has(IteratorFlag::ExcludeDirs)) || (file && has(IteratorFlag::ExcludeFiles)) ||
            (!dir && !file && has(IteratorFlag::ExcludeSpecial)))
            return false;
        if (filters.empty())
            return true;
        for (const QRegularExpression &filter : filters) {
            if (filter.match(name).hasMatch())
                return true;
        }
        return false;
    }

    // Moves to the next entry that is listed. Returns false at the end.
    bool next(QFileInfo *out) {
        if (!started) {
            started = true;
            enter(path);
        }
        while (!stack.empty()) {
            Level &level = stack.back();
            if (level.index >= level.entries.size()) {
                stack.pop_back();
                continue;
            }
            const QFileInfo info = level.entries.at(level.index++);
            const QString name = info.fileName();
            const bool dots = name == QString(".") || name == QString("..");
            const bool wanted = listed(info, dots);
            // The entries of a directory follow the directory. `level` is not used after this.
            const bool hidden = name.startsWith(QChar(u'.')) && !has(IteratorFlag::IncludeHidden);
            if (!dots && !hidden && has(IteratorFlag::Recursive) && info.isDir() &&
                (!info.isSymLink() || has(IteratorFlag::FollowDirSymlinks)))
                enter(info.filePath());
            if (wanted) {
                *out = info;
                return true;
            }
        }
        return false;
    }
};

/*!
    Constructs a listing of the directory \a path with the given \a flags. Nothing is read until
    begin() is called.

    \sa begin()
*/
QDirListing::QDirListing(const QString &path, IteratorFlags flags) : QDirListing(path, QStringList(), flags) {}

/*!
    \overload
    Constructs a listing of the entries of \a path whose names match one of \a nameFilters, with
    the given \a flags. The filters are wildcard patterns such as \c{*.txt}, matched
    case-insensitively unless the flags have \c CaseSensitive. An empty list matches every name.
*/
QDirListing::QDirListing(const QString &path, const QStringList &nameFilters, IteratorFlags flags)
    : d(std::make_unique<Private>()) {
    d->path = path;
    d->nameFilters = nameFilters;
    d->flags = flags;
    const Qt::CaseSensitivity cs = d->has(IteratorFlag::CaseSensitive) ? Qt::CaseSensitive : Qt::CaseInsensitive;
    for (const QString &filter : nameFilters)
        d->filters.push_back(QRegularExpression::fromWildcard(filter, cs));
}

/*!
    Move-constructs a listing from \a other, which may only be destroyed or assigned to afterwards.
*/
QDirListing::QDirListing(QDirListing &&other) noexcept = default;

/*!
    Move-assigns \a other to this listing and returns a reference to it.
*/
QDirListing &QDirListing::operator=(QDirListing &&other) noexcept = default;

/*!
    Destroys the listing. Its iterators must not be used afterwards.
*/
QDirListing::~QDirListing() = default;

/*!
    Returns the directory that the listing was constructed with.
*/
QString QDirListing::iteratorPath() const { return d->path; }

/*!
    Returns the flags that the listing was constructed with.
*/
QDirListing::IteratorFlags QDirListing::iteratorFlags() const { return d->flags; }

/*!
    Returns the name filters that the listing was constructed with.
*/
QStringList QDirListing::nameFilters() const { return d->nameFilters; }

/*!
    Starts the listing and returns an iterator at its first entry, or one that is equal to end()
    if there is none. Call this once for each QDirListing; a second call continues where the
    first iterator is.

    \sa end()
*/
QDirListing::const_iterator QDirListing::begin() const {
    const_iterator it;
    it.m_listing = this;
    return ++it;
}

/*!
    Moves the iterator to the next entry and returns it. The iterator is equal to
    QDirListing::end() when there are no more entries.
*/
QDirListing::const_iterator &QDirListing::const_iterator::operator++() {
    if (m_listing && !m_listing->d->next(&m_entry.m_info))
        m_listing = nullptr;
    return *this;
}
