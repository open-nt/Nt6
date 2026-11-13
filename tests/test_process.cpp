// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/qprocess.h>
#include <QtCore/qtemporarydir.h>

#include "qprocess_p.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static const char *g_modeName = "pidfd";

static QString helper() { return QString::fromUtf8(NT_PROCESS_HELPER); }

// Runs the event loop until cond() holds; returns false on timeout.
static bool waitUntil(const std::function<bool()> &cond, int timeoutMs = 30000) {
    QElapsedTimer clock;
    clock.start();
    QEventLoop loop;
    QTimer tick;
    QObject::connect(&tick, &QTimer::timeout, [&] {
        if (cond() || clock.elapsed() > timeoutMs)
            loop.quit();
    });
    tick.start(2);
    if (!cond())
        loop.exec();
    return cond();
}

static bool finishedWithin(QProcess &p, int timeoutMs = 30000) {
    return waitUntil([&] { return p.state() == QProcess::NotRunning; }, timeoutMs);
}

static bool noStrayChildren() {
    int status = 0;
    errno = 0;
    const pid_t r = ::waitpid(-1, &status, WNOHANG);
    return r == -1 && errno == ECHILD;
}

static QByteArray readFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

static bool writeFile(const QString &path, const QByteArray &data) {
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}

static QByteArray pattern(qsizetype n) {
    QByteArray b;
    b.resize(n);
    for (qsizetype i = 0; i < n; ++i)
        b[i] = static_cast<char>('a' + i % 26);
    return b;
}

struct Recorder {
    explicit Recorder(QProcess &p) {
        QObject::connect(&p, &QProcess::started, [this] { events.append("started"); });
        QObject::connect(&p, &QProcess::finished, [this](int code, QProcess::ExitStatus st) {
            events.append("finished:" + QString::number(code) + ":" + QString::number(st));
        });
        QObject::connect(&p, &QProcess::errorOccurred,
                         [this](QProcess::ProcessError e) { events.append("error:" + QString::number(e)); });
        QObject::connect(&p, &QProcess::stateChanged,
                         [this](QProcess::ProcessState s) { events.append("state:" + QString::number(s)); });
    }
    QStringList events;
    bool has(const QString &e) const { return events.contains(e); }
    qsizetype count(const QString &e) const { return events.count(e); }
};

static void testEnvironment() {
    QProcessEnvironment env;
    CHECK(env.isEmpty() && !env.inheritsFromParent());
    env.insert("A", "1");
    env.insert("B", "two words");
    CHECK(env.contains("A") && !env.contains("C"));
    CHECK(env.value("B") == "two words" && env.value("C", "dflt") == "dflt" && env.value("C").isEmpty());
    CHECK(env.keys() == QStringList({"A", "B"}));
    CHECK(env.toStringList() == QStringList({"A=1", "B=two words"}));
    env.insert("A", "3");
    CHECK(env.value("A") == "3");
    env.remove("A");
    CHECK(!env.contains("A") && !env.isEmpty());

    QProcessEnvironment other;
    other.insert("B", "x");
    other.insert("C", "y");
    env.insert(other);
    CHECK(env.value("B") == "x" && env.value("C") == "y" && env.keys().size() == 2);
    QProcessEnvironment copy = env;
    CHECK(copy == env);
    copy.insert("D", "1");
    CHECK(copy != env && !env.contains("D"));
    env.clear();
    CHECK(env.isEmpty() && !env.contains("C"));

    QProcessEnvironment inherit(QProcessEnvironment::InheritFromParent);
    CHECK(inherit.inheritsFromParent() && inherit.isEmpty());
    inherit.insert("Z", "1");
    CHECK(!inherit.inheritsFromParent() && inherit.keys() == QStringList({"Z"}));
    QProcessEnvironment merged(QProcessEnvironment::InheritFromParent);
    merged.insert(QProcessEnvironment(QProcessEnvironment::InheritFromParent));
    CHECK(merged.inheritsFromParent());

    ::setenv("NT_TEST_SYSENV", "from-parent=with-equals", 1);
    const QProcessEnvironment sys = QProcessEnvironment::systemEnvironment();
    CHECK(!sys.inheritsFromParent());
    CHECK(sys.value("NT_TEST_SYSENV") == "from-parent=with-equals");
    CHECK(QProcess::systemEnvironment().contains("NT_TEST_SYSENV=from-parent=with-equals"));
    ::unsetenv("NT_TEST_SYSENV");
}

static void testSplitCommand() {
    CHECK(QProcess::splitCommand(u"a b  c") == QStringList({"a", "b", "c"}));
    CHECK(QProcess::splitCommand(u"  a\t b \n") == QStringList({"a", "b"}));
    CHECK(QProcess::splitCommand(u"prog \"hello world\" x") == QStringList({"prog", "hello world", "x"}));
    CHECK(QProcess::splitCommand(u"\"a b\"\"c d\"") == QStringList({"a bc d"}));
    CHECK(QProcess::splitCommand(u"say \"\"\"hi\"\"\"") == QStringList({"say", "\"hi\""}));
    CHECK(QProcess::splitCommand(u"").isEmpty());
    CHECK(QProcess::splitCommand(u"   ").isEmpty());
    CHECK(QProcess::splitCommand(QString("x \"unterminated y")) == QStringList({"x", "unterminated y"}));
}

static void testBasicLifecycle() {
    QProcess p;
    Recorder rec(p);
    CHECK(p.state() == QProcess::NotRunning && p.processId() == 0 && p.isSequential());
    CHECK(p.error() == QProcess::UnknownError && p.errorString() == "Unknown error");
    p.start("/bin/sh", {"-c", "echo hello; exit 3"});
    CHECK(p.state() == QProcess::Starting);
    CHECK(p.processId() > 0);
    CHECK(finishedWithin(p));
    CHECK(p.exitCode() == 3 && p.exitStatus() == QProcess::NormalExit);
    CHECK(rec.events == QStringList({"state:1", "state:2", "started", "state:0", "finished:3:0"}));
    CHECK(p.readAllStandardOutput() == "hello\n");
    CHECK(p.processId() == 0);
    CHECK(p.error() == QProcess::UnknownError);
    CHECK(noStrayChildren());

    // The object can be started again; results of the first run are reset.
    rec.events.clear();
    p.start(helper(), {"exit", "0"});
    CHECK(finishedWithin(p));
    CHECK(p.exitCode() == 0 && rec.count("finished:0:0") == 1);
    p.start(helper(), {"exit", "42"});
    CHECK(finishedWithin(p));
    CHECK(p.exitCode() == 42);
    CHECK(noStrayChildren());
}

