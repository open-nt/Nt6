// MIT License - see LICENSE
#include <QtGui/qcursor.h>
#include <QtGui/qguiapplication.h>

#include "qfont_p.h"
#include "qpa/qplatformintegration_p.h"
#include "qpa/qplatformwindow_p.h"
#include "qpa/qplatformscreen_p.h"
#include "qwindow_p.h"

#include <QtCore/qdebug.h>
#include <QtGui/qevent.h>
#include <QtGui/qscreen.h>
#include <QtGui/qstylehints.h>
#include <QtGui/qwindow.h>

#include <cstdio>
#include <cstdlib>
#include <memory>

/*!
    \module NtGui
    \title Nt GUI C++ Classes
    \brief Windows, events, painting and images.
    \cmaketarget Nt::Gui

    Nt GUI provides the classes for graphical applications: QGuiApplication,
    QWindow and its input events, and QPainter with QImage for drawing. It talks
    to the window system through a platform integration, as Qt does. The
    platforms are \c xcb (X11, also through XWayland) and \c offscreen, a
    headless platform for tests. Choose one with the \c{-platform} option or
    the \c QT_QPA_PLATFORM environment variable.

    Link with \c Nt::Gui and include \c{<QGuiApplication>}.
*/

namespace {

struct GuiState {
    std::unique_ptr<QPlatformIntegration> integration;
    QList<QScreen *> screens;
    QList<QWindow *> windows;
    QWindow *focus = nullptr;
    Qt::MouseButtons buttons;
    Qt::KeyboardModifiers modifiers;
    bool quitOnLastWindowClosed = true;
    QPoint cursorPosition;
    QList<QCursor> overrideCursors; // the last one is the one that shows
    QString platformName;
    QStyleHints *styleHints = nullptr;
};

GuiState *g_state = nullptr;
QGuiApplication *g_app = nullptr;

// "-platform name", "-platform=name" or "--platform name"; empty when absent.
QString platformFromArguments(const QStringList &args) {
    for (qsizetype i = 1; i < args.size(); ++i) {
        const QString &a = args.at(i);
        if ((a == QString::fromLatin1("-platform") || a == QString::fromLatin1("--platform")) && i + 1 < args.size())
            return args.at(i + 1);
        if (a.startsWith(QString::fromLatin1("-platform=")) || a.startsWith(QString::fromLatin1("--platform=")))
            return a.mid(a.indexOf(QChar('=')) + 1);
    }
    return QString();
}

bool envSet(const char *name) {
    const char *v = std::getenv(name);
    return v && *v;
}

QString chooseDefaultPlatform() {
    const QStringList names = QtPrivate::availablePlatforms();
    const bool haveDisplay = envSet("DISPLAY");
    if (haveDisplay && names.contains(QString::fromLatin1("xcb")))
        return QString::fromLatin1("xcb");
    if (!haveDisplay)
        std::fputs("Nt GUI: no display found (DISPLAY is not set); using the offscreen platform\n", stderr);
    return QString::fromLatin1("offscreen");
}

} // namespace

/*!
    \class QGuiApplication
    \brief The QGuiApplication class manages the GUI application's control flow and main settings.

    QGuiApplication adds to QCoreApplication what graphical programs need: it
    connects to the window system, owns the screens, and delivers input
    events to windows. Create one in \c main() before any QWindow:

    \code
    int main(int argc, char *argv[])
    {
        QGuiApplication app(argc, argv);
        MyWindow window;
        window.show();
        return app.exec();
    }
    \endcode

    The platform is chosen from the \c{-platform} command line option, then from
    the \c QT_QPA_PLATFORM environment variable, and otherwise it is \c xcb if
    \c DISPLAY is set and \c offscreen if not. An unknown or unavailable platform
    aborts the program, as in Qt.

    \differences
    Only the functions below exist. There is no clipboard, no style hints, no
    palette or font, no translators, no high-DPI settings, no \c queryKeyboardModifiers()
    and no session management. The \c{-platform} option stays in arguments().
    Platform options after a colon (\c{xcb:foo}) are ignored. There are no platform plugins:
    the platforms are built in. \c qApp is not redefined by Nt GUI; use \c qGuiApp.
    \enddifferences

    \sa QCoreApplication, QWindow, QScreen
*/

