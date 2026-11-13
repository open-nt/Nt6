// MIT License - see LICENSE
#include <QtCore/qprocess.h>

#include <QtCore/qdebug.h>
#include <QtCore/qpointer.h>
#include <QtCore/qsocketnotifier.h>

#include "qprocess_p.h"

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern char **environ;

namespace {

using Clock = std::chrono::steady_clock;

std::atomic<bool> g_pidfdEnabled{true};

constexpr int kPollSliceMs = 10;
constexpr qsizetype kReadChunk = 65536;
constexpr int kMergeIntoOut = -2;

int openPidfd(pid_t pid) {
#ifdef SYS_pidfd_open
    if (!g_pidfdEnabled.load())
        return -1;
    const long fd = ::syscall(SYS_pidfd_open, static_cast<long>(pid), 0L);
    return fd < 0 ? -1 : static_cast<int>(fd);
#else
    (void)pid;
    return -1;
#endif
}

void closeFd(int &fd) {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

bool setNonBlocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL);
    return flags >= 0 && ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

// Moves a descriptor out of 0..2 so that the child's dup2() calls cannot clobber it.
bool ensureHigh(int &fd) {
    if (fd < 0 || fd > 2)
        return true;
    const int moved = ::fcntl(fd, F_DUPFD_CLOEXEC, 3);
    if (moved < 0)
        return false;
    ::close(fd);
    fd = moved;
    return true;
}

bool makePipe(int fds[2]) {
    if (::pipe2(fds, O_CLOEXEC) != 0)
        return false;
    if (!ensureHigh(fds[0]) || !ensureHigh(fds[1])) {
        const int e = errno;
        ::close(fds[0]);
        ::close(fds[1]);
        errno = e;
        return false;
    }
    return true;
}

int openFile(const QString &path, int flags) {
    const QByteArray encoded = path.toUtf8();
    int fd;
    do {
        fd = ::open(encoded.constData(), flags | O_CLOEXEC, 0666);
    } while (fd < 0 && errno == EINTR);
    if (fd >= 0 && !ensureHigh(fd)) {
        const int e = errno;
        ::close(fd);
        errno = e;
        return -1;
    }
    return fd;
}

QString errnoString(int e) { return QString::fromUtf8(std::strerror(e)); }

// A write that reports EPIPE instead of killing the process with SIGPIPE, without changing the
// application's SIGPIPE disposition.
ssize_t writeNoSigpipe(int fd, const char *data, std::size_t size) {
    sigset_t block;
    sigset_t previous;
    sigemptyset(&block);
    sigaddset(&block, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &block, &previous);
    sigset_t pending;
    sigemptyset(&pending);
    sigpending(&pending);
    const bool wasPending = sigismember(&pending, SIGPIPE) == 1;
    ssize_t n;
    do {
        n = ::write(fd, data, size);
    } while (n < 0 && errno == EINTR);
    const int e = errno;
    if (n < 0 && e == EPIPE && !wasPending) {
        const timespec zero = {0, 0};
        while (::sigtimedwait(&block, nullptr, &zero) < 0 && errno == EINTR) {
        }
    }
    pthread_sigmask(SIG_SETMASK, &previous, nullptr);
    errno = e;
    return n;
}

pid_t waitPid(pid_t pid, int *status, int options) {
    pid_t r;
    do {
        r = ::waitpid(pid, status, options);
    } while (r < 0 && errno == EINTR);
    return r;
}

// Returns the program to execute: names without a slash are searched in the colon separated \a path.
QByteArray resolveProgram(const QByteArray &program, const QByteArray &path) {
    if (program.isEmpty() || program.contains('/'))
        return program;
    QByteArray fallback;
    qsizetype start = 0;
    while (start <= path.size()) {
        qsizetype end = path.indexOf(':', start);
        if (end < 0)
            end = path.size();
        QByteArray dir = path.mid(start, end - start);
        if (dir.isEmpty())
            dir = QByteArray(".");
        const QByteArray candidate = dir + '/' + program;
        struct stat st;
        if (::stat(candidate.constData(), &st) == 0 && S_ISREG(st.st_mode)) {
            if (::access(candidate.constData(), X_OK) == 0)
                return candidate;
            if (fallback.isEmpty())
                fallback = candidate;
        }
        start = end + 1;
    }
    return fallback.isEmpty() ? program : fallback;
}

struct Launch {
    QByteArray path;
    std::vector<QByteArray> argStore;
    std::vector<char *> argv;
    std::vector<QByteArray> envStore;
    std::vector<char *> envp;
    bool customEnv = false;
    QByteArray cwd;
    bool hasCwd = false;
};

Launch prepareLaunch(const QString &program, const QStringList &arguments, const QString &workingDirectory,
                     const QProcessEnvironment &environment) {
    Launch l;
    const QByteArray programBytes = program.toUtf8();
    l.argStore.push_back(programBytes);
    for (const QString &a : arguments)
        l.argStore.push_back(a.toUtf8());
    for (QByteArray &a : l.argStore)
        l.argv.push_back(a.data());
    l.argv.push_back(nullptr);

    QByteArray pathVar;
    if (environment.inheritsFromParent()) {
        const char *p = ::getenv("PATH");
        pathVar = QByteArray(p ? p : "/usr/bin:/bin");
    } else {
        l.customEnv = true;
        for (const QString &entry : environment.toStringList())
            l.envStore.push_back(entry.toUtf8());
        for (QByteArray &e : l.envStore)
            l.envp.push_back(e.data());
        l.envp.push_back(nullptr);
        pathVar = environment.contains(QString::fromUtf8("PATH"))
                      ? environment.value(QString::fromUtf8("PATH")).toUtf8()
                      : QByteArray("/usr/bin:/bin");
    }
    l.path = resolveProgram(programBytes, pathVar);
    if (!workingDirectory.isEmpty()) {
        l.cwd = workingDirectory.toUtf8();
        l.hasCwd = true;
    }
    return l;
}

struct ChildSpec {
    const char *path;
    char *const *argv;
    char *const *envp;
    const char *cwd;
    int in;
    int out;
    int err;
    int reportFd;
    const std::function<void()> *modifier;
    QProcess::UnixProcessParameters params;
};

// What a child that could not start writes to the report pipe, in one write that is smaller than PIPE_BUF.
struct ChildReport {
    int error = 0;
    int fromModifier = 0; // set by QProcess::failChildProcessModifier()
    char description[248] = {};
};

// The report pipe of the child that is being set up; only the forked child sets it.
int g_childReportFd = -1;

[[noreturn]] void reportChildFailure(int reportFd, int error, const char *description) {
    ChildReport report;
    report.error = error;
    if (description) {
        report.fromModifier = 1;
        std::size_t i = 0;
        for (; description[i] && i + 1 < sizeof report.description; ++i)
            report.description[i] = description[i];
        report.description[i] = '\0';
    }
    [[maybe_unused]] const ssize_t r = ::write(reportFd, &report, sizeof report);
    ::_exit(127);
}

// Closes the descriptors from `lowest` on, except `keep`.
void closeDescriptorsFrom(int lowest, int keep) {
    struct rlimit limit;
    int max = 65536;
    if (::getrlimit(RLIMIT_NOFILE, &limit) == 0 && limit.rlim_cur != RLIM_INFINITY)
        max = static_cast<int>(std::min<rlim_t>(limit.rlim_cur, 1 << 20));
#ifdef SYS_close_range
    // One call for each side of the descriptor that is kept; fall back to the loop on older kernels.
    if (keep < lowest || keep >= max) {
        if (::syscall(SYS_close_range, lowest, ~0U, 0) == 0)
            return;
    } else if ((keep == lowest || ::syscall(SYS_close_range, lowest, keep - 1, 0) == 0) &&
               ::syscall(SYS_close_range, keep + 1, ~0U, 0) == 0) {
        return;
    }
#endif
    for (int fd = lowest; fd < max; ++fd) {
        if (fd != keep)
            ::close(fd);
    }
}

// The error text for a child that could not start.
QString childFailureText(const ChildReport &report) {
    if (report.fromModifier) {
        const QString what = QString::fromUtf8(report.description);
        return report.error ? QString::fromUtf8("Child process modifier reported error: %1: %2")
                                  .arg(what, errnoString(report.error))
                            : QString::fromUtf8("Child process modifier reported error: %1").arg(what);
    }
    return QString::fromUtf8("Process failed to start: %1").arg(errnoString(report.error));
}

// Runs in the forked child; only async-signal-safe calls are made besides the user's modifier.
[[noreturn]] void childMain(const ChildSpec &c) {
    g_childReportFd = c.reportFd;
    auto fail = [&c]() { reportChildFailure(c.reportFd, errno, nullptr); };
    using Flag = QProcess::UnixProcessFlag;
    const QProcess::UnixProcessFlags flags = c.params.flags;
    for (int sig = 1; sig < 65; ++sig) {
        struct sigaction sa;
        if (::sigaction(sig, nullptr, &sa) != 0)
            continue;
        if (sa.sa_handler == SIG_IGN || sa.sa_handler == SIG_DFL)
            continue;
        std::memset(&sa, 0, sizeof sa);
        sa.sa_handler = SIG_DFL;
        ::sigaction(sig, &sa, nullptr);
    }
    ::signal(SIGPIPE, flags.testFlag(Flag::IgnoreSigPipe) ? SIG_IGN : SIG_DFL);
    if (c.in >= 0 && ::dup2(c.in, 0) < 0)
        fail();
    if (c.out >= 0 && ::dup2(c.out, 1) < 0)
        fail();
    if (c.err == kMergeIntoOut) {
        if (::dup2(1, 2) < 0)
            fail();
    } else if (c.err >= 0 && ::dup2(c.err, 2) < 0) {
        fail();
    }
    if (c.cwd && ::chdir(c.cwd) != 0)
        fail();
    if (flags.testFlag(Flag::CloseFileDescriptors))
        closeDescriptorsFrom(std::max(3, c.params.lowestFileDescriptorToClose), c.reportFd);
    if (flags.testFlag(Flag::CreateNewSession) && ::setsid() < 0)
        fail();
    if (flags.testFlag(Flag::DisconnectControllingTerminal)) {
        const int tty = ::open("/dev/tty", O_RDONLY | O_NOCTTY | O_CLOEXEC);
        if (tty >= 0) {
            ::ioctl(tty, TIOCNOTTY);
            ::close(tty);
        }
    }
    if (flags.testFlag(Flag::ResetIds)) {
        if (::setgid(::getgid()) != 0 || ::setuid(::getuid()) != 0)
            fail();
    }
    if (flags.testFlag(Flag::DisableCoreDumps)) {
        struct rlimit none = {0, 0};
        if (::setrlimit(RLIMIT_CORE, &none) != 0)
            fail();
    }
    sigset_t none;
    sigemptyset(&none);
    ::sigprocmask(SIG_SETMASK, &none, nullptr);
    if (c.modifier && *c.modifier)
        (*c.modifier)();
    ::execve(c.path, c.argv, c.envp ? c.envp : environ);
    fail();
    ::_exit(127);
}

struct SignalBlock {
    sigset_t previous;
    SignalBlock() {
        sigset_t all;
        sigfillset(&all);
        pthread_sigmask(SIG_SETMASK, &all, &previous);
    }
    ~SignalBlock() { pthread_sigmask(SIG_SETMASK, &previous, nullptr); }
    SignalBlock(const SignalBlock &) = delete;
    SignalBlock &operator=(const SignalBlock &) = delete;
};

struct StdioFds {
    int in = -1;
    int out = -1;
    int err = -1;
    int parentIn = -1;
    int parentOut = -1;
    int parentErr = -1;
    std::vector<int> childSide;

