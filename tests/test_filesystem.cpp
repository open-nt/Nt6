// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/qsavefile.h>
#include <QtCore/qstandardpaths.h>
#include <QtCore/qtemporarydir.h>
#include <QtCore/qtemporaryfile.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>

static int g_failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

static QString g_root;

static bool writeFile(const QString &path, const QByteArray &data) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(data) == data.size();
}

static QByteArray readFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

static void setTimes(const QString &path, long mtime) {
    timeval tv[2] = {{mtime, 0}, {mtime, 0}};
    ::utimes(path.toUtf8().constData(), tv);
}

static void testCleanPath() {
    CHECK(QDir::cleanPath("/a//b/./c/../d/") == "/a/b/d");
    CHECK(QDir::cleanPath("a/b/..") == "a");
    CHECK(QDir::cleanPath("a/..") == ".");
    CHECK(QDir::cleanPath("../a/../..") == "../..");
    CHECK(QDir::cleanPath("/..") == "/");
    CHECK(QDir::cleanPath("/") == "/");
    CHECK(QDir::cleanPath("./a") == "a");
    CHECK(QDir::cleanPath("").isEmpty());
    CHECK(QDir::isAbsolutePath("/x") && QDir::isRelativePath("x"));
    CHECK(QDir::separator() == QChar('/'));
    CHECK(QDir::rootPath() == "/");
    CHECK(QDir::toNativeSeparators("a/b") == "a/b");
}

static void testMatch() {
    CHECK(QDir::match("*.txt", "a.txt"));
    CHECK(QDir::match("*.txt", "A.TXT"));
    CHECK(!QDir::match("*.txt", "a.txt.bak"));
    CHECK(QDir::match("a?c", "abc") && !QDir::match("a?c", "ac"));
    CHECK(QDir::match("[abc]x", "bx") && !QDir::match("[abc]x", "dx"));
    CHECK(QDir::match("[a-c]x", "bx") && !QDir::match("[a-c]x", "dx"));
    CHECK(QDir::match("[!a-c]x", "dx") && !QDir::match("[!a-c]x", "bx"));
    CHECK(QDir::match("*.cpp *.h", "x.h") && QDir::match("*.cpp;*.h", "x.cpp"));
    CHECK(!QDir::match("*.cpp *.h", "x.o"));
    CHECK(QDir::match(QStringList{"*.a", "b*"}, "bar"));
    CHECK(QDir::match("*", ""));
    CHECK(QDir::match("a*b*c", "aXXbYYc") && !QDir::match("a*b*c", "aXXbYY"));
    CHECK(QDir::match("a\\*b", "a*b") && !QDir::match("a\\*b", "aXb"));
}

static void testFileInfoNames() {
    QFileInfo fi("/tmp/archive.tar.gz");
    CHECK(fi.filePath() == "/tmp/archive.tar.gz");
    CHECK(fi.fileName() == "archive.tar.gz");
    CHECK(fi.baseName() == "archive");
    CHECK(fi.completeBaseName() == "archive.tar");
    CHECK(fi.suffix() == "gz");
    CHECK(fi.completeSuffix() == "tar.gz");
    CHECK(fi.path() == "/tmp");
    CHECK(fi.isAbsolute() && !fi.isRelative());

    QFileInfo rel("dir/file");
    CHECK(rel.path() == "dir" && rel.fileName() == "file" && rel.suffix().isEmpty());
    CHECK(rel.isRelative());
    CHECK(QFileInfo("file").path() == ".");
    CHECK(QFileInfo("/file").path() == "/");
    CHECK(QFileInfo("dir/").fileName().isEmpty());
    CHECK(QFileInfo("dir/").path() == "dir");
    CHECK(QFileInfo(".bashrc").suffix() == "bashrc" && QFileInfo(".bashrc").baseName().isEmpty());
    CHECK(QFileInfo(".bashrc").isHidden() && !QFileInfo("bashrc").isHidden());
    CHECK(QFileInfo("noext").suffix().isEmpty() && QFileInfo("noext").baseName() == "noext");

    QFileInfo empty;
    CHECK(!empty.exists() && empty.fileName().isEmpty() && empty.absoluteFilePath().isEmpty());
    CHECK(empty.size() == 0 && !empty.lastModified().isValid());

    const QString cwd = QDir::currentPath();
    QFileInfo r("sub/../x.txt");
    CHECK(r.absoluteFilePath() == cwd + "/x.txt");
    CHECK(r.absolutePath() == cwd);
    CHECK(QFileInfo("/").isRoot() && !QFileInfo("/tmp").isRoot());
    CHECK(QFileInfo("/a/b") == QFileInfo("/a/x/../b"));
    CHECK(QFileInfo("/a/b") != QFileInfo("/a/c"));
}

static void testFileInfoOnDisk() {
    const QString dir = g_root + "/info";
    QDir().mkpath(dir);
    const QString file = dir + "/data.txt";
    CHECK(writeFile(file, "hello world"));
    ::chmod(file.toUtf8().constData(), 0640);
    setTimes(file, 1700000000);

    QFileInfo fi(file);
    CHECK(fi.exists() && QFileInfo::exists(file));
    CHECK(fi.isFile() && !fi.isDir() && !fi.isSymLink());
    CHECK(fi.size() == 11);
    CHECK(fi.isReadable() && fi.isWritable() && !fi.isExecutable());
    CHECK(fi.lastModified().toSecsSinceEpoch() == 1700000000);
    CHECK(fi.lastRead().isValid() && fi.metadataChangeTime().isValid());
    CHECK(fi.ownerId() == ::getuid() && fi.groupId() == ::getgid());
    CHECK(!fi.owner().isEmpty() && !fi.group().isEmpty());
    const QFileInfo::Permissions p = fi.permissions();
    CHECK(p.testFlag(QFileInfo::ReadOwner) && p.testFlag(QFileInfo::WriteOwner) && !p.testFlag(QFileInfo::ExeOwner));
    CHECK(p.testFlag(QFileInfo::ReadGroup) && !p.testFlag(QFileInfo::WriteGroup));
    CHECK(!p.testFlag(QFileInfo::ReadOther));
    CHECK(fi.permission(QFileInfo::ReadOwner | QFileInfo::ReadUser));
    CHECK(!fi.permission(QFileInfo::ReadOwner | QFileInfo::WriteOther));
    CHECK(fi.canonicalFilePath() == QFileInfo(dir).canonicalFilePath() + "/data.txt");
    CHECK(fi.canonicalPath() == QFileInfo(dir).canonicalFilePath());
    CHECK(fi.dir().path() == dir && fi.absoluteDir().absolutePath() == dir);

    QFileInfo d(dir);
    CHECK(d.isDir() && !d.isFile() && d.exists());
    CHECK(!QFileInfo(dir + "/missing").exists());
    CHECK(QFileInfo(dir + "/missing").canonicalFilePath().isEmpty());
    CHECK(QFileInfo(QFile(file)).filePath() == file);
    CHECK(QFileInfo(QDir(dir), "data.txt").filePath() == file);
    CHECK(QFileInfo(QDir(dir), "/abs/path").filePath() == "/abs/path");

    // Caching: the stat result survives a change until refresh().
    QFileInfo cached(file);
    CHECK(cached.size() == 11);
    CHECK(writeFile(file, "hello world!!"));
    CHECK(cached.size() == 11);
    cached.refresh();
    CHECK(cached.size() == 13);
    cached.setCaching(false);
    CHECK(writeFile(file, "x"));
    CHECK(cached.size() == 1 && !cached.caching());

    // Symbolic links, including a broken one.
    const QString link = dir + "/link";
    CHECK(::symlink("data.txt", link.toUtf8().constData()) == 0);
    QFileInfo li(link);
    CHECK(li.isSymLink() && li.isFile() && li.exists());
    CHECK(li.symLinkTarget() == file);
    CHECK(li.canonicalFilePath() == fi.canonicalFilePath());
    CHECK(fi.symLinkTarget().isEmpty());
    const QString broken = dir + "/broken";
    CHECK(::symlink("nowhere", broken.toUtf8().constData()) == 0);
    QFileInfo bi(broken);
    CHECK(bi.isSymLink() && !bi.exists() && bi.symLinkTarget() == dir + "/nowhere");

    QFileInfo exe(dir + "/script.sh");
    CHECK(writeFile(exe.filePath(), "#!/bin/sh\n"));
    ::chmod(exe.filePath().toUtf8().constData(), 0755);
    CHECK(exe.isExecutable() && exe.permission(QFileInfo::ExeOther));

    QFileInfo m("data.txt");
    CHECK(m.makeAbsolute() && m.filePath() == QDir::currentPath() + "/data.txt");
    QFileInfo s1("a"), s2("b");
    s1.swap(s2);
    CHECK(s1.filePath() == "b" && s2.filePath() == "a");
    s1.setFile(QDir(dir), "data.txt");
    CHECK(s1.filePath() == file);
}

