// MIT License - see LICENSE
#include <QtCore/qcoreapplication.h>

#include "dispatcher.h"
#include "qabstracteventdispatcher_p.h"
#include "qobject_p.h"
#include "threaddata_p.h"

#include <QtCore/qabstracteventdispatcher.h>
#include <QtCore/qdebug.h>
#include <QtCore/qdir.h>

#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <vector>

/*!
    \module NtCore
    \title Nt Core C++ Classes
    \brief Core non-graphical classes: objects, signals, events, strings and containers.
    \cmaketarget Nt::Core

    Nt Core provides the classes every Nt program builds on. Headers keep Qt's
    names, so \c{#include <QObject>} and \c{#include <QtCore/QTimer>} both work.

    See \l{Signals and Slots} for how objects communicate.
*/

namespace {
std::atomic<QCoreApplication *> g_self{nullptr};
std::atomic<quint64> g_attributes{0};
std::atomic<bool> g_setuidAllowed{false};

// The routines of qAddPreRoutine() and qAddPostRoutine(). They are registered from static initializers too, so the
// lists are made on first use.
struct Routines {
    std::mutex mutex;
    std::vector<QtStartUpFunction> pre;
    std::vector<QtCleanUpFunction> post;
};
Routines &routines() {
    static Routines r;
    return r;
}

void callPreRoutines() {
    std::vector<QtStartUpFunction> pre;
    {
        const std::lock_guard<std::mutex> lock(routines().mutex);
        pre = routines().pre;
    }
    for (QtStartUpFunction function : pre)
        function();
}

// Last added, first called. A routine may add or remove routines, so the lock is not held while one runs.
void callPostRoutines() {
    for (;;) {
        QtCleanUpFunction function = nullptr;
        {
            const std::lock_guard<std::mutex> lock(routines().mutex);
            if (routines().post.empty())
                return;
            function = routines().post.back();
            routines().post.pop_back();
        }
        function();
    }
}
QStringList g_arguments;
std::atomic<bool> g_closingDown{false};
QString g_appName;
QString g_appVersion;
QString g_orgName;
QString g_orgDomain;
bool g_appNameSet = false;
}

/*!
    \class QCoreApplication
    \brief The QCoreApplication class provides an event loop for console applications.

    Create one QCoreApplication in \c main() before using timers, posted
    events or queued connections, and call exec() to run the event loop:

    \code
    int main(int argc, char *argv[])
    {
        QCoreApplication app(argc, argv);
        QTimer::singleShot(100, [] { QCoreApplication::quit(); });
        return app.exec();
    }
    \endcode

    The class also holds the command line, the application name and version,
    and provides functions to send and post events.

    \differences
    Only part of the Qt API is present: the library paths are only a list, as
    Nt loads no plugins, and there is no \c postEvent() priority.
    Creating a second application object aborts the process. \c exit() and
    \c quit() stop every running QEventLoop of the thread that created the
    application object, and aboutToQuit() is emitted after the main loop has
    stopped. Unlike Qt, they do not make later calls to \c exec() return at once.
    postEvent() may be called from any thread; processEvents() and
    sendPostedEvents() work on the calling thread's queue. \c qApp is defined as
    \c QCoreApplication::instance(). The platforms that Nt supports have no
    permission prompts: checkPermission() grants every valid permission.
    \enddifferences

    \sa QEventLoop, QObject, QCommandLineParser
*/

/*!
    Constructs a core application from the command line given by \a argc and
    \a argv. Both must remain valid for the lifetime of the application, and
    only one application object may exist at a time; creating a second one
    prints a message and aborts the process.

    \sa arguments()
*/
QCoreApplication::QCoreApplication(int &argc, char **argv) {
    QCoreApplication *expected = nullptr;
    if (!g_self.compare_exchange_strong(expected, this)) {
        std::fputs("QCoreApplication: there should be only one application object\n", stderr);
        std::abort();
    }
#ifndef __ZEPHYR__
    if (!g_setuidAllowed.load() && (::getuid() != ::geteuid() || ::getgid() != ::getegid())) {
        std::fputs("FATAL: The application binary appears to be running setuid, this is a security hole.\n", stderr);
        std::abort();
    }
#endif
    g_closingDown.store(false);
    g_arguments.clear();
    for (int i = 0; argv && i < argc; ++i)
        g_arguments.append(QString::fromUtf8(argv[i]));
    QtPrivate::eventDispatcherStarting(QtPrivate::objectThreadData(this));
    callPreRoutines();
}

