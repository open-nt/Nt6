// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/qdiriterator.h>
#include <QtCore/qlockfile.h>
#include <QtCore/qtemporarydir.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <thread>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static bool writeFile(const QString &path, const QByteArray &data) {
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}

static QByteArray readFile(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

static void setTimes(const QString &path, long mtime) {
    timeval tv[2] = {{mtime, 0}, {mtime, 0}};
    ::utimes(path.toUtf8().constData(), tv);
}

static QString hostName() {
    char buf[256] = {};
    ::gethostname(buf, sizeof buf - 1);
    return QString::fromUtf8(buf);
}

static QString selfExe() {
    char buf[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof buf);
    return n > 0 ? QString::fromUtf8(buf, n) : QString();
}

// ---- QDirIterator -------------------------------------------------------------------------

static QStringList iterate(QDirIterator &it) {
    QStringList out;
    while (it.hasNext())
        out.append(it.next());
    return out;
}

static QStringList relative(const QStringList &paths, const QString &root) {
    QStringList out;
    for (const QString &p : paths)
        out.append(p.startsWith(root + "/") ? p.mid(root.size() + 1) : p);
    out.sort();
    return out;
}

static void testDirIterator() {
    QTemporaryDir tmp;
    const QString root = tmp.path();
    QDir dir(root);
    CHECK(dir.mkpath("sub1/deep") && dir.mkpath("sub2") && dir.mkpath("empty") && dir.mkpath(".hiddendir"));
    CHECK(writeFile(root + "/a.txt", "a") && writeFile(root + "/b.cpp", "bb"));
    CHECK(writeFile(root + "/.hidden", "h") && writeFile(root + "/sub1/c.txt", "c"));
    CHECK(writeFile(root + "/sub1/d.h", "d") && writeFile(root + "/sub1/deep/e.txt", "e"));
    CHECK(writeFile(root + "/sub2/f.txt", "f") && writeFile(root + "/.hiddendir/g.txt", "g"));
    CHECK(::symlink("sub1", (root + "/linkdir").toUtf8().constData()) == 0);
    CHECK(::symlink(".", (root + "/loop").toUtf8().constData()) == 0);
    CHECK(::symlink("a.txt", (root + "/linkfile").toUtf8().constData()) == 0);
    CHECK(::symlink("missing-target", (root + "/broken").toUtf8().constData()) == 0);

    {
        // The default is what QDir::entryList() gives for the directory, in directory order.
        QDirIterator it(root);
        const QStringList got = relative(iterate(it), root);
        QStringList expect = QDir(root).entryList(QDir::AllEntries, QDir::SortFlags(QDir::Name));
        expect.sort();
        CHECK(got == expect);
        CHECK(got.contains(".") && got.contains("..") && got.contains("a.txt") && got.contains("sub1") &&
              got.contains("linkdir") && got.contains("linkfile") && !got.contains(".hidden") &&
              !got.contains("broken"));
        CHECK(!got.contains("c.txt"));
        CHECK(it.path() == root && !it.hasNext() && it.next().isEmpty());
        CHECK(it.nextFileInfo().filePath().isEmpty());
    }
    {
        QDirIterator it(root, QDir::Files | QDir::NoDotAndDotDot);
        CHECK(it.fileName().isEmpty() && it.filePath().isEmpty() && !it.fileInfo().exists());
        QStringList names;
        while (it.hasNext()) {
            const QString path = it.next();
            CHECK(path == root + "/" + it.fileName() && it.filePath() == path);
            CHECK(it.fileInfo().filePath() == path && it.fileInfo().isFile());
            names.append(it.fileName());
        }
        names.sort();
        CHECK(names == QStringList({"a.txt", "b.cpp", "linkfile"}));
    }
    {
        QDirIterator it(root, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
        QStringList names = relative(iterate(it), root);
        CHECK(names == QStringList({".hidden", "a.txt", "b.cpp", "linkfile"}));
    }
    {
        QDirIterator it(root, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
        CHECK(relative(iterate(it), root) == QStringList({"empty", "sub1", "sub2"}));
    }
    {
        QDirIterator it(root, QStringList() << "*.txt", QDir::Files);
        CHECK(relative(iterate(it), root) == QStringList({"a.txt"}));
    }
    {
        QDirIterator it(root, QStringList() << "*.txt", QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        CHECK(relative(iterate(it), root) == QStringList({"a.txt", "sub1/c.txt", "sub1/deep/e.txt", "sub2/f.txt"}));
    }
    {
        QDirIterator it(root, QStringList() << "*.txt", QDir::Files | QDir::Hidden | QDir::NoSymLinks,
                        QDirIterator::Subdirectories);
        CHECK(relative(iterate(it), root) ==
              QStringList({".hiddendir/g.txt", "a.txt", "sub1/c.txt", "sub1/deep/e.txt", "sub2/f.txt"}));
    }
    {
        // Without FollowSymlinks a link to a directory is listed but not entered.
        QDirIterator it(root, QStringList() << "*.txt", QDir::Files, QDirIterator::Subdirectories);
        const QStringList got = relative(iterate(it), root);
        CHECK(got.contains("sub1/c.txt") && !got.contains("linkdir/c.txt") && !got.contains("loop/a.txt"));
    }
    {
        QDirIterator it(root, QStringList() << "*.txt", QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
        const QStringList got = relative(iterate(it), root);
        CHECK(got.contains("sub1/c.txt") && got.contains("linkdir/c.txt") && got.contains("linkdir/deep/e.txt"));
        // The link to "." is a loop; it is not entered, so nothing is listed through it.
        CHECK(!got.contains("loop/a.txt") && !got.contains("loop/sub1/c.txt"));
        CHECK(got.count("a.txt") == 1);
    }
    {
        // A directory comes before its contents, and a directory is entered although it does not match.
        QDirIterator it(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::Subdirectories);
        const QStringList got = iterate(it);
        const qsizetype sub1 = got.indexOf(root + "/sub1");
        const qsizetype c = got.indexOf(root + "/sub1/c.txt");
        const qsizetype deep = got.indexOf(root + "/sub1/deep");
        const qsizetype e = got.indexOf(root + "/sub1/deep/e.txt");
        CHECK(sub1 >= 0 && c > sub1 && deep > sub1 && e > deep);
        CHECK(!got.contains(root + "/.hiddendir") && !got.contains(root + "/.hiddendir/g.txt"));
        CHECK(got.contains(root + "/empty"));
        // Entries of a directory are contiguous with it.
        const qsizetype last = std::max({c, deep, e});
        for (qsizetype i = sub1 + 1; i <= last; ++i)
            CHECK(got.at(i).startsWith(root + "/sub1/"));
    }
    {
        // Directories only, recursively.
        QDirIterator it(root, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::Subdirectories);
        CHECK(relative(iterate(it), root) == QStringList({"empty", "sub1", "sub1/deep", "sub2"}));
    }
    {
        // AllDirs lists directories whatever the name filter says.
        QDirIterator it(root, QStringList() << "*.cpp",
                        QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
        CHECK(relative(iterate(it), root) == QStringList({"b.cpp", "empty", "sub1", "sub2"}));
    }
    {
        // From a QDir, taking its filters; relative paths stay relative.
        QDir d(root);
        d.setNameFilters(QStringList() << "*.cpp");
        d.setFilter(QDir::Files);
        QDirIterator it(d);
        CHECK(relative(iterate(it), root) == QStringList({"b.cpp"}));
        const QString oldCwd = QDir::currentPath();
        CHECK(QDir::setCurrent(root));
        QDirIterator rel("sub1", QDir::Files);
        const QStringList got = iterate(rel);
        CHECK(got.contains("sub1/c.txt") && got.contains("sub1/d.h"));
        CHECK(rel.fileInfo().exists());
        QDir::setCurrent(oldCwd);
    }
    {
        // Nonexistent and empty directories.
        QDirIterator none(root + "/does-not-exist");
        CHECK(!none.hasNext() && none.next().isEmpty());
        QDirIterator empty(root + "/empty", QDir::NoDotAndDotDot);
        CHECK(!empty.hasNext());
        QDirIterator withDots(root + "/empty");
        CHECK(relative(iterate(withDots), root + "/empty") == QStringList({".", ".."}));
    }
    {
        // Permissions: unreadable directories are skipped (root can read everything).
        QDir(root).mkpath("locked/inner");
        CHECK(writeFile(root + "/locked/inner/x.txt", "x"));
        ::chmod((root + "/locked").toUtf8().constData(), 0);
        QDirIterator it(root, QStringList() << "*.txt", QDir::Files, QDirIterator::Subdirectories);
        const QStringList got = relative(iterate(it), root);
        CHECK(got.contains("a.txt"));
        if (::geteuid() != 0)
            CHECK(!got.contains("locked/inner/x.txt"));
        ::chmod((root + "/locked").toUtf8().constData(), 0755);
    }
}

// ---- QLockFile ----------------------------------------------------------------------------

static pid_t deadPid() {
    const pid_t p = ::fork();
    if (p == 0)
        ::_exit(0);
    int status = 0;
    ::waitpid(p, &status, 0);
    return p;
}

static pid_t livePid() {
    const pid_t p = ::fork();
    if (p == 0) {
        for (;;)
            ::pause();
    }
    return p;
}

static void reap(pid_t p) {
    ::kill(p, SIGKILL);
    int status = 0;
    ::waitpid(p, &status, 0);
}

static QByteArray lockContents(pid_t pid, const QString &app, const QString &host) {
    return QByteArray::number(static_cast<qint64>(pid)) + "\n" + app.toUtf8() + "\n" + host.toUtf8() + "\n";
}

static void testLockBasics() {
    QTemporaryDir tmp;
    const QString path = tmp.path() + "/app.lock";
    QLockFile a(path);
    QLockFile b(path);
    CHECK(!a.isLocked() && a.error() == QLockFile::NoError && a.staleLockTime() == 30000);
    CHECK(a.tryLock() && a.isLocked() && a.error() == QLockFile::NoError);
    CHECK(QFileInfo::exists(path));

    qint64 pid = 0;
    QString host;
    QString app;
    CHECK(b.getLockInfo(&pid, &host, &app));
    CHECK(pid == ::getpid() && host == hostName());
    CHECK(app == selfExe());
    CHECK(a.getLockInfo(nullptr, nullptr, nullptr));
    const QList<QByteArray> lines = readFile(path).split('\n');
    CHECK(lines.size() == 4 && lines.at(0).toLongLong() == ::getpid() && lines.at(3).isEmpty());

    CHECK(!b.tryLock() && !b.isLocked() && b.error() == QLockFile::LockFailedError);
    QElapsedTimer t;
    t.start();
    CHECK(!b.tryLock(300));
    CHECK(t.elapsed() >= 250 && t.elapsed() < 5000);
    CHECK(b.error() == QLockFile::LockFailedError);

    // The same object cannot lock twice.
    CHECK(!a.tryLock() && a.isLocked());
    CHECK(a.error() == QLockFile::LockFailedError);

    a.unlock();
    CHECK(!a.isLocked() && !QFileInfo::exists(path));
    a.unlock();
    CHECK(b.tryLock() && b.isLocked() && b.error() == QLockFile::NoError);
    CHECK(!a.tryLock());
    b.unlock();

    // Only the lock file itself is left, and nothing when unlocked.
    CHECK(QDir(tmp.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden).isEmpty());

    // The destructor unlocks.
    {
        QLockFile c(path);
        CHECK(c.lock());
        CHECK(QFileInfo::exists(path));
    }
    CHECK(!QFileInfo::exists(path));

    // No information without a lock file.
    CHECK(!a.getLockInfo(&pid, &host, &app));
    CHECK(writeFile(path, "garbage"));
    CHECK(!a.getLockInfo(&pid, &host, &app));
    CHECK(writeFile(path, "0\napp\nhost\n"));
    CHECK(!a.getLockInfo(&pid, &host, &app));
    CHECK(writeFile(path, "12\napp\nhost\n"));
    CHECK(a.getLockInfo(&pid, &host, &app) && pid == 12 && host == "host" && app == "app");
    ::unlink(path.toUtf8().constData());
}

static void testLockBlocking() {
    QTemporaryDir tmp;
    const QString path = tmp.path() + "/blocking.lock";
    QLockFile holder(path);
    CHECK(holder.lock());
    std::thread releaser([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        holder.unlock();
    });
    QLockFile waiter(path);
    QElapsedTimer t;
    t.start();
    CHECK(waiter.lock());
    CHECK(t.elapsed() >= 250);
    releaser.join();
    CHECK(waiter.isLocked());
    waiter.unlock();

    CHECK(holder.tryLock(0));
    std::thread releaser2([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        holder.unlock();
    });
    CHECK(waiter.tryLock(20000));
    releaser2.join();
    waiter.unlock();
}

static void testLockStale() {
    QTemporaryDir tmp;
    const QString path = tmp.path() + "/stale.lock";

    // A dead process on this host: stale.
    {
        const pid_t dead = deadPid();
        CHECK(writeFile(path, lockContents(dead, selfExe(), hostName())));
        QLockFile l(path);
        CHECK(l.tryLock());
        qint64 pid = 0;
        CHECK(l.getLockInfo(&pid, nullptr, nullptr) && pid == ::getpid());
        l.unlock();
    }
    // A live process that is the program recorded in the file: not stale.
    {
        const pid_t live = livePid();
        CHECK(writeFile(path, lockContents(live, selfExe(), hostName())));
        QLockFile l(path);
        CHECK(!l.tryLock() && l.error() == QLockFile::LockFailedError);
        qint64 pid = 0;
        CHECK(l.getLockInfo(&pid, nullptr, nullptr) && pid == live);
        // The stale check is by age as well; disabling it keeps the lock.
        l.setStaleLockTime(0);
        CHECK(l.staleLockTime() == 0);
        setTimes(path, 1000000000);
        CHECK(!l.tryLock());
        // removeStaleLockFile() removes the file on request.
        CHECK(l.removeStaleLockFile());
        CHECK(!QFileInfo::exists(path));
        CHECK(!l.removeStaleLockFile());
        // A live pid recorded with a different program: the pid was reused, so the lock is stale.
        CHECK(writeFile(path, lockContents(live, "/usr/bin/some-other-program-xyz", hostName())));
        QLockFile m(path);
        CHECK(m.tryLock());
        m.unlock();
        // An application name that is no path cannot be checked.
        CHECK(writeFile(path, lockContents(live, "MyApplication", hostName())));
        CHECK(!m.tryLock());
        // Too old, with the default stale time of 30 s: stale even though the process lives.
        setTimes(path, ::time(nullptr) - 3600);
        CHECK(m.tryLock());
        m.unlock();
        reap(live);
    }
    // Another host: the pid says nothing; only the age counts.
    {
        const pid_t dead = deadPid();
        CHECK(writeFile(path, lockContents(dead, selfExe(), "some-other-host.invalid")));
        QLockFile l(path);
        CHECK(!l.tryLock());
        l.setStaleLockTime(100);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        CHECK(l.tryLock());
        l.unlock();
    }
    // Contents we cannot parse count by age only.
    {
        CHECK(writeFile(path, "not a lock file"));
        QLockFile l(path);
        CHECK(!l.tryLock());
        setTimes(path, ::time(nullptr) - 3600);
        CHECK(l.tryLock());
        l.unlock();
    }
    // The remove lock of a crashed remover does not block recovery either.
    {
        const pid_t dead = deadPid();
        CHECK(writeFile(path, lockContents(dead, selfExe(), hostName())));
        CHECK(writeFile(path + ".rmlock", lockContents(dead, selfExe(), hostName())));
        QLockFile l(path);
        CHECK(l.tryLock());
        l.unlock();
        CHECK(!QFileInfo::exists(path + ".rmlock"));
    }
}

static void testLockErrors() {
    QTemporaryDir tmp;
    {
        QLockFile l(tmp.path() + "/no/such/dir/x.lock");
        CHECK(!l.tryLock() && l.error() == QLockFile::UnknownError && !l.isLocked());
    }
    if (::geteuid() != 0) {
        QDir(tmp.path()).mkpath("ro");
        ::chmod((tmp.path() + "/ro").toUtf8().constData(), 0555);
        QLockFile l(tmp.path() + "/ro/x.lock");
        CHECK(!l.tryLock() && l.error() == QLockFile::PermissionError);
        ::chmod((tmp.path() + "/ro").toUtf8().constData(), 0755);
    }
    {
        // unlock() leaves a lock file that is not ours alone.
        const QString path = tmp.path() + "/stolen.lock";
        QLockFile l(path);
        CHECK(l.tryLock());
        // Keeping the old file open stops the file system from reusing its inode number for the new file.
        QFile keepInode(path);
        CHECK(keepInode.open(QIODevice::ReadOnly));
        ::unlink(path.toUtf8().constData());
        CHECK(writeFile(path, lockContents(1, "other", "other")));
        l.unlock();
        CHECK(QFileInfo::exists(path) && readFile(path).startsWith("1\n"));
        ::unlink(path.toUtf8().constData());
    }
    {
        // A lock held by this object cannot be removed as stale.
        const QString path = tmp.path() + "/held.lock";
        QLockFile l(path);
        CHECK(l.lock());
        CHECK(!l.removeStaleLockFile() && QFileInfo::exists(path) && l.isLocked());
        l.unlock();
    }
    {
        // A relative file name works.
        const QString oldCwd = QDir::currentPath();
        CHECK(QDir::setCurrent(tmp.path()));
        QLockFile l("relative.lock");
        CHECK(l.tryLock() && QFileInfo::exists(tmp.path() + "/relative.lock"));
        l.unlock();
        QDir::setCurrent(oldCwd);
    }
}

static void testLockProcesses() {
    QTemporaryDir tmp;
    const QString lockPath = tmp.path() + "/mutex.lock";
    const QString counter = tmp.path() + "/counter";
    CHECK(writeFile(counter, "0"));
    const int workers = 4;
    const int rounds = 30;
    std::vector<pid_t> pids;
    for (int w = 0; w < workers; ++w) {
        const pid_t p = ::fork();
        if (p == 0) {
            int bad = 0;
            for (int i = 0; i < rounds; ++i) {
                QLockFile lock(lockPath);
                if (!lock.lock()) {
                    bad = 1;
                    break;
                }
                const int value = readFile(counter).toInt();
                ::usleep(500);
                if (!writeFile(counter, QByteArray::number(value + 1)))
                    bad = 1;
                lock.unlock();
            }
            ::_exit(bad);
        }
        pids.push_back(p);
    }
    for (pid_t p : pids) {
        int status = 0;
        ::waitpid(p, &status, 0);
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    CHECK(readFile(counter).toInt() == workers * rounds);
    CHECK(!QFileInfo::exists(lockPath));
    CHECK(QDir(tmp.path()).entryList(QDir::Files | QDir::Hidden).size() == 1);
}

static void testLockThreads() {
    QTemporaryDir tmp;
    const QString lockPath = tmp.path() + "/threads.lock";
    int counter = 0;
    std::atomic<int> inside{0};
    std::atomic<int> overlaps{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 20; ++i) {
                QLockFile lock(lockPath);
                if (!lock.lock())
                    continue;
                if (inside.fetch_add(1) != 0)
                    ++overlaps;
                ++counter;
                inside.fetch_sub(1);
                lock.unlock();
            }
        });
    }
    for (std::thread &t : threads)
        t.join();
    CHECK(counter == 80 && overlaps == 0);
}

int main() {
    testDirIterator();
    testLockBasics();
    testLockBlocking();
    testLockStale();
    testLockErrors();
    testLockProcesses();
    testLockThreads();
    if (g_failures == 0)
        std::printf("test_dirlock: all tests passed\n");
    else
        std::printf("test_dirlock: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