/*!
    Constructs a GUI application from \a argc and \a argv, as QCoreApplication
    does, and connects to the window system. The program aborts if the platform
    cannot be started.
*/
QGuiApplication::QGuiApplication(int &argc, char **argv) : QCoreApplication(argc, argv) {
    g_state = new GuiState;
    g_app = this;
    QString name = platformFromArguments(arguments());
    if (name.isEmpty()) {
        const char *env = std::getenv("QT_QPA_PLATFORM");
        if (env)
            name = QString::fromLocal8Bit(env);
    }
    if (name.isEmpty())
        name = chooseDefaultPlatform();
    const qsizetype colon = name.indexOf(QChar(':'));
    if (colon >= 0)
        name = name.left(colon);
    g_state->platformName = name.toLower();
    g_state->styleHints = new QStyleHints;
    g_state->integration.reset(QtPrivate::createPlatformIntegration(g_state->platformName, argc, argv));
    if (!g_state->integration) {
        std::fprintf(stderr, "Nt GUI: could not start the platform \"%s\". Available platforms: %s\n",
                     name.toUtf8().constData(),
                     QtPrivate::availablePlatforms().join(QString::fromLatin1(", ")).toUtf8().constData());
        std::abort();
    }
    g_state->integration->initialize();
    QtPrivate::GuiAccess::setPlatformColorScheme(g_state->integration->colorScheme());
}

/*!
    Destroys the application object: the platform windows are released and the
    connection to the window system is closed.
*/
QGuiApplication::~QGuiApplication() {
    for (QWindow *w : g_state->windows)
        w->destroy();
    QtPrivate::GuiAccess::shutdown();
    g_app = nullptr;
}

/*!
    Returns the application object, or \c nullptr if there is none or it is
    not a QGuiApplication.
*/
QGuiApplication *QGuiApplication::instance() { return g_app; }

/*!
    Returns the style hints of the application, such as its color scheme. They exist as long as the
    application object does.
*/
QStyleHints *QGuiApplication::styleHints() { return g_state ? g_state->styleHints : nullptr; }

/*!
    Returns the name of the platform, such as \c{"xcb"} or \c{"offscreen"}.
*/
QString QGuiApplication::platformName() { return g_state ? g_state->platformName : QString(); }

/*!
    Returns the primary screen, or \c nullptr if there are none.

    \sa screens()
*/
QScreen *QGuiApplication::primaryScreen() {
    return g_state && !g_state->screens.isEmpty() ? g_state->screens.first() : nullptr;
}

/*!
    Returns all screens; the primary screen comes first.
*/
QList<QScreen *> QGuiApplication::screens() { return g_state ? g_state->screens : QList<QScreen *>(); }

/*!
    Returns all windows of the application, visible or not.

    \sa topLevelWindows()
*/
QList<QWindow *> QGuiApplication::allWindows() { return g_state ? g_state->windows : QList<QWindow *>(); }

/*!
    Returns the top-level windows of the application. Every window is a
    top-level window, so this is the same as allWindows().
*/
QList<QWindow *> QGuiApplication::topLevelWindows() { return allWindows(); }

/*!
    Returns the window that has the keyboard focus, or \c nullptr.
*/
QWindow *QGuiApplication::focusWindow() { return g_state ? g_state->focus : nullptr; }

/*!
    Returns the modifier keys that were down at the last key or mouse event.
*/
Qt::KeyboardModifiers QGuiApplication::keyboardModifiers() {
    return g_state ? g_state->modifiers : Qt::KeyboardModifiers();
}

/*!
    Returns the mouse buttons that were down at the last mouse event.
*/
Qt::MouseButtons QGuiApplication::mouseButtons() { return g_state ? g_state->buttons : Qt::MouseButtons(); }

/*!
    Returns the default font of the application: the font that QFont() constructs. It is
    \c{"Sans Serif"} at 10 points until setFont() changes it.

    \sa setFont()
*/
QFont QGuiApplication::font() { return QtPrivate::applicationFont(); }

/*!
    Sets the default font of the application to \a font. It applies to fonts constructed afterwards.

    \sa font()
*/
void QGuiApplication::setFont(const QFont &font) { QtPrivate::setApplicationFont(font); }

/*!
    Sets whether the application quits when its last visible window closes to
    \a quit. The default is \c true.

    \sa quitOnLastWindowClosed(), lastWindowClosed()
*/
void QGuiApplication::setQuitOnLastWindowClosed(bool quit) {
    if (g_state)
        g_state->quitOnLastWindowClosed = quit;
}

/*!
    Returns whether the application quits when its last visible window closes.
    The default is \c true.

    \sa setQuitOnLastWindowClosed()
*/
bool QGuiApplication::quitOnLastWindowClosed() { return g_state && g_state->quitOnLastWindowClosed; }

namespace {
void applyCursorToAllWindows() {
    for (QWindow *window : g_state->windows)
        QtPrivate::GuiAccess::applyCursor(window);
}
} // namespace

/*!
    Sets the cursor of the whole application to \a cursor: it shows over
    every window, whatever cursors the windows and their widgets have,
    until restoreOverrideCursor() takes it away again. Override cursors
    stack: each call needs a restoreOverrideCursor() of its own, and the
    cursor of the last call is the one that shows.

    \code
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    compute();
    QGuiApplication::restoreOverrideCursor();
    \endcode

    \sa restoreOverrideCursor(), changeOverrideCursor(), overrideCursor()
*/
void QGuiApplication::setOverrideCursor(const QCursor &cursor) {
    if (!g_state)
        return;
    g_state->overrideCursors.append(cursor);
    applyCursorToAllWindows();
}

