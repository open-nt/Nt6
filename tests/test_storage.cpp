// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/QApplicationStatic>
#include <QtCore/QDirListing>
#include <QtCore/QFileSelector>
#include <QtCore/QStorageInfo>
#include <QtCore/qtemporarydir.h>

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static QString g_root;

static bool writeFile(const QString &path, const QByteArray &data = QByteArray("x")) {
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}

static bool link(const QString &target, const QString &path) {
    return ::symlink(target.toUtf8().constData(), path.toUtf8().constData()) == 0;
}

// QStorageInfo

static void testStorageInfo() {
    const QStorageInfo invalid;
    CHECK(!invalid.isValid() && !invalid.isReady() && !invalid.isRoot() && !invalid.isReadOnly());
    CHECK(invalid.rootPath().isEmpty() && invalid.device().isEmpty() && invalid.fileSystemType().isEmpty());
    CHECK(invalid.name().isEmpty() && invalid.displayName().isEmpty() && invalid.subvolume().isEmpty());
    CHECK(invalid.bytesTotal() == -1 && invalid.bytesFree() == -1 && invalid.bytesAvailable() == -1);
    CHECK(invalid.blockSize() == -1);
    CHECK(invalid == QStorageInfo() && !QStorageInfo(g_root + "/no/such/thing").isValid());
    CHECK(!QStorageInfo(QString()).isValid());

    const QStorageInfo root = QStorageInfo::root();
    CHECK(root.isValid() && root.isReady() && root.isRoot() && root.rootPath() == QString("/"));
    CHECK(root == QStorageInfo(QString("/")) && root != invalid);
    CHECK(!root.fileSystemType().isEmpty() && !root.device().isEmpty());
    CHECK(root.displayName() == (root.name().isEmpty() ? QString("/") : root.name()));

    // The volume of the test directory: its sizes are those the system reports
    QStorageInfo here(g_root);
    CHECK(here.isValid() && here.isReady());
    CHECK(QDir(here.rootPath()).exists() && here.rootPath().startsWith(QChar(u'/')));
    CHECK(g_root == here.rootPath() || here.rootPath() == QString("/") ||
          QFileInfo(g_root).canonicalFilePath().startsWith(here.rootPath() + QChar(u'/')));
    struct statvfs vfs = {};
    CHECK(statvfs(here.rootPath().toUtf8().constData(), &vfs) == 0);
    CHECK(here.bytesTotal() == static_cast<qint64>(vfs.f_blocks) * static_cast<qint64>(vfs.f_frsize));
    CHECK(here.blockSize() == static_cast<int>(vfs.f_bsize) && here.blockSize() > 0);
    CHECK(here.isReadOnly() == ((vfs.f_flag & ST_RDONLY) != 0) && !here.isReadOnly());
    CHECK(here.bytesTotal() > 0 && here.bytesFree() >= 0 && here.bytesFree() <= here.bytesTotal());
    CHECK(here.bytesAvailable() >= 0 && here.bytesAvailable() <= here.bytesFree());
    // The mount point is a directory on another device than its parent, or the root
    if (here.rootPath() != QString("/")) {
        struct stat mount = {}, parent = {};
        CHECK(::stat(here.rootPath().toUtf8().constData(), &mount) == 0);
        CHECK(::stat((here.rootPath() + "/..").toUtf8().constData(), &parent) == 0);
        CHECK(mount.st_dev != parent.st_dev || here.fileSystemType() == QByteArray("btrfs") ||
              here.fileSystemType() == QByteArray("overlay"));
    }

    // A file, a directory and a link on the volume give the same volume
    CHECK(writeFile(g_root + "/storage.txt"));
    CHECK(QStorageInfo(g_root + "/storage.txt") == here && QStorageInfo(QDir(g_root)) == here);
    CHECK(link(g_root + "/storage.txt", g_root + "/storage.link"));
    CHECK(QStorageInfo(g_root + "/storage.link") == here);
    CHECK(QStorageInfo(g_root + "/../" + QFileInfo(g_root).fileName() + "/./storage.txt") == here);

    // setPath(), copies, swap() and refresh()
    QStorageInfo other;
    other.setPath(g_root);
    CHECK(other == here && other.bytesTotal() == here.bytesTotal());
    QStorageInfo copy(other);
    CHECK(copy == other && copy.fileSystemType() == other.fileSystemType());
    QStorageInfo empty;
    copy.swap(empty);
    CHECK(!copy.isValid() && empty == here);
    other.setPath(g_root + "/missing");
    CHECK(!other.isValid() && other.rootPath().isEmpty() && other.bytesTotal() == -1);
    other.refresh();
    CHECK(!other.isValid() && !other.isReady());
    here.refresh();
    CHECK(here.isValid() && here.isReady() && here.bytesTotal() > 0 && here == empty);

    // The kernel's file systems are volumes when asked for by path
    if (QFileInfo::exists(QString("/proc/self/mountinfo"))) {
        const QStorageInfo proc(QString("/proc/self"));
        CHECK(proc.isValid() && proc.rootPath() == QString("/proc") && proc.fileSystemType() == QByteArray("proc"));
        CHECK(!proc.isRoot() && proc != root);
    }

    // The list of volumes has the root and that of the test directory, once each, and nothing of the kernel
    const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
    CHECK(!volumes.isEmpty());
    int roots = 0, heres = 0;
    for (const QStorageInfo &volume : volumes) {
        CHECK(volume.isValid() && !volume.rootPath().isEmpty());
        CHECK(volume.fileSystemType() != QByteArray("proc") && volume.fileSystemType() != QByteArray("sysfs"));
        CHECK(std::count(volumes.begin(), volumes.end(), volume) == 1);
        // A volume found by its root path is the volume
        if (QFileInfo(volume.rootPath()).canonicalFilePath() == volume.rootPath())
            CHECK(QStorageInfo(volume.rootPath()) == volume);
        roots += volume.isRoot() ? 1 : 0;
        heres += volume == here ? 1 : 0;
    }
    CHECK(roots == 1 && heres == 1);
}

