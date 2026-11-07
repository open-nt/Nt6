// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/qfilesystemwatcher.h>
#include <QtCore/qtemporarydir.h>

#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static const char *g_engine = "inotify";

static bool writeFile(const QString &path, const QByteArray &data, QIODevice::OpenMode mode = QIODevice::WriteOnly) {
    QFile f(path);
    return f.open(mode) && f.write(data) == data.size();
}

// Runs the event loop for at most timeoutMs, or until cond() holds.
static bool waitUntil(const std::function<bool()> &cond, int timeoutMs = 15000) {
    QElapsedTimer clock;
    clock.start();
    QEventLoop loop;
    QTimer tick;
    QObject::connect(&tick, &QTimer::timeout, [&] {
        if (cond() || clock.elapsed() > timeoutMs)
            loop.quit();
    });
    tick.start(5);
    if (!cond())
        loop.exec();
    return cond();
}

// Lets the event loop run for a while, to show that something does not happen.
static void settle(int ms = 600) {
    QElapsedTimer clock;
    clock.start();
    waitUntil([&] { return clock.elapsed() >= ms; }, ms + 5000);
}

struct Watch {
    explicit Watch(const QString &name = QString()) {
        if (std::strcmp(g_engine, "poller") == 0)
            watcher.setObjectName("_qt_autotest_force_engine_poller");
        (void)name;
        QObject::connect(&watcher, &QFileSystemWatcher::fileChanged,
                         [this](const QString &p) { fileEvents.append(p); });
        QObject::connect(&watcher, &QFileSystemWatcher::directoryChanged,
                         [this](const QString &p) { dirEvents.append(p); });
    }
    QFileSystemWatcher watcher;
    QStringList fileEvents;
    QStringList dirEvents;
    qsizetype fileCount(const QString &p) const { return fileEvents.count(p); }
    qsizetype dirCount(const QString &p) const { return dirEvents.count(p); }
};

static void testAddRemove() {
    QTemporaryDir tmp;
    const QString file = tmp.path() + "/a.txt";
    const QString sub = tmp.path() + "/sub";
    CHECK(writeFile(file, "x") && QDir(tmp.path()).mkdir("sub"));

    Watch w;
    QFileSystemWatcher &fw = w.watcher;
    CHECK(fw.files().isEmpty() && fw.directories().isEmpty());
    CHECK(fw.addPath(file));
    CHECK(fw.files() == QStringList({file}) && fw.directories().isEmpty());
    CHECK(!fw.addPath(file));
    CHECK(fw.addPath(sub + "/"));
    CHECK(fw.directories() == QStringList({sub}));
    CHECK(!fw.addPath(sub));
    CHECK(!fw.addPath(tmp.path() + "/missing"));
    const QStringList failed = fw.addPaths({tmp.path(), tmp.path() + "/missing1", file, tmp.path() + "/missing2"});
    CHECK(failed == QStringList({tmp.path() + "/missing1", file, tmp.path() + "/missing2"}));
    CHECK(fw.directories() == QStringList({sub, tmp.path()}));

    CHECK(fw.removePath(file) && fw.files().isEmpty());
    CHECK(!fw.removePath(file));
    CHECK(fw.removePaths({sub, "/not/watched", tmp.path() + "/"}) == QStringList({"/not/watched"}));
    CHECK(fw.directories().isEmpty());

    QFileSystemWatcher constructed(QStringList() << file << sub, nullptr);
    CHECK(constructed.files() == QStringList({file}) && constructed.directories() == QStringList({sub}));
}