/*!
    Destroys the application object. After this, instance() returns
    \c nullptr.
*/
QCoreApplication::~QCoreApplication() {
    g_closingDown.store(true);
    callPostRoutines();
    // Like Qt, wait for the global thread pool so that its tasks cannot outlive the application.
    QtPrivate::waitForGlobalThreadPool();
    QtPrivate::eventDispatcherFinished(QtPrivate::objectThreadData(this));
    QCoreApplication *expected = this;
    g_self.compare_exchange_strong(expected, nullptr);
}

void qAddPreRoutine(QtStartUpFunction function) {
    if (!function)
        return;
    {
        const std::lock_guard<std::mutex> lock(routines().mutex);
        routines().pre.push_back(function);
    }
    if (g_self.load())
        function();
}

void qAddPostRoutine(QtCleanUpFunction function) {
    if (!function)
        return;
    const std::lock_guard<std::mutex> lock(routines().mutex);
    routines().post.push_back(function);
}

void qRemovePostRoutine(QtCleanUpFunction function) {
    const std::lock_guard<std::mutex> lock(routines().mutex);
    auto &post = routines().post;
    post.erase(std::remove(post.begin(), post.end(), function), post.end());
}

/*!
    Returns the application object, or \c nullptr if none exists.
*/
QCoreApplication *QCoreApplication::instance() { return g_self.load(); }

/*!
    \since 6.10
    Returns \c true if an application object exists; otherwise returns
    \c false. Unlike instance(), this is also \c false while the application
    object is being destroyed.

    \sa instance(), closingDown()
*/
bool QCoreApplication::instanceExists() noexcept { return g_self.load() != nullptr && !g_closingDown.load(); }

/*!
    Returns \c true if no application object has been created yet; otherwise
    returns \c false.

    \sa closingDown()
*/
bool QCoreApplication::startingUp() { return g_self.load() == nullptr && !g_closingDown.load(); }

/*!
    Returns \c true if the application object is being destroyed or has been
    destroyed; otherwise returns \c false. Creating a new application object
    makes this \c false again.

    \sa startingUp()
*/
bool QCoreApplication::closingDown() { return g_closingDown.load(); }

/*!
    Returns the event dispatcher of the thread that the application object lives in, which is
    the main thread, or \c nullptr if there is no application object. It is the one that setEventDispatcher()
    installed, if any.

    \sa QAbstractEventDispatcher::instance(), QThread::eventDispatcher()
*/
QAbstractEventDispatcher *QCoreApplication::eventDispatcher() {
    QCoreApplication *app = g_self.load();
    return app ? QAbstractEventDispatcher::instance(app->thread()) : nullptr;
}

/*!
    \since 5.0
    Installs \a eventDispatcher as the event dispatcher of the main thread, which then owns it. Call it before the
    application object is made, from the main thread; it is not installed if the application object exists or
    the main thread already has an event dispatcher, such as one that eventDispatcher() or
    QAbstractEventDispatcher::instance() created.

    The application object calls its QAbstractEventDispatcher::startingUp() when it is made, and its
    QAbstractEventDispatcher::closingDown() when it is destroyed, before it deletes it.

    \sa eventDispatcher(), QThread::setEventDispatcher()
*/
void QCoreApplication::setEventDispatcher(QAbstractEventDispatcher *eventDispatcher) {
    if (g_self.load()) {
        qWarning("QCoreApplication::setEventDispatcher: An event dispatcher has already been created for the main "
                 "thread");
        return;
    }
    QtPrivate::ThreadData *td = QtPrivate::ThreadData::mainThread();
    if (!td)
        td = QtPrivate::ThreadData::current();
    QtPrivate::installEventDispatcher(td->thread.load(), td, eventDispatcher, "QCoreApplication::setEventDispatcher");
}

