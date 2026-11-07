// MIT License - see LICENSE
#include <QtCore/qfilesystemwatcher.h>

#include <QtCore/qdebug.h>
#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qmap.h>
#include <QtCore/qpointer.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qtimer.h>

#include <dirent.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <vector>

namespace {

constexpr int kPollIntervalMs = 200;
constexpr uint32_t kFileMask = IN_ATTRIB | IN_MODIFY | IN_MOVE_SELF | IN_DELETE_SELF;
constexpr uint32_t kDirMask = IN_ATTRIB | IN_MOVE | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF;
constexpr uint32_t kGoneMask = IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT | IN_IGNORED;

struct Snapshot {
    bool exists = false;
    bool isDir = false;
    qint64 size = 0;
    qint64 mtimeNs = 0;
    qint64 inode = 0;
    unsigned mode = 0;
    QStringList names;

    bool operator==(const Snapshot &o) const {
        return exists == o.exists && isDir == o.isDir && size == o.size && mtimeNs == o.mtimeNs && inode == o.inode &&
               mode == o.mode && names == o.names;
    }
    bool operator!=(const Snapshot &o) const { return !(*this == o); }
};

Snapshot takeSnapshot(const QString &absolutePath) {
    Snapshot s;
    const QByteArray encoded = absolutePath.toUtf8();
    struct stat st;
    if (::stat(encoded.constData(), &st) != 0)
        return s;
    s.exists = true;
    s.isDir = S_ISDIR(st.st_mode);
    s.size = static_cast<qint64>(st.st_size);
    s.mtimeNs = static_cast<qint64>(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec;
    s.inode = static_cast<qint64>(st.st_ino);
    s.mode = static_cast<unsigned>(st.st_mode);
    if (s.isDir) {
        if (DIR *d = ::opendir(encoded.constData())) {
            while (const dirent *e = ::readdir(d))
                s.names.append(QString::fromUtf8(e->d_name));
            ::closedir(d);
        }
        s.names.sort();
    }
    return s;
}

QString normalized(const QString &path) {
    QString p = path;
    while (p.size() > 1 && p.endsWith(QChar(u'/')))
        p.chop(1);
    return p;
}

} // namespace

struct QFileSystemWatcher::Private {
    struct Entry {
        bool isDir = false;
        int wd = -1;
        QString absolute;
        Snapshot snapshot;
    };

    explicit Private(QFileSystemWatcher *watcher) : q(watcher) {}
    ~Private() {
        if (inotifyFd >= 0)
            ::close(inotifyFd);
    }
    Private(const Private &) = delete;
    Private &operator=(const Private &) = delete;

    QFileSystemWatcher *q;
    QStringList fileList;
    QStringList dirList;
    QMap<QString, Entry> entries;
    QMap<int, QStringList> wdPaths;
    int inotifyFd = -1;
    QSocketNotifier *notifier = nullptr;
    QTimer *pollTimer = nullptr;
    bool engineChosen = false;
    bool forcePoller = false;

    void chooseEngine() {
        if (engineChosen)
            return;
        engineChosen = true;
        // Qt's tests select an engine through the object name; the poller one is honoured here as well.
        forcePoller = q->objectName().startsWith(QString::fromUtf8("_qt_autotest_force_engine_poller"));
        if (forcePoller)
            return;
        inotifyFd = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (inotifyFd < 0)
            return;
        notifier = new QSocketNotifier(inotifyFd, QSocketNotifier::Read, q);
        QObject::connect(notifier, &QSocketNotifier::activated, q, [this] { readEvents(); });
    }

    void updatePoller() {
        bool needed = false;
        for (auto it = entries.cbegin(); it != entries.cend(); ++it)
            if (it.value().wd < 0)
                needed = true;
        if (needed && !pollTimer) {
            pollTimer = new QTimer(q);
            QObject::connect(pollTimer, &QTimer::timeout, q, [this] { poll(); });
            pollTimer->start(kPollIntervalMs);
        } else if (!needed && pollTimer) {
            delete pollTimer;
            pollTimer = nullptr;
        }
    }

    bool add(const QString &path) {
        if (entries.contains(path))
            return false;
        const QFileInfo info(path);
        if (!info.exists())
            return false;
        Entry entry;
        entry.isDir = info.isDir();
        entry.absolute = info.absoluteFilePath();
        if (inotifyFd >= 0) {
            const int wd =
                ::inotify_add_watch(inotifyFd, path.toUtf8().constData(), entry.isDir ? kDirMask : kFileMask);
            if (wd >= 0) {
                entry.wd = wd;
            } else if (errno == ENOSPC || errno == ENOMEM) {
                qWarning("QFileSystemWatcher: inotify_add_watch failed for \"%s\": %s; polling it instead",
                         path.toUtf8().constData(), std::strerror(errno));
            } else {
                return false;
            }
        }
        if (entry.wd >= 0)
            wdPaths[entry.wd].append(path);
        else
            entry.snapshot = takeSnapshot(entry.absolute);
        (entry.isDir ? dirList : fileList).append(path);
        entries.insert(path, entry);
        updatePoller();
        return true;
    }