    void closeAll() {
        for (int fd : childSide)
            ::close(fd);
        childSide.clear();
        closeFd(parentIn);
        closeFd(parentOut);
        closeFd(parentErr);
    }
};

} // namespace

namespace QtPrivate {

void setProcessPidfdEnabled(bool enabled) { g_pidfdEnabled.store(enabled); }

} // namespace QtPrivate

/*!
    \class QProcessEnvironment
    \brief The QProcessEnvironment class holds the environment variables that can be passed to a program.

    A process environment is a set of \c{NAME=value} pairs. It is a value
    class: copies are independent. Use it with QProcess::setProcessEnvironment()
    to control what a child process sees.

    \code
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LANG", "C");
    env.remove("DISPLAY");
    process.setProcessEnvironment(env);
    \endcode

    A default-constructed environment is empty; a child started with it has no
    variables at all. An environment constructed with \c InheritFromParent is
    a placeholder that makes the child inherit the parent's environment, which
    is what a QProcess does until an environment is set. Modifying such a
    placeholder turns it into an ordinary (initially empty) environment.

    \differences
    Names and values are stored as QString in a sorted map, so keys() and
    toStringList() are sorted by name. Names and values are converted to UTF-8
    when the child is started. There is no case-insensitive Windows mode.
    \enddifferences

    \sa QProcess
*/

/*!
    Constructs an empty environment. A child process started with it has no
    environment variables.
*/
QProcessEnvironment::QProcessEnvironment() = default;

/*!
    Constructs a placeholder environment that makes the child process inherit
    the environment of the parent process.

    \sa inheritsFromParent()
*/
QProcessEnvironment::QProcessEnvironment(Initialization) : m_inherit(true) {}

/*!
    Returns \c true if this environment has no variables, which includes an
    inheriting placeholder; otherwise returns \c false.

    \sa clear(), inheritsFromParent()
*/
bool QProcessEnvironment::isEmpty() const { return m_vars.isEmpty(); }

/*!
    Returns \c true if this object is an unmodified placeholder created with
    \c InheritFromParent; otherwise returns \c false.

    \sa QProcessEnvironment(Initialization)
*/
bool QProcessEnvironment::inheritsFromParent() const { return m_inherit; }

/*!
    Removes all variables. The environment no longer inherits from the parent.

    \sa remove(), isEmpty()
*/
void QProcessEnvironment::clear() {
    m_vars.clear();
    m_inherit = false;
}

/*!
    Returns \c true if the environment contains a variable called \a name;
    otherwise returns \c false.

    \sa insert(), value()
*/
bool QProcessEnvironment::contains(const QString &name) const { return m_vars.contains(name); }

/*!
    Inserts the variable \a name with the value \a value, replacing an
    existing variable of that name.

    \sa remove(), value()
*/
void QProcessEnvironment::insert(const QString &name, const QString &value) {
    m_inherit = false;
    m_vars.insert(name, value);
}

/*!
    Removes the variable \a name if it exists.

    \sa insert(), contains()
*/
void QProcessEnvironment::remove(const QString &name) {
    m_inherit = false;
    m_vars.remove(name);
}

/*!
    Returns the value of the variable \a name, or \a defaultValue if it does
    not exist.

    \sa contains(), insert()
*/
QString QProcessEnvironment::value(const QString &name, const QString &defaultValue) const {
    const auto it = m_vars.constFind(name);
    return it == m_vars.constEnd() ? defaultValue : it.value();
}

/*!
    Returns the variables as a list of \c{NAME=value} strings, sorted by
    name.

    \sa keys()
*/
QStringList QProcessEnvironment::toStringList() const {
    QStringList list;
    for (auto it = m_vars.constBegin(); it != m_vars.constEnd(); ++it)
        list.append(it.key() + QChar(u'=') + it.value());
    return list;
}

/*!
    Returns the names of all variables, sorted.

    \sa toStringList()
*/
QStringList QProcessEnvironment::keys() const {
    QStringList list;
    for (auto it = m_vars.constBegin(); it != m_vars.constEnd(); ++it)
        list.append(it.key());
    return list;
}

/*!
    \overload

    Inserts all variables of \a e into this environment. Variables that exist
    in both take the value from \a e. Does nothing if \a e is an inheriting
    placeholder.
*/
void QProcessEnvironment::insert(const QProcessEnvironment &e) {
    if (e.m_inherit)
        return;
    m_inherit = false;
    for (auto it = e.m_vars.constBegin(); it != e.m_vars.constEnd(); ++it)
        m_vars.insert(it.key(), it.value());
}

/*!
    Returns a copy of the environment of the calling process.

    \sa QProcess::systemEnvironment()
*/
QProcessEnvironment QProcessEnvironment::systemEnvironment() {
    QProcessEnvironment env;
    for (char **e = environ; e && *e; ++e) {
        const char *eq = std::strchr(*e, '=');
        if (!eq || eq == *e)
            continue;
        env.m_vars.insert(QString::fromUtf8(*e, static_cast<qsizetype>(eq - *e)), QString::fromUtf8(eq + 1));
    }
    return env;
}