/*!
    Replaces the override cursor that shows with \a cursor. Nothing happens
    if there is no override cursor.

    \sa setOverrideCursor()
*/
void QGuiApplication::changeOverrideCursor(const QCursor &cursor) {
    if (!g_state || g_state->overrideCursors.isEmpty())
        return;
    g_state->overrideCursors.last() = cursor;
    applyCursorToAllWindows();
}

/*!
    Takes away the override cursor that was set last. The one before it
    shows again, or, after the last one, the cursors of the windows.
    Nothing happens if there is no override cursor.

    \sa setOverrideCursor()
*/
void QGuiApplication::restoreOverrideCursor() {
    if (!g_state || g_state->overrideCursors.isEmpty())
        return;
    g_state->overrideCursors.removeLast();
    applyCursorToAllWindows();
}

/*!
    Returns the override cursor that shows, or \c nullptr if there is
    none. The pointer is valid until the override cursors change.

    \sa setOverrideCursor()
*/
QCursor *QGuiApplication::overrideCursor() {
    return g_state && !g_state->overrideCursors.isEmpty() ? &g_state->overrideCursors.last() : nullptr;
}

namespace QtPrivate {

QPlatformIntegration *GuiAccess::integration() { return g_state ? g_state->integration.get() : nullptr; }

void GuiAccess::registerWindow(QWindow *window) { g_state->windows.append(window); }

void GuiAccess::unregisterWindow(QWindow *window) {
    if (!g_state)
        return;
    g_state->windows.removeAll(window);
    if (g_state->focus == window)
        g_state->focus = nullptr;
}

void GuiAccess::maybeQuitOnLastWindowClosed(QWindow *closed) {
    if (!g_state || !g_app)
        return;
    // Only the windows that keep the application running count: a tool window that is left open does not
    // keep it alive, and closing one does not end it.
    if (closed && !quitOnClose(closed))
        return;
    for (QWindow *w : g_state->windows) {
        if (w->isVisible() && quitOnClose(w))
            return;
    }
    emit g_app->lastWindowClosed();
    if (g_state->quitOnLastWindowClosed)
        QCoreApplication::quit();
}

bool GuiAccess::inputWindowActivated(QWindow *window) {
    if (!g_state || g_state->focus == window)
        return false;
    QWindow *old = g_state->focus;
    if (old) {
        QFocusEvent out(QEvent::FocusOut);
        QCoreApplication::sendEvent(old, &out);
        setActive(old, false);
    }
    g_state->focus = window;
    if (window) {
        setActive(window, true);
        QFocusEvent in(QEvent::FocusIn);
        QCoreApplication::sendEvent(window, &in);
    }
    emit g_app->focusWindowChanged(window);
    return true;
}

void GuiAccess::addScreen(QPlatformScreen *screen) {
    auto *s = new QScreen(screen);
    g_state->screens.append(s);
    if (g_app)
        emit g_app->screenAdded(s);
}

void GuiAccess::removeScreen(QPlatformScreen *screen) {
    QScreen *s = screen->screen();
    if (!s || !g_state)
        return;
    if (g_app)
        emit g_app->screenRemoved(s);
    g_state->screens.removeAll(s);
    delete s;
}

void GuiAccess::screenGeometryChanged(QPlatformScreen *screen) {
    if (QScreen *s = screen->screen())
        emit s->geometryChanged(screen->geometry());
}

void GuiAccess::setPlatformColorScheme(Qt::ColorScheme scheme) {
    if (g_state && g_state->styleHints)
        g_state->styleHints->setPlatformColorScheme(scheme);
}

void GuiAccess::setFocusWindow(QWindow *window) { g_state->focus = window; }

void GuiAccess::setInput(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers) {
    g_state->buttons = buttons;
    g_state->modifiers = modifiers;
}

void GuiAccess::setCursorPosition(const QPoint &pos) {
    if (g_state)
        g_state->cursorPosition = pos;
}

QPoint GuiAccess::cursorPosition() { return g_state ? g_state->cursorPosition : QPoint(); }

void GuiAccess::applyCursor(QWindow *window) {
    QPlatformWindow *platform = platformWindow(window);
    if (!platform || !g_state)
        return;
    platform->setCursor(g_state->overrideCursors.isEmpty() ? window->cursor().shape()
                                                           : g_state->overrideCursors.last().shape());
}

void GuiAccess::shutdown() {
    if (!g_state)
        return;
    const QList<QScreen *> screens = g_state->screens;
    for (QScreen *s : screens)
        removeScreen(s->m_platform);
    g_state->integration.reset();
    delete g_state->styleHints;
    delete g_state;
    g_state = nullptr;
}

} // namespace QtPrivate

#include "moc_qguiapplication.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