static void testDirBasics() {
    QDir d("/usr//lib/");
    CHECK(d.path() == "/usr/lib");
    CHECK(d.dirName() == "lib");
    CHECK(QDir().path() == "." && QDir("").path() == ".");
    CHECK(QDir().absolutePath() == QDir::currentPath());
    CHECK(QDir("/").isRoot() && QDir("/").dirName().isEmpty() && !d.isRoot());
    CHECK(d.isAbsolute() && QDir("rel").isRelative());
    CHECK(d.filePath("x/y") == "/usr/lib/x/y");
    CHECK(d.filePath("/abs") == "/abs");
    CHECK(d.absoluteFilePath("x") == "/usr/lib/x");
    CHECK(QDir("/").absoluteFilePath("x") == "/x");
    CHECK(QDir("rel/dir").absoluteFilePath("f") == QDir::currentPath() + "/rel/dir/f");
    CHECK(QDir("/a/b").relativeFilePath("/a/b/c/d") == "c/d");
    CHECK(QDir("/a/b").relativeFilePath("/a/x/y") == "../x/y");
    CHECK(QDir("/a/b").relativeFilePath("/a/b") == ".");
    CHECK(QDir("/a/b").relativeFilePath("/") == "../..");
    CHECK(QDir("/a/b").relativeFilePath("c") == "c");
    CHECK(QDir("/a/b") == QDir("/a/c/../b") && QDir("/a") != QDir("/b"));

    QDir home = QDir::home();
    CHECK(home.path() == QDir::homePath());
    CHECK(QDir::temp().path() == QDir::tempPath());
    CHECK(QDir::root().path() == "/");
    CHECK(QDir::current().path() == QDir::currentPath());
    CHECK(QDir::drives().size() == 1);
    CHECK(QDir::listSeparator() == QChar(':'));
    CHECK(QDir("/usr/lib").nameFilters() == QStringList{"*"});

    QDir c("/usr/lib/../lib");
    CHECK(c.absolutePath() == "/usr/lib");
    CHECK(c.canonicalPath() == QFileInfo("/usr/lib").canonicalFilePath());
    CHECK(QDir("/nonexistent/zzz").canonicalPath().isEmpty());
    CHECK(c.exists() && !QDir("/nonexistent/zzz").exists());
}

static void testDirNavigation() {
    const QString base = g_root + "/nav";
    CHECK(QDir().mkpath(base + "/a/b"));
    QDir d(base);
    CHECK(d.cd("a"));
    CHECK(d.path() == base + "/a");
    CHECK(d.cd("b") && d.dirName() == "b");
    CHECK(d.cdUp() && d.path() == base + "/a");
    CHECK(!d.cd("missing") && d.path() == base + "/a");
    CHECK(d.cd(".") && d.cd("") && d.path() == base + "/a");
    CHECK(d.cd(base) && d.path() == base);
    CHECK(d.cd("a/b") && d.dirName() == "b");
    CHECK(d.cd("../..") && d.path() == base);

    QDir r("nav-relative");
    CHECK(r.isRelative() && r.makeAbsolute() && r.isAbsolute());
    CHECK(r.path() == QDir::currentPath() + "/nav-relative");

    // File as target of cd must fail.
    CHECK(writeFile(base + "/file", "x"));
    QDir f(base);
    CHECK(!f.cd("file"));

    const QString oldCwd = QDir::currentPath();
    CHECK(QDir::setCurrent(base + "/a"));
    CHECK(QDir::currentPath() == QFileInfo(base + "/a").canonicalFilePath());
    CHECK(QFileInfo("b").isDir() && QDir("b").exists() && QDir("b").absolutePath() == QDir::currentPath() + "/b");
    CHECK(QDir::setCurrent(oldCwd));
    CHECK(!QDir::setCurrent(base + "/nope"));
}

static void makeListingTree(const QString &base) {
    QDir().mkpath(base + "/Adir");
    QDir().mkpath(base + "/bdir");
    QDir().mkpath(base + "/.hiddendir");
    writeFile(base + "/c.txt", QByteArray(30, 'c'));
    writeFile(base + "/B.TXT", QByteArray(10, 'b'));
    writeFile(base + "/a.cpp", QByteArray(20, 'a'));
    writeFile(base + "/d.h", QByteArray(5, 'd'));
    writeFile(base + "/.hidden", "h");
    writeFile(base + "/noext", "n");
    CHECK(::symlink("c.txt", (base + "/linkc").toUtf8().constData()) == 0);
    CHECK(::symlink("missing", (base + "/dangling").toUtf8().constData()) == 0);
    setTimes(base + "/c.txt", 1000000);
    setTimes(base + "/B.TXT", 3000000);
    setTimes(base + "/a.cpp", 2000000);
    setTimes(base + "/d.h", 4000000);
}

