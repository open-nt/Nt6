// MIT License - see LICENSE
#pragma once

#include <QtCore/qcoreapplication.h>
#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qstring.h>
#include <QtGui/qfont.h>

class QScreen;
class QStyleHints;
class QWindow;

class QGuiApplication : public QCoreApplication {
    Q_OBJECT
    Q_PROPERTY(bool quitOnLastWindowClosed READ quitOnLastWindowClosed WRITE setQuitOnLastWindowClosed)
public:
    QGuiApplication(int &argc, char **argv);
    ~QGuiApplication() override;

    static QGuiApplication *instance();
    static QString platformName();
    static QScreen *primaryScreen();
    static QList<QScreen *> screens();
    static QList<QWindow *> allWindows();
    static QList<QWindow *> topLevelWindows();
    static QWindow *focusWindow();
    static Qt::KeyboardModifiers keyboardModifiers();
    static Qt::MouseButtons mouseButtons();
    static QStyleHints *styleHints();
    static QFont font();
    static void setFont(const QFont &font);
    static void setQuitOnLastWindowClosed(bool quit);
    static bool quitOnLastWindowClosed();

signals:
    /*!
        This signal is emitted from the application when the last visible
        window is closed, before the application quits if
        quitOnLastWindowClosed() is \c true.
    */
    void lastWindowClosed() { emitSignal(&QGuiApplication::lastWindowClosed); }
    /*!
        This signal is emitted when \a screen is added.
    */
    void screenAdded(QScreen *screen) { emitSignal(&QGuiApplication::screenAdded, screen); }
    /*!
        This signal is emitted when \a screen is removed.
    */
    void screenRemoved(QScreen *screen) { emitSignal(&QGuiApplication::screenRemoved, screen); }
    /*!
        This signal is emitted when the window that has the keyboard focus
        changes to \a focusWindow.
    */
    void focusWindowChanged(QWindow *focusWindow) { emitSignal(&QGuiApplication::focusWindowChanged, focusWindow); }
};

#define qGuiApp QGuiApplication::instance()