/*!
    \since 4.4
    Sets the application attribute \a attribute if \a on is \c true; otherwise clears it. Most attributes are read
    when the application object or the first window is made, so set them before that.

    \sa testAttribute()
*/
void QCoreApplication::setAttribute(Qt::ApplicationAttribute attribute, bool on) {
    const int bit = static_cast<int>(attribute);
    if (bit < 0 || bit >= 64)
        return;
    const quint64 mask = quint64{1} << bit;
    if (on)
        g_attributes.fetch_or(mask);
    else
        g_attributes.fetch_and(~mask);
}

/*!
    \since 4.4
    Returns \c true if the application attribute \a attribute is set; otherwise returns \c false.

    \sa setAttribute()
*/
bool QCoreApplication::testAttribute(Qt::ApplicationAttribute attribute) {
    const int bit = static_cast<int>(attribute);
    return bit >= 0 && bit < 64 && (g_attributes.load() & (quint64{1} << bit)) != 0;
}

/*!
    \since 5.3
    Allows the application to run setuid or setgid if \a allow is \c true. Without it, the constructor of the
    application object stops a program whose effective user or group differs from the real one, since a library
    as large as Nt is not written to be safe with the privileges of another user. Call it before the
    application object is made.

    \sa isSetuidAllowed()
*/
void QCoreApplication::setSetuidAllowed(bool allow) { g_setuidAllowed.store(allow); }

/*!
    \since 5.3
    Returns \c true if the application may run setuid or setgid; otherwise returns \c false.

    \sa setSetuidAllowed()
*/
bool QCoreApplication::isSetuidAllowed() { return g_setuidAllowed.load(); }

/*!
    \since 5.0
    Installs \a filterObj on the event dispatcher of the main thread, so that it sees the native events
    that the platform reads there, such as the events of the window system. The filter that was installed last
    is called first.

    \sa removeNativeEventFilter(), QAbstractEventDispatcher::installNativeEventFilter()
*/
void QCoreApplication::installNativeEventFilter(QAbstractNativeEventFilter *filterObj) {
    if (QAbstractEventDispatcher *dispatcher = QAbstractEventDispatcher::instance(thread()))
        dispatcher->installNativeEventFilter(filterObj);
}

/*!
    \since 5.0
    Removes \a filterObj from the event dispatcher of the main thread. Nothing happens if it is not installed.
    A filter is also removed when it is destroyed.

    \sa installNativeEventFilter()
*/
void QCoreApplication::removeNativeEventFilter(QAbstractNativeEventFilter *filterObj) {
    if (QAbstractEventDispatcher *dispatcher = QAbstractEventDispatcher::instance(thread()))
        dispatcher->removeNativeEventFilter(filterObj);
}

/*!
    Returns the command line as passed to the constructor, decoded as UTF-8.
    The first entry is the program name.

    \sa applicationName()
*/
QStringList QCoreApplication::arguments() { return g_arguments; }

/*!
    \since 4.0
    Returns the absolute path of the executable file of the application, with
    symbolic links resolved. On Linux it is read from \c{/proc/self/exe};
    elsewhere it is derived from the first command-line argument.

    \sa applicationDirPath(), arguments()
*/
QString QCoreApplication::applicationFilePath() {
#if defined(__linux__)
    std::vector<char> buf(PATH_MAX);
    const ssize_t n = ::readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if (n > 0)
        return QString::fromUtf8(buf.data(), static_cast<qsizetype>(n));
#endif
    if (g_arguments.isEmpty())
        return QString();
    char resolved[PATH_MAX];
    if (::realpath(g_arguments.first().toUtf8().constData(), resolved))
        return QString::fromUtf8(resolved);
    return QString();
}