static void testEntryList() {
    const QString base = g_root + "/list";
    makeListingTree(base);
    QDir d(base);

    // Default: AllEntries, Name|IgnoreCase. "." and ".." included, hidden/system excluded.
    CHECK((d.entryList() == QStringList{".", "..", "a.cpp", "Adir", "B.TXT", "bdir", "c.txt", "d.h", "linkc", "noext"}));
    CHECK(d.count() == 10 && d[2] == "a.cpp");

    CHECK((d.entryList(QDir::Files) == QStringList{"a.cpp", "B.TXT", "c.txt", "d.h", "linkc", "noext"}));
    CHECK((d.entryList(QDir::Dirs | QDir::NoDotAndDotDot) == QStringList{"Adir", "bdir"}));
    CHECK((d.entryList(QDir::Dirs | QDir::NoDot) == QStringList{"..", "Adir", "bdir"}));
    CHECK((d.entryList(QDir::Dirs | QDir::NoDotDot) == QStringList{".", "Adir", "bdir"}));
    CHECK((d.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden) == QStringList{".hiddendir", "Adir", "bdir"}));
    CHECK((d.entryList(QDir::Files | QDir::Hidden) == QStringList{".hidden", "a.cpp", "B.TXT", "c.txt", "d.h", "linkc", "noext"}));
    CHECK((d.entryList(QDir::Files | QDir::NoSymLinks) == QStringList{"a.cpp", "B.TXT", "c.txt", "d.h", "noext"}));
    CHECK((d.entryList(QDir::System) == QStringList{"dangling"}));
    CHECK((d.entryList(QDir::Files | QDir::System | QDir::NoSymLinks).size() == 5));
    CHECK((d.entryList(QDir::Files | QDir::Executable).isEmpty()));
    CHECK((d.entryList(QDir::Files | QDir::Readable).size() == 6));
    CHECK((d.entryList(QDir::Dirs | QDir::Writable | QDir::NoDotAndDotDot).size() == 2));

    // Name filters.
    CHECK((d.entryList(QStringList{"*.txt"}, QDir::Files) == QStringList{"B.TXT", "c.txt"}));
    CHECK((d.entryList(QStringList{"*.txt"}, QDir::Files | QDir::CaseSensitive) == QStringList{"c.txt"}));
    CHECK((d.entryList(QStringList{"*.cpp", "*.h"}, QDir::Files) == QStringList{"a.cpp", "d.h"}));
    CHECK((d.entryList(QStringList{"?dir"}, QDir::AllEntries | QDir::NoDotAndDotDot) == QStringList{"Adir", "bdir"}));
    CHECK((d.entryList(QStringList{"[a-b]*"}, QDir::Files) == QStringList{"a.cpp", "B.TXT"}));
    // AllDirs ignores name filters for directories only.
    CHECK((d.entryList(QStringList{"*.h"}, QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot) == QStringList{"Adir", "bdir", "d.h"}));
    CHECK((d.entryList(QStringList{"*.zzz"}, QDir::Files).isEmpty()));

    // setNameFilters/setFilter/setSorting persist on the object.
    QDir f(base);
    f.setNameFilters(QStringList{"*.cpp"});
    f.setFilter(QDir::Files);
    CHECK(f.nameFilters() == QStringList{"*.cpp"} && f.filter() == QDir::Files);
    CHECK((f.entryList() == QStringList{"a.cpp"}));
    QDir ctor(base, "*.txt *.h", QDir::Name | QDir::IgnoreCase, QDir::Files);
    CHECK((ctor.entryList() == QStringList{"B.TXT", "c.txt", "d.h"}));

    // Sorting.
    const QDir::Filters files = QDir::Files | QDir::NoSymLinks;
    CHECK((d.entryList(files, QDir::Name) == QStringList{"B.TXT", "a.cpp", "c.txt", "d.h", "noext"}));
    CHECK((d.entryList(files, QDir::Name | QDir::IgnoreCase) == QStringList{"a.cpp", "B.TXT", "c.txt", "d.h", "noext"}));
    CHECK((d.entryList(files, QDir::Name | QDir::Reversed) == QStringList{"noext", "d.h", "c.txt", "a.cpp", "B.TXT"}));
    CHECK((d.entryList(files, QDir::Size) == QStringList{"c.txt", "a.cpp", "B.TXT", "d.h", "noext"}));
    CHECK((d.entryList(files, QDir::Size | QDir::Reversed).first() == "noext"));
    CHECK((d.entryList(files, QDir::Time) == QStringList{"noext", "d.h", "B.TXT", "a.cpp", "c.txt"}));
    CHECK((d.entryList(files, QDir::Time | QDir::Reversed) == QStringList{"c.txt", "a.cpp", "B.TXT", "d.h", "noext"}));
    CHECK((d.entryList(files, QDir::Type | QDir::IgnoreCase) == QStringList{"noext", "a.cpp", "d.h", "B.TXT", "c.txt"}));
    CHECK((d.entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::IgnoreCase) ==
           QStringList{"Adir", "bdir", "a.cpp", "B.TXT", "c.txt", "d.h", "linkc", "noext"}));
    CHECK((d.entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsLast | QDir::IgnoreCase) ==
           QStringList{"a.cpp", "B.TXT", "c.txt", "d.h", "linkc", "noext", "Adir", "bdir"}));
    CHECK((d.entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Reversed | QDir::IgnoreCase) ==
           QStringList{"bdir", "Adir", "noext", "linkc", "d.h", "c.txt", "B.TXT", "a.cpp"}));
    CHECK((d.entryList(QDir::Files, QDir::Unsorted).size() == 6));

    // Entry info.
    const QFileInfoList infos = d.entryInfoList(QStringList{"*.cpp"}, QDir::Files);
    CHECK(infos.size() == 1 && infos[0].fileName() == "a.cpp" && infos[0].size() == 20);
    CHECK(infos[0].filePath() == base + "/a.cpp");
    CHECK(d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot).size() == 2);

    CHECK(!d.isEmpty());
    CHECK(!d.isEmpty(QDir::Dirs | QDir::NoDotAndDotDot));
    QDir().mkpath(g_root + "/emptydir");
    CHECK(QDir(g_root + "/emptydir").isEmpty());
    CHECK(!QDir(g_root + "/emptydir").isEmpty(QDir::AllEntries));
    CHECK(QDir(g_root + "/nonexistent-dir").entryList().isEmpty());
}