static void testExitCodes() {
    for (int code : {0, 1, 2, 7, 127, 255}) {
        QProcess p;
        p.start(helper(), {"exit", QString::number(code)});
        CHECK(finishedWithin(p));
        CHECK(p.exitCode() == code && p.exitStatus() == QProcess::NormalExit);
    }
    CHECK(QProcess::execute(helper(), {"exit", "5"}) == 5);
    CHECK(QProcess::execute(helper(), {"exit", "0"}) == 0);
    CHECK(QProcess::execute("/nonexistent/prog/xyz") == -2);
    CHECK(QProcess::execute(helper(), {"signal", "9"}) == -1);
    CHECK(noStrayChildren());
}

static void testFailedToStart() {
    {
        QProcess p;
        Recorder rec(p);
        p.start("/nonexistent/program/xyz");
        CHECK(p.state() == QProcess::Starting);
        CHECK(!p.waitForStarted(10000));
        CHECK(p.state() == QProcess::NotRunning && p.error() == QProcess::FailedToStart);
        CHECK(p.errorString().contains("No such file or directory"));
        CHECK(rec.events == QStringList({"state:1", "state:0", "error:0"}));
        CHECK(!p.waitForFinished(100));
        CHECK(p.exitCode() == 0);
    }
    {
        QProcess p;
        Recorder rec(p);
        p.start("nt-surely-no-such-program-in-path");
        CHECK(waitUntil([&] { return rec.has("error:0"); }));
        CHECK(p.state() == QProcess::NotRunning && p.error() == QProcess::FailedToStart);
        CHECK(rec.count("started") == 0 && rec.count("finished:0:0") == 0);
    }
    {
        // A file without execute permission cannot run, even for root.
        QTemporaryDir dir;
        const QString f = dir.filePath("script");
        CHECK(writeFile(f, "#!/bin/sh\nexit 0\n"));
        ::chmod(f.toUtf8().constData(), 0644);
        QProcess p;
        p.start(f);
        CHECK(!p.waitForStarted(10000));
        CHECK(p.error() == QProcess::FailedToStart && p.errorString().contains("Permission denied"));
    }
    {
        QProcess p;
        Recorder rec(p);
        p.start(QString());
        CHECK(p.state() == QProcess::NotRunning && p.error() == QProcess::FailedToStart);
        CHECK(p.errorString() == "No program defined" && rec.events == QStringList({"error:0"}));
    }
    {
        QProcess p;
        p.setProgram(helper());
        p.setWorkingDirectory("/nonexistent/working/dir");
        p.start();
        CHECK(!p.waitForStarted(10000));
        CHECK(p.error() == QProcess::FailedToStart);
    }
    {
        // A failed start can be followed by a successful one.
        QProcess p;
        p.start("/nonexistent/x");
        CHECK(!p.waitForStarted(10000));
        p.start(helper(), {"exit", "4"});
        CHECK(p.waitForStarted(10000));
        CHECK(p.waitForFinished(30000) && p.exitCode() == 4);
        CHECK(p.error() == QProcess::UnknownError);
    }
    CHECK(noStrayChildren());
}

static void testProgramLookup() {
    QProcess p;
    p.start("sh", {"-c", "exit 6"});
    CHECK(p.waitForFinished(30000) && p.exitCode() == 6);

    QTemporaryDir dir;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString script = dir.filePath("nt-test-tool");
    CHECK(writeFile(script, "#!/bin/sh\nexit 9\n"));
    ::chmod(script.toUtf8().constData(), 0755);
    env.insert("PATH", dir.path() + ":" + env.value("PATH"));
    QProcess q;
    q.setProcessEnvironment(env);
    q.start("nt-test-tool");
    CHECK(q.waitForFinished(30000) && q.exitCode() == 9 && q.exitStatus() == QProcess::NormalExit);

    // A relative program with a slash is resolved in the working directory.
    QProcess r;
    r.setWorkingDirectory(dir.path());
    r.start("./nt-test-tool");
    CHECK(r.waitForFinished(30000) && r.exitCode() == 9);
}

static void testWorkingDirectory() {
    QTemporaryDir dir;
    const QString canonical = QFileInfo(dir.path()).canonicalFilePath();
    QProcess p;
    p.setProgram(helper());
    p.setArguments({"cwd"});
    p.setWorkingDirectory(dir.path());
    CHECK(p.workingDirectory() == dir.path());
    p.start();
    CHECK(p.waitForFinished(30000));
    CHECK(QString::fromUtf8(p.readAllStandardOutput()) == canonical);
    // Without a working directory the child inherits ours.
    QProcess q;
    q.start(helper(), {"cwd"});
    CHECK(q.waitForFinished(30000));
    CHECK(QString::fromUtf8(q.readAllStandardOutput()) == QDir::currentPath());
}

static void testEnvironmentPassing() {
    ::setenv("NT_TEST_INHERIT", "inherited", 1);
    {
        QProcess p;
        CHECK(p.processEnvironment().inheritsFromParent());
        p.start(helper(), {"env", "NT_TEST_INHERIT"});
        CHECK(p.waitForFinished(30000));
        CHECK(p.readAllStandardOutput() == "inherited");
    }
    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("NT_TEST_NEW", "new value");
        env.remove("NT_TEST_INHERIT");
        QProcess p;
        p.setProcessEnvironment(env);
        CHECK(p.processEnvironment() == env);
        p.start(helper(), {"env", "NT_TEST_NEW"});
        CHECK(p.waitForFinished(30000));
        CHECK(p.readAllStandardOutput() == "new value");
        p.start(helper(), {"env", "NT_TEST_INHERIT"});
        CHECK(p.waitForFinished(30000));
        CHECK(p.readAllStandardOutput() == "<unset>");
    }
    {
        // An empty environment is exactly that; the program is then looked up in the default path.
        QProcessEnvironment env;
        env.insert("ONLY", "this");
        QProcess p;
        p.setProcessEnvironment(env);
        p.start(helper(), {"envcount"});
        CHECK(p.waitForFinished(30000));
        CHECK(p.readAllStandardOutput() == "1");
        p.start(helper(), {"env", "ONLY"});
        CHECK(p.waitForFinished(30000));
        CHECK(p.readAllStandardOutput() == "this");
    }
    {
        QProcess p;
        p.setProcessEnvironment(QProcessEnvironment());
        p.start(helper(), {"envcount"});
        CHECK(p.waitForFinished(30000));
        CHECK(p.readAllStandardOutput() == "0");
    }
    {
        // Entering inheritance again after an environment was set.
        QProcess p;
        QProcessEnvironment env;
        env.insert("X", "1");
        p.setProcessEnvironment(env);
        p.setProcessEnvironment(QProcessEnvironment(QProcessEnvironment::InheritFromParent));
        p.start(helper(), {"env", "NT_TEST_INHERIT"});
        CHECK(p.waitForFinished(30000));
        CHECK(p.readAllStandardOutput() == "inherited");
    }
    ::unsetenv("NT_TEST_INHERIT");
}