// QDirListing

using F = QDirListing::IteratorFlag;

static QStringList listed(const QString &path, QDirListing::IteratorFlags flags,
                          const QStringList &filters = QStringList()) {
    QStringList out;
    const qsizetype prefix = path.size() + 1;
    for (const auto &entry : QDirListing(path, filters, flags))
        out.append(entry.filePath().mid(prefix));
    out.sort();
    return out;
}

static void testDirListing() {
    // tree/
    //   a.txt  B.TXT  c.dat  .hidden  link-file -> a.txt  link-dir -> sub  broken -> nowhere
    //   sub/  inner.txt  deep/  leaf.txt  .secret
    //   .hiddendir/  in.txt
    //   loop/  back -> ..(tree)
    const QString tree = g_root + "/tree";
    QDir dir;
    CHECK(dir.mkpath(tree + "/sub/deep") && dir.mkpath(tree + "/.hiddendir") && dir.mkpath(tree + "/loop"));
    CHECK(writeFile(tree + "/a.txt", "12345") && writeFile(tree + "/B.TXT") && writeFile(tree + "/c.dat"));
    CHECK(writeFile(tree + "/.hidden") && writeFile(tree + "/sub/inner.txt") && writeFile(tree + "/sub/deep/leaf.txt"));
    CHECK(writeFile(tree + "/sub/deep/.secret") && writeFile(tree + "/.hiddendir/in.txt"));
    CHECK(link(QString("a.txt"), tree + "/link-file") && link(QString("sub"), tree + "/link-dir"));
    CHECK(link(QString("nowhere"), tree + "/broken") && link(tree, tree + "/loop/back"));

    const QStringList all = {"B.TXT", "a.txt", "broken", "c.dat", "link-dir", "link-file", "loop", "sub"};
    CHECK(listed(tree, F::Default) == all);
    CHECK(listed(tree, QDirListing::IteratorFlags()) == all);

    // Files, directories and the rest; links are the rest until they are resolved
    CHECK(listed(tree, F::FilesOnly) == QStringList({"B.TXT", "a.txt", "c.dat"}));
    CHECK(listed(tree, F::DirsOnly) == QStringList({"loop", "sub"}));
    CHECK(listed(tree, F::ExcludeFiles) == QStringList({"broken", "link-dir", "link-file", "loop", "sub"}));
    CHECK(listed(tree, F::ExcludeDirs) == QStringList({"B.TXT", "a.txt", "broken", "c.dat", "link-dir", "link-file"}));
    CHECK(listed(tree, F::ExcludeSpecial) == QStringList({"B.TXT", "a.txt", "c.dat", "loop", "sub"}));
    CHECK(listed(tree, F::ExcludeOther) == listed(tree, F::ExcludeSpecial));
    CHECK(listed(tree, F::ExcludeFiles | F::ExcludeDirs) == QStringList({"broken", "link-dir", "link-file"}));
    CHECK(listed(tree, F::ExcludeFiles | F::ExcludeDirs | F::ExcludeSpecial).isEmpty());
    CHECK(listed(tree, F::ResolveSymlinks) ==
          QStringList({"B.TXT", "a.txt", "c.dat", "link-dir", "link-file", "loop", "sub"}));
    CHECK(listed(tree, F::FilesOnly | F::ResolveSymlinks) == QStringList({"B.TXT", "a.txt", "c.dat", "link-file"}));
    CHECK(listed(tree, F::DirsOnly | F::ResolveSymlinks) == QStringList({"link-dir", "loop", "sub"}));
    CHECK(listed(tree, F::ExcludeFiles | F::ExcludeDirs | F::ResolveSymlinks).isEmpty());

    // Hidden entries and the dot directories
    CHECK(listed(tree, F::FilesOnly | F::IncludeHidden) == QStringList({".hidden", "B.TXT", "a.txt", "c.dat"}));
    CHECK(listed(tree, F::DirsOnly | F::IncludeHidden) == QStringList({".hiddendir", "loop", "sub"}));
    CHECK(listed(tree, F::DirsOnly | F::IncludeDotAndDotDot) == QStringList({".", "..", "loop", "sub"}));
    CHECK(listed(tree, F::DirsOnly | F::IncludeDotAndDotDot | F::IncludeHidden) ==
          QStringList({".", "..", ".hiddendir", "loop", "sub"}));
    CHECK(listed(tree, F::FilesOnly | F::IncludeDotAndDotDot) == QStringList({"B.TXT", "a.txt", "c.dat"}));

    // Name filters, with and without case
    CHECK(listed(tree, F::Default, {"*.txt"}) == QStringList({"B.TXT", "a.txt"}));
    CHECK(listed(tree, F::CaseSensitive, {"*.txt"}) == QStringList({"a.txt"}));
    CHECK(listed(tree, F::CaseSensitive, {"*.TXT"}) == QStringList({"B.TXT"}));
    CHECK(listed(tree, F::Default, {"*.dat", "a.*"}) == QStringList({"a.txt", "c.dat"}));
    CHECK(listed(tree, F::Default, {"link-*"}) == QStringList({"link-dir", "link-file"}));
    CHECK(listed(tree, F::Default, {"?.txt"}) == QStringList({"B.TXT", "a.txt"}));
    CHECK(listed(tree, F::Default, {"txt"}).isEmpty() && listed(tree, F::Default, {"*.nothing"}).isEmpty());
    CHECK(listed(tree, F::IncludeHidden, {".h*"}) == QStringList({".hidden", ".hiddendir"}));

    // Subdirectories: entered whether listed or not; links to directories only when asked to
    CHECK(listed(tree, F::Recursive) ==
          QStringList({"B.TXT", "a.txt", "broken", "c.dat", "link-dir", "link-file", "loop", "loop/back", "sub",
                       "sub/deep", "sub/deep/leaf.txt", "sub/inner.txt"}));
    CHECK(listed(tree, F::Recursive | F::FilesOnly) ==
          QStringList({"B.TXT", "a.txt", "c.dat", "sub/deep/leaf.txt", "sub/inner.txt"}));
    CHECK(listed(tree, F::Recursive | F::DirsOnly) == QStringList({"loop", "sub", "sub/deep"}));
    CHECK(listed(tree, F::Recursive, {"*.txt"}) ==
          QStringList({"B.TXT", "a.txt", "sub/deep/leaf.txt", "sub/inner.txt"}));
    CHECK(listed(tree, F::Recursive | F::CaseSensitive, {"leaf.*", "INNER.*"}) == QStringList({"sub/deep/leaf.txt"}));
    CHECK(listed(tree, F::Recursive | F::FilesOnly | F::IncludeHidden) ==
          QStringList({".hidden", ".hiddendir/in.txt", "B.TXT", "a.txt", "c.dat", "sub/deep/.secret",
                       "sub/deep/leaf.txt", "sub/inner.txt"}));
    // A hidden directory is entered only with the hidden entries
    CHECK(listed(tree, F::Recursive | F::FilesOnly, {"in.txt"}).isEmpty());
    CHECK(listed(tree, F::Recursive | F::FilesOnly | F::IncludeHidden, {"in.txt"}) ==
          QStringList({".hiddendir/in.txt"}));
    // Following links: the directory behind link-dir is listed again under that name; the loop ends
    CHECK(listed(tree, F::Recursive | F::FilesOnly | F::FollowDirSymlinks) ==
          QStringList({"B.TXT", "a.txt", "c.dat", "link-dir/deep/leaf.txt", "link-dir/inner.txt", "sub/deep/leaf.txt",
                       "sub/inner.txt"}));
    CHECK(listed(tree, F::Recursive | F::FollowDirSymlinks | F::DirsOnly | F::ResolveSymlinks) ==
          QStringList({"link-dir", "link-dir/deep", "loop", "loop/back", "sub", "sub/deep"}));
    // Without Recursive the flag changes nothing
    CHECK(listed(tree, F::FollowDirSymlinks) == all);

    // The entries
    {
        QDirListing listing(tree, QStringList({"a.txt", "link-file", "broken", "sub"}), F::Default);
        CHECK(listing.iteratorPath() == tree && listing.iteratorFlags() == QDirListing::IteratorFlags(F::Default));
        CHECK(listing.nameFilters() == QStringList({"a.txt", "link-file", "broken", "sub"}));
        int seen = 0;
        for (const QDirListing::DirEntry &entry : listing) {
            ++seen;
            CHECK(entry.filePath() == tree + "/" + entry.fileName());
            CHECK(entry.absoluteFilePath() == entry.filePath() && entry.absolutePath() == tree);
            CHECK(entry.fileInfo() == QFileInfo(entry.filePath()));
            CHECK(!entry.isHidden() && entry.bundleName().isEmpty());
            if (entry.fileName() == QString("a.txt")) {
                CHECK(entry.isFile() && !entry.isDir() && !entry.isSymLink() && entry.exists() && entry.size() == 5);
                CHECK(entry.baseName() == QString("a") && entry.completeBaseName() == QString("a"));
                CHECK(entry.suffix() == QString("txt") && entry.completeSuffix() == QString("txt"));
                CHECK(entry.isReadable() && entry.isWritable() && !entry.isExecutable());
                CHECK(entry.canonicalFilePath() == QFileInfo(tree + "/a.txt").canonicalFilePath());
                const QDateTime modified = entry.lastModified(QTimeZone::UTC);
                CHECK(modified.isValid() && modified.timeSpec() == Qt::UTC);
                CHECK(modified == QFileInfo(tree + "/a.txt").lastModified());
                CHECK(qAbs(modified.secsTo(QDateTime::currentDateTimeUtc())) < 3600);
                CHECK(entry.fileTime(QFile::FileModificationTime, QTimeZone::UTC) == modified);
                CHECK(entry.lastRead(QTimeZone::UTC).isValid() && entry.metadataChangeTime(QTimeZone::UTC).isValid());
                CHECK(entry.lastModified(QTimeZone(3600)).offsetFromUtc() == 3600);
                CHECK(entry.lastModified(QTimeZone(3600)) == modified);
                const QDateTime born = entry.birthTime(QTimeZone::UTC);
                CHECK(!born.isValid() || born <= modified.addSecs(1));
            } else if (entry.fileName() == QString("link-file")) {
                CHECK(entry.isSymLink() && entry.isFile() && entry.exists() && entry.size() == 5);
                CHECK(entry.canonicalFilePath() == QFileInfo(tree + "/a.txt").canonicalFilePath());
            } else if (entry.fileName() == QString("broken")) {
                CHECK(entry.isSymLink() && !entry.isFile() && !entry.isDir() && !entry.exists());
                CHECK(entry.canonicalFilePath().isEmpty());
            } else {
                CHECK(entry.fileName() == QString("sub") && entry.isDir() && !entry.isFile() && entry.isExecutable());
            }
        }
        CHECK(seen == 4);
    }
    {
        // Hidden entries say so, and a name with two dots has two suffixes
        CHECK(writeFile(tree + "/sub/archive.tar.gz"));
        const QDirListing listing(tree + "/sub", F::IncludeHidden | F::Recursive | F::FilesOnly);
        int hidden = 0, archives = 0;
        for (const auto &entry : listing) {
            hidden += entry.isHidden() ? 1 : 0;
            if (entry.fileName() == QString("archive.tar.gz")) {
                ++archives;
                CHECK(entry.baseName() == QString("archive") && entry.completeBaseName() == QString("archive.tar"));
                CHECK(entry.suffix() == QString("gz") && entry.completeSuffix() == QString("tar.gz"));
            }
        }
        CHECK(hidden == 1 && archives == 1);
        CHECK(QFile::remove(tree + "/sub/archive.tar.gz"));
    }

    // The iterator by hand, and with an algorithm
    {
        const QDirListing listing(tree, F::FilesOnly);
        auto it = listing.begin();
        const auto end = listing.end();
        CHECK(it != end && !(it == end) && end != it && !(end == it));
        QStringList names;
        names.append((*it).fileName());
        ++it;
        names.append(it->fileName());
        it++;
        names.append(it->fileName());
        const QDirListing::DirEntry kept = *it;
        ++it;
        CHECK(it == end && end == it && !(it != end));
        ++it; // stays at the end
        CHECK(it == end);
        names.sort();
        CHECK(names == QStringList({"B.TXT", "a.txt", "c.dat"}));
        // An entry that was copied outlives the iterator's move
        CHECK(names.contains(kept.fileName()) && kept.isFile());
        CHECK(QDirListing::const_iterator() == end);
        static_assert(std::is_same_v<std::iterator_traits<QDirListing::const_iterator>::iterator_category,
                                     std::input_iterator_tag>);
        static_assert(std::is_same_v<QDirListing::const_iterator::value_type, QDirListing::DirEntry>);
        static_assert(!std::is_copy_constructible_v<QDirListing> && std::is_move_constructible_v<QDirListing>);
    }
    {
        const QDirListing listing(tree, F::Recursive | F::FilesOnly);
        int count = 0;
        for (auto it = listing.cbegin(); it != listing.cend(); ++it)
            ++count;
        CHECK(count == 5);
        const QDirListing again(tree, F::Recursive | F::FilesOnly);
        count = 0;
        for (auto it = again.constBegin(); it != again.constEnd(); ++it)
            ++count;
        CHECK(count == 5);
    }

    // Moving a listing, before it has started
    {
        QDirListing first(tree, QStringList({"*.dat"}), F::FilesOnly);
        QDirListing second(std::move(first));
        CHECK(second.iteratorPath() == tree && second.nameFilters() == QStringList({"*.dat"}));
        QDirListing third(g_root, F::Default);
        third = std::move(second);
        QStringList names;
        for (const auto &entry : third)
            names.append(entry.fileName());
        CHECK(names == QStringList({"c.dat"}));
    }

    // Nothing to list: an empty directory, a file, and a path that does not exist
    CHECK(dir.mkpath(g_root + "/empty"));
    CHECK(listed(g_root + "/empty", F::Default).isEmpty() && listed(g_root + "/empty", F::Recursive).isEmpty());
    CHECK(listed(g_root + "/empty", F::IncludeDotAndDotDot) == QStringList({".", ".."}));
    CHECK(listed(tree + "/a.txt", F::Default).isEmpty() && listed(g_root + "/missing", F::Recursive).isEmpty());
    {
        const QDirListing none(g_root + "/missing", F::Default);
        CHECK(none.begin() == none.end());
    }

    // The listing agrees with QDirIterator on a tree of some size
    const QString big = g_root + "/big";
    for (int i = 0; i < 6; ++i) {
        const QString sub = big + "/d" + QString::number(i) + "/e" + QString::number(i % 2);
        CHECK(dir.mkpath(sub));
        for (int j = 0; j < 7; ++j)
            CHECK(writeFile(sub + "/f" + QString::number(j) + (j % 2 ? ".txt" : ".bin")));
        CHECK(writeFile(big + "/d" + QString::number(i) + "/top.txt"));
    }
    QStringList expected;
    QDirIterator walker(big, QStringList({"*.txt"}), QDir::Files, QDirIterator::Subdirectories);
    while (walker.hasNext())
        expected.append(walker.next().mid(big.size() + 1));
    expected.sort();
    CHECK(expected.size() == 6 * 3 + 6 && listed(big, F::Recursive | F::FilesOnly, {"*.txt"}) == expected);
}

