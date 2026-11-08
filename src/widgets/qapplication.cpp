// MIT License - see LICENSE
#include <QtWidgets/qapplication.h>

#include "qfusionstyle_p.h"
#include "qwidget_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtGui/qevent.h>
#include <QtGui/qstylehints.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qwidget.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace {

QApplication *g_app = nullptr;
std::unique_ptr<QStyle> g_style;
QPalette *g_palette = nullptr;
std::vector<QWidget *> g_topLevels;

// Tells the widgets that use the application palette that it changed.
void sendPaletteChange() {
    for (QWidget *w : QApplication::allWidgets()) {
        if (!QtPrivate::WidgetAccess::data(w).paletteSet) {
            QEvent change(QEvent::PaletteChange);
            QCoreApplication::sendEvent(w, &change);
        }
    }
}

} // namespace

namespace QtPrivate {

void WidgetAccess::registerTopLevel(QWidget *w) { g_topLevels.push_back(w); }

void WidgetAccess::unregisterTopLevel(QWidget *w) {
    g_topLevels.erase(std::remove(g_topLevels.begin(), g_topLevels.end(), w), g_topLevels.end());
}

} // namespace QtPrivate

/*!
    \module NtWidgets
    \title Nt Widgets C++ Classes
    \brief Widgets, layouts and styles for desktop user interfaces.
    \cmaketarget Nt::Widgets

    Nt Widgets builds user interfaces from widgets: QWidget and its subclasses such as QLabel, QPushButton and
    QCheckBox, arranged by layouts (QVBoxLayout, QHBoxLayout) and drawn by a QStyle. Create a QApplication,
    then a window, add widgets to it, and show it. See \l{Using Nt Widgets}.

    Link with \c Nt::Widgets and include \c{<QApplication>} and the widget headers you use.
*/

/*!
    \class QApplication
    \brief The QApplication class manages the GUI application's control flow and main settings.

    QApplication is a QGuiApplication that also knows about widgets: it owns the style and the default
    palette, and lists the top-level widgets. Create it before any widget:

    \code
    int main(int argc, char *argv[])
    {
        QApplication app(argc, argv);
        QPushButton button("Quit");
        QObject::connect(&button, &QPushButton::clicked, &app, &QApplication::quit);
        button.show();
        return app.exec();
    }
    \endcode

    The application quits when the last window is closed, as QGuiApplication does.

    \differences
    There are no \c setStyleSheet(), \c setActiveWindow(), \c setFont() for classes, \c beep(), \c alert(),
    \c setWindowIcon(), cursor overrides, double click interval, \c focusChanged() signal or style factory.
    \enddifferences

    \sa QGuiApplication, QWidget
*/

/*!
    Constructs a widget application from \a argc and \a argv, as QGuiApplication does.
*/
QApplication::QApplication(int &argc, char **argv) : QGuiApplication(argc, argv) {
    g_app = this;
    // The default palette follows the color scheme, unless the application set its own.
    QObject::connect(styleHints(), &QStyleHints::colorSchemeChanged, this, [] {
        if (!g_palette)
            sendPaletteChange();
    });
}

/*!
    Destroys the application object and the style.
*/
QApplication::~QApplication() {
    g_app = nullptr;
    g_style.reset();
    delete g_palette;
    g_palette = nullptr;
}

/*!
    Returns the application object, or \c nullptr if there is none or it is not a QApplication.
*/
QApplication *QApplication::instance() { return g_app; }

/*!
    Returns the style of the application, which draws every widget that has no style of its own. It is
    the default style until setStyle() changes it.

    \sa setStyle(), QWidget::style()
*/
QStyle *QApplication::style() {
    if (!g_style)
        g_style = std::make_unique<QFusionStyle>();
    return g_style.get();
}

/*!
    Makes \a style the style of the application, which takes ownership of it. A \c nullptr restores the default
    style. Widgets are drawn again.

    \sa style()
*/
void QApplication::setStyle(QStyle *style) {
    if (style && style == g_style.get())
        return;
    if (style)
        g_style.reset(style);
    else
        g_style = std::make_unique<QFusionStyle>();
    for (QWidget *w : allWidgets()) {
        QEvent change(QEvent::StyleChange);
        QCoreApplication::sendEvent(w, &change);
    }
    if (!g_palette)
        sendPaletteChange();
}

/*!
    Returns the default palette, which widgets use unless they or an ancestor have their own. Unless
    setPalette() set one, it is the standardPalette() of the style, which follows the color scheme of the
    desktop: widgets turn dark when the desktop does.

    \sa setPalette(), QStyle::standardPalette(), QStyleHints::colorScheme()
*/
QPalette QApplication::palette() { return g_palette ? *g_palette : style()->standardPalette(); }

/*!
    Sets the default palette to \a palette. Widgets that have no palette of their own are painted again.
    Passing a default-constructed QPalette restores the standard palette of the style, which follows the
    color scheme of the desktop again.

    \sa palette(), QStyle::standardPalette()
*/
void QApplication::setPalette(const QPalette &palette) {
    delete g_palette;
    g_palette = palette == QPalette() ? nullptr : new QPalette(palette);
    sendPaletteChange();
}

/*!
    Returns the widgets that have no parent, windows, whether they are visible or not.

    \sa allWidgets()
*/
QList<QWidget *> QApplication::topLevelWidgets() {
    QList<QWidget *> list;
    for (QWidget *w : g_topLevels)
        list.append(w);
    return list;
}

/*!
    Returns all widgets of the application, windows and their descendants.

    \sa topLevelWidgets()
*/
QList<QWidget *> QApplication::allWidgets() {
    std::vector<QWidget *> all(g_topLevels.begin(), g_topLevels.end());
    for (std::size_t i = 0; i < all.size(); ++i)
        for (QWidget *c : QtPrivate::WidgetAccess::childWidgets(all[i]))
            all.push_back(c);
    QList<QWidget *> list;
    for (QWidget *w : all)
        list.append(w);
    return list;
}

/*!
    Returns the top-level widget whose window is the active window, or \c nullptr.

    \sa focusWidget()
*/
QWidget *QApplication::activeWindow() {
    for (QWidget *w : g_topLevels)
        if (w->isActiveWindow())
            return w;
    return nullptr;
}

/*!
    Returns the widget that has the keyboard focus: the focus widget of the active window, or \c nullptr.

    \sa activeWindow()
*/
QWidget *QApplication::focusWidget() {
    QWidget *active = activeWindow();
    return active ? active->focusWidget() : nullptr;
}

/*!
    Returns the visible widget at \a point, in screen coordinates, or \c nullptr if there is none.
*/
QWidget *QApplication::widgetAt(const QPoint &point) {
    for (auto it = g_topLevels.rbegin(); it != g_topLevels.rend(); ++it) {
        QWidget *top = *it;
        if (top->isVisible() && top->geometry().contains(point))
            return QtPrivate::WidgetAccess::widgetAt(top, point - top->pos());
    }
    return nullptr;
}

/*!
    Closes all top-level windows. The application quits if the last one closed and
    QGuiApplication::quitOnLastWindowClosed() is \c true.

    \sa QWidget::close()
*/
void QApplication::closeAllWindows() {
    const std::vector<QWidget *> windows = g_topLevels;
    for (QWidget *w : windows)
        if (w->isVisible())
            w->close();
}

#include "moc_qapplication.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