static void testChannelsSeparateAndMerged() {
    {
        QProcess p;
        int outSignals = 0;
        int errSignals = 0;
        QObject::connect(&p, &QProcess::readyReadStandardOutput, [&] { ++outSignals; });
        QObject::connect(&p, &QProcess::readyReadStandardError, [&] { ++errSignals; });
        p.start(helper(), {"both"});
        CHECK(finishedWithin(p));
        CHECK(outSignals >= 1 && errSignals >= 1);
        CHECK(p.readAllStandardOutput() == "out1;out2;");
        CHECK(p.readAllStandardError() == "err1;err2;");
        CHECK(p.readAllStandardOutput().isEmpty());
    }
    {
        QProcess p;
        p.setProcessChannelMode(QProcess::MergedChannels);
        CHECK(p.processChannelMode() == QProcess::MergedChannels);
        p.start(helper(), {"both"});
        CHECK(finishedWithin(p));
        const QByteArray all = p.readAllStandardOutput();
        CHECK(all.size() == 20 && all.count("out") == 2 && all.count("err") == 2);
        CHECK(p.readAllStandardError().isEmpty());
    }
    {
        // The read channel decides what read() sees and what triggers readyRead().
        QProcess p;
        int readyReads = 0;
        QObject::connect(&p, &QProcess::readyRead, [&] { ++readyReads; });
        p.setReadChannel(QProcess::StandardError);
        CHECK(p.readChannel() == QProcess::StandardError);
        p.start(helper(), {"both"});
        CHECK(finishedWithin(p));
        CHECK(readyReads >= 1);
        CHECK(p.bytesAvailable() == 10);
        CHECK(p.readAll() == "err1;err2;");
        p.setReadChannel(QProcess::StandardOutput);
        CHECK(p.bytesAvailable() == 10 && p.read(4) == "out1");
        CHECK(p.readAllStandardOutput() == ";out2;");
    }
    {
        // The channels of QIODevice: QProcess has two to read from and one to write to.
        QProcess p;
        CHECK(p.readChannelCount() == 0 && p.writeChannelCount() == 0);
        int ready[2] = {0, 0};
        int readyReads = 0;
        QObject::connect(&p, &QIODevice::channelReadyRead, [&](int channel) { ++ready[channel]; });
        QObject::connect(&p, &QIODevice::readyRead, [&] { ++readyReads; });
        p.start(helper(), {"both"});
        CHECK(p.readChannelCount() == 2 && p.writeChannelCount() == 1);
        CHECK(p.currentReadChannel() == 0 && p.currentWriteChannel() == 0);
        CHECK(finishedWithin(p));
        CHECK(ready[0] >= 1 && ready[1] >= 1 && readyReads >= 1);

        // Look-ahead data stays with its channel.
        CHECK(p.peek(4) == "out1");
        p.setCurrentReadChannel(1);
        CHECK(p.readChannel() == QProcess::StandardError && p.currentReadChannel() == 1);
        CHECK(p.bytesAvailable() == 10 && p.read(5) == "err1;");
        p.ungetChar('!');
        p.setReadChannel(QProcess::StandardOutput);
        CHECK(p.currentReadChannel() == 0 && p.bytesAvailable() == 10);
        CHECK(p.readAll() == "out1;out2;");
        p.setCurrentReadChannel(1);
        CHECK(p.readAll() == "!err2;");
        p.setCurrentReadChannel(-1); // ignored
        CHECK(p.currentReadChannel() == 1);

        // The channel cannot change during a transaction.
        p.startTransaction();
        p.setCurrentReadChannel(0);
        CHECK(p.currentReadChannel() == 1);
        p.rollbackTransaction();
        p.setCurrentReadChannel(0);
        CHECK(p.currentReadChannel() == 0);
        p.close();
        CHECK(p.readChannelCount() == 0);
    }
    {
        QProcess p;
        qint64 written = 0;
        int writeChannel = -1;
        QObject::connect(&p, &QIODevice::channelBytesWritten, [&](int channel, qint64 bytes) {
            writeChannel = channel;
            written += bytes;
        });
        p.start("/bin/cat", QStringList());
        CHECK(p.waitForStarted(5000));
        CHECK(p.write("hello") == 5);
        CHECK(p.waitForBytesWritten(5000));
        CHECK(written == 5 && writeChannel == 0);
        p.closeWriteChannel();
        CHECK(p.writeChannelCount() == 0 && p.readChannelCount() == 2);
        CHECK(finishedWithin(p));
        CHECK(p.readAll() == "hello");
    }
    {
        QProcess p;
        p.start("/bin/sh", {"-c", "echo one; echo two; printf three"});
        CHECK(finishedWithin(p));
        CHECK(p.canReadLine());
        CHECK(p.readLine() == "one\n");
        CHECK(p.canReadLine() && p.readLine() == "two\n");
        CHECK(!p.canReadLine());
        CHECK(p.readAll() == "three");
        CHECK(p.atEnd());
    }
}