/*!
    \since 4.0
    Returns the directory that contains the executable file of the
    application.

    \sa applicationFilePath()
*/
QString QCoreApplication::applicationDirPath() {
    const QString file = applicationFilePath();
    const qsizetype slash = file.lastIndexOf(QChar(u'/'));
    return slash <= 0 ? (slash == 0 ? QString::fromUtf8("/") : QString()) : file.left(slash);
}

/*!
    \since 4.4
    Returns the process identifier of the application.
*/
qint64 QCoreApplication::applicationPid() { return static_cast<qint64>(::getpid()); }

namespace {
std::mutex &libraryPathLock() {
    static std::mutex m;
    return m;
}
// Null until set: then libraryPaths() computes the default.
QStringList &libraryPathList() {
    static QStringList paths;
    return paths;
}
bool g_libraryPathsSet = false;

QStringList defaultLibraryPaths() {
    QStringList paths;
    const QString dir = QCoreApplication::applicationDirPath();
    if (!dir.isEmpty())
        paths.append(dir);
    return paths;
}
} // namespace

/*!
    Returns the directories searched for plugins and libraries. Unless
    setLibraryPaths() was called, the list holds the directory of the
    executable. Nt loads no plugins, so the list is only kept for
    applications that use it themselves.

    \sa setLibraryPaths(), addLibraryPath()
*/
QStringList QCoreApplication::libraryPaths() {
    std::lock_guard<std::mutex> lk(libraryPathLock());
    return g_libraryPathsSet ? libraryPathList() : defaultLibraryPaths();
}

/*!
    Replaces the library paths with \a paths.

    \sa libraryPaths()
*/
void QCoreApplication::setLibraryPaths(const QStringList &paths) {
    std::lock_guard<std::mutex> lk(libraryPathLock());
    libraryPathList() = paths;
    g_libraryPathsSet = true;
}

/*!
    Puts \a path at the front of the library paths, so it is searched first.
    Nothing changes if \a path is empty or already in the list.

    \sa removeLibraryPath(), libraryPaths()
*/
void QCoreApplication::addLibraryPath(const QString &path) {
    if (path.isEmpty())
        return;
    std::lock_guard<std::mutex> lk(libraryPathLock());
    if (!g_libraryPathsSet) {
        libraryPathList() = defaultLibraryPaths();
        g_libraryPathsSet = true;
    }
    const QString clean = QDir::cleanPath(path);
    if (!libraryPathList().contains(clean))
        libraryPathList().prepend(clean);
}

/*!
    Removes \a path from the library paths.

    \sa addLibraryPath()
*/
void QCoreApplication::removeLibraryPath(const QString &path) {
    if (path.isEmpty())
        return;
    std::lock_guard<std::mutex> lk(libraryPathLock());
    if (!g_libraryPathsSet) {
        libraryPathList() = defaultLibraryPaths();
        g_libraryPathsSet = true;
    }
    libraryPathList().removeAll(QDir::cleanPath(path));
}

/*!
    Sets the application name to \a name.

    \sa applicationName()
*/
void QCoreApplication::setApplicationName(const QString &name) {
    const QString before = applicationName();
    g_appName = name;
    g_appNameSet = true;
    if (QCoreApplication *app = g_self.load(); app && applicationName() != before)
        emit app->applicationNameChanged();
}

/*!
    Returns the application name. Unless it was set with
    setApplicationName(), this is the file name part of the program path in
    the command line, or an empty string if there is no command line.

    \sa setApplicationName(), arguments()
*/
QString QCoreApplication::applicationName() {
    if (g_appNameSet)
        return g_appName;
    if (g_arguments.isEmpty())
        return QString();
    const QString &exe = g_arguments.first();
    return exe.mid(exe.lastIndexOf(QChar(u'/')) + 1);
}

/*!
    Sets the name of the organization to \a orgName. QSettings uses it to
    locate the settings of the application.

    \sa organizationName(), setOrganizationDomain(), QSettings
*/
void QCoreApplication::setOrganizationName(const QString &orgName) {
    if (g_orgName == orgName)
        return;
    g_orgName = orgName;
    if (QCoreApplication *app = g_self.load())
        emit app->organizationNameChanged();
}