static void testDirModify() {
    const QString base = g_root + "/modify";
    QDir root;
    CHECK(root.mkpath(base));
    CHECK(root.mkpath(base)); // already exists
    QDir d(base);
    CHECK(d.mkdir("one"));
    CHECK(!d.mkdir("one"));
    CHECK(!d.mkdir("x/y/z"));
    CHECK(d.exists("one") && d.exists("."));
    CHECK(d.mkpath("p/q/r"));
    CHECK(QFileInfo(base + "/p/q/r").isDir());
    CHECK(d.mkpath(base + "/abs/lute"));
    CHECK(QFileInfo(base + "/abs/lute").isDir());
    CHECK(writeFile(base + "/p/file", "x"));
    CHECK(!d.mkpath("p/file/sub"));

    CHECK(writeFile(base + "/f1", "1"));
    CHECK(d.rename("f1", "f2"));
    CHECK(!d.exists("f1") && d.exists("f2"));
    CHECK(!d.rename("nope", "other"));
    CHECK(d.remove("f2"));
    CHECK(!d.remove("f2"));
    CHECK(!d.remove(""));

    CHECK(d.rmdir("one"));
    CHECK(!d.rmdir("one"));
    CHECK(!d.rmdir("p")); // not empty

    // rmpath removes empty parents too, stopping at the first non-empty one.
    CHECK(d.rmpath("p/q/r"));
    CHECK(!d.exists("p/q") && d.exists("p") && d.exists("p/file"));
    CHECK(d.mkpath("e1/e2/e3"));
    CHECK(d.rmpath("e1/e2/e3"));
    CHECK(!d.exists("e1"));
    CHECK(!d.rmpath("e1/e2/e3"));

    // removeRecursively deletes contents, nested trees and symlinks without following them.
    QDir tree(base + "/tree");
    CHECK(root.mkpath(base + "/tree/x/y"));
    CHECK(writeFile(base + "/tree/x/y/f", "1") && writeFile(base + "/tree/x/g", "2"));
    CHECK(writeFile(base + "/keep", "keep"));
    CHECK(::symlink(base.toUtf8().constData(), (base + "/tree/linktobase").toUtf8().constData()) == 0);
    CHECK(tree.removeRecursively());
    CHECK(!tree.exists() && d.exists("keep") && d.exists());
    CHECK(tree.removeRecursively()); // already gone
}

static void testTemporaryFile() {
    QString kept;
    {
        QTemporaryFile tf(g_root + "/tmpf-XXXXXX");
        CHECK(tf.fileName().isEmpty());
        CHECK(tf.autoRemove() && tf.fileTemplate() == g_root + "/tmpf-XXXXXX");
        CHECK(tf.open());
        CHECK(tf.isOpen() && tf.isReadable() && tf.isWritable());
        const QString name = tf.fileName();
        CHECK(name.startsWith(g_root + "/tmpf-") && name.size() == (g_root + "/tmpf-XXXXXX").size());
        CHECK(!name.endsWith("XXXXXX"));
        CHECK(QFileInfo(name).isFile());
        CHECK(QFileInfo(name).permissions() == (QFileInfo::ReadOwner | QFileInfo::WriteOwner | QFileInfo::ReadUser | QFileInfo::WriteUser));
        CHECK(tf.write("abc") == 3);
        CHECK(tf.seek(0) && tf.readAll() == "abc");
        tf.close();
        CHECK(QFileInfo(name).exists()); // closing keeps the file
        CHECK(tf.open() && tf.fileName() == name); // reopening reuses it
        CHECK(tf.readAll() == "abc");
        kept = name;
    }
    CHECK(!QFileInfo(kept).exists()); // auto-removed

    {
        QTemporaryFile tf(g_root + "/keepme-XXXXXX");
        tf.setAutoRemove(false);
        CHECK(tf.open());
        kept = tf.fileName();
    }
    CHECK(QFileInfo(kept).exists());
    QFile::remove(kept);

    // Embedded placeholder with a suffix, and a template without placeholder.
    {
        QTemporaryFile tf(g_root + "/data-XXXXXX.txt");
        CHECK(tf.open());
        CHECK(tf.fileName().endsWith(".txt") && tf.fileName().startsWith(g_root + "/data-"));
        CHECK(!tf.fileName().contains("XXXXXX"));
        CHECK(QFileInfo(tf.fileName()).suffix() == "txt");
    }
    {
        QTemporaryFile tf(g_root + "/plain");
        CHECK(tf.open());
        CHECK(tf.fileName().startsWith(g_root + "/plain.") && tf.fileName().size() == (g_root + "/plain.").size() + 6);
    }
    // Default template lives in the temp dir; two files never collide.
    {
        QTemporaryFile a, b;
        CHECK(a.open() && b.open());
        CHECK(a.fileName() != b.fileName());
        CHECK(a.fileName().startsWith(QDir::tempPath() + "/"));
    }
    {
        QTemporaryFile bad("/nonexistent-dir-zzz/file-XXXXXX");
        CHECK(!bad.open() && !bad.isOpen() && bad.error() == QFileDevice::OpenError);
        CHECK(!bad.errorString().isEmpty());
    }
    {
        QObject parent;
        QTemporaryFile *child = new QTemporaryFile(g_root + "/child-XXXXXX", &parent);
        CHECK(child->parent() == &parent);
    }
}

static void testTemporaryDir() {
    QString path;
    {
        QTemporaryDir td(g_root + "/tdir-XXXXXX");
        CHECK(td.isValid() && td.errorString().isEmpty());
        path = td.path();
        CHECK(path.startsWith(g_root + "/tdir-") && QFileInfo(path).isDir());
        CHECK(QFileInfo(path).permissions() ==
              (QFileInfo::ReadOwner | QFileInfo::WriteOwner | QFileInfo::ExeOwner | QFileInfo::ReadUser | QFileInfo::WriteUser | QFileInfo::ExeUser));
        CHECK(td.filePath("a/b") == path + "/a/b");
        CHECK(td.filePath("/abs").isEmpty());
        CHECK(QDir(path).mkpath("sub/dir") && writeFile(td.filePath("sub/dir/f"), "x"));
        CHECK(td.autoRemove());
    }
    CHECK(!QFileInfo(path).exists());

    {
        QTemporaryDir td(g_root + "/tdkeep-XXXXXX");
        td.setAutoRemove(false);
        path = td.path();
    }
    CHECK(QFileInfo(path).isDir());
    CHECK(QDir(path).removeRecursively());

    {
        QTemporaryDir td(g_root + "/tdexplicit");
        CHECK(td.isValid() && td.path().startsWith(g_root + "/tdexplicit"));
        CHECK(td.path() != g_root + "/tdexplicit" && QFileInfo(td.path()).isDir());
        QTemporaryDir suffixed(g_root + "/sfx-XXXXXX-tail");
        CHECK(suffixed.isValid() && suffixed.path().endsWith("-tail") && !suffixed.path().contains("XXXXXX"));
        QTemporaryDir other(g_root + "/tdexplicit");
        CHECK(other.isValid() && other.path() != td.path());
    }
    {
        QTemporaryDir td;
        CHECK(td.isValid() && td.path().startsWith(QDir::tempPath() + "/") && QFileInfo(td.path()).isDir());
        QTemporaryDir moved(std::move(td));
        CHECK(moved.isValid() && !td.isValid());
        const QString p = moved.path();
        CHECK(moved.remove() && !QFileInfo(p).exists());
    }
    {
        QTemporaryDir bad("/nonexistent-dir-zzz/d-XXXXXX");
        CHECK(!bad.isValid() && bad.path().isEmpty() && !bad.errorString().isEmpty());
        CHECK(bad.filePath("x").isEmpty() && !bad.remove());
    }
    {
        QTemporaryDir relative("relative-tmpdir-XXXXXX");
        CHECK(relative.isValid() && QDir::isAbsolutePath(relative.path()));
    }
}