static void testForwardedChannels() {
    QTemporaryDir dir;
    const QString captured = dir.filePath("captured");
    for (auto mode : {QProcess::ForwardedChannels, QProcess::ForwardedOutputChannel, QProcess::ForwardedErrorChannel}) {
        std::fflush(stdout);
        const int saveOut = ::dup(1);
        const int saveErr = ::dup(2);
        const int fd = ::open(captured.toUtf8().constData(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        ::dup2(fd, 1);
        ::dup2(fd, 2);
        ::close(fd);
        QProcess p;
        p.setProcessChannelMode(mode);
        p.start(helper(), {"both"});
        const bool done = finishedWithin(p);
        std::fflush(stdout);
        ::dup2(saveOut, 1);
        ::dup2(saveErr, 2);
        ::close(saveOut);
        ::close(saveErr);
        CHECK(done);
        const QByteArray fileData = readFile(captured);
        const QByteArray managedOut = p.readAllStandardOutput();
        const QByteArray managedErr = p.readAllStandardError();
        if (mode == QProcess::ForwardedChannels) {
            CHECK(fileData.count("out") == 2 && fileData.count("err") == 2);
            CHECK(managedOut.isEmpty() && managedErr.isEmpty());
        } else if (mode == QProcess::ForwardedOutputChannel) {
            CHECK(fileData == "out1;out2;");
            CHECK(managedOut.isEmpty() && managedErr == "err1;err2;");
        } else {
            CHECK(fileData == "err1;err2;");
            CHECK(managedErr.isEmpty() && managedOut == "out1;out2;");
        }
    }
    // execute() forwards.
    std::fflush(stdout);
    const int saveOut = ::dup(1);
    const int fd = ::open(captured.toUtf8().constData(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    ::dup2(fd, 1);
    ::close(fd);
    const int code = QProcess::execute(helper(), {"out", "executed"});
    ::dup2(saveOut, 1);
    ::close(saveOut);
    CHECK(code == 0 && readFile(captured) == "executed");
}

static void testFileRedirection() {
    QTemporaryDir dir;
    const QString in = dir.filePath("in.txt");
    const QString out = dir.filePath("out.txt");
    const QString err = dir.filePath("err.txt");
    CHECK(writeFile(in, "from a file"));
    {
        QProcess p;
        p.setStandardInputFile(in);
        p.setStandardOutputFile(out);
        p.setStandardErrorFile(err);
        p.start("/bin/sh", {"-c", "cat; echo to-err >&2"});
        CHECK(finishedWithin(p));
        CHECK(readFile(out) == "from a file");
        CHECK(readFile(err) == "to-err\n");
        CHECK(p.readAllStandardOutput().isEmpty() && p.readAllStandardError().isEmpty());
        CHECK(!p.isWritable());
    }
    {
        // Truncate by default, append on request.
        QProcess p;
        p.setStandardOutputFile(out);
        p.start(helper(), {"out", "AB"});
        CHECK(finishedWithin(p));
        CHECK(readFile(out) == "AB");
        QProcess q;
        q.setStandardOutputFile(out, QIODevice::Append);
        q.start(helper(), {"out", "CD"});
        CHECK(finishedWithin(q));
        CHECK(readFile(out) == "ABCD");
    }
    {
        QProcess p;
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.setStandardOutputFile(out);
        p.start(helper(), {"both"});
        CHECK(finishedWithin(p));
        CHECK(readFile(out).size() == 20);
    }
    {
        QProcess p;
        p.setStandardOutputFile(QProcess::nullDevice());
        p.start(helper(), {"out", "discarded"});
        CHECK(finishedWithin(p));
        CHECK(p.readAllStandardOutput().isEmpty() && p.exitCode() == 0);
    }
    {
        QProcess p;
        p.setStandardInputFile(dir.filePath("does-not-exist"));
        p.start(helper(), {"cat"});
        CHECK(!p.waitForStarted(10000));
        CHECK(p.error() == QProcess::FailedToStart && p.errorString().contains("input file"));
    }
    {
        QProcess p;
        p.setStandardOutputFile(dir.filePath("no/such/dir/out"));
        p.start(helper(), {"out", "x"});
        CHECK(!p.waitForStarted(10000));
        CHECK(p.error() == QProcess::FailedToStart);
    }
    CHECK(noStrayChildren());
}

static void testOpenModes() {
    {
        // Without ReadOnly the output is discarded.
        QProcess p;
        p.start(helper(), {"both"}, QIODevice::WriteOnly);
        CHECK(finishedWithin(p));
        CHECK(p.readAllStandardOutput().isEmpty() && p.readAllStandardError().isEmpty());
    }
    {
        // Without WriteOnly, standard input is /dev/null, so cat finishes at once.
        QProcess p;
        p.start(helper(), {"count"}, QIODevice::ReadOnly);
        CHECK(finishedWithin(p));
        CHECK(p.readAllStandardOutput() == "0");
        CHECK(!p.isWritable() && p.write("x") == -1);
    }
    {
        QProcess p;
        p.start(helper(), {"out", "z"}, QIODevice::NotOpen);
        CHECK(finishedWithin(p));
        CHECK(p.readAllStandardOutput().isEmpty());
    }
}

static void testStdin() {
    {
        QProcess p;
        p.start("/bin/cat");
        CHECK(p.waitForStarted(10000));
        CHECK(p.state() == QProcess::Running && p.isWritable() && p.isReadable());
        CHECK(p.write("hello ") == 6);
        CHECK(p.write(QByteArray("world")) == 5);
        CHECK(p.bytesToWrite() == 11);
        CHECK(p.waitForBytesWritten(10000));
        CHECK(p.bytesToWrite() == 0);
        p.closeWriteChannel();
        CHECK(!p.isWritable());
        CHECK(p.write("more") == -1);
        CHECK(p.waitForFinished(30000));
        CHECK(p.readAllStandardOutput() == "hello world");
    }
    {
        // Event-loop driven round trip, closing the channel after the data went out.
        QProcess p;
        QByteArray got;
        qint64 written = 0;
        QObject::connect(&p, &QProcess::readyReadStandardOutput, [&] { got += p.readAllStandardOutput(); });
        QObject::connect(&p, &QProcess::bytesWritten, [&](qint64 n) { written += n; });
        p.start("/bin/cat");
        p.write("line 1\n");
        p.write("line 2\n");
        p.closeWriteChannel();
        CHECK(finishedWithin(p));
        got += p.readAllStandardOutput();
        CHECK(got == "line 1\nline 2\n" && written == 14);
    }
    {
        // Data written before the process started running is not lost.
        QProcess p;
        p.start(helper(), {"count"});
        p.write("12345");
        p.closeWriteChannel();
        CHECK(finishedWithin(p));
        CHECK(p.readAllStandardOutput() == "5");
    }
    {
        // Writing to a closed process fails cleanly.
        QProcess p;
        p.start(helper(), {"exit", "0"});
        CHECK(finishedWithin(p));
        CHECK(p.write("late") == -1);
    }
}

static void testLargeTransfers() {
    const qsizetype size = 6 * 1024 * 1024;
    const QByteArray data = pattern(size);
    {
        // 6 MB through cat, event loop driven: both pipes are far beyond their buffers.
        QProcess p;
        QByteArray got;
        QObject::connect(&p, &QProcess::readyReadStandardOutput, [&] { got += p.readAllStandardOutput(); });
        p.start("/bin/cat");
        p.write(data);
        p.closeWriteChannel();
        CHECK(finishedWithin(p));
        got += p.readAllStandardOutput();
        CHECK(got.size() == size && got == data);
    }
    {
        // The same with blocking waits only; they must move data in both directions.
        QProcess p;
        p.start("/bin/cat");
        CHECK(p.waitForStarted(10000));
        p.write(data);
        p.closeWriteChannel();
        QByteArray got;
        while (p.waitForReadyRead(20000))
            got += p.readAllStandardOutput();
        CHECK(p.state() == QProcess::NotRunning);
        got += p.readAllStandardOutput();
        CHECK(got.size() == size && got == data);
    }
    {
        // Output of both channels, larger than the pipe buffers, without reading in between.
        QProcess p;
        p.start(helper(), {"bigboth", QString::number(3 * 1024 * 1024)});
        CHECK(p.waitForFinished(30000));
        CHECK(p.readAllStandardOutput() == pattern(3 * 1024 * 1024));
        CHECK(p.readAllStandardError() == pattern(3 * 1024 * 1024));
    }
    {
        // Large output with the process dying right after writing it.
        QProcess p;
        qint64 total = 0;
        QObject::connect(&p, &QProcess::readyReadStandardOutput, [&] { total += p.readAllStandardOutput().size(); });
        p.start(helper(), {"big", "10000000"});
        CHECK(finishedWithin(p));
        total += p.readAllStandardOutput().size();
        CHECK(total == 10000000);
    }
    CHECK(noStrayChildren());
}

static void testWaitFunctions() {
    {
        QProcess p;
        p.start(helper(), {"ready", "30000"});
        CHECK(p.waitForStarted(10000));
        CHECK(p.waitForStarted(10) && p.state() == QProcess::Running);
        CHECK(p.waitForReadyRead(10000));
        CHECK(p.readAllStandardOutput() == "ready\n");
        // Nothing more arrives: the wait times out and the process keeps running.
        CHECK(!p.waitForReadyRead(100));
        CHECK(p.error() == QProcess::Timedout && p.state() == QProcess::Running);
        CHECK(!p.waitForFinished(100));
        CHECK(p.error() == QProcess::Timedout && p.state() == QProcess::Running);
        CHECK(!p.waitForBytesWritten(10));
        p.kill();
        CHECK(p.waitForFinished(10000));
        CHECK(p.exitStatus() == QProcess::CrashExit);
        CHECK(!p.waitForFinished(10) && !p.waitForReadyRead(10) && !p.waitForStarted(10));
    }
    {
        // Timedout is not signalled through errorOccurred().
        QProcess p;
        Recorder rec(p);
        p.start(helper(), {"sleep", "5000"});
        CHECK(!p.waitForFinished(50));
        CHECK(p.error() == QProcess::Timedout && !rec.has("error:2"));
        p.kill();
        CHECK(p.waitForFinished(10000));
    }
    {
        QProcess p;
        p.start(helper(), {"out", "abc"});
        QByteArray got;
        int readyReads = 0;
        QObject::connect(&p, &QProcess::readyRead, [&] { ++readyReads; });
        CHECK(p.waitForReadyRead(10000));
        CHECK(readyReads == 1 && p.bytesAvailable() == 3);
        CHECK(p.readAll() == "abc");
        CHECK(finishedWithin(p));
        CHECK(!p.waitForReadyRead(10));
    }
    {
        // Finishing during a wait emits the signals from within the wait.
        QProcess p;
        Recorder rec(p);
        p.start(helper(), {"exit", "2"});
        CHECK(p.waitForFinished(30000));
        CHECK(rec.has("started") && rec.has("finished:2:0") && rec.has("state:0"));
    }
    {
        QProcess p;
        p.start(helper(), {"exit", "0"});
        CHECK(p.waitForFinished(-1));
    }
}

static void testKillTerminateCrash() {
    {
        QProcess p;
        Recorder rec(p);
        p.start(helper(), {"ready", "30000"});
        CHECK(p.waitForStarted(10000));
        p.terminate();
        CHECK(finishedWithin(p));
        CHECK(p.exitStatus() == QProcess::CrashExit && p.error() == QProcess::Crashed);
        CHECK(p.errorString() == "Process crashed");
        CHECK(rec.events == QStringList({"state:1", "state:2", "started", "error:1", "state:0", "finished:0:1"}));
    }
    {
        QProcess p;
        p.start(helper(), {"ignoreterm", "30000"});
        CHECK(p.waitForReadyRead(10000));
        p.terminate();
        CHECK(!p.waitForFinished(200));
        CHECK(p.state() == QProcess::Running);
        p.kill();
        CHECK(p.waitForFinished(10000));
        CHECK(p.exitStatus() == QProcess::CrashExit);
    }
    for (int sig : {SIGSEGV, SIGABRT, SIGKILL, SIGTERM, SIGUSR1}) {
        QProcess p;
        Recorder rec(p);
        p.start(helper(), {"signal", QString::number(sig)});
        CHECK(finishedWithin(p));
        CHECK(p.exitStatus() == QProcess::CrashExit && p.error() == QProcess::Crashed);
        CHECK(rec.has("error:1") && rec.has("finished:0:1"));
    }
    {
        // kill() right after start(), before the event loop ran.
        QProcess p;
        Recorder rec(p);
        p.start(helper(), {"sleep", "30000"});
        p.kill();
        CHECK(finishedWithin(p));
        CHECK(p.exitStatus() == QProcess::CrashExit);
        CHECK(rec.count("state:0") == 1);
    }
    {
        // kill() and terminate() on a process that is not running do nothing.
        QProcess p;
        p.kill();
        p.terminate();
        CHECK(p.state() == QProcess::NotRunning);
    }
    CHECK(noStrayChildren());
}

static void testWriteError() {
    QProcess p;
    Recorder rec(p);
    p.start(helper(), {"closestdin", "30000"});
    CHECK(p.waitForReadyRead(10000));
    p.write(pattern(256 * 1024));
    CHECK(waitUntil([&] { return rec.has("error:4"); }));
    CHECK(p.error() == QProcess::WriteError && p.state() == QProcess::Running);
    CHECK(!p.isWritable() && p.write("more") == -1);
    p.kill();
    CHECK(p.waitForFinished(10000));
    CHECK(noStrayChildren());
}

static void testChildExitsBeforeReading() {
    // The child never reads stdin and exits; the write ends with WriteError or goes unnoticed, never a SIGPIPE.
    QProcess p;
    p.start(helper(), {"exit", "0"});
    CHECK(p.waitForStarted(10000));
    p.write(pattern(512 * 1024));
    CHECK(finishedWithin(p));
    CHECK(p.exitCode() == 0);
}

static void testChildProcessModifier() {
    QProcess p;
    p.setChildProcessModifier([] { ::umask(0077); });
    p.start(helper(), {"umask"});
    CHECK(p.waitForFinished(30000));
    CHECK(p.readAllStandardOutput() == "077");

    QProcess q;
    q.setWorkingDirectory("/tmp");
    q.setChildProcessModifier([] {
        if (::chdir("/") != 0)
            ::_exit(99);
    });
    q.start(helper(), {"cwd"});
    CHECK(q.waitForFinished(30000));
    CHECK(q.readAllStandardOutput() == "/");
}

static void testChildStartState() {
    // Signal dispositions and mask in the child are reset, even if the parent changed them.
    struct sigaction ign;
    struct sigaction old;
    std::memset(&ign, 0, sizeof ign);
    ign.sa_handler = SIG_IGN;
    ::sigaction(SIGPIPE, &ign, &old);
    sigset_t blockUsr;
    sigset_t oldMask;
    sigemptyset(&blockUsr);
    sigaddset(&blockUsr, SIGUSR2);
    sigaddset(&blockUsr, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &blockUsr, &oldMask);
    QProcess p;
    p.start(helper(), {"sigpipe"});
    CHECK(p.waitForFinished(30000));
    CHECK(p.readAllStandardOutput() == "default");
    QProcess q;
    q.start(helper(), {"sigmask"});
    CHECK(q.waitForFinished(30000));
    CHECK(q.readAllStandardOutput() == "0");
    // The parent's own state is untouched.
    struct sigaction now;
    ::sigaction(SIGPIPE, nullptr, &now);
    CHECK(now.sa_handler == SIG_IGN);
    sigset_t cur;
    pthread_sigmask(SIG_BLOCK, nullptr, &cur);
    CHECK(sigismember(&cur, SIGUSR1) == 1 && sigismember(&cur, SIGUSR2) == 1);
    pthread_sigmask(SIG_SETMASK, &oldMask, nullptr);
    ::sigaction(SIGPIPE, &old, nullptr);
}

static void testNoDescriptorLeaks() {
    QProcess p;
    QProcess other;
    other.start(helper(), {"ready", "30000"});
    CHECK(other.waitForStarted(10000));
    p.start(helper(), {"fds"});
    CHECK(p.waitForFinished(30000));
    // Descriptors the test process inherited without close-on-exec are legitimately passed on.
    QByteArray inherited = "[";
    for (int fd = 3; fd < 1024; ++fd) {
        const int flags = ::fcntl(fd, F_GETFD);
        if (flags >= 0 && !(flags & FD_CLOEXEC))
            inherited += QByteArray::number(fd) + " ";
    }
    inherited += "]";
    const QByteArray openFds = p.readAllStandardOutput();
    CHECK(openFds == inherited);
    if (openFds != inherited)
        std::printf("open descriptors in child: %s, expected %s\n", openFds.constData(), inherited.constData());
    other.kill();
    CHECK(other.waitForFinished(10000));
}

static void testPipeline() {
    for (int order = 0; order < 2; ++order) {
        QProcess producer;
        QProcess consumer;
        producer.setStandardOutputProcess(&consumer);
        if (order == 0) {
            producer.start(helper(), {"big", "300000"});
            consumer.start(helper(), {"count"});
        } else {
            consumer.start(helper(), {"count"});
            producer.start(helper(), {"big", "300000"});
        }
        CHECK(finishedWithin(producer) && finishedWithin(consumer));
        CHECK(consumer.readAllStandardOutput() == "300000");
        CHECK(producer.readAllStandardOutput().isEmpty());
        CHECK(consumer.exitCode() == 0 && producer.exitCode() == 0);
    }
    {
        QProcess producer;
        QProcess consumer;
        producer.setStandardOutputProcess(&consumer);
        producer.setStandardOutputProcess(nullptr);
        producer.start(helper(), {"out", "direct"});
        CHECK(finishedWithin(producer));
        CHECK(producer.readAllStandardOutput() == "direct");
    }
    CHECK(noStrayChildren());
}

static void testCloseAndDestroy() {
    // A process that ignores SIGTERM is killed after five seconds; once is enough.
    if (std::strcmp(g_modeName, "pidfd") == 0) {
        QProcess p;
        p.start(helper(), {"ignoreterm", "30000"});
        CHECK(p.waitForReadyRead(10000));
        QElapsedTimer t;
        t.start();
        bool aboutToClose = false;
        QObject::connect(&p, &QProcess::aboutToClose, [&] { aboutToClose = true; });
        p.close();
        CHECK(aboutToClose && !p.isOpen());
        CHECK(p.state() == QProcess::NotRunning);
        CHECK(t.elapsed() < 20000);
    }
    {
        QProcess p;
        p.start("/bin/cat");
        CHECK(p.waitForStarted(10000));
        p.close();
        CHECK(p.state() == QProcess::NotRunning && !p.isOpen());
    }
    {
        // Destroying a running process kills it and reaps it.
        auto *p = new QProcess;
        p->start(helper(), {"sleep", "30000"});
        CHECK(p->waitForStarted(10000));
        const qint64 pid = p->processId();
        delete p;
        CHECK(::kill(static_cast<pid_t>(pid), 0) != 0 && errno == ESRCH);
    }
    {
        // Deleting a process from its own finished() slot.
        auto *p = new QProcess;
        bool gone = false;
        QObject::connect(p, &QProcess::finished, [&] {
            delete p;
            gone = true;
        });
        p->start(helper(), {"out", "bye"});
        CHECK(waitUntil([&] { return gone; }));
    }
    {
        auto *p = new QProcess;
        QPointer<QProcess> guard(p);
        QObject::connect(p, &QProcess::readyReadStandardOutput, [p] { p->deleteLater(); });
        p->start(helper(), {"out", "bye"});
        CHECK(waitUntil([&] { return guard.isNull(); }));
    }
    CHECK(noStrayChildren());
}

static void testManyProcesses() {
    const std::size_t n = 24;
    std::vector<std::unique_ptr<QProcess>> procs;
    std::vector<int> finished(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        procs.emplace_back(new QProcess);
        QProcess *p = procs.back().get();
        QObject::connect(p, &QProcess::finished,
                         [&finished, i](int code, QProcess::ExitStatus) { finished[i] = code + 1; });
        p->start(helper(), {"exit", QString::number(i)});
    }
    CHECK(waitUntil([&] {
        for (int v : finished)
            if (v == 0)
                return false;
        return true;
    }));
    for (std::size_t i = 0; i < n; ++i)
        CHECK(static_cast<std::size_t>(finished[i]) == i + 1);
    procs.clear();
    CHECK(noStrayChildren());
}

static void testGrandchildHoldingPipe() {
    // The grandchild keeps stdout open; QProcess still finishes when its own child exits.
    QProcess p;
    p.start(helper(), {"background", "4000"});
    QElapsedTimer t;
    t.start();
    CHECK(finishedWithin(p, 3500));
    CHECK(p.readAllStandardOutput() == "parent-done\n");
    CHECK(t.elapsed() < 3500);
    CHECK(noStrayChildren());
}

static void testDetached() {
    QTemporaryDir dir;
    const QString marker = dir.filePath("marker");
    qint64 pid = 0;
    CHECK(QProcess::startDetached(
        "/bin/sh", {"-c", "echo $$ > " + marker + "; echo detached-out; echo detached-err >&2"}, QString(), &pid));
    CHECK(pid > 0);
    CHECK(waitUntil([&] { return readFile(marker).trimmed().toLongLong() > 0; }));
    CHECK(readFile(marker).trimmed().toLongLong() == pid);
    CHECK(noStrayChildren());

    // Working directory and the member form with the object's settings.
    const QString cwdFile = dir.filePath("cwd");
    QProcess p;
    p.setProgram("/bin/sh");
    p.setArguments({"-c", "pwd > " + cwdFile});
    p.setWorkingDirectory(dir.path());
    qint64 pid2 = 0;
    CHECK(p.startDetached(&pid2) && pid2 > 0 && pid2 != pid);
    CHECK(p.state() == QProcess::NotRunning);
    CHECK(waitUntil([&] { return !readFile(cwdFile).isEmpty(); }));
    CHECK(QString::fromUtf8(readFile(cwdFile).trimmed()) == QFileInfo(dir.path()).canonicalFilePath());

    // Redirected output of a detached process, and its own environment.
    const QString outFile = dir.filePath("out");
    QProcess q;
    q.setProgram(helper());
    q.setArguments({"env", "NT_DETACHED"});
    QProcessEnvironment env;
    env.insert("NT_DETACHED", "yes");
    q.setProcessEnvironment(env);
    q.setStandardOutputFile(outFile);
    CHECK(q.startDetached());
    CHECK(waitUntil([&] { return !readFile(outFile).isEmpty(); }));
    CHECK(readFile(outFile) == "yes");

    // A detached process survives the object that started it.
    const QString sessFile = dir.filePath("sess");
    {
        QProcess r;
        r.setProgram("/bin/sh");
        r.setArguments({"-c", "sleep 1; echo done > " + sessFile});
        CHECK(r.startDetached());
    }
    CHECK(waitUntil([&] { return QFileInfo::exists(sessFile); }));

    // Failures.
    QProcess bad;
    Recorder rec(bad);
    CHECK(!QProcess::startDetached("/nonexistent/detached/program"));
    bad.setProgram("/nonexistent/detached/program");
    CHECK(!bad.startDetached());
    CHECK(bad.error() == QProcess::FailedToStart && bad.errorString().contains("No such file or directory"));
    CHECK(rec.has("error:0") && bad.state() == QProcess::NotRunning);
    QProcess none;
    CHECK(!none.startDetached() && none.error() == QProcess::FailedToStart);
    CHECK(!QProcess::startDetached(helper(), {}, "/nonexistent/dir"));
    CHECK(noStrayChildren());
}

static void testStartCommandAndOpen() {
    {
        QProcess p;
        p.startCommand(helper() + " args \"two words\"");
        CHECK(p.waitForFinished(30000));
        CHECK(p.readAllStandardOutput() == "two words\n");
    }
    {
        QProcess p;
        p.startCommand("   ");
        CHECK(p.state() == QProcess::NotRunning && p.error() == QProcess::FailedToStart);
    }
    {
        QProcess p;
        CHECK(!p.open(QIODevice::ReadWrite));
        p.setProgram(helper());
        p.setArguments({"count"});
        CHECK(p.open(QIODevice::ReadWrite));
        CHECK(p.isOpen());
        p.closeWriteChannel();
        CHECK(p.waitForFinished(30000));
        CHECK(p.readAll() == "0");
        CHECK(!p.open(QIODevice::ReadWrite) || p.state() != QProcess::NotRunning);
        p.close();
    }
}

static std::atomic<int> g_sigchld{0};
static void onSigchld(int) { ++g_sigchld; }

static void testSigchldCoexistence() {
    struct sigaction mine;
    struct sigaction old;
    std::memset(&mine, 0, sizeof mine);
    mine.sa_handler = onSigchld;
    sigemptyset(&mine.sa_mask);
    mine.sa_flags = SA_RESTART;
    ::sigaction(SIGCHLD, &mine, &old);
    g_sigchld = 0;
    {
        QProcess p;
        p.start(helper(), {"exit", "3"});
        CHECK(finishedWithin(p));
        CHECK(p.exitCode() == 3 && p.exitStatus() == QProcess::NormalExit);
    }
    CHECK(g_sigchld.load() >= 1);
    struct sigaction now;
    ::sigaction(SIGCHLD, nullptr, &now);
    CHECK(now.sa_handler == onSigchld);
    CHECK(noStrayChildren());
    ::sigaction(SIGCHLD, &old, nullptr);

    // With SIGCHLD ignored the kernel reaps the child itself: the exit status is lost, but QProcess still finishes.
    struct sigaction ign;
    std::memset(&ign, 0, sizeof ign);
    ign.sa_handler = SIG_IGN;
    ::sigaction(SIGCHLD, &ign, &old);
    {
        QProcess p;
        Recorder rec(p);
        p.start(helper(), {"exit", "3"});
        CHECK(finishedWithin(p));
        CHECK(rec.count("started") == 1 && p.state() == QProcess::NotRunning);
    }
    ::sigaction(SIGCHLD, &old, nullptr);
}

static void testRestartInsideSlots() {
    // A new run started from finished().
    QProcess p;
    int runs = 0;
    QObject::connect(&p, &QProcess::finished, [&](int code, QProcess::ExitStatus) {
        ++runs;
        if (runs < 3)
            p.start(helper(), {"exit", QString::number(code + 1)});
    });
    p.start(helper(), {"exit", "1"});
    CHECK(waitUntil([&] { return runs == 3; }));
    CHECK(p.waitForFinished(10) == false);
    CHECK(waitUntil([&] { return p.state() == QProcess::NotRunning; }));
    CHECK(p.exitCode() == 3);
}

static void testStdinNotConsumedByTty() {
    QProcess p;
    p.start(helper(), {"isatty"});
    CHECK(p.waitForFinished(30000));
    CHECK(p.readAllStandardOutput() == "notty");
}

static void testInThreads() {
    // Each thread runs its own event loop and its own processes.
    const int n = 4;
    std::atomic<int> okCount{0};
    std::vector<QThread *> threads;
    for (int t = 0; t < n; ++t) {
        threads.push_back(QThread::create([&, t] {
            QEventLoop loop;
            QProcess p;
            QByteArray out;
            int code = -1;
            QObject::connect(&p, &QProcess::readyReadStandardOutput, [&] { out += p.readAllStandardOutput(); });
            QObject::connect(&p, &QProcess::finished, [&](int c, QProcess::ExitStatus) {
                code = c;
                loop.quit();
            });
            QTimer guard;
            guard.setSingleShot(true);
            QObject::connect(&guard, &QTimer::timeout, [&] { loop.quit(); });
            guard.start(30000);
            p.start(helper(), {"big", "700000"});
            loop.exec();
            out += p.readAllStandardOutput();
            QProcess q;
            q.start(helper(), {"exit", QString::number(t + 10)});
            const bool waited = q.waitForFinished(30000);
            if (code == 0 && out.size() == 700000 && waited && q.exitCode() == t + 10)
                ++okCount;
        }));
        threads.back()->start();
    }
    for (QThread *t : threads) {
        CHECK(t->wait(60000));
        delete t;
    }
    CHECK(okCount == n);
    CHECK(noStrayChildren());
}

static void run(const char *name, void (*test)()) {
    if (std::getenv("NT_TEST_VERBOSE"))
        std::fprintf(stderr, "[%s] %s\n", g_modeName, name);
    test();
}

static void runAll() {
    run("testExitCodes", testExitCodes);
    run("testBasicLifecycle", testBasicLifecycle);
    run("testFailedToStart", testFailedToStart);
    run("testProgramLookup", testProgramLookup);
    run("testWorkingDirectory", testWorkingDirectory);
    run("testEnvironmentPassing", testEnvironmentPassing);
    run("testChannelsSeparateAndMerged", testChannelsSeparateAndMerged);
    run("testFileRedirection", testFileRedirection);
    run("testOpenModes", testOpenModes);
    run("testStdin", testStdin);
    run("testWaitFunctions", testWaitFunctions);
    run("testKillTerminateCrash", testKillTerminateCrash);
    run("testWriteError", testWriteError);
    run("testChildExitsBeforeReading", testChildExitsBeforeReading);
    run("testChildProcessModifier", testChildProcessModifier);
    run("testChildStartState", testChildStartState);
    run("testNoDescriptorLeaks", testNoDescriptorLeaks);
    run("testPipeline", testPipeline);
    run("testCloseAndDestroy", testCloseAndDestroy);
    run("testManyProcesses", testManyProcesses);
    run("testSigchldCoexistence", testSigchldCoexistence);
    run("testRestartInsideSlots", testRestartInsideSlots);
    run("testStdinNotConsumedByTty", testStdinNotConsumedByTty);
    run("testInThreads", testInThreads);
}

// Runs a shell command with `setup` applied to the process and returns its standard output, trimmed.
static QByteArray shellOutput(const QString &command, const std::function<void(QProcess &)> &setup = {}) {
    QProcess p;
    if (setup)
        setup(p);
    p.start(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), command});
    if (!p.waitForFinished(30000))
        return QByteArray("<timeout>");
    return p.readAllStandardOutput().trimmed();
}

static void testUnixProcessParameters() {
    // The environment as a list of NAME=VALUE strings.
    QProcess env;
    CHECK(env.environment().isEmpty());
    env.setEnvironment({QStringLiteral("NT_A=1"), QStringLiteral("NT_B=x=y"), QStringLiteral("broken")});
    CHECK(env.processEnvironment().value(QStringLiteral("NT_B")) == QStringLiteral("x=y"));
    CHECK(env.environment().contains(QStringLiteral("NT_A=1")) && env.environment().size() == 2);
    CHECK(shellOutput(QStringLiteral("echo $NT_B"), [](QProcess &p) {
              p.setEnvironment({QStringLiteral("NT_B=x=y"), QStringLiteral("PATH=/usr/bin:/bin")});
          }) == "x=y");

    // The modifier can be read back, and it can make the start fail with a reason.
    QProcess failing;
    CHECK(!failing.childProcessModifier());
    failing.setChildProcessModifier([] { QProcess::failChildProcessModifier("no jail", ENOENT); });
    CHECK(static_cast<bool>(failing.childProcessModifier()));
    failing.start(helper(), {"exit", "0"});
    CHECK(!failing.waitForStarted(30000));
    CHECK(failing.error() == QProcess::FailedToStart);
    CHECK(failing.errorString().contains(QStringLiteral("no jail")) &&
          failing.errorString().contains(QString::fromUtf8(std::strerror(ENOENT))));
    QProcess failingQuietly;
    failingQuietly.setChildProcessModifier([] { QProcess::failChildProcessModifier("plain"); });
    failingQuietly.start(helper(), {"exit", "0"});
    CHECK(!failingQuietly.waitForStarted(30000) && failingQuietly.errorString().endsWith(QStringLiteral("plain")));

    // The parameters are kept, and the flags-only overload resets the rest.
    QProcess params;
    CHECK(params.unixProcessParameters().flags == QProcess::UnixProcessFlags());
    QProcess::UnixProcessParameters up;
    up.flags = QProcess::UnixProcessFlag::CloseFileDescriptors;
    up.lowestFileDescriptorToClose = 40;
    params.setUnixProcessParameters(up);
    CHECK(params.unixProcessParameters().lowestFileDescriptorToClose == 40);
    params.setUnixProcessParameters(QProcess::UnixProcessFlag::IgnoreSigPipe);
    CHECK(params.unixProcessParameters().flags == QProcess::UnixProcessFlag::IgnoreSigPipe &&
          params.unixProcessParameters().lowestFileDescriptorToClose == 0);

    // A descriptor that is not close-on-exec is inherited, unless CloseFileDescriptors closes it.
    const int fd = ::open("/dev/null", O_RDONLY);
    const int leaked = ::fcntl(fd, F_DUPFD, 50);
    ::close(fd);
    CHECK(leaked >= 50);
    const QString check = QStringLiteral("test -e /proc/self/fd/%1 && echo open || echo closed").arg(leaked);
    CHECK(shellOutput(check) == "open");
    CHECK(shellOutput(check, [](QProcess &p) {
              p.setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors);
          }) == "closed");
    CHECK(shellOutput(check, [leaked](QProcess &p) {
              QProcess::UnixProcessParameters keepLow;
              keepLow.flags = QProcess::UnixProcessFlag::CloseFileDescriptors;
              keepLow.lowestFileDescriptorToClose = leaked + 1;
              p.setUnixProcessParameters(keepLow);
          }) == "open");
    ::close(leaked);

    // SIGPIPE ignored (bit 13 of SigIgn), a new session, no core dumps.
    const QString sigIgn = QStringLiteral("grep SigIgn /proc/self/status | cut -f2");
    auto ignoresPipe = [](const QByteArray &mask) { return (mask.toULongLong(nullptr, 16) & (1ULL << 12)) != 0; };
    CHECK(!ignoresPipe(shellOutput(sigIgn)));
    CHECK(ignoresPipe(shellOutput(
        sigIgn, [](QProcess &p) { p.setUnixProcessParameters(QProcess::UnixProcessFlag::IgnoreSigPipe); })));
    const QString session = QStringLiteral("echo $$ $(cut -d' ' -f6 /proc/$$/stat)");
    const QList<QByteArray> fresh =
        shellOutput(session, [](QProcess &p) {
            p.setUnixProcessParameters(QProcess::UnixProcessFlag::CreateNewSession |
                                       QProcess::UnixProcessFlag::DisconnectControllingTerminal);
        }).split(' ');
    CHECK(fresh.size() == 2 && fresh[0] == fresh[1]);
    const QList<QByteArray> inherited = shellOutput(session).split(' ');
    CHECK(inherited.size() == 2 && inherited[0] != inherited[1]);
    CHECK(shellOutput(QStringLiteral("ulimit -c"), [](QProcess &p) {
              p.setUnixProcessParameters(QProcess::UnixProcessFlag::DisableCoreDumps |
                                         QProcess::UnixProcessFlag::ResetIds);
          }) == "0");
}

int main() {
    testEnvironment();
    testSplitCommand();

    runAll();
    testLargeTransfers();
    testForwardedChannels();
    testGrandchildHoldingPipe();
    testDetached();
    testStartCommandAndOpen();
    testUnixProcessParameters();

    // The same again with a kernel that has no pidfd: children are polled with waitpid().
    g_modeName = "poll";
    QtPrivate::setProcessPidfdEnabled(false);
    runAll();
    testLargeTransfers();
    testGrandchildHoldingPipe();
    QtPrivate::setProcessPidfdEnabled(true);

    if (g_failures == 0)
        std::printf("test_process: all tests passed (%s)\n", g_modeName);
    else
        std::printf("test_process: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