struct QProcess::Private {
    enum class Wait { Start, ReadyRead, BytesWritten, Finished };

    explicit Private(QProcess *proc) : q(proc) {}

    QProcess *q;

    QString program;
    QStringList arguments;
    QString workingDirectory;
    QProcessEnvironment environment{QProcessEnvironment::InheritFromParent};
    ProcessChannelMode channelMode = SeparateChannels;
    InputChannelMode inputMode = ManagedInputChannel;
    // The index of the current read channel: 0 for standard output, 1 for standard error.
    int current() const { return q->currentReadChannel() == 0 ? 0 : 1; }
    QString stdinFile;
    QString stdoutFile;
    QString stderrFile;
    bool stdoutAppend = false;
    bool stderrAppend = false;
    QPointer<QProcess> stdoutTarget;
    QPointer<QProcess> stdinSource;
    int chain[2] = {-1, -1};
    std::function<void()> childModifier;
    QProcess::UnixProcessParameters unixParams;

    ProcessState state = NotRunning;
    ProcessError lastError = UnknownError;
    int exitCode = 0;
    ExitStatus exitStatus = NormalExit;
    pid_t pid = -1;
    int pidfd = -1;
    int startFd = -1;
    int inFd = -1;
    int outFd = -1;
    int errFd = -1;
    QSocketNotifier *startNotifier = nullptr;
    QSocketNotifier *pidNotifier = nullptr;
    QSocketNotifier *inNotifier = nullptr;
    QSocketNotifier *outNotifier = nullptr;
    QSocketNotifier *errNotifier = nullptr;
    int pollTimer = 0;
    int notifierDepth = 0;

    QByteArray buf[2];
    qsizetype bufPos[2] = {0, 0};
    bool eofSeen[2] = {false, false};
    bool finishedEmitted[2] = {false, false};
    QByteArray wbuf;
    qsizetype wpos = 0;
    bool closeWritePending = false;
    bool startedOk = false;
    bool diedOk = false;
    bool dying = false;
    bool inReadyRead = false;
    unsigned dataSeq[2] = {0, 0};

    ~Private() {
        closeFd(chain[0]);
        closeFd(chain[1]);
    }
    Private(const Private &) = delete;
    Private &operator=(const Private &) = delete;

    int &channelFd(int ch) { return ch == 0 ? outFd : errFd; }
    QSocketNotifier *&channelNotifier(int ch) { return ch == 0 ? outNotifier : errNotifier; }
    qsizetype pending(int ch) const { return buf[ch].size() - bufPos[ch]; }

    void setError(ProcessError e, const QString &text) {
        lastError = e;
        q->setErrorString(text);
    }

    void dropNotifier(QSocketNotifier *&n) {
        if (!n)
            return;
        n->setEnabled(false);
        if (notifierDepth > 0)
            n->deleteLater();
        else
            delete n;
        n = nullptr;
    }

    void closeStdin() {
        dropNotifier(inNotifier);
        closeFd(inFd);
        closeWritePending = false;
        q->setOpenMode(q->openMode() & ~OpenMode(WriteOnly));
    }

    void closeChannelFd(int ch) {
        dropNotifier(channelNotifier(ch));
        closeFd(channelFd(ch));
    }

    void closeAll() {
        dropNotifier(startNotifier);
        dropNotifier(pidNotifier);
        dropNotifier(inNotifier);
        dropNotifier(outNotifier);
        dropNotifier(errNotifier);
        closeFd(startFd);
        closeFd(pidfd);
        closeFd(inFd);
        closeFd(outFd);
        closeFd(errFd);
        if (pollTimer) {
            q->killTimer(pollTimer);
            pollTimer = 0;
        }
    }

    template <typename F> void inNotification(F f) {
        QPointer<QProcess> guard(q);
        ++notifierDepth;
        f();
        if (guard)
            --notifierDepth;
    }

    int takeChainRead() {
        if (chain[0] < 0) {
            closeFd(chain[1]);
            if (!makePipe(chain))
                return -1;
        }
        const int fd = chain[0];
        chain[0] = -1;
        return fd;
    }

    int takeChainWrite() {
        if (chain[1] < 0) {
            closeFd(chain[0]);
            if (!makePipe(chain))
                return -1;
        }
        const int fd = chain[1];
        chain[1] = -1;
        return fd;
    }

    bool setupChannels(StdioFds &ch, OpenMode mode, bool detached, QString *err);
    bool launch(OpenMode mode, QString *err);
    bool launchDetached(qint64 *pidOut);
    void start(OpenMode mode);
    void failStart(const QString &text);
    bool onStartup();
    void onPidReady();
    bool reapIfExited();
    void processDied(int status);
    bool readChannelOnce(int ch);
    void drainChannel(int ch);
    void emitReadyRead(int ch);
    void onEof(int ch);
    bool canWrite();
    bool waitFor(Wait what, int msecs);
    qint64 readFrom(int ch, char *data, qint64 maxSize);
};

/*!
    \class QProcess
    \brief The QProcess class is used to start external programs and to communicate with them.

    QProcess runs a program in a child process and exchanges data with it
    through pipes. Start a program with start(); the process runs
    asynchronously and QProcess reports progress through signals, driven by
    the event loop. Output arrives on readyReadStandardOutput() and
    readyReadStandardError(); write input with write() and call
    closeWriteChannel() when done. The process is complete once finished() has
    been emitted.

    \code
    QProcess process;
    QObject::connect(&process, &QProcess::finished, [&](int code, QProcess::ExitStatus status) {
        qDebug() << "exit code" << code << process.readAllStandardOutput();
    });
    process.start("sort", QStringList());
    process.write("b\na\n");
    process.closeWriteChannel();
    \endcode

    The waitFor...() functions block instead, without running the event loop;
    they still emit signals and move data while they wait.

    The program is started with \c fork() and \c execve(). A program name
    without a slash is searched in \c PATH (the child's \c PATH when an
    environment was set). A failure to execute the program, or to change to
    the working directory, is reported through a close-on-exec pipe: state()
    goes back to \c NotRunning and errorOccurred() is emitted with
    \c FailedToStart. The end of the child is detected through a pidfd where
    the kernel provides one, and by polling \c waitpid() otherwise; the
    child is always reaped and QProcess installs no \c SIGCHLD handler, so it
    does not interfere with the application's own. The application must not
    reap children it did not start itself with \c waitpid(-1).

    Destroying a QProcess that is still running kills the child, after a
    warning, and waits for it.

    \differences
    The platform is POSIX only. The write buffer is unbounded and there is no
    setReadBufferSize(). A crashed child reports an exitCode() of 0 and
    exitStatus() \c CrashExit. There is no \c setCreateProcessArgumentsModifier(), and
    \c UnixProcessFlag::UseVFork is ignored.
    The poller of an unsupported kernel adds up to 10 ms of delay in
    detecting the end of a child. The finished() signal has no overload
    taking only the exit code.

    The class is not built for Zephyr, where \c QT_NO_PROCESS is defined instead. See \l{Nt on Zephyr}.
    \enddifferences

    \sa QProcessEnvironment, QIODevice
*/

/*!
    Constructs a QProcess object with the given \a parent. The process is not
    running.
*/
QProcess::QProcess(QObject *parent) : QIODevice(parent), d(std::make_unique<Private>(this)) {
    m_readChannels = 2; // standard output and standard error
}

/*!
    Destroys the QProcess object. If the process is still running, a warning
    is printed, the process is killed and the destructor waits for it to
    finish.
*/
QProcess::~QProcess() {
    if (d->state != NotRunning) {
        qWarning("QProcess: Destroyed while process (\"%s\") is still running.", d->program.toUtf8().constData());
        blockSignals(true);
        kill();
        waitForFinished(-1);
    }
    d->closeAll();
}

/*!
    Starts the given \a program in a new process with \a arguments, if no
    process is running, and opens the device in \a mode. The function returns
    immediately; the process is started when the event loop runs, or when a
    waitFor...() function is called. A failure to start is reported through
    errorOccurred().

    The program name is looked up in \c PATH if it has no slash. Arguments
    are passed as given, without shell interpretation.

    \sa startCommand(), setProgram(), waitForStarted()
*/
void QProcess::start(const QString &program, const QStringList &arguments, OpenMode mode) {
    if (d->state != NotRunning) {
        qWarning("QProcess::start: Process is already running");
        return;
    }
    d->program = program;
    d->arguments = arguments;
    d->start(mode);
}