// QFileSelector

static void testFileSelector() {
    const QString base = g_root + "/select";
    QDir dir;
    CHECK(dir.mkpath(base + "/+one/+two") && dir.mkpath(base + "/+two") && dir.mkpath(base + "/+three"));
    CHECK(dir.mkpath(base + "/+unix") && dir.mkpath(base + "/+zz_ZZ") && dir.mkpath(base + "/+env"));
    for (const char *name : {"/plain.txt", "/a.txt", "/b.txt", "/c.txt", "/d.txt", "/e.txt", "/f.txt", "/g.txt"})
        CHECK(writeFile(base + name));
    CHECK(writeFile(base + "/+one/a.txt") && writeFile(base + "/+two/a.txt") && writeFile(base + "/+one/+two/a.txt"));
    CHECK(writeFile(base + "/+two/b.txt") && writeFile(base + "/+three/b.txt"));
    CHECK(writeFile(base + "/+one/+two/c.txt") && writeFile(base + "/+three/c.txt"));
    CHECK(writeFile(base + "/+unix/d.txt") && writeFile(base + "/+zz_ZZ/e.txt") && writeFile(base + "/+env/f.txt"));
    CHECK(writeFile(base + "/+three/g.txt") && writeFile(base + "/+unix/g.txt") && writeFile(base + "/+env/g.txt"));
    CHECK(writeFile(base + "/+one/only.txt")); // no file outside the selector directory
    CHECK(qunsetenv("QT_FILE_SELECTORS"));

    QFileSelector selector;
    CHECK(selector.extraSelectors().isEmpty());
    // The selectors of the platform and the locale
    const QStringList platform = selector.allSelectors();
    CHECK(platform.size() >= 3 && platform.first() == QLocale().name());
    CHECK(platform.contains(QString("unix")) && platform.contains(QSysInfo::kernelType()));
    CHECK(platform.indexOf(QString("unix")) < platform.indexOf(QSysInfo::kernelType()));
    CHECK(QSysInfo::productType() == QString("unknown") || platform.last() == QSysInfo::productType() ||
          QSysInfo::productType() == QSysInfo::kernelType());

    // No variant fits: the path comes back, as does one of a file that does not exist
    CHECK(selector.select(base + "/plain.txt") == base + "/plain.txt");
    CHECK(selector.select(base + "/a.txt") == base + "/a.txt");
    CHECK(selector.select(base + "/missing.txt") == base + "/missing.txt");
    CHECK(selector.select(base + "/only.txt") == base + "/only.txt");
    CHECK(selector.select(QString()) == QString());
    // The platform
    CHECK(selector.select(base + "/d.txt") == base + "/+unix/d.txt");

    // The selectors of the program, the first one first, and nested directories before plain ones
    selector.setExtraSelectors({"two"});
    CHECK(selector.extraSelectors() == QStringList({"two"}));
    CHECK(selector.allSelectors().first() == QString("two") && selector.allSelectors().size() == platform.size() + 1);
    CHECK(selector.select(base + "/a.txt") == base + "/+two/a.txt");
    CHECK(selector.select(base + "/b.txt") == base + "/+two/b.txt");
    CHECK(selector.select(base + "/c.txt") == base + "/c.txt"); // +one/+two needs "one"
    selector.setExtraSelectors({"one", "two", "three"});
    CHECK(selector.select(base + "/a.txt") == base + "/+one/+two/a.txt");
    CHECK(selector.select(base + "/b.txt") == base + "/+two/b.txt");
    CHECK(selector.select(base + "/c.txt") == base + "/+one/+two/c.txt");
    CHECK(selector.select(base + "/only.txt") == base + "/only.txt");
    selector.setExtraSelectors({"three", "two", "one"});
    CHECK(selector.select(base + "/a.txt") == base + "/+two/a.txt"); // +one/+two is not reached from "two"
    CHECK(selector.select(base + "/b.txt") == base + "/+three/b.txt");
    CHECK(selector.select(base + "/c.txt") == base + "/+three/c.txt");
    selector.setExtraSelectors({"one"});
    CHECK(selector.select(base + "/a.txt") == base + "/+one/a.txt");
    // The program's selectors come before the platform's
    selector.setExtraSelectors({"three"});
    CHECK(selector.select(base + "/g.txt") == base + "/+three/g.txt");
    selector.setExtraSelectors({});
    CHECK(selector.select(base + "/g.txt") == base + "/+unix/g.txt");

    // The environment: after the program's selectors, before the platform's
    CHECK(qputenv("QT_FILE_SELECTORS", "env,,other"));
    CHECK(selector.allSelectors().mid(0, 2) == QStringList({"env", "other"}));
    CHECK(selector.allSelectors().size() == platform.size() + 2);
    CHECK(selector.select(base + "/f.txt") == base + "/+env/f.txt");
    CHECK(selector.select(base + "/g.txt") == base + "/+env/g.txt");
    selector.setExtraSelectors({"three"});
    CHECK(selector.allSelectors().mid(0, 3) == QStringList({"three", "env", "other"}));
    CHECK(selector.select(base + "/g.txt") == base + "/+three/g.txt");
    CHECK(qunsetenv("QT_FILE_SELECTORS"));
    selector.setExtraSelectors({});
    CHECK(selector.select(base + "/f.txt") == base + "/f.txt");

    // The locale
    const QLocale before;
    CHECK(selector.select(base + "/e.txt") == base + "/e.txt");
    CHECK(dir.mkpath(base + "/+" + QLocale(QLocale::German, QLocale::Germany).name()));
    CHECK(writeFile(base + "/+de_DE/e.txt"));
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
    CHECK(selector.allSelectors().first() == QString("de_DE"));
    CHECK(selector.select(base + "/e.txt") == base + "/+de_DE/e.txt");
    QLocale::setDefault(before);

    // A relative path stays relative
    const QString cwd = QDir::currentPath();
    CHECK(QDir::setCurrent(base));
    selector.setExtraSelectors({"one"});
    CHECK(selector.select(QString("a.txt")) == QString("+one/a.txt"));
    CHECK(selector.select(QString("./a.txt")) == QString("./+one/a.txt"));
    CHECK(QDir::setCurrent(cwd));

    // URLs: local files are selected, with their query and fragment kept; others are left alone
    selector.setExtraSelectors({"two"});
    CHECK(selector.select(QUrl::fromLocalFile(base + "/a.txt")) == QUrl::fromLocalFile(base + "/+two/a.txt"));
    CHECK(selector.select(QUrl::fromLocalFile(base + "/plain.txt")) == QUrl::fromLocalFile(base + "/plain.txt"));
    QUrl decorated = QUrl::fromLocalFile(base + "/a.txt");
    decorated.setQuery(QString("k=v"));
    decorated.setFragment(QString("part"));
    const QUrl chosen = selector.select(decorated);
    CHECK(chosen.toLocalFile() == base + "/+two/a.txt" && chosen.query() == QString("k=v") &&
          chosen.fragment() == QString("part"));
    const QUrl web(QString("http://example.org/a.txt"));
    CHECK(selector.select(web) == web);
    const QUrl noResource(QString("qrc:/no/such/resource.txt"));
    CHECK(selector.select(noResource) == noResource);
    CHECK(selector.select(QString(":/no/such/resource.txt")) == QString(":/no/such/resource.txt"));
    CHECK(selector.select(QUrl()) == QUrl());

    // It is a QObject
    QObject parent;
    auto *child = new QFileSelector(&parent);
    CHECK(child->parent() == &parent && std::string(child->metaObject()->className()) == "QFileSelector");
    CHECK(qobject_cast<QFileSelector *>(static_cast<QObject *>(child)) == child);
}