static void testFileChanges() {
    QTemporaryDir tmp;
    const QString file = tmp.path() + "/watched.txt";
    const QString other = tmp.path() + "/other.txt";
    CHECK(writeFile(file, "one") && writeFile(other, "x"));
    Watch w;
    CHECK(w.watcher.addPath(file));

    // Modification.
    CHECK(writeFile(file, "two", QIODevice::Append | QIODevice::WriteOnly));
    CHECK(waitUntil([&] { return w.fileCount(file) >= 1; }));
    CHECK(w.watcher.files() == QStringList({file}));

    // Attribute change.
    w.fileEvents.clear();
    CHECK(::chmod(file.toUtf8().constData(), 0600) == 0);
    CHECK(waitUntil([&] { return w.fileCount(file) >= 1; }));

    // Truncation and rewrite.
    w.fileEvents.clear();
    CHECK(writeFile(file, "rewritten"));
    CHECK(waitUntil([&] { return w.fileCount(file) >= 1; }));
    settle(300);
    CHECK(w.fileEvents.size() < 10);

    // Other files in the directory are none of its business.
    w.fileEvents.clear();
    CHECK(writeFile(other, "y"));
    settle(400);
    CHECK(w.fileEvents.isEmpty());

    // Removed: reported once, and no longer watched.
    w.fileEvents.clear();
    CHECK(QFile::remove(file));
    CHECK(waitUntil([&] { return !w.watcher.files().contains(file); }));
    CHECK(w.fileCount(file) >= 1 && w.watcher.files().isEmpty());
    settle(300);
    w.fileEvents.clear();

    // Created again: add it again and it works.
    CHECK(writeFile(file, "back"));
    CHECK(w.watcher.addPath(file));
    CHECK(writeFile(file, "more", QIODevice::Append | QIODevice::WriteOnly));
    CHECK(waitUntil([&] { return w.fileCount(file) >= 1; }));
}

static void testFileRenames() {
    QTemporaryDir tmp;
    const QString file = tmp.path() + "/doc.txt";
    CHECK(writeFile(file, "v1"));
    Watch w;
    CHECK(w.watcher.addPath(file));

    // Renamed away.
    CHECK(::rename(file.toUtf8().constData(), (tmp.path() + "/moved.txt").toUtf8().constData()) == 0);
    CHECK(waitUntil([&] { return w.fileCount(file) >= 1 && w.watcher.files().isEmpty(); }));

    // The atomic save pattern: a new file is renamed over the watched one.
    CHECK(writeFile(file, "v2"));
    CHECK(w.watcher.addPath(file));
    w.fileEvents.clear();
    CHECK(writeFile(tmp.path() + "/doc.txt.new", "v3"));
    CHECK(::rename((tmp.path() + "/doc.txt.new").toUtf8().constData(), file.toUtf8().constData()) == 0);
    CHECK(waitUntil([&] { return w.fileCount(file) >= 1; }));
    // The watch followed the old inode, which is gone now; the application re-adds the path.
    CHECK(waitUntil([&] { return !w.watcher.files().contains(file); }));
    CHECK(w.watcher.addPath(file));
    w.fileEvents.clear();
    CHECK(writeFile(file, "v4", QIODevice::Append | QIODevice::WriteOnly));
    CHECK(waitUntil([&] { return w.fileCount(file) >= 1; }));
}

static void testDirectoryChanges() {
    QTemporaryDir tmp;
    const QString dir = tmp.path() + "/watched";
    CHECK(QDir(tmp.path()).mkdir("watched"));
    CHECK(writeFile(dir + "/existing.txt", "e"));
    Watch w;
    CHECK(w.watcher.addPath(dir));

    // A file is created.
    CHECK(writeFile(dir + "/new.txt", "n"));
    CHECK(waitUntil([&] { return w.dirCount(dir) >= 1; }));
    settle(300);

    // Modifying the content of an entry is not a change of the directory.
    w.dirEvents.clear();
    CHECK(writeFile(dir + "/existing.txt", "changed", QIODevice::Append | QIODevice::WriteOnly));
    CHECK(::chmod((dir + "/existing.txt").toUtf8().constData(), 0600) == 0);
    settle(500);
    CHECK(w.dirEvents.isEmpty());

    // A file is deleted.
    CHECK(QFile::remove(dir + "/new.txt"));
    CHECK(waitUntil([&] { return w.dirCount(dir) >= 1; }));
    settle(300);

    // A file is renamed inside, and a directory created.
    w.dirEvents.clear();
    CHECK(::rename((dir + "/existing.txt").toUtf8().constData(), (dir + "/renamed.txt").toUtf8().constData()) == 0);
    CHECK(waitUntil([&] { return w.dirCount(dir) >= 1; }));
    settle(300);
    w.dirEvents.clear();
    CHECK(QDir(dir).mkdir("child"));
    CHECK(waitUntil([&] { return w.dirCount(dir) >= 1; }));
    settle(300);

    // Changes in a sub-directory are not seen.
    w.dirEvents.clear();
    CHECK(writeFile(dir + "/child/inner.txt", "i"));
    settle(500);
    CHECK(w.dirEvents.isEmpty());
    CHECK(w.fileEvents.isEmpty());

    // A file moved out of the directory.
    CHECK(::rename((dir + "/renamed.txt").toUtf8().constData(), (tmp.path() + "/outside.txt").toUtf8().constData()) ==
          0);
    CHECK(waitUntil([&] { return w.dirCount(dir) >= 1; }));
    settle(300);

    // The directory itself is removed.
    w.dirEvents.clear();
    CHECK(QDir(dir).removeRecursively());
    CHECK(waitUntil([&] { return !w.watcher.directories().contains(dir); }));
    CHECK(w.dirCount(dir) >= 1);
}