/*!
    \overload

    Starts the program set by setProgram() with the arguments set by
    setArguments(), opening the device in \a mode.

    \sa open()
*/
void QProcess::start(OpenMode mode) { d->start(mode); }

/*!
    Starts the command \a command in a new process and opens the device in
    \a mode. The command is split with splitCommand(): the first token is
    the program and the others are its arguments. Nothing is interpreted by
    a shell.

    \sa splitCommand(), start()
*/
void QProcess::startCommand(const QString &command, OpenMode mode) {
    QStringList args = splitCommand(command);
    if (d->state != NotRunning) {
        qWarning("QProcess::startCommand: Process is already running");
        return;
    }
    if (args.isEmpty()) {
        d->setError(FailedToStart, QString::fromUtf8("No program defined"));
        errorOccurred(FailedToStart);
        return;
    }
    d->program = args.takeFirst();
    d->arguments = args;
    d->start(mode);
}

/*!
    Opens the device in \a mode and starts the program set with
    setProgram(), like start(). Returns \c true if a start was attempted;
    returns \c false and does nothing if the process is already running or
    no program was set.

    \sa start()
*/
bool QProcess::open(OpenMode mode) {
    if (d->state != NotRunning) {
        qWarning("QProcess::open: Process is already running");
        return false;
    }
    if (d->program.isEmpty()) {
        qWarning("QProcess::open: program not set");
        return false;
    }
    d->start(mode);
    return true;
}

/*!
    Splits \a command into tokens separated by white space and returns them.
    A token that contains white space can be surrounded by double quotes;
    three consecutive double quotes stand for a quote character itself.

    \sa startCommand()
*/
QStringList QProcess::splitCommand(QStringView command) {
    QStringList args;
    QString tmp;
    int quoteCount = 0;
    bool inQuote = false;
    for (qsizetype i = 0; i < command.size(); ++i) {
        const QChar c = command.at(i);
        if (c == QChar(u'"')) {
            ++quoteCount;
            if (quoteCount == 3) {
                quoteCount = 0;
                tmp += c;
            }
            continue;
        }
        if (quoteCount) {
            if (quoteCount == 1)
                inQuote = !inQuote;
            quoteCount = 0;
        }
        if (!inQuote && c.isSpace()) {
            if (!tmp.isEmpty()) {
                args.append(tmp);
                tmp.clear();
            }
        } else {
            tmp += c;
        }
    }
    if (!tmp.isEmpty())
        args.append(tmp);
    return args;
}

/*!
    Returns the environment of the calling process as a list of
    \c{NAME=value} strings.

    \sa QProcessEnvironment::systemEnvironment()
*/
QStringList QProcess::systemEnvironment() { return QProcessEnvironment::systemEnvironment().toStringList(); }

/*!
    Returns the path of the system's null device, \c{/dev/null}.

    \sa setStandardOutputFile()
*/
QString QProcess::nullDevice() { return QString::fromUtf8("/dev/null"); }

/*!
    Returns the program set with setProgram() or start().

    \sa setProgram()
*/
QString QProcess::program() const { return d->program; }

/*!
    Sets the \a program to run. It is used by the next start() without a
    program argument. This has no effect on a running process.

    \sa program(), setArguments()
*/
void QProcess::setProgram(const QString &program) {
    if (d->state != NotRunning) {
        qWarning("QProcess::setProgram: Process is already running");
        return;
    }
    d->program = program;
}

/*!
    Returns the arguments set with setArguments() or start().

    \sa setArguments()
*/
QStringList QProcess::arguments() const { return d->arguments; }

/*!
    Sets the \a arguments passed to the program at the next start. This has
    no effect on a running process.

    \sa arguments(), setProgram()
*/
void QProcess::setArguments(const QStringList &arguments) {
    if (d->state != NotRunning) {
        qWarning("QProcess::setArguments: Process is already running");
        return;
    }
    d->arguments = arguments;
}

/*!
    Returns the working directory the process will start in. An empty string
    means the child inherits the working directory of the parent.

    \sa setWorkingDirectory()
*/
QString QProcess::workingDirectory() const { return d->workingDirectory; }

/*!
    Sets the working directory of the next process to \a dir. If the
    directory cannot be entered, the process fails to start.

    \sa workingDirectory()
*/
void QProcess::setWorkingDirectory(const QString &dir) {
    if (d->state != NotRunning) {
        qWarning("QProcess::setWorkingDirectory: Process is already running");
        return;
    }
    d->workingDirectory = dir;
}

/*!
    Returns the environment set with setProcessEnvironment(). If none was
    set, the result inherits from the parent (see
    QProcessEnvironment::inheritsFromParent()).

    \sa setProcessEnvironment(), systemEnvironment()
*/
QProcessEnvironment QProcess::processEnvironment() const { return d->environment; }

/*!
    Sets the environment passed to the next process to \a environment. If
    \a environment inherits from the parent, which is the default, the child
    gets the parent's environment.

    \sa processEnvironment(), QProcessEnvironment::systemEnvironment()
*/
void QProcess::setProcessEnvironment(const QProcessEnvironment &environment) {
    if (d->state != NotRunning) {
        qWarning("QProcess::setProcessEnvironment: Process is already running");
        return;
    }
    d->environment = environment;
}

/*!
    Returns the channel mode of the standard output and standard error
    channels.

    \sa setProcessChannelMode()
*/
QProcess::ProcessChannelMode QProcess::processChannelMode() const { return d->channelMode; }

/*!
    Sets the channel mode of the standard output and standard error channels
    to \a mode. It takes effect at the next start.

    \sa processChannelMode(), setReadChannel()
*/
void QProcess::setProcessChannelMode(ProcessChannelMode mode) { d->channelMode = mode; }

/*!
    Returns the channel mode of the standard input channel.

    \sa setInputChannelMode()
*/
QProcess::InputChannelMode QProcess::inputChannelMode() const { return d->inputMode; }

/*!
    Sets the channel mode of the standard input channel to \a mode. It takes
    effect at the next start.

    \sa inputChannelMode()
*/
void QProcess::setInputChannelMode(InputChannelMode mode) { d->inputMode = mode; }

/*!
    Returns the current read channel, the one that read(), readAll() and
    readLine() read from and that triggers readyRead().

    \sa setReadChannel()
*/
QProcess::ProcessChannel QProcess::readChannel() const { return d->current() == 0 ? StandardOutput : StandardError; }

/*!
    Sets the current read channel to \a channel.

    \sa readChannel(), readAllStandardOutput()
*/
void QProcess::setReadChannel(ProcessChannel channel) { setCurrentReadChannel(channel == StandardOutput ? 0 : 1); }

/*!
    Closes the read channel \a channel. The process may receive SIGPIPE when
    it next writes to it. Data that has already been buffered stays
    readable.

    \sa closeWriteChannel()
*/
void QProcess::closeReadChannel(ProcessChannel channel) { d->closeChannelFd(channel == StandardOutput ? 0 : 1); }

/*!
    Schedules the write channel to be closed. It closes once all data written
    so far has reached the process, which then sees end-of-file on its
    standard input. The device is no longer writable.

    \sa write(), closeReadChannel()
*/
void QProcess::closeWriteChannel() {
    d->closeWritePending = true;
    setOpenMode(openMode() & ~OpenMode(WriteOnly));
    if (d->wpos >= d->wbuf.size())
        d->closeStdin();
}

/*!
    Redirects the standard input of the next process to read from the file
    \a fileName instead of a pipe.

    \sa setStandardOutputFile(), setInputChannelMode()
*/
void QProcess::setStandardInputFile(const QString &fileName) { d->stdinFile = fileName; }

/*!
    Redirects the standard output of the next process to the file
    \a fileName, created if it does not exist. With \a mode \c Append the
    file is appended to; otherwise it is truncated. Pass nullDevice() to
    discard output. An empty \a fileName cancels the redirection.

    If the process channel mode is \c MergedChannels, standard error goes to
    the file as well.

    \sa setStandardErrorFile(), setStandardInputFile()
*/
void QProcess::setStandardOutputFile(const QString &fileName, OpenMode mode) {
    d->stdoutFile = fileName;
    d->stdoutAppend = mode.testFlag(Append);
}