// Q_APPLICATION_STATIC

namespace {

int g_alive = 0;
int g_made = 0;
struct Counted {
    Counted() : value(1) {
        ++g_alive;
        ++g_made;
    }
    explicit Counted(int v, const char *t = "") : value(v), text(t) {
        ++g_alive;
        ++g_made;
    }
    ~Counted() { --g_alive; }
    Counted(const Counted &) = delete;
    Counted &operator=(const Counted &) = delete;
    int value;
    std::string text;
};

Q_APPLICATION_STATIC(Counted, plainStatic)
Q_APPLICATION_STATIC(Counted, argsStatic, 42, "text")
Q_APPLICATION_STATIC(QObject, objectStatic)

} // namespace

static void testApplicationStatic(int &argc, char **argv) {
    CHECK(g_made == 0 && !plainStatic.exists() && !argsStatic.exists());
    {
        QCoreApplication app(argc, argv);
        CHECK(!plainStatic.exists() && g_made == 0); // nothing is made until it is used
        CHECK(plainStatic->value == 1 && plainStatic.exists() && g_alive == 1 && g_made == 1);
        Counted *first = plainStatic;
        CHECK(first == plainStatic() && first == &*plainStatic && first == plainStatic.operator->());
        plainStatic->value = 7;
        CHECK((*plainStatic).value == 7 && g_made == 1);
        CHECK(argsStatic->value == 42 && argsStatic->text == "text" && g_alive == 2);
        CHECK(objectStatic->thread() == QThread::currentThread() && objectStatic.exists());

        // reset() destroys now; the next use makes a new object
        plainStatic.reset();
        CHECK(!plainStatic.exists() && g_alive == 1);
        plainStatic.reset(); // nothing to destroy
        CHECK(plainStatic->value == 1 && g_alive == 2 && g_made == 3);
        static_assert(std::is_same_v<decltype(plainStatic)::Type, Counted>);
    }
    // Destroyed with the application
    CHECK(g_alive == 0 && !plainStatic.exists() && !argsStatic.exists() && !objectStatic.exists());
    {
        // A new application gets new objects
        QCoreApplication app(argc, argv);
        CHECK(!argsStatic.exists());
        CHECK(argsStatic->value == 42 && g_alive == 1 && g_made == 4);
    }
    CHECK(g_alive == 0 && g_made == 4);
}

int main(int argc, char **argv) {
    QTemporaryDir temp;
    if (!temp.isValid()) {
        std::puts("cannot create a temporary directory");
        return 1;
    }
    g_root = QFileInfo(temp.path()).canonicalFilePath();

    testStorageInfo();
    testDirListing();
    testFileSelector();
    testApplicationStatic(argc, argv);

    if (g_failures) {
        std::printf("%d storage test check(s) failed\n", g_failures);
        return 1;
    }
    std::puts("all storage tests passed");
    return 0;
}