    bool remove(const QString &path) {
        const auto it = entries.find(path);
        if (it == entries.end())
            return false;
        const bool isDir = it.value().isDir;
        const int wd = it.value().wd;
        entries.erase(it);
        (isDir ? dirList : fileList).removeAll(path);
        if (wd >= 0) {
            QStringList &paths = wdPaths[wd];
            paths.removeAll(path);
            if (paths.isEmpty()) {
                wdPaths.remove(wd);
                ::inotify_rm_watch(inotifyFd, wd);
            }
        }
        updatePoller();
        return true;
    }

    void notify(const QString &path, bool isDir) {
        if (isDir)
            q->directoryChanged(path);
        else
            q->fileChanged(path);
    }

    struct Summary {
        int wd;
        bool changed = false;
        bool gone = false;
    };

    void readEvents() {
        std::vector<Summary> summaries;
        bool overflow = false;
        auto summaryFor = [&summaries](int wd) -> Summary & {
            for (Summary &s : summaries)
                if (s.wd == wd)
                    return s;
            summaries.push_back(Summary{wd});
            return summaries.back();
        };
        alignas(inotify_event) char buffer[16384];
        for (;;) {
            const ssize_t n = ::read(inotifyFd, buffer, sizeof buffer);
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0)
                break;
            for (ssize_t offset = 0; offset + static_cast<ssize_t>(sizeof(inotify_event)) <= n;) {
                inotify_event ev;
                std::memcpy(&ev, buffer + offset, sizeof ev);
                offset += static_cast<ssize_t>(sizeof ev) + static_cast<ssize_t>(ev.len);
                if (ev.mask & IN_Q_OVERFLOW) {
                    overflow = true;
                    continue;
                }
                const auto paths = wdPaths.constFind(ev.wd);
                if (paths == wdPaths.constEnd())
                    continue;
                Summary &s = summaryFor(ev.wd);
                if (ev.mask & kGoneMask) {
                    s.gone = true;
                    continue;
                }
                const bool isDirWatch = entries.value(paths.value().first()).isDir;
                // The attributes of a child change the directory's inode, but not its listing.
                if (isDirWatch && ev.len > 0 && (ev.mask & ~uint32_t(IN_ISDIR)) == IN_ATTRIB)
                    continue;
                s.changed = true;
            }
        }
        if (overflow) {
            for (auto it = entries.cbegin(); it != entries.cend(); ++it)
                if (it.value().wd >= 0)
                    summaryFor(it.value().wd).changed = true;
        }
        QPointer<QFileSystemWatcher> guard(q);
        for (const Summary &s : summaries) {
            const QStringList paths = wdPaths.value(s.wd);
            for (const QString &path : paths) {
                if (!guard)
                    return;
                const auto it = entries.constFind(path);
                if (it == entries.constEnd())
                    continue;
                const bool isDir = it.value().isDir;
                if (s.gone) {
                    remove(path);
                    notify(path, isDir);
                } else if (s.changed) {
                    notify(path, isDir);
                }
            }
        }
    }

    void poll() {
        QPointer<QFileSystemWatcher> guard(q);
        const QStringList paths = entries.keys();
        for (const QString &path : paths) {
            if (!guard)
                return;
            const auto it = entries.find(path);
            if (it == entries.end() || it.value().wd >= 0)
                continue;
            const Snapshot now = takeSnapshot(it.value().absolute);
            if (now == it.value().snapshot)
                continue;
            const bool isDir = it.value().isDir;
            // A different inode is a replaced file, which inotify reports as the removal of the old one.
            if (!now.exists || now.isDir != isDir || now.inode != it.value().snapshot.inode) {
                remove(path);
            } else {
                it.value().snapshot = now;
            }
            notify(path, isDir);
        }
    }
};