/*!
    Redirects the standard error of the next process to the file
    \a fileName, created if it does not exist. With \a mode \c Append the
    file is appended to; otherwise it is truncated. It has no effect with
    the \c MergedChannels mode.

    \sa setStandardOutputFile()
*/
void QProcess::setStandardErrorFile(const QString &fileName, OpenMode mode) {
    d->stderrFile = fileName;
    d->stderrAppend = mode.testFlag(Append);
}

/*!
    Pipes the standard output of this process directly into the standard
    input of \a destination, like the shell's \c |. Either process can be
    started first; the pipe exists from the first start. Pass \c nullptr to
    undo it. The standard output of this process is then no longer readable.

    \sa setStandardOutputFile()
*/
void QProcess::setStandardOutputProcess(QProcess *destination) {
    if (d->stdoutTarget)
        d->stdoutTarget->d->stdinSource = nullptr;
    d->stdoutTarget = destination;
    if (destination)
        destination->d->stdinSource = this;
}

/*!
    Sets \a modifier to be called in the child process after it has been
    forked and its channels set up, just before the program is executed. It
    runs in the child of a possibly multi-threaded process, so it may only
    call async-signal-safe functions.

    \sa start()
*/
void QProcess::setChildProcessModifier(const std::function<void()> &modifier) { d->childModifier = modifier; }

/*!
    \since 6.0
    Returns the modifier set with setChildProcessModifier(), or an empty function if there is none.
*/
std::function<void()> QProcess::childProcessModifier() const { return d->childModifier; }

/*!
    \since 6.7
    Reports from a child process modifier that the child cannot go on, and ends the child. The QProcess fails to
    start with \c FailedToStart, and errorString() holds \a description and, if \a error is not 0, the text of
    that \c errno value. Only call it from the modifier; it is async-signal-safe.

    \code
    process.setChildProcessModifier([] {
        if (::chroot("/srv/jail") != 0)
            QProcess::failChildProcessModifier("chroot", errno);
    });
    \endcode

    \sa setChildProcessModifier()
*/
void QProcess::failChildProcessModifier(const char *description, int error) noexcept {
    if (g_childReportFd < 0) {
        std::fputs("QProcess::failChildProcessModifier() called outside a child process modifier\n", stderr);
        std::abort();
    }
    reportChildFailure(g_childReportFd, error, description ? description : "");
}

/*!
    \since 6.6
    Returns what the child process does to itself before it runs the program.

    \sa setUnixProcessParameters()
*/
QProcess::UnixProcessParameters QProcess::unixProcessParameters() const noexcept { return d->unixParams; }

/*!
    \since 6.6
    Sets what the child process does to itself before it runs the program, and before the child process
    modifier, to \a params.

    \code
    QProcess process;
    process.setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors |
                                     QProcess::UnixProcessFlag::ResetIds);
    \endcode

    \sa unixProcessParameters(), setChildProcessModifier()
*/
void QProcess::setUnixProcessParameters(const UnixProcessParameters &params) { d->unixParams = params; }

/*!
    \since 6.6
    \overload
    Sets the flags of the parameters to \a flagsOnly and the other members to their defaults.
*/
void QProcess::setUnixProcessParameters(UnixProcessFlags flagsOnly) {
    d->unixParams = UnixProcessParameters();
    d->unixParams.flags = flagsOnly;
}

/*!
    Returns the environment of the child as a list of \c{NAME=VALUE} strings, or an empty list if none was set
    and the child inherits the environment of this process.

    \sa setEnvironment(), processEnvironment()
*/
QStringList QProcess::environment() const { return d->environment.toStringList(); }

/*!
    Sets the environment of the child to \a environment, a list of \c{NAME=VALUE} strings.

    \sa environment(), setProcessEnvironment()
*/
void QProcess::setEnvironment(const QStringList &environment) {
    QProcessEnvironment env;
    for (const QString &entry : environment) {
        const qsizetype eq = entry.indexOf(QLatin1Char('='), 1);
        if (eq > 0)
            env.insert(entry.left(eq), entry.mid(eq + 1));
    }
    setProcessEnvironment(env);
}

/*!
    Returns the type of error that occurred last, or \c UnknownError if none
    has occurred since the last start.

    \sa errorString(), errorOccurred()
*/
QProcess::ProcessError QProcess::error() const { return d->lastError; }

/*!
    Returns the current state of the process.

    \sa stateChanged(), error()
*/
QProcess::ProcessState QProcess::state() const { return d->state; }

/*!
    Returns the native process identifier of the running process, or 0 if
    no process is running.
*/
qint64 QProcess::processId() const { return d->state == NotRunning || d->pid <= 0 ? 0 : static_cast<qint64>(d->pid); }

/*!
    Returns the exit code of the last process that finished. It is only
    meaningful if exitStatus() is \c NormalExit.

    \sa exitStatus(), finished()
*/
int QProcess::exitCode() const { return d->exitCode; }

/*!
    Returns the exit status of the last process that finished: \c CrashExit if
    it was terminated by a signal.

    \sa exitCode(), finished()
*/
QProcess::ExitStatus QProcess::exitStatus() const { return d->exitStatus; }

/*!
    Returns the number of bytes available for reading on the current read
    channel.

    \sa setReadChannel(), readAllStandardOutput()
*/
qint64 QProcess::bytesAvailable() const { return QIODevice::bytesAvailable() + d->pending(d->current()); }

/*!
    Returns the number of bytes written to the process that have not been
    passed to the operating system yet.

    \sa write(), waitForBytesWritten()
*/
qint64 QProcess::bytesToWrite() const { return d->wbuf.size() - d->wpos; }

/*!
    Returns \c true if a complete line can be read from the current read
    channel; otherwise returns \c false.

    \sa readLine()
*/
bool QProcess::canReadLine() const {
    const int ch = d->current();
    const QByteArray &b = d->buf[ch];
    return QIODevice::canReadLine() || (b.size() > d->bufPos[ch] && b.indexOf('\n', d->bufPos[ch]) >= 0);
}

/*!
    Reads and returns all data buffered from the standard output channel,
    whatever the current read channel is.

    \sa readAllStandardError(), readyReadStandardOutput()
*/
QByteArray QProcess::readAllStandardOutput() {
    QByteArray r = d->buf[0].mid(d->bufPos[0]);
    d->buf[0].clear();
    d->bufPos[0] = 0;
    return r;
}

/*!
    Reads and returns all data buffered from the standard error channel,
    whatever the current read channel is.

    \sa readAllStandardOutput(), readyReadStandardError()
*/
QByteArray QProcess::readAllStandardError() {
    QByteArray r = d->buf[1].mid(d->bufPos[1]);
    d->buf[1].clear();
    d->bufPos[1] = 0;
    return r;
}

qint64 QProcess::Private::readFrom(int ch, char *data, qint64 maxSize) {
    const qint64 n = std::min<qint64>(pending(ch), maxSize);
    if (n <= 0)
        return 0;
    std::memcpy(data, buf[ch].constData() + bufPos[ch], static_cast<std::size_t>(n));
    bufPos[ch] += n;
    if (bufPos[ch] >= buf[ch].size()) {
        buf[ch].clear();
        bufPos[ch] = 0;
    } else if (bufPos[ch] > (1 << 20) && bufPos[ch] * 2 > buf[ch].size()) {
        buf[ch].remove(0, bufPos[ch]);
        bufPos[ch] = 0;
    }
    return n;
}

/*!
    \reimp
*/
qint64 QProcess::readData(char *data, qint64 maxSize) { return d->readFrom(d->current(), data, maxSize); }

/*!
    \reimp
*/
qint64 QProcess::writeData(const char *data, qint64 maxSize) {
    if (d->inFd < 0 || d->closeWritePending) {
        setErrorString(QString::fromUtf8("Write channel closed"));
        return -1;
    }
    d->wbuf.append(data, maxSize);
    if (d->inNotifier)
        d->inNotifier->setEnabled(true);
    return maxSize;
}

/*!
    Terminates the process, giving it the chance to clean up: it sends
    \c SIGTERM. Does nothing if no process is running.

    \sa kill()
*/
void QProcess::terminate() {
    if (d->state != NotRunning && d->pid > 0)
        ::kill(d->pid, SIGTERM);
}