static void testSaveFile() {
    const QString target = g_root + "/save.txt";
    CHECK(writeFile(target, "old content"));
    ::chmod(target.toUtf8().constData(), 0640);

    {
        QSaveFile sf(target);
        CHECK(sf.fileName() == target);
        CHECK(sf.open(QIODevice::WriteOnly));
        CHECK(sf.write("new content") == 11);
        CHECK(readFile(target) == "old content"); // untouched until commit
        // The temporary file lives next to the target.
        QStringList siblings = QDir(g_root).entryList(QStringList{"save.txt.*"}, QDir::Files);
        CHECK(siblings.size() == 1);
        CHECK(sf.commit());
        CHECK(!sf.isOpen());
        CHECK(readFile(target) == "new content");
        CHECK(QDir(g_root).entryList(QStringList{"save.txt.*"}, QDir::Files).isEmpty());
        CHECK(QFileInfo(target).permissions().testFlag(QFileInfo::ReadGroup) && !QFileInfo(target).permissions().testFlag(QFileInfo::WriteGroup));
        CHECK(!sf.commit());
    }

    {
        QSaveFile sf(target);
        CHECK(sf.open(QIODevice::WriteOnly));
        sf.write("discard me");
        sf.cancelWriting();
        CHECK(sf.error() == QFileDevice::AbortError);
        CHECK(!sf.commit());
        CHECK(readFile(target) == "new content");
        CHECK(QDir(g_root).entryList(QStringList{"save.txt.*"}, QDir::Files).isEmpty());
    }
    {
        QSaveFile sf(target);
        CHECK(sf.open(QIODevice::WriteOnly));
        sf.write("never committed");
    }
    CHECK(readFile(target) == "new content");
    CHECK(QDir(g_root).entryList(QStringList{"save.txt.*"}, QDir::Files).isEmpty());

    // A new target is created with the default mode.
    const QString fresh = g_root + "/save-new.txt";
    {
        QSaveFile sf(fresh);
        CHECK(!QFileInfo(fresh).exists());
        CHECK(sf.open(QIODevice::WriteOnly | QIODevice::Text));
        sf.write("fresh");
        CHECK(sf.commit());
        CHECK(readFile(fresh) == "fresh");
        const mode_t mask = ::umask(0);
        ::umask(mask);
        struct stat st;
        CHECK(::stat(fresh.toUtf8().constData(), &st) == 0 && (st.st_mode & 0777) == (0666 & ~mask));
    }

    // Rejected open modes.
    {
        QSaveFile sf(target);
        CHECK(!sf.open(QIODevice::ReadOnly));
        CHECK(!sf.open(QIODevice::WriteOnly | QIODevice::Append));
        QSaveFile unnamed;
        CHECK(!unnamed.open(QIODevice::WriteOnly));
        QSaveFile newOnly(target);
        CHECK(!newOnly.open(QIODevice::WriteOnly | QIODevice::NewOnly));
    }

    // Missing directory: fails, and the fallback cannot help either.
    {
        QSaveFile sf(g_root + "/no-such-dir/file");
        CHECK(!sf.open(QIODevice::WriteOnly) && !sf.isOpen());
        sf.setDirectWriteFallback(true);
        CHECK(sf.directWriteFallback() && !sf.open(QIODevice::WriteOnly));
    }

    // Read-only directory (skipped for root): temp creation fails; fallback writes directly.
    if (::geteuid() != 0) {
        const QString rodir = g_root + "/rodir";
        QDir().mkpath(rodir);
        const QString rofile = rodir + "/file.txt";
        CHECK(writeFile(rofile, "orig"));
        ::chmod(rodir.toUtf8().constData(), 0555);
        {
            QSaveFile sf(rofile);
            CHECK(!sf.open(QIODevice::WriteOnly));
        }
        CHECK(readFile(rofile) == "orig");
        {
            QSaveFile sf(rofile);
            sf.setDirectWriteFallback(true);
            CHECK(sf.open(QIODevice::WriteOnly));
            CHECK(sf.write("direct") == 6);
            CHECK(sf.commit());
        }
        CHECK(readFile(rofile) == "direct");
        ::chmod(rodir.toUtf8().constData(), 0755);

        const QString rofile2 = g_root + "/readonly-file";
        CHECK(writeFile(rofile2, "x"));
        ::chmod(rofile2.toUtf8().constData(), 0444);
        QSaveFile sf(rofile2);
        CHECK(!sf.open(QIODevice::WriteOnly) && sf.error() == QFileDevice::WriteError);
    }
}