/*!
    \class QFileSystemWatcher
    \brief The QFileSystemWatcher class provides an interface for monitoring files and directories for modifications.

    QFileSystemWatcher monitors the file system for changes to files and
    directories by watching a list of specified paths.

    Call addPath() to watch a particular file or directory. Multiple paths
    can be added using the addPaths() function. Existing paths can be removed
    by using the removePath() and removePaths() functions.

    QFileSystemWatcher examines each path added to it. Files that have been
    added to the QFileSystemWatcher can be accessed using the files()
    function, and directories using the directories() function.

    The fileChanged() signal is emitted when a file has been modified,
    renamed or removed from disk. Similarly, the directoryChanged() signal is
    emitted when a directory or its contents is modified or removed. Note
    that QFileSystemWatcher stops monitoring files once they have been
    renamed or removed from disk, and directories once they have been
    removed from disk.

    \code
    QFileSystemWatcher watcher;
    watcher.addPath("/etc/hosts");
    QObject::connect(&watcher, &QFileSystemWatcher::fileChanged, [&](const QString &path) {
        if (QFileInfo::exists(path))
            watcher.addPath(path);
    });
    \endcode

    The watcher uses inotify through a QSocketNotifier, so it needs a running
    event loop to emit its signals. Events are merged per read: a burst of
    changes of one path can produce a single signal.

    \differences
    The platform is Linux only, using inotify. If inotify is not available,
    or the kernel limit of watches is reached for a path, that path is polled
    by comparing \c stat() results and directory listings every 200 ms
    instead, which cannot see changes that are undone within an interval. The
    polled engine can be forced for all paths by giving the watcher the
    object name \c{_qt_autotest_force_engine_poller} before adding paths, as
    in Qt's tests. Paths are watched as written: two spellings of the same
    file are two watches. There is no signal parameter \c QPrivateSignal.
    \enddifferences

    \sa QFile, QDir
*/

/*!
    Constructs a new file system watcher object with the given \a parent.

    \sa addPath(), addPaths()
*/
QFileSystemWatcher::QFileSystemWatcher(QObject *parent) : QObject(parent), d(std::make_unique<Private>(this)) {}

/*!
    Constructs a new file system watcher object with the given \a parent and
    starts watching the specified \a paths.

    \sa addPaths()
*/
QFileSystemWatcher::QFileSystemWatcher(const QStringList &paths, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(this)) {
    addPaths(paths);
}

/*!
    Destroys the file system watcher.
*/
QFileSystemWatcher::~QFileSystemWatcher() = default;

/*!
    Adds \a file to the file system watcher if the path exists. The path is
    not added if it does not exist, or if it is already being monitored by
    the file system watcher.

    If \a file specifies a directory, the directoryChanged() signal will be
    emitted when \a file is modified or removed from disk; otherwise the
    fileChanged() signal is emitted when \a file is modified, renamed or
    removed.

    Returns \c true if the path was added; otherwise returns \c false.
    The paths that could not be added can be had by using addPaths().

    \sa addPaths(), removePath(), files(), directories()
*/
bool QFileSystemWatcher::addPath(const QString &file) {
    if (file.isEmpty()) {
        qWarning("QFileSystemWatcher::addPath: path is empty");
        return true;
    }
    return addPaths(QStringList(file)).isEmpty();
}

/*!
    Adds each path in \a files to the file system watcher. Paths are not
    added if they do not exist, or if they are already being monitored by the
    file system watcher.

    The return value is a list of paths that could not be watched.

    \sa addPath(), removePaths()
*/
QStringList QFileSystemWatcher::addPaths(const QStringList &files) {
    if (files.isEmpty()) {
        qWarning("QFileSystemWatcher::addPaths: list is empty");
        return QStringList();
    }
    d->chooseEngine();
    QStringList failed;
    for (const QString &file : files) {
        const QString path = normalized(file);
        if (path.isEmpty() || !d->add(path))
            failed.append(file);
    }
    return failed;
}

/*!
    Removes the specified \a file from the file system watcher.

    Returns \c true if the path was being watched and has been removed;
    otherwise returns \c false.

    \sa removePaths(), addPath()
*/
bool QFileSystemWatcher::removePath(const QString &file) {
    if (file.isEmpty()) {
        qWarning("QFileSystemWatcher::removePath: path is empty");
        return true;
    }
    return removePaths(QStringList(file)).isEmpty();
}

/*!
    Removes the specified \a files from the file system watcher.

    The return value is a list of paths that were not being watched.

    \sa removePath(), addPaths()
*/
QStringList QFileSystemWatcher::removePaths(const QStringList &files) {
    if (files.isEmpty()) {
        qWarning("QFileSystemWatcher::removePaths: list is empty");
        return QStringList();
    }
    QStringList failed;
    for (const QString &file : files)
        if (!d->remove(normalized(file)))
            failed.append(file);
    return failed;
}

/*!
    Returns a list of paths to files that are being watched.

    \sa directories()
*/
QStringList QFileSystemWatcher::files() const { return d->fileList; }

/*!
    Returns a list of paths to directories that are being watched.

    \sa files()
*/
QStringList QFileSystemWatcher::directories() const { return d->dirList; }