static void testFileAndDirectoryTogether() {
    QTemporaryDir tmp;
    const QString file = tmp.path() + "/f.txt";
    CHECK(writeFile(file, "1"));
    Watch w;
    CHECK(w.watcher.addPaths({tmp.path(), file}).isEmpty());
    CHECK(writeFile(file, "2", QIODevice::Append | QIODevice::WriteOnly));
    CHECK(waitUntil([&] { return w.fileCount(file) >= 1; }));
    settle(400);
    CHECK(w.dirEvents.isEmpty());
    CHECK(writeFile(tmp.path() + "/g.txt", "g"));
    CHECK(waitUntil([&] { return w.dirCount(tmp.path()) >= 1; }));
    CHECK(QFile::remove(file));
    CHECK(waitUntil([&] { return w.watcher.files().isEmpty(); }));
    CHECK(w.fileCount(file) >= 1);
    CHECK(w.watcher.directories() == QStringList({tmp.path()}));
}

static void testRemovedPathStaysQuiet() {
    QTemporaryDir tmp;
    const QString file = tmp.path() + "/q.txt";
    CHECK(writeFile(file, "1"));
    Watch w;
    CHECK(w.watcher.addPath(file));
    CHECK(w.watcher.addPath(tmp.path()));
    CHECK(w.watcher.removePath(file));
    CHECK(w.watcher.removePath(tmp.path()));
    CHECK(writeFile(file, "2", QIODevice::Append | QIODevice::WriteOnly));
    CHECK(writeFile(tmp.path() + "/new", "2"));
    settle(500);
    CHECK(w.fileEvents.isEmpty() && w.dirEvents.isEmpty());
}

static void testManyPaths() {
    QTemporaryDir tmp;
    const int n = 150;
    QStringList files;
    for (int i = 0; i < n; ++i) {
        files.append(tmp.path() + "/f" + QString::number(i));
        CHECK(writeFile(files.last(), "x"));
    }
    Watch w;
    CHECK(w.watcher.addPaths(files).isEmpty());
    CHECK(w.watcher.files().size() == n);
    for (const QString &f : files)
        CHECK(writeFile(f, "y", QIODevice::Append | QIODevice::WriteOnly));
    CHECK(waitUntil([&] {
        for (const QString &f : files)
            if (w.fileCount(f) == 0)
                return false;
        return true;
    }));
    CHECK(w.watcher.removePaths(files).isEmpty() && w.watcher.files().isEmpty());
}