/*!
    Returns the name of the organization, which is empty unless it was set with
    setOrganizationName().

    \sa setOrganizationName(), organizationDomain()
*/
QString QCoreApplication::organizationName() { return g_orgName; }

/*!
    Sets the Internet domain of the organization to \a orgDomain.

    \sa organizationDomain(), setOrganizationName()
*/
void QCoreApplication::setOrganizationDomain(const QString &orgDomain) {
    if (g_orgDomain == orgDomain)
        return;
    g_orgDomain = orgDomain;
    if (QCoreApplication *app = g_self.load())
        emit app->organizationDomainChanged();
}

/*!
    Returns the Internet domain of the organization, which is empty unless it
    was set with setOrganizationDomain().

    \sa setOrganizationDomain(), organizationName()
*/
QString QCoreApplication::organizationDomain() { return g_orgDomain; }

/*!
    Sets the application version to \a version.

    \sa applicationVersion(), QCommandLineParser::addVersionOption()
*/
void QCoreApplication::setApplicationVersion(const QString &version) {
    if (g_appVersion == version)
        return;
    g_appVersion = version;
    if (QCoreApplication *app = g_self.load())
        emit app->applicationVersionChanged();
}

/*!
    Returns the application version, which is empty unless it was set with
    setApplicationVersion().

    \sa setApplicationVersion()
*/
QString QCoreApplication::applicationVersion() { return g_appVersion; }

/*!
    Enters the main event loop and waits until exit() or quit() is called,
    then emits aboutToQuit() and returns the code passed to exit().

    \sa quit(), exit(), QEventLoop::exec()
*/
int QCoreApplication::exec() {
    QEventLoop loop;
    const int code = loop.exec(QEventLoop::ApplicationExec);
    if (QCoreApplication *app = g_self.load())
        app->aboutToQuit();
    return code;
}

/*!
    Tells every running event loop to exit with the return code
    \a returnCode.

    \sa quit(), exec()
*/
void QCoreApplication::exit(int returnCode) {
    QCoreApplication *app = g_self.load();
    QtPrivate::ThreadData *td = app ? QtPrivate::acquireThreadData(app) : QtPrivate::ThreadData::current();
    td->dispatcher.exitAll(returnCode);
    if (app)
        td->release();
}

/*!
    Tells the application to exit with return code 0. This is the same as
    calling exit(0).

    \sa exit(), aboutToQuit()
*/
void QCoreApplication::quit() { exit(0); }

namespace {
std::atomic<bool> g_quitLockEnabled{true};
}

/*!
    Sets whether the application quits when the last QEventLoopLocker created
    without a target is destroyed to \a enabled. The default is \c true.

    \sa isQuitLockEnabled(), QEventLoopLocker
*/
void QCoreApplication::setQuitLockEnabled(bool enabled) { g_quitLockEnabled.store(enabled); }

/*!
    Returns \c true if the application quits when the last QEventLoopLocker
    created without a target is destroyed.

    \sa setQuitLockEnabled()
*/
bool QCoreApplication::isQuitLockEnabled() { return g_quitLockEnabled.load(); }

/*!
    Processes pending events once according to \a flags. With
    \c QEventLoop::WaitForMoreEvents the call blocks until an event is
    available. Other flags are ignored.

    \sa QEventLoop::processEvents()
*/
void QCoreApplication::processEvents(QEventLoop::ProcessEventsFlags flags) {
    QtPrivate::processEvents(QtPrivate::ThreadData::current(), flags, nullptr);
}

/*!
    \overload
    Processes the pending events of the calling thread for at most \a ms
    milliseconds, or until there are none left, as \a flags selects. It does
    not wait for new events.
*/
void QCoreApplication::processEvents(QEventLoop::ProcessEventsFlags flags, int ms) {
    processEvents(flags, QDeadlineTimer(ms));
}