/*!
    Kills the process, causing it to exit immediately: it sends \c SIGKILL.
    The process then finishes with the \c CrashExit status. Does nothing if
    no process is running.

    \sa terminate()
*/
void QProcess::kill() {
    if (d->state != NotRunning && d->pid > 0)
        ::kill(d->pid, SIGKILL);
}

/*!
    \reimp

    Closes the read and write channels, terminates the process if it is
    running (killing it if it does not finish within five seconds) and sets
    the open mode to \c NotOpen. Emits aboutToClose() first.
*/
void QProcess::close() {
    if (!isOpen() && d->state == NotRunning)
        return;
    QPointer<QProcess> guard(this);
    if (isOpen())
        aboutToClose();
    if (!guard)
        return;
    d->closeChannelFd(0);
    d->closeChannelFd(1);
    d->closeStdin();
    d->wbuf.clear();
    d->wpos = 0;
    if (d->state != NotRunning) {
        terminate();
        if (!waitForFinished(5000))
            kill();
        waitForFinished(-1);
        if (!guard)
            return;
    }
    setOpenMode(NotOpen);
}

/*!
    \reimp
*/
void QProcess::timerEvent(QTimerEvent *event) {
    if (event->timerId() == d->pollTimer && d->pollTimer != 0) {
        d->reapIfExited();
        return;
    }
    QIODevice::timerEvent(event);
}

/*!
    Blocks until the process has started and started() has been emitted, or
    until \a msecs milliseconds have passed; a negative value waits
    indefinitely. Returns \c true if the process started; returns \c false
    if it failed to start, was not started, or the wait timed out (the error
    is then \c Timedout).

    \sa waitForFinished(), started()
*/
bool QProcess::waitForStarted(int msecs) {
    if (d->state == Running)
        return true;
    if (d->state == NotRunning)
        return false;
    return d->waitFor(Private::Wait::Start, msecs);
}

/*!
    \reimp

    Blocks until new data is available on the current read channel and
    readyRead() has been emitted, or until \a msecs milliseconds have passed.
    Returns \c false if the process ended, failed or the wait timed out.
    While it waits, the data of both channels is read and input is written.

    \sa waitForBytesWritten()
*/
bool QProcess::waitForReadyRead(int msecs) {
    if (d->state == NotRunning)
        return false;
    if (d->state == Starting && !waitForStarted(msecs))
        return false;
    return d->waitFor(Private::Wait::ReadyRead, msecs);
}

/*!
    \reimp

    Blocks until data has been written to the process and bytesWritten() has
    been emitted, or until \a msecs milliseconds have passed. Returns
    \c false if nothing is waiting to be written, the process ended or the
    wait timed out. Output of the process is read in the meantime.

    \sa waitForReadyRead(), bytesToWrite()
*/
bool QProcess::waitForBytesWritten(int msecs) {
    if (d->state == NotRunning)
        return false;
    if (d->state == Starting && !waitForStarted(msecs))
        return false;
    if (d->wpos >= d->wbuf.size())
        return false;
    return d->waitFor(Private::Wait::BytesWritten, msecs);
}

/*!
    Blocks until the process has finished and finished() has been emitted,
    or until \a msecs milliseconds have passed; a negative value waits
    indefinitely. Returns \c true if the process finished; returns \c false
    if it already had, failed to start or the wait timed out. Data is read
    and written while waiting.

    \sa waitForStarted()
*/
bool QProcess::waitForFinished(int msecs) {
    if (d->state == NotRunning)
        return false;
    if (d->state == Starting && !waitForStarted(msecs))
        return false;
    return d->waitFor(Private::Wait::Finished, msecs);
}

/*!
    Starts the program in a new process that is detached from this one and
    continues to run after the application exits, using the program,
    arguments, working directory and environment set on this object. The
    process becomes a session leader, its standard input is \c{/dev/null}
    and its output goes to the null device, unless a redirection or a
    forwarded channel mode says otherwise. Returns \c true on success; if
    \a pid is not \c nullptr, it receives the process identifier. The
    QProcess object stays in the \c NotRunning state.

    \sa start()
*/
bool QProcess::startDetached(qint64 *pid) {
    if (d->state != NotRunning) {
        qWarning("QProcess::startDetached: Process is already running");
        return false;
    }
    if (d->program.isEmpty()) {
        d->setError(FailedToStart, QString::fromUtf8("No program defined"));
        errorOccurred(FailedToStart);
        return false;
    }
    d->lastError = UnknownError;
    setErrorString(QString());
    return d->launchDetached(pid);
}

/*!
    \overload

    Starts \a program with \a arguments in the working directory
    \a workingDirectory as a detached process. Returns \c true on success; if
    \a pid is not \c nullptr, it receives the process identifier.
*/
bool QProcess::startDetached(const QString &program, const QStringList &arguments, const QString &workingDirectory,
                             qint64 *pid) {
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setWorkingDirectory(workingDirectory);
    return process.startDetached(pid);
}

/*!
    Starts \a program with \a arguments in a new process, forwarding its
    output channels, and waits for it to finish. Returns the exit code of
    the process, -2 if the process could not be started and -1 if it
    crashed.

    \sa start(), waitForFinished()
*/
int QProcess::execute(const QString &program, const QStringList &arguments) {
    QProcess process;
    process.setProcessChannelMode(ForwardedChannels);
    process.start(program, arguments);
    if (!process.waitForFinished(-1) || process.error() == FailedToStart)
        return -2;
    return process.exitStatus() == NormalExit ? process.exitCode() : -1;
}

bool QProcess::Private::setupChannels(StdioFds &ch, OpenMode mode, bool detached, QString *err) {
    auto fileFail = [&](const char *what, const QString &name) {
        *err =
            QString::fromUtf8("Could not open %1 file '%2': %3").arg(QString::fromUtf8(what), name, errnoString(errno));
        return false;
    };
    auto track = [&ch](int fd) {
        ch.childSide.push_back(fd);
        return fd;
    };
    const QString devNull = nullDevice();

    if (!detached && stdinSource) {
        ch.in = stdinSource->d->takeChainRead();
        if (ch.in < 0)
            return fileFail("input", QString::fromUtf8("pipe"));
        track(ch.in);
    } else if (inputMode == ForwardedInputChannel) {
        ch.in = -1;
    } else if (!stdinFile.isEmpty()) {
        ch.in = openFile(stdinFile, O_RDONLY);
        if (ch.in < 0)
            return fileFail("input", stdinFile);
        track(ch.in);
    } else if (detached || !(mode & WriteOnly)) {
        ch.in = openFile(devNull, O_RDONLY);
        if (ch.in < 0)
            return fileFail("input", devNull);
        track(ch.in);
    } else {
        int p[2];
        if (!makePipe(p))
            return fileFail("input", QString::fromUtf8("pipe"));
        ch.in = track(p[0]);
        ch.parentIn = p[1];
    }

    const bool forwardOut = channelMode == ForwardedChannels || channelMode == ForwardedOutputChannel;
    const bool forwardErr = channelMode == ForwardedChannels || channelMode == ForwardedErrorChannel;
    if (!detached && stdoutTarget) {
        ch.out = takeChainWrite();
        if (ch.out < 0)
            return fileFail("output", QString::fromUtf8("pipe"));
        track(ch.out);
    } else if (!stdoutFile.isEmpty()) {
        ch.out = openFile(stdoutFile, O_WRONLY | O_CREAT | (stdoutAppend ? O_APPEND : O_TRUNC));
        if (ch.out < 0)
            return fileFail("output", stdoutFile);
        track(ch.out);
    } else if (forwardOut) {
        ch.out = -1;
    } else if (detached || !(mode & ReadOnly)) {
        ch.out = openFile(devNull, O_WRONLY);
        if (ch.out < 0)
            return fileFail("output", devNull);
        track(ch.out);
    } else {
        int p[2];
        if (!makePipe(p))
            return fileFail("output", QString::fromUtf8("pipe"));
        ch.out = track(p[1]);
        ch.parentOut = p[0];
    }

    if (channelMode == MergedChannels) {
        ch.err = kMergeIntoOut;
    } else if (!stderrFile.isEmpty()) {
        ch.err = openFile(stderrFile, O_WRONLY | O_CREAT | (stderrAppend ? O_APPEND : O_TRUNC));
        if (ch.err < 0)
            return fileFail("error", stderrFile);
        track(ch.err);
    } else if (forwardErr) {
        ch.err = -1;
    } else if (detached || !(mode & ReadOnly)) {
        ch.err = openFile(devNull, O_WRONLY);
        if (ch.err < 0)
            return fileFail("error", devNull);
        track(ch.err);
    } else {
        int p[2];
        if (!makePipe(p))
            return fileFail("error", QString::fromUtf8("pipe"));
        ch.err = track(p[1]);
        ch.parentErr = p[0];
    }
    return true;
}