static void testStandardPaths() {
    const QString oldHome = QString::fromUtf8(std::getenv("HOME") ? std::getenv("HOME") : "");
    const QString fakeHome = g_root + "/home";
    QDir().mkpath(fakeHome + "/.config");
    ::setenv("HOME", fakeHome.toUtf8().constData(), 1);
    ::unsetenv("XDG_DATA_HOME");
    ::unsetenv("XDG_CONFIG_HOME");
    ::unsetenv("XDG_CACHE_HOME");
    ::unsetenv("XDG_STATE_HOME");
    ::unsetenv("XDG_DATA_DIRS");
    ::unsetenv("XDG_CONFIG_DIRS");

    CHECK(QDir::homePath() == fakeHome);
    CHECK(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) == fakeHome);
    CHECK(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) == fakeHome + "/.local/share");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) == fakeHome + "/.config");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) == fakeHome + "/.config");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) == fakeHome + "/.cache");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) == fakeHome + "/.local/state");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) == fakeHome + "/Desktop");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) == fakeHome + "/Downloads");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) == fakeHome + "/Videos");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::FontsLocation) == fakeHome + "/.local/share/fonts");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) == fakeHome + "/.local/share/applications");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::TempLocation) == QDir::tempPath());
    const QString app = QFileInfo(QString::fromUtf8("/proc/self/exe")).canonicalFilePath();
    const QString appName = QFileInfo(app).fileName();
    CHECK(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) == fakeHome + "/.local/share/" + appName);
    CHECK(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) == fakeHome + "/.config/" + appName);
    CHECK(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) == fakeHome + "/.cache/" + appName);

    CHECK((QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation) ==
           QStringList{fakeHome + "/.local/share", "/usr/local/share", "/usr/share"}));
    CHECK((QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation) ==
           QStringList{fakeHome + "/.config", "/etc/xdg"}));
    CHECK(QStandardPaths::standardLocations(QStandardPaths::HomeLocation) == QStringList{fakeHome});

    // XDG variables are honoured; relative values are ignored.
    ::setenv("XDG_DATA_HOME", (g_root + "/xdg-data").toUtf8().constData(), 1);
    ::setenv("XDG_CONFIG_HOME", (g_root + "/xdg-config").toUtf8().constData(), 1);
    ::setenv("XDG_CACHE_HOME", "relative/cache", 1);
    ::setenv("XDG_DATA_DIRS", (g_root + "/dd1:" + g_root + "/dd2/:rel:").toUtf8().constData(), 1);
    ::setenv("XDG_CONFIG_DIRS", (g_root + "/cd1").toUtf8().constData(), 1);
    CHECK(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) == g_root + "/xdg-data");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) == g_root + "/xdg-config");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) == fakeHome + "/.cache");
    CHECK((QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation) ==
           QStringList{g_root + "/xdg-data", g_root + "/dd1", g_root + "/dd2"}));
    CHECK((QStandardPaths::standardLocations(QStandardPaths::ConfigLocation) ==
           QStringList{g_root + "/xdg-config", g_root + "/cd1"}));

    // user-dirs.dirs
    CHECK(QDir().mkpath(g_root + "/xdg-config"));
    CHECK(writeFile(g_root + "/xdg-config/user-dirs.dirs",
                    "# comment\nXDG_DOCUMENTS_DIR=\"$HOME/Papers\"\nXDG_MUSIC_DIR=\"/srv/music\"\nXDG_DESKTOP_DIR=\"$HOME\"\n"));
    CHECK(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) == fakeHome + "/Papers");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::MusicLocation) == "/srv/music");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) == fakeHome + "/Desktop");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) == fakeHome + "/Pictures");

    // locate / locateAll
    CHECK(QDir().mkpath(g_root + "/xdg-data/nt-test/sub") && QDir().mkpath(g_root + "/dd1/nt-test") && QDir().mkpath(g_root + "/dd2/nt-test"));
    CHECK(writeFile(g_root + "/dd1/nt-test/res.txt", "1") && writeFile(g_root + "/dd2/nt-test/res.txt", "2"));
    CHECK(writeFile(g_root + "/xdg-data/nt-test/sub/only.txt", "3"));
    const auto gd = QStandardPaths::GenericDataLocation;
    CHECK(QStandardPaths::locate(gd, "nt-test/res.txt") == g_root + "/dd1/nt-test/res.txt");
    CHECK((QStandardPaths::locateAll(gd, "nt-test/res.txt") == QStringList{g_root + "/dd1/nt-test/res.txt", g_root + "/dd2/nt-test/res.txt"}));
    CHECK(QStandardPaths::locate(gd, "nt-test/missing").isEmpty());
    CHECK(QStandardPaths::locate(gd, "nt-test/sub").isEmpty()); // a directory is not a file
    CHECK(QStandardPaths::locate(gd, "nt-test/sub", QStandardPaths::LocateDirectory) == g_root + "/xdg-data/nt-test/sub");
    CHECK(QStandardPaths::locateAll(gd, "nt-test", QStandardPaths::LocateDirectory).size() == 3);
    CHECK(QStandardPaths::locate(gd, "nt-test/sub/only.txt") == g_root + "/xdg-data/nt-test/sub/only.txt");

    // Runtime dir honours XDG_RUNTIME_DIR.
    ::setenv("XDG_RUNTIME_DIR", (g_root + "/runtime").toUtf8().constData(), 1);
    CHECK(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) == g_root + "/runtime");

    // TMPDIR
    const QString oldTmp = QString::fromUtf8(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "");
    ::setenv("TMPDIR", (g_root + "/tmpdir//").toUtf8().constData(), 1);
    CHECK(QDir::tempPath() == g_root + "/tmpdir");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::TempLocation) == g_root + "/tmpdir");
    ::unsetenv("TMPDIR");
    CHECK(QDir::tempPath() == "/tmp");
    if (!oldTmp.isEmpty())
        ::setenv("TMPDIR", oldTmp.toUtf8().constData(), 1);

    CHECK(QStandardPaths::displayName(QStandardPaths::DesktopLocation) == "Desktop");
    CHECK(QStandardPaths::displayName(QStandardPaths::HomeLocation) == "Home");
    CHECK(QStandardPaths::displayName(QStandardPaths::TempLocation) == "Temporary Directory");
    CHECK(QStandardPaths::displayName(QStandardPaths::GenericDataLocation) == "Shared Data");
    CHECK(QStandardPaths::displayName(QStandardPaths::DownloadLocation) == "Download");

    // findExecutable
    const QString bin = g_root + "/bin";
    QDir().mkpath(bin);
    CHECK(writeFile(bin + "/mytool", "#!/bin/sh\n") && writeFile(bin + "/notexec", "x"));
    ::chmod((bin + "/mytool").toUtf8().constData(), 0755);
    ::chmod((bin + "/notexec").toUtf8().constData(), 0644);
    QDir().mkpath(bin + "/adir");
    ::chmod((bin + "/adir").toUtf8().constData(), 0755);
    CHECK(QStandardPaths::findExecutable("mytool", QStringList{bin}) == bin + "/mytool");
    CHECK(QStandardPaths::findExecutable("mytool", QStringList{"/nonexistent", bin}) == bin + "/mytool");
    CHECK(QStandardPaths::findExecutable("notexec", QStringList{bin}).isEmpty());
    CHECK(QStandardPaths::findExecutable("adir", QStringList{bin}).isEmpty());
    CHECK(QStandardPaths::findExecutable("mytool", QStringList{"/nonexistent"}).isEmpty());
    CHECK(QStandardPaths::findExecutable(bin + "/mytool") == bin + "/mytool");
    CHECK(QStandardPaths::findExecutable(bin + "/notexec").isEmpty());
    CHECK(QStandardPaths::findExecutable("").isEmpty());
    const QString oldPath = QString::fromUtf8(std::getenv("PATH") ? std::getenv("PATH") : "");
    ::setenv("PATH", (QString("/nonexistent:") + bin).toUtf8().constData(), 1);
    CHECK(QStandardPaths::findExecutable("mytool") == bin + "/mytool");
    ::setenv("PATH", oldPath.toUtf8().constData(), 1);
    CHECK(!QStandardPaths::findExecutable("sh").isEmpty());

    ::unsetenv("XDG_DATA_HOME");
    ::unsetenv("XDG_CONFIG_HOME");
    ::unsetenv("XDG_CACHE_HOME");
    ::unsetenv("XDG_DATA_DIRS");
    ::unsetenv("XDG_CONFIG_DIRS");
    ::unsetenv("XDG_RUNTIME_DIR");
    if (oldHome.isEmpty())
        ::unsetenv("HOME");
    else
        ::setenv("HOME", oldHome.toUtf8().constData(), 1);
}

