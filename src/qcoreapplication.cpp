// MIT License - see LICENSE
#include <QtCore/qcoreapplication.h>

#include "dispatcher.h"

#include <cstdio>
#include <cstdlib>

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
QCoreApplication *g_self = nullptr;
QStringList g_arguments;
QString g_appName;
QString g_appVersion;
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
    Only part of the Qt API is present: there is no \c organizationName(),
    \c installTranslator(), \c libraryPaths(), \c applicationFilePath(),
    \c sendPostedEvents() or \c processEvents() overload with a timeout.
    Creating a second application object aborts the process. \c exit() and
    \c quit() stop every running QEventLoop, and aboutToQuit() is emitted
    after the main loop has stopped. \c qApp is defined as
    \c QCoreApplication::instance().
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
    if (g_self) {
        std::fputs("QCoreApplication: there should be only one application object\n", stderr);
        std::abort();
    }
    g_self = this;
    g_arguments.clear();
    for (int i = 0; argv && i < argc; ++i)
        g_arguments.append(QString::fromUtf8(argv[i]));
}

/*!
    Destroys the application object. After this, instance() returns
    \c nullptr.
*/
QCoreApplication::~QCoreApplication() {
    if (g_self == this)
        g_self = nullptr;
}

/*!
    Returns the application object, or \c nullptr if none exists.
*/
QCoreApplication *QCoreApplication::instance() { return g_self; }

/*!
    Returns the command line as passed to the constructor, decoded as UTF-8.
    The first entry is the program name.

    \sa applicationName()
*/
QStringList QCoreApplication::arguments() { return g_arguments; }

/*!
    Sets the application name to \a name.

    \sa applicationName()
*/
void QCoreApplication::setApplicationName(const QString &name) {
    g_appName = name;
    g_appNameSet = true;
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
    Sets the application version to \a version.

    \sa applicationVersion(), QCommandLineParser::addVersionOption()
*/
void QCoreApplication::setApplicationVersion(const QString &version) { g_appVersion = version; }

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
    const int code = loop.exec();
    if (g_self)
        g_self->aboutToQuit();
    return code;
}

/*!
    Tells every running event loop to exit with the return code
    \a returnCode.

    \sa quit(), exec()
*/
void QCoreApplication::exit(int returnCode) { QtPrivate::Dispatcher::instance().exitAll(returnCode); }

/*!
    Tells the application to exit with return code 0. This is the same as
    calling exit(0).

    \sa exit(), aboutToQuit()
*/
void QCoreApplication::quit() { exit(0); }

/*!
    Processes pending events once according to \a flags. With
    \c QEventLoop::WaitForMoreEvents the call blocks until an event is
    available. Other flags are ignored.

    \sa QEventLoop::processEvents()
*/
void QCoreApplication::processEvents(QEventLoop::ProcessEventsFlags flags) {
    QtPrivate::Dispatcher::instance().process(flags.testFlag(QEventLoop::WaitForMoreEvents), nullptr);
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
    QtPrivate::Dispatcher::instance().post(receiver, event);
}

/*!
    Sends \a event directly to \a receiver through notify() and returns the
    value the event handler returned. The caller keeps ownership of
    \a event. Returns \c false if \a receiver or \a event is \c nullptr.

    \sa postEvent(), notify()
*/
bool QCoreApplication::sendEvent(QObject *receiver, QEvent *event) {
    if (!receiver || !event)
        return false;
    if (g_self)
        return g_self->notify(receiver, event);
    return receiver->event(event);
}

/*!
    Removes the posted events for \a receiver that have not been delivered
    yet. If \a eventType is 0, every pending event is removed; otherwise only
    events of that type.

    \sa postEvent()
*/
void QCoreApplication::removePostedEvents(QObject *receiver, int eventType) {
    QtPrivate::Dispatcher::instance().removePosted(receiver, eventType);
}

/*!
    Delivers \a event to \a receiver by calling QObject::event() and returns
    its result. Reimplement it to intercept every event sent to any object
    in the application.

    \sa sendEvent(), QObject::event()
*/
bool QCoreApplication::notify(QObject *receiver, QEvent *event) { return receiver->event(event); }