static void testSameFileTwoNames() {
    QTemporaryDir tmp;
    const QString file = tmp.path() + "/real.txt";
    const QString link = tmp.path() + "/hard.txt";
    CHECK(writeFile(file, "1"));
    CHECK(::link(file.toUtf8().constData(), link.toUtf8().constData()) == 0);
    Watch w;
    CHECK(w.watcher.addPath(file) && w.watcher.addPath(link));
    CHECK(writeFile(file, "2", QIODevice::Append | QIODevice::WriteOnly));
    CHECK(waitUntil([&] { return w.fileCount(file) >= 1 && w.fileCount(link) >= 1; }));
    CHECK(w.watcher.removePath(file));
    w.fileEvents.clear();
    CHECK(writeFile(file, "3", QIODevice::Append | QIODevice::WriteOnly));
    CHECK(waitUntil([&] { return w.fileCount(link) >= 1; }));
    CHECK(w.fileCount(file) == 0);
}

static void testDeleteInSlot() {
    QTemporaryDir tmp;
    const QString file = tmp.path() + "/d.txt";
    CHECK(writeFile(file, "1"));
    auto *watcher = new QFileSystemWatcher;
    if (std::strcmp(g_engine, "poller") == 0)
        watcher->setObjectName("_qt_autotest_force_engine_poller");
    CHECK(watcher->addPath(file));
    CHECK(watcher->addPath(tmp.path()));
    bool gone = false;
    QObject::connect(watcher, &QFileSystemWatcher::fileChanged, [&](const QString &) {
        delete watcher;
        gone = true;
    });
    CHECK(writeFile(file, "2", QIODevice::Append | QIODevice::WriteOnly));
    CHECK(waitUntil([&] { return gone; }));
    settle(200);
}

static void testRemoveInSlot() {
    QTemporaryDir tmp;
    const QString a = tmp.path() + "/a";
    const QString b = tmp.path() + "/b";
    CHECK(writeFile(a, "1") && writeFile(b, "1"));
    Watch w;
    CHECK(w.watcher.addPaths({a, b}).isEmpty());
    QObject::connect(&w.watcher, &QFileSystemWatcher::fileChanged,
                     [&](const QString &) { w.watcher.removePaths({a, b}); });
    CHECK(writeFile(a, "2", QIODevice::Append | QIODevice::WriteOnly));
    CHECK(writeFile(b, "2", QIODevice::Append | QIODevice::WriteOnly));
    CHECK(waitUntil([&] { return w.watcher.files().isEmpty(); }));
    settle(300);
}

static void testInThread() {
    QTemporaryDir tmp;
    const QString file = tmp.path() + "/t.txt";
    CHECK(writeFile(file, "1"));
    std::atomic<int> fileSignals{0};
    std::atomic<int> dirSignals{0};
    std::atomic<bool> ready{false};
    QThread *thread = QThread::create([&] {
        QEventLoop loop;
        QFileSystemWatcher watcher;
        if (std::strcmp(g_engine, "poller") == 0)
            watcher.setObjectName("_qt_autotest_force_engine_poller");
        QObject::connect(&watcher, &QFileSystemWatcher::fileChanged, [&](const QString &) {
            ++fileSignals;
            loop.quit();
        });
        QObject::connect(&watcher, &QFileSystemWatcher::directoryChanged, [&](const QString &) { ++dirSignals; });
        watcher.addPaths({file, tmp.path()});
        QTimer guard;
        guard.setSingleShot(true);
        QObject::connect(&guard, &QTimer::timeout, [&] { loop.quit(); });
        guard.start(15000);
        ready = true;
        loop.exec();
    });
    thread->start();
    CHECK(waitUntil([&] { return ready.load(); }));
    QThread::msleep(100);
    CHECK(writeFile(file, "2", QIODevice::Append | QIODevice::WriteOnly));
    CHECK(thread->wait(30000));
    delete thread;
    CHECK(fileSignals >= 1);
}

static void runAll() {
    testAddRemove();
    testFileChanges();
    testFileRenames();
    testDirectoryChanges();
    testFileAndDirectoryTogether();
    testRemovedPathStaysQuiet();
    testManyPaths();
    testSameFileTwoNames();
    testDeleteInSlot();
    testRemoveInSlot();
    testInThread();
}

int main() {
    runAll();
    g_engine = "poller";
    runAll();
    if (g_failures == 0)
        std::printf("test_filesystemwatcher: all tests passed\n");
    else
        std::printf("test_filesystemwatcher: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