static void testFilePermissionsAndTimes() {
    const QString path = g_root + "/perm.txt";
    CHECK(writeFile(path, "data"));
    QFile f(path);
    CHECK(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup));
    CHECK(f.permissions() ==
          (QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser | QFile::ReadGroup));
    CHECK(QFile::permissions(path) == QFileInfo(path).permissions());
    CHECK(QFile::setPermissions(path, QFileInfo::ReadUser | QFileInfo::WriteUser | QFileInfo::ExeOther));
    struct stat st;
    CHECK(::stat(path.toUtf8().constData(), &st) == 0 && (st.st_mode & 07777) == 0601);
    CHECK(QFile::permissions(g_root + "/missing") == QFile::Permissions());
    CHECK(!QFile::setPermissions(g_root + "/missing", QFile::ReadOwner));
    QFile missing(g_root + "/missing");
    CHECK(!missing.setPermissions(QFile::ReadOwner) && missing.error() == QFile::PermissionsError);

    const QDateTime when = QDateTime::fromMSecsSinceEpoch(1500000000123LL);
    CHECK(f.setFileTime(when, QFileDevice::FileModificationTime));
    CHECK(f.fileTime(QFileDevice::FileModificationTime) == when);
    CHECK(QFileInfo(path).lastModified() == when);
    const QDateTime earlier = QDateTime::fromMSecsSinceEpoch(-1500LL);
    CHECK(f.setFileTime(earlier, QFileDevice::FileAccessTime));
    CHECK(f.fileTime(QFileDevice::FileAccessTime) == earlier);
    CHECK(f.fileTime(QFileDevice::FileModificationTime) == when); // left alone
    CHECK(f.fileTime(QFileDevice::FileMetadataChangeTime).isValid());
    CHECK(!f.setFileTime(when, QFileDevice::FileBirthTime) && f.error() == QFile::UnspecifiedError);
    CHECK(!f.setFileTime(QDateTime(), QFileDevice::FileModificationTime));
    const QDateTime birth = f.fileTime(QFileDevice::FileBirthTime);
    CHECK(!birth.isValid() || birth <= QDateTime::currentDateTime().addSecs(5));
    CHECK(!missing.fileTime(QFileDevice::FileModificationTime).isValid());

    CHECK(f.open(QIODevice::ReadWrite));
    CHECK(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner));
    CHECK(f.permissions().testFlag(QFile::WriteOwner) && !f.permissions().testFlag(QFile::ReadGroup));
    CHECK(f.setFileTime(when.addSecs(10), QFileDevice::FileModificationTime));
    CHECK(f.fileTime(QFileDevice::FileModificationTime) == when.addSecs(10));
    f.close();

    const QString linkPath = g_root + "/perm-link";
    CHECK(f.link(linkPath));
    CHECK(QFile::symLinkTarget(linkPath) == path && QFile(linkPath).symLinkTarget() == path);
    CHECK(QFileInfo(linkPath).isSymLink() && readFile(linkPath) == "data");
    CHECK(!f.link(linkPath) && f.error() == QFile::RenameError);
    CHECK(QFile::link("perm.txt", g_root + "/rel-link") && readFile(g_root + "/rel-link") == "data");
    CHECK(QFile::symLinkTarget(path).isEmpty());
    CHECK(QFile::remove(linkPath) && QFile::remove(g_root + "/rel-link") && QFile::remove(path));
}

static void testSearchPaths() {
    const QString a = g_root + "/sp_a";
    const QString b = g_root + "/sp_b";
    CHECK(QDir().mkpath(a) && QDir().mkpath(b));
    CHECK(writeFile(b + "/only_b.txt", "B") && writeFile(a + "/both.txt", "A") && writeFile(b + "/both.txt", "B"));
    QDir::setSearchPaths("data", {a});
    QDir::addSearchPath("data", b);
    CHECK(QDir::searchPaths("data") == QStringList({a, b}));
    QDir::addSearchPath("x", a); // a one-letter prefix is not accepted
    CHECK(QDir::searchPaths("x").isEmpty());

    // The first directory that has the file wins.
    QFile both("data:both.txt");
    CHECK(both.open(QIODevice::ReadOnly) && both.readAll() == "A");
    QFile onlyB("data:only_b.txt");
    CHECK(onlyB.open(QIODevice::ReadOnly) && onlyB.readAll() == "B");
    CHECK(QFile::exists("data:only_b.txt") && !QFile::exists("data:missing.txt"));
    CHECK(QFileInfo("data:only_b.txt").absoluteFilePath() == b + "/only_b.txt");
    // A missing file resolves into the first directory, so it can be created there.
    CHECK(QFileInfo("data:new.txt").absoluteFilePath() == a + "/new.txt");
    CHECK(QDir("data:").exists());

    QDir::setSearchPaths("data", {});
    CHECK(QDir::searchPaths("data").isEmpty() && !QFile::exists("data:only_b.txt"));

    CHECK(QDir::nameFiltersFromString("*.cpp *.h") == QStringList({"*.cpp", "*.h"}));
    CHECK(QDir::nameFiltersFromString(" *.txt; *.md ;") == QStringList({"*.txt", "*.md"}));
}

static void testFileExtras() {
    // Trash under a private XDG_DATA_HOME on the same file system.
    const QString dataHome = g_root + "/xdg";
    CHECK(QDir().mkpath(dataHome));
    const char *previous = std::getenv("XDG_DATA_HOME");
    const std::string saved = previous ? previous : "";
    const bool hadSaved = previous != nullptr;
    ::setenv("XDG_DATA_HOME", dataHome.toUtf8().constData(), 1);
    const QString victim = g_root + "/trash me.txt";
    CHECK(writeFile(victim, "bye"));
    QString inTrash;
    CHECK(QFile::moveToTrash(victim, &inTrash));
    CHECK(!QFile::exists(victim) && inTrash == dataHome + "/Trash/files/trash me.txt");
    QFile info(dataHome + "/Trash/info/trash me.txt.trashinfo");
    CHECK(info.open(QIODevice::ReadOnly));
    const QByteArray text = info.readAll();
    CHECK(text.startsWith("[Trash Info]\nPath=") && text.contains("/trash%20me.txt\n") &&
          text.contains("DeletionDate="));
    // A second file of the same name gets a new name in the trash.
    CHECK(writeFile(victim, "again"));
    QFile again(victim);
    CHECK(again.moveToTrash() && again.fileName() != inTrash && QFile::exists(again.fileName()));
    CHECK(!QFile::moveToTrash(g_root + "/does-not-exist"));
    if (hadSaved)
        ::setenv("XDG_DATA_HOME", saved.c_str(), 1);
    else
        ::unsetenv("XDG_DATA_HOME");

    // readLink() returns the stored target; symLinkTarget() makes it absolute.
    CHECK(writeFile(g_root + "/target.txt", "t"));
    CHECK(QFile::link("target.txt", g_root + "/rel.lnk"));
    CHECK(QFile::readLink(g_root + "/rel.lnk") == "target.txt");
    CHECK(QFile(g_root + "/rel.lnk").readLink() == "target.txt");
    CHECK(QFile::readLink(g_root + "/target.txt").isEmpty());

    CHECK(QFile::encodeName(QString::fromUtf8("caf\xc3\xa9")) == "caf\xc3\xa9");
    CHECK(QFile::decodeName(QByteArray("caf\xc3\xa9")) == QString::fromUtf8("caf\xc3\xa9"));

    // QTemporaryFile::rename() keeps the file open.
    QTemporaryFile temp(g_root + "/tmpXXXXXX");
    CHECK(temp.open());
    CHECK(temp.write("abc") == 3);
    const QString renamed = g_root + "/renamed.tmp";
    CHECK(temp.rename(renamed) && temp.fileName() == renamed && temp.isOpen());
    CHECK(temp.write("def") == 3 && temp.flush());
    CHECK(QFileInfo(renamed).size() == 6);
    QFile plain(renamed);
    CHECK(QTemporaryFile::createNativeFile(plain) == nullptr && QTemporaryFile::createNativeFile(renamed) == nullptr);
}