/*!
    \since 6.7
    \overload
    Processes the pending events of the calling thread until \a deadline
    expires or there are none left. \a flags is accepted for compatibility.
*/
void QCoreApplication::processEvents(QEventLoop::ProcessEventsFlags flags, QDeadlineTimer deadline) {
    flags &= ~QEventLoop::ProcessEventsFlags(QEventLoop::WaitForMoreEvents);
    QtPrivate::ThreadData *td = QtPrivate::ThreadData::current();
    while (QtPrivate::processEvents(td, flags, nullptr) && !deadline.hasExpired()) {
    }
}

/*!
    Delivers the events posted to \a receiver that match \a eventType right away, in the
    calling thread, and removes them from the queue. If \a receiver is \c nullptr, the events
    of every object that lives in the calling thread match. An \a eventType of 0 matches all
    events except \c QEvent::DeferredDelete, which is delivered only when asked for by type
    or by an event loop. \a receiver must live in the calling thread.

    \sa postEvent(), processEvents()
*/
void QCoreApplication::sendPostedEvents(QObject *receiver, int eventType) {
    auto *td = QtPrivate::ThreadData::current();
    if (receiver && QtPrivate::objectThreadData(receiver) != td) {
        qWarning("QCoreApplication::sendPostedEvents: Cannot send posted events for objects in another thread");
        return;
    }
    td->dispatcher.sendPosted(receiver, eventType);
}

/*!
    Adds \a event to the event queue of \a receiver and returns immediately.
    The event is delivered from the event loop. The queue takes ownership of
    \a event and deletes it after delivery; if \a receiver is \c nullptr the
    event is deleted at once.

    \sa sendEvent(), removePostedEvents()
*/
void QCoreApplication::postEvent(QObject *receiver, QEvent *event) {
    if (!event)
        return;
    if (!receiver) {
        delete event;
        return;
    }
    QtPrivate::postToObject(receiver, event);
}

namespace QtPrivate {

// The event filters of the application, then those of `receiver`, the last installed first; true if one stopped
// the event.
bool runEventFilters(QObject *receiver, QEvent *event) {
    const auto run = [receiver, event](QObject *owner) {
        // A copy: a filter may install or remove filters.
        const std::vector<QPointer<QObject>> filters = Access::d(owner).eventFilters;
        for (auto it = filters.rbegin(); it != filters.rend(); ++it) {
            QObject *filter = it->data();
            if (filter && filter != receiver && filter->eventFilter(receiver, event))
                return true;
        }
        return false;
    };
    if (QCoreApplication *app = g_self.load(); app && app != receiver && run(app))
        return true;
    return run(receiver);
}

} // namespace QtPrivate

/*!
    Sends \a event directly to \a receiver through notify() and returns the
    value the event handler returned. The caller keeps ownership of
    \a event. Returns \c false if \a receiver or \a event is \c nullptr.

    \sa postEvent(), notify()
*/
bool QCoreApplication::sendEvent(QObject *receiver, QEvent *event) {
    if (!receiver || !event)
        return false;
    if (QCoreApplication *app = g_self.load())
        return app->notify(receiver, event);
    if (QtPrivate::runEventFilters(receiver, event))
        return true;
    return receiver->event(event);
}

/*!
    Removes the posted events for \a receiver that have not been delivered
    yet. If \a eventType is 0, every pending event is removed; otherwise only
    events of that type.

    \sa postEvent()
*/
void QCoreApplication::removePostedEvents(QObject *receiver, int eventType) {
    if (!receiver)
        return;
    QtPrivate::ThreadData *td = QtPrivate::acquireThreadData(receiver);
    td->dispatcher.removePosted(receiver, eventType);
    td->release();
}

/*!
    Delivers \a event to \a receiver by calling QObject::event() and returns
    its result. Reimplement it to intercept every event sent to any object
    in the application.

    \sa sendEvent(), QObject::event()
*/
bool QCoreApplication::notify(QObject *receiver, QEvent *event) {
    if (QtPrivate::runEventFilters(receiver, event))
        return true;
    return receiver->event(event);
}

QString qAppName() { return QCoreApplication::applicationName(); }

#include "moc_qcoreapplication.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