bool QProcess::Private::launch(OpenMode mode, QString *err) {
    StdioFds ch;
    if (!setupChannels(ch, mode, false, err)) {
        ch.closeAll();
        return false;
    }
    const Launch l = prepareLaunch(program, arguments, workingDirectory, environment);
    int report[2];
    if (!makePipe(report)) {
        *err = QString::fromUtf8("Resource error (pipe creation failure): %1").arg(errnoString(errno));
        ch.closeAll();
        return false;
    }
    const std::function<void()> modifier = childModifier;
    const ChildSpec spec{l.path.constData(),
                         l.argv.data(),
                         l.customEnv ? l.envp.data() : nullptr,
                         l.hasCwd ? l.cwd.constData() : nullptr,
                         ch.in,
                         ch.out,
                         ch.err,
                         report[1],
                         &modifier,
                         unixParams};
    pid_t child;
    {
        SignalBlock block;
        child = ::fork();
        if (child == 0)
            childMain(spec);
    }
    if (child < 0) {
        const int e = errno;
        *err = QString::fromUtf8("Resource error (fork failure): %1").arg(errnoString(e));
        ::close(report[0]);
        ::close(report[1]);
        ch.closeAll();
        return false;
    }

    ::close(report[1]);
    for (int fd : ch.childSide)
        ::close(fd);
    ch.childSide.clear();
    pid = child;
    startFd = report[0];
    setNonBlocking(startFd);
    inFd = ch.parentIn;
    outFd = ch.parentOut;
    errFd = ch.parentErr;
    for (int fd : {inFd, outFd, errFd})
        if (fd >= 0)
            setNonBlocking(fd);
    pidfd = openPidfd(pid);

    startNotifier = new QSocketNotifier(startFd, QSocketNotifier::Read, q);
    QObject::connect(startNotifier, &QSocketNotifier::activated, q,
                     [this] { inNotification([this] { onStartup(); }); });
    if (pidfd >= 0) {
        pidNotifier = new QSocketNotifier(pidfd, QSocketNotifier::Read, q);
        QObject::connect(pidNotifier, &QSocketNotifier::activated, q,
                         [this] { inNotification([this] { onPidReady(); }); });
    } else {
        pollTimer = q->startTimer(kPollSliceMs);
    }
    if (inFd >= 0) {
        inNotifier = new QSocketNotifier(inFd, QSocketNotifier::Write, q);
        inNotifier->setEnabled(false);
        QObject::connect(inNotifier, &QSocketNotifier::activated, q,
                         [this] { inNotification([this] { canWrite(); }); });
    }
    if (outFd >= 0) {
        outNotifier = new QSocketNotifier(outFd, QSocketNotifier::Read, q);
        QObject::connect(outNotifier, &QSocketNotifier::activated, q,
                         [this] { inNotification([this] { readChannelOnce(0); }); });
    }
    if (errFd >= 0) {
        errNotifier = new QSocketNotifier(errFd, QSocketNotifier::Read, q);
        QObject::connect(errNotifier, &QSocketNotifier::activated, q,
                         [this] { inNotification([this] { readChannelOnce(1); }); });
    }
    return true;
}

bool QProcess::Private::launchDetached(qint64 *pidOut) {
    StdioFds ch;
    QString err;
    const OpenMode noPipes = ReadOnly;
    if (!setupChannels(ch, noPipes, true, &err)) {
        ch.closeAll();
        setError(FailedToStart, err);
        q->errorOccurred(FailedToStart);
        return false;
    }
    const Launch l = prepareLaunch(program, arguments, workingDirectory, environment);
    int report[2];
    int pidPipe[2];
    if (!makePipe(report)) {
        err = QString::fromUtf8("Resource error (pipe creation failure): %1").arg(errnoString(errno));
    } else if (!makePipe(pidPipe)) {
        err = QString::fromUtf8("Resource error (pipe creation failure): %1").arg(errnoString(errno));
        ::close(report[0]);
        ::close(report[1]);
    } else {
        const std::function<void()> modifier = childModifier;
        const ChildSpec spec{l.path.constData(),
                             l.argv.data(),
                             l.customEnv ? l.envp.data() : nullptr,
                             l.hasCwd ? l.cwd.constData() : nullptr,
                             ch.in,
                             ch.out,
                             ch.err,
                             report[1],
                             &modifier,
                             unixParams};
        pid_t first;
        {
            SignalBlock block;
            first = ::fork();
            if (first == 0) {
                ::setsid();
                const pid_t second = ::fork();
                if (second == 0)
                    childMain(spec);
                [[maybe_unused]] const ssize_t w = ::write(pidPipe[1], &second, sizeof second);
                ::_exit(second < 0 ? 1 : 0);
            }
        }
        const int forkError = errno;
        ::close(report[1]);
        ::close(pidPipe[1]);
        ch.closeAll();
        if (first < 0) {
            err = QString::fromUtf8("Resource error (fork failure): %1").arg(errnoString(forkError));
        } else {
            waitPid(first, nullptr, 0);
            pid_t grandchild = -1;
            ssize_t n;
            do {
                n = ::read(pidPipe[0], &grandchild, sizeof grandchild);
            } while (n < 0 && errno == EINTR);
            if (n != static_cast<ssize_t>(sizeof grandchild) || grandchild < 0) {
                err = QString::fromUtf8("Resource error (fork failure): %1").arg(errnoString(EAGAIN));
            } else {
                ChildReport childReport;
                do {
                    n = ::read(report[0], &childReport, sizeof childReport);
                } while (n < 0 && errno == EINTR);
                if (n == static_cast<ssize_t>(sizeof childReport)) {
                    err = childFailureText(childReport);
                } else {
                    if (pidOut)
                        *pidOut = grandchild;
                    ::close(report[0]);
                    ::close(pidPipe[0]);
                    return true;
                }
            }
        }
        ::close(report[0]);
        ::close(pidPipe[0]);
        setError(FailedToStart, err);
        q->errorOccurred(FailedToStart);
        return false;
    }
    ch.closeAll();
    setError(FailedToStart, err);
    q->errorOccurred(FailedToStart);
    return false;
}

void QProcess::Private::start(OpenMode mode) {
    if (state != NotRunning) {
        qWarning("QProcess::start: Process is already running");
        return;
    }
    if (program.isEmpty()) {
        setError(FailedToStart, QString::fromUtf8("No program defined"));
        q->errorOccurred(FailedToStart);
        return;
    }
    exitCode = 0;
    exitStatus = NormalExit;
    for (int i = 0; i < 2; ++i) {
        buf[i].clear();
        bufPos[i] = 0;
        eofSeen[i] = false;
        finishedEmitted[i] = false;
    }
    wbuf.clear();
    wpos = 0;
    closeWritePending = false;
    startedOk = false;
    diedOk = false;
    dying = false;
    q->QIODevice::open(mode);
    lastError = UnknownError;

    QPointer<QProcess> guard(q);
    state = Starting;
    q->stateChanged(Starting);
    if (!guard)
        return;
    QString err;
    if (!launch(mode, &err)) {
        failStart(err.isEmpty() ? QString::fromUtf8("Unknown error") : err);
        return;
    }
    if (inFd < 0)
        q->setOpenMode(q->openMode() & ~OpenMode(WriteOnly));
}

void QProcess::Private::failStart(const QString &text) {
    QPointer<QProcess> guard(q);
    closeAll();
    pid = -1;
    wbuf.clear();
    wpos = 0;
    setError(FailedToStart, text);
    state = NotRunning;
    q->stateChanged(NotRunning);
    if (!guard)
        return;
    q->errorOccurred(FailedToStart);
}