static void testFileMappingAndHandles() {
    const QString path = g_root + "/mapped.bin";
    QByteArray content(10000, 'a');
    content[5000] = 'X';
    CHECK(writeFile(path, content));
    QFile f(path);
    CHECK(f.map(0, 10) == nullptr && f.error() == QFileDevice::PermissionsError); // not open
    CHECK(f.open(QIODevice::ReadWrite));
    // An offset that is not page-aligned works, and writes reach the file.
    uchar *p = f.map(4999, 3);
    CHECK(p && p[0] == 'a' && p[1] == 'X');
    if (p)
        p[2] = 'Y';
    CHECK(f.map(9990, 20) == nullptr);
    uchar *priv = f.map(0, 4, QFileDevice::MapPrivateOption);
    CHECK(priv != nullptr);
    if (priv)
        priv[0] = 'Z'; // copy-on-write: not written to the file
    f.close();
    CHECK(p && p[1] == 'X'); // mappings outlive close()
    CHECK(f.unmap(p) && f.unmap(priv) && !f.unmap(p));
    QFile check(path);
    CHECK(check.open(QIODevice::ReadOnly));
    const QByteArray after = check.readAll();
    CHECK(after.at(5001) == 'Y' && after.at(0) == 'a');

    // QFile::open() on a descriptor and on a stdio stream.
    const int fd = ::open(path.toUtf8().constData(), O_RDONLY | O_CLOEXEC);
    {
        QFile viaFd;
        CHECK(viaFd.open(fd, QIODevice::ReadOnly) && viaFd.read(3) == "aaa");
        viaFd.close();
        CHECK(::fcntl(fd, F_GETFD) != -1); // DontCloseHandle: still open
    }
    {
        QFile viaFd;
        CHECK(viaFd.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle));
        viaFd.close();
        CHECK(::fcntl(fd, F_GETFD) == -1);
    }
    std::FILE *fh = std::fopen((g_root + "/stdio.txt").toUtf8().constData(), "w");
    CHECK(fh != nullptr);
    if (fh) {
        std::fputs("from stdio ", fh);
        QFile viaStream;
        CHECK(viaStream.open(fh, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle));
        CHECK(viaStream.write("then QFile") == 10);
        viaStream.close();
    }
    QFile written(g_root + "/stdio.txt");
    CHECK(written.open(QIODevice::ReadOnly) && written.readAll() == "from stdio then QFile");
    QFile none;
    CHECK(!none.open(static_cast<std::FILE *>(nullptr), QIODevice::ReadOnly));

    // QFileInfo queries.
    CHECK(QFile::link(path, g_root + "/mapped.lnk"));
    QFileInfo link(g_root + "/mapped.lnk");
    CHECK(link.isSymbolicLink() && !QFileInfo(path).isSymbolicLink());
    CHECK(!link.isShortcut() && !link.isAlias() && !link.isJunction() && !link.isBundle());
    CHECK(link.isNativePath() && !QFileInfo().isNativePath());
    const QDateTime born = QFileInfo(path).birthTime();
    CHECK(!born.isValid() || std::llabs(born.secsTo(QDateTime::currentDateTime())) < 3600);
    CHECK(!QFileInfo(g_root + "/none").birthTime().isValid());
    const QFileInfo info(path);
    CHECK(info.fileTime(QFileDevice::FileModificationTime) == info.lastModified());
    CHECK(info.fileTime(QFile::FileAccessTime) == info.lastRead());
    CHECK(info.fileTime(QFile::FileMetadataChangeTime) == info.metadataChangeTime());
    CHECK(info.fileTime(QFile::FileBirthTime) == info.birthTime());
    CHECK(info.fileTime(QFile::FileModificationTime) == QFile(path).fileTime(QFileDevice::FileModificationTime));
}

static void testStandardPathsTestMode() {
    CHECK(!QStandardPaths::isTestModeEnabled());
    QStandardPaths::setTestModeEnabled(true);
    CHECK(QStandardPaths::isTestModeEnabled());
    const QString home = QDir::homePath();
    CHECK(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) == home + "/.qttest/share");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) == home + "/.qttest/config");
    CHECK(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) == home + "/.qttest/cache");
    CHECK(QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation) ==
          QStringList({home + "/.qttest/share"}));
    QStandardPaths::setTestModeEnabled(false);
    CHECK(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) != home + "/.qttest/share");
}

int main() {
    const char *tmp = std::getenv("TMPDIR");
    const QString tmpBase = tmp && *tmp ? QDir::cleanPath(QString::fromUtf8(tmp)) : QString::fromUtf8("/tmp");
    g_root = tmpBase + "/nt_fs_test_" + QString::number(static_cast<qlonglong>(::getpid()));
    if (!QDir().mkpath(g_root)) {
        std::printf("cannot create %s\n", g_root.toUtf8().constData());
        return 1;
    }
    g_root = QFileInfo(g_root).canonicalFilePath();

    testCleanPath();
    testMatch();
    testFileInfoNames();
    testFileInfoOnDisk();
    testDirBasics();
    testDirNavigation();
    testEntryList();
    testDirModify();
    testTemporaryFile();
    testTemporaryDir();
    testSaveFile();
    testFilePermissionsAndTimes();
    testStandardPaths();
    testSearchPaths();
    testFileExtras();
    testFileMappingAndHandles();
    testStandardPathsTestMode();

    // Restore permissions that could block cleanup, then remove the sandbox.
    ::chmod((g_root + "/rodir").toUtf8().constData(), 0755);
    CHECK(QDir(g_root).removeRecursively());
    CHECK(!QFileInfo(g_root).exists());

    if (g_failures == 0)
        std::puts("all filesystem tests passed");
    return g_failures == 0 ? 0 : 1;
}