// Reads the result of exec(): end of file means the program started, an int is its errno.
bool QProcess::Private::onStartup() {
    if (startFd < 0 || state != Starting)
        return state != Starting;
    ChildReport report;
    ssize_t n;
    do {
        n = ::read(startFd, &report, sizeof report);
    } while (n < 0 && errno == EINTR);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return false;
    dropNotifier(startNotifier);
    closeFd(startFd);
    if (n == static_cast<ssize_t>(sizeof report)) {
        if (pid > 0)
            waitPid(pid, nullptr, 0);
        failStart(childFailureText(report));
        return true;
    }
    QPointer<QProcess> guard(q);
    startedOk = true;
    state = Running;
    q->stateChanged(Running);
    if (!guard)
        return true;
    q->started();
    return true;
}

void QProcess::Private::onPidReady() {
    if (!reapIfExited() && pidNotifier) {
        // The process is still being torn down; poll until its status is available.
        dropNotifier(pidNotifier);
        if (!pollTimer)
            pollTimer = q->startTimer(kPollSliceMs);
    }
}

// Returns true if the child has been reaped and processDied() has run.
bool QProcess::Private::reapIfExited() {
    if (pid <= 0)
        return false;
    if (state == Starting) {
        QPointer<QProcess> guard(q);
        onStartup();
        if (!guard || state == NotRunning)
            return true;
    }
    int status = 0;
    const pid_t r = waitPid(pid, &status, WNOHANG);
    if (r == 0)
        return false;
    if (r < 0 && errno != ECHILD)
        return false;
    if (r < 0)
        status = 0;
    pid = -1;
    processDied(status);
    return true;
}

void QProcess::Private::processDied(int status) {
    if (dying)
        return;
    dying = true;
    QPointer<QProcess> guard(q);
    drainChannel(0);
    if (!guard)
        return;
    drainChannel(1);
    if (!guard)
        return;
    const bool crashed = WIFSIGNALED(status);
    exitStatus = crashed ? CrashExit : NormalExit;
    exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
    closeAll();
    wbuf.clear();
    wpos = 0;
    pid = -1;
    diedOk = true;

    const int ch = current();
    if (!finishedEmitted[ch]) {
        finishedEmitted[ch] = true;
        q->readChannelFinished();
        if (!guard)
            return;
    }
    if (crashed) {
        setError(Crashed, QString::fromUtf8("Process crashed"));
        q->errorOccurred(Crashed);
        if (!guard)
            return;
    }
    state = NotRunning;
    q->stateChanged(NotRunning);
    if (!guard)
        return;
    q->finished(exitCode, exitStatus);
}

void QProcess::Private::emitReadyRead(int ch) {
    QPointer<QProcess> guard(q);
    if (current() == ch && !inReadyRead) {
        inReadyRead = true;
        q->readyRead();
        if (!guard)
            return;
        inReadyRead = false;
    }
    q->channelReadyRead(ch);
    if (!guard)
        return;
    if (ch == 0)
        q->readyReadStandardOutput();
    else
        q->readyReadStandardError();
}

void QProcess::Private::onEof(int ch) {
    QPointer<QProcess> guard(q);
    closeChannelFd(ch);
    eofSeen[ch] = true;
    if (current() == ch && !finishedEmitted[ch]) {
        finishedEmitted[ch] = true;
        q->readChannelFinished();
    }
}

// Reads at most one chunk. Returns true if data arrived.
bool QProcess::Private::readChannelOnce(int ch) {
    int &fd = channelFd(ch);
    if (fd < 0)
        return false;
    QByteArray &b = buf[ch];
    const qsizetype old = b.size();
    b.resize(old + kReadChunk);
    ssize_t n;
    do {
        n = ::read(fd, b.data() + old, static_cast<std::size_t>(kReadChunk));
    } while (n < 0 && errno == EINTR);
    if (n > 0) {
        b.resize(old + n);
        ++dataSeq[ch];
        emitReadyRead(ch);
        return true;
    }
    b.resize(old);
    if (n == 0) {
        onEof(ch);
        return false;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK)
        return false;
    const int e = errno;
    QPointer<QProcess> guard(q);
    closeChannelFd(ch);
    setError(ReadError, QString::fromUtf8("Error reading from process: %1").arg(errnoString(e)));
    q->errorOccurred(ReadError);
    return false;
}

void QProcess::Private::drainChannel(int ch) {
    QPointer<QProcess> guard(q);
    while (channelFd(ch) >= 0) {
        if (!readChannelOnce(ch) || !guard)
            break;
    }
}

bool QProcess::Private::canWrite() {
    if (inFd < 0)
        return false;
    if (wpos >= wbuf.size()) {
        if (inNotifier)
            inNotifier->setEnabled(false);
        if (closeWritePending)
            closeStdin();
        return false;
    }
    const ssize_t n = writeNoSigpipe(inFd, wbuf.constData() + wpos, static_cast<std::size_t>(wbuf.size() - wpos));
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return false;
        const int e = errno;
        closeStdin();
        wbuf.clear();
        wpos = 0;
        setError(WriteError, QString::fromUtf8("Error writing to process: %1").arg(errnoString(e)));
        q->errorOccurred(WriteError);
        return false;
    }
    wpos += n;
    if (wpos >= wbuf.size()) {
        wbuf.clear();
        wpos = 0;
        if (inNotifier)
            inNotifier->setEnabled(false);
        if (closeWritePending)
            closeStdin();
    }
    QPointer<QProcess> guard(q);
    q->bytesWritten(n);
    if (guard)
        q->channelBytesWritten(0, n);
    return true;
}

bool QProcess::Private::waitFor(Wait what, int msecs) {
    QPointer<QProcess> guard(q);
    const bool infinite = msecs < 0;
    const auto deadline = Clock::now() + std::chrono::milliseconds(infinite ? 0 : msecs);
    enum Kind { StartK, PidK, OutK, ErrK, InK };
    const int channel = current();
    const unsigned seqBefore = dataSeq[channel];
    for (;;) {
        std::vector<pollfd> fds;
        std::vector<Kind> kinds;
        auto add = [&](int fd, short events, Kind k) {
            if (fd >= 0) {
                fds.push_back({fd, events, 0});
                kinds.push_back(k);
            }
        };
        // Like Qt, waiting for the start only looks at the result of exec().
        if (what == Wait::Start) {
            add(startFd, POLLIN, StartK);
        } else {
            if (state == Starting)
                add(startFd, POLLIN, StartK);
            add(pidfd, POLLIN, PidK);
            add(outFd, POLLIN, OutK);
            add(errFd, POLLIN, ErrK);
            if (wpos < wbuf.size())
                add(inFd, POLLOUT, InK);
        }

        int timeout = -1;
        if (!infinite) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
            timeout = left < 0 ? 0 : static_cast<int>(std::min<long long>(left, 1 << 30));
        }
        const bool polling = what != Wait::Start && pidfd < 0 && pid > 0;
        if (polling && (timeout < 0 || timeout > kPollSliceMs))
            timeout = kPollSliceMs;

        if (fds.empty() && !polling)
            return false;
        const int r = ::poll(fds.data(), static_cast<nfds_t>(fds.size()), timeout);
        if (r < 0 && errno != EINTR) {
            setError(UnknownError, QString::fromUtf8("Error waiting for process: %1").arg(errnoString(errno)));
            return false;
        }
        bool wrote = false;
        if (r > 0) {
            for (std::size_t i = 0; i < fds.size() && guard; ++i) {
                if (fds[i].revents == 0)
                    continue;
                switch (kinds[i]) {
                case StartK:
                    onStartup();
                    break;
                case PidK:
                    onPidReady();
                    break;
                case OutK:
                    readChannelOnce(0);
                    break;
                case ErrK:
                    readChannelOnce(1);
                    break;
                case InK:
                    wrote = canWrite() || wrote;
                    break;
                }
            }
        }
        if (polling && guard)
            reapIfExited();
        if (!guard)
            return false;

        switch (what) {
        case Wait::Start:
            if (startedOk)
                return true;
            if (state == NotRunning)
                return false;
            break;
        case Wait::ReadyRead:
            if (dataSeq[channel] != seqBefore)
                return true;
            if (state == NotRunning)
                return false;
            break;
        case Wait::BytesWritten:
            if (wrote)
                return true;
            if (state == NotRunning)
                return false;
            break;
        case Wait::Finished:
            if (state == NotRunning)
                return diedOk;
            break;
        }
        if (!infinite && Clock::now() >= deadline) {
            setError(Timedout, QString::fromUtf8("Process operation timed out"));
            return false;
        }
    }
}

#include "moc_qprocess.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
