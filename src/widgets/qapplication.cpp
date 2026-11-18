// MIT License - see LICENSE
#include <QtWidgets/qstylefactory.h>

#include "gui/qshortcut_p.h"
#include <QtWidgets/qapplication.h>

#include "qfusionstyle_p.h"
#include "qwidget_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtGui/qevent.h>
#include <QtGui/qstylehints.h>
#include <QtWidgets/qmessagebox.h>
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
std::vector<QWidget *> g_modals; // the modal windows that are shown, in the order they were shown

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
    modalHidden(w);
}

void WidgetAccess::modalShown(QWidget *top) {
    modalHidden(top);
    g_modals.push_back(top);
}

void WidgetAccess::modalHidden(QWidget *top) {
    g_modals.erase(std::remove(g_modals.begin(), g_modals.end(), top), g_modals.end());
}

bool WidgetAccess::blockedByModal(const QWidget *top) {
    // A window is not blocked by a modal window that it belongs to (a dialog of the dialog), nor by one shown before
    // a modal window that it belongs to.
    const auto belongsTo = [](const QWidget *w, const QWidget *owner) {
        for (; w; w = w->parentWidget() ? w->parentWidget()->window() : nullptr)
            if (w == owner)
                return true;
        return false;
    };
    for (auto it = g_modals.rbegin(); it != g_modals.rend(); ++it) {
        const QWidget *modal = *it;
        if (belongsTo(top, modal))
            return false;
        if (modal->windowModality() == Qt::ApplicationModal)
            return true;
        // A window-modal window blocks the windows that it belongs to.
        if (belongsTo(modal, top))
            return true;
    }
    return false;
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
    The style sheet of setStyleSheet() is kept but does not change how widgets look, the effects of
    setEffectEnabled() and the navigation mode are only kept, and beep() makes no sound. There are no
    \c setActiveWindow() and \c setFont() for classes.
    \enddifferences

    \sa QGuiApplication, QWidget
*/

namespace {

// When a QShortcut of a widget is active: the widget is shown, enabled and not blocked by a modal window, and the
// context relates it to the window the key was pressed in.
bool widgetShortcutMatcher(QObject *parent, Qt::ShortcutContext context, QWindow *window) {
    QWidget *widget = nullptr;
    for (QObject *o = parent; o && !widget; o = o->parent())
        widget = qobject_cast<QWidget *>(o);
    if (!widget) {
        if (context == Qt::ApplicationShortcut)
            return true;
        for (QObject *o = parent; o; o = o->parent()) {
            if (auto *w = qobject_cast<QWindow *>(o))
                return w == window;
        }
        return false;
    }
    if (!widget->isVisible() || !widget->isEnabled())
        return false;
    QWidget *top = widget->window();
    if (QtPrivate::WidgetAccess::blockedByModal(top))
        return false;
    switch (context) {
    case Qt::ApplicationShortcut:
        return true;
    case Qt::WindowShortcut:
        return top->windowHandle() == window;
    case Qt::WidgetShortcut:
        return top->windowHandle() == window && QApplication::focusWidget() == widget;
    case Qt::WidgetWithChildrenShortcut:
        if (top->windowHandle() != window)
            return false;
        for (QWidget *f = QApplication::focusWidget(); f; f = f->parentWidget()) {
            if (f == widget)
                return true;
        }
        return false;
    }
    return false;
}

} // namespace

/*!
    Constructs a widget application from \a argc and \a argv, as QGuiApplication does.
*/
QApplication::QApplication(int &argc, char **argv) : QGuiApplication(argc, argv) {
    g_app = this;
    QtPrivate::setShortcutContextMatcher(widgetShortcutMatcher);
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
    QtPrivate::setShortcutContextMatcher(nullptr);
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
    if (!g_style) {
        g_style = std::make_unique<QFusionStyle>();
        g_style->polish(qobject_cast<QApplication *>(QCoreApplication::instance()));
    }
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
    // The widgets that the old style polished are unpolished by it, and polished by the new one.
    const QList<QWidget *> widgets = allWidgets();
    const auto usesApplicationStyle = [](QWidget *w) { return w->style() == g_style.get(); };
    if (g_style) {
        for (QWidget *w : widgets) {
            if (QtPrivate::WidgetAccess::data(w).polished && usesApplicationStyle(w))
                g_style->unpolish(w);
        }
        g_style->unpolish(qobject_cast<QApplication *>(QCoreApplication::instance()));
    }
    if (style)
        g_style.reset(style);
    else
        g_style = std::make_unique<QFusionStyle>();
    g_style->polish(qobject_cast<QApplication *>(QCoreApplication::instance()));
    for (QWidget *w : widgets) {
        if (QtPrivate::WidgetAccess::data(w).polished && usesApplicationStyle(w))
            g_style->polish(w);
    }
    for (QWidget *w : allWidgets()) {
        QEvent change(QEvent::StyleChange);
        QCoreApplication::sendEvent(w, &change);
    }
    if (!g_palette)
        sendPaletteChange();
}

/*!
    \overload

    Makes the style called \a style the style of the application, and
    returns it; QStyleFactory::keys() lists the names. Returns \c nullptr,
    and leaves the style as it is, if there is no style of that name.

    \sa QStyleFactory::create()
*/
QStyle *QApplication::setStyle(const QString &style) {
    QStyle *created = QStyleFactory::create(style);
    if (created)
        setStyle(created);
    return created;
}

/*!
    Returns the default palette, which widgets use unless they or an ancestor have their own. Unless
    setPalette() set one, it is the standardPalette() of the style, which follows the color scheme of the
    desktop: widgets turn dark when the desktop does.

    \sa setPalette(), QStyle::standardPalette(), QStyleHints::colorScheme()
*/
QPalette QApplication::palette() {
    if (g_palette)
        return *g_palette;
    QPalette standard = style()->standardPalette();
    style()->polish(standard);
    return standard;
}

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
    Returns the modal window that was shown last and is still shown, or \c nullptr if no modal window is shown.

    \sa QWidget::setWindowModality()
*/
QWidget *QApplication::activeModalWidget() { return g_modals.empty() ? nullptr : g_modals.back(); }

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
    \since 6.0

    Returns how many pixels the mouse must move with a button held before a drag starts.

    \sa QStyleHints::startDragDistance()
*/
int QApplication::startDragDistance() { return styleHints()->startDragDistance(); }

/*!
    \since 6.0

    Sets how many pixels the mouse must move before a drag starts to \a l.
*/
void QApplication::setStartDragDistance(int l) { styleHints()->setStartDragDistance(l); }

/*!
    \since 6.0

    Returns how many milliseconds a button must be held before a drag starts without moving.
*/
int QApplication::startDragTime() { return styleHints()->startDragTime(); }

/*!
    \since 6.0

    Sets how long a button must be held before a drag starts to \a ms milliseconds.
*/
void QApplication::setStartDragTime(int ms) { styleHints()->setStartDragTime(ms); }

/*!
    \since 6.0

    Asks for the user's attention to the window of \a widget, for \a msec milliseconds or until it is activated if
    \a msec is 0. Nt has no window system call for it yet, so nothing visible happens; the window is not raised.
*/
void QApplication::alert(QWidget *widget, int msec) {
    (void)widget;
    (void)msec;
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

namespace {
std::vector<bool> &effects() {
    static auto *list = new std::vector<bool>(7, false);
    return *list;
}
Qt::NavigationMode g_navigationMode = Qt::NavigationModeNone;
} // namespace

/*!
    \since 6.0

    Returns how many milliseconds a blink of the text cursor takes; see QStyleHints::cursorFlashTime().
*/
int QApplication::cursorFlashTime() { return styleHints()->cursorFlashTime(); }

/*!
    \since 6.0

    Sets how long a blink of the text cursor takes to \a msecs milliseconds.
*/
void QApplication::setCursorFlashTime(int msecs) { styleHints()->setCursorFlashTime(msecs); }

/*!
    \since 6.0

    Returns how many milliseconds may pass between the clicks of a double click.
*/
int QApplication::doubleClickInterval() { return styleHints()->mouseDoubleClickInterval(); }

/*!
    \since 6.0

    Sets how long may pass between the clicks of a double click to \a ms milliseconds.
*/
void QApplication::setDoubleClickInterval(int ms) { styleHints()->setMouseDoubleClickInterval(ms); }

/*!
    \since 6.0

    Returns how many milliseconds may pass between the key presses of one keyboard search.
*/
int QApplication::keyboardInputInterval() { return styleHints()->keyboardInputInterval(); }

/*!
    \since 6.0

    Sets how long may pass between the key presses of one keyboard search to \a ms milliseconds.
*/
void QApplication::setKeyboardInputInterval(int ms) { styleHints()->setKeyboardInputInterval(ms); }

/*!
    \since 6.0

    Returns how many lines a notch of the mouse wheel scrolls.
*/
int QApplication::wheelScrollLines() { return styleHints()->wheelScrollLines(); }

/*!
    \since 6.0

    Sets how many lines a notch of the mouse wheel scrolls to \a lines.
*/
void QApplication::setWheelScrollLines(int lines) { styleHints()->setWheelScrollLines(lines); }

/*!
    \since 6.0

    Returns \c true if \a effect is enabled. All effects are off by default.
*/
bool QApplication::isEffectEnabled(Qt::UIEffect effect) {
    const auto index = static_cast<std::size_t>(effect);
    return index < effects().size() && (effects()[0] || effects()[index]);
}

/*!
    \since 6.0

    Enables \a effect if \a enable is \c true, and disables it otherwise. Nt keeps the value; menus and tool tips
    are not animated.
*/
void QApplication::setEffectEnabled(Qt::UIEffect effect, bool enable) {
    const auto index = static_cast<std::size_t>(effect);
    if (index < effects().size())
        effects()[index] = enable;
}

/*!
    \since 6.0

    Returns how the keys move the focus, Qt::NavigationModeNone by default.
*/
Qt::NavigationMode QApplication::navigationMode() { return g_navigationMode; }

/*!
    \since 6.0

    Sets how the keys move the focus to \a mode. Nt keeps the value; Tab moves the focus whatever it is.
*/
void QApplication::setNavigationMode(Qt::NavigationMode mode) { g_navigationMode = mode; }

/*!
    \since 6.0

    Returns the popup, such as an open menu, of the active window, or else of another window, or \c nullptr if
    no popup is open.
*/
QWidget *QApplication::activePopupWidget() {
    const auto popupOf = [](QWidget *top) -> QWidget * {
        const auto &window = QtPrivate::WidgetAccess::data(top).window;
        QWidget *popup = window ? window->popup() : nullptr;
        return popup && popup->isVisible() ? popup : nullptr;
    };
    if (QWidget *active = activeWindow())
        if (QWidget *popup = popupOf(active))
            return popup;
    for (QWidget *top : topLevelWidgets())
        if (QWidget *popup = popupOf(top))
            return popup;
    return nullptr;
}

/*!
    \since 6.0

    Returns the visible window at \a point on the screen, or \c nullptr. Of windows that overlap there, the one
    shown last is returned.
*/
QWidget *QApplication::topLevelAt(const QPoint &point) {
    const QList<QWidget *> tops = topLevelWidgets();
    for (auto it = tops.rbegin(); it != tops.rend(); ++it)
        if ((*it)->isVisible() && (*it)->geometry().contains(point))
            return *it;
    return nullptr;
}

/*!
    \since 6.0

    Sounds the bell of the system. Nt has no sound, so this does nothing.
*/
void QApplication::beep() {}

/*!
    \since 6.0

    Shows a message box about Nt; see QMessageBox::aboutQt().
*/
void QApplication::aboutQt() { QMessageBox::aboutQt(activeWindow()); }

/*!
    \since 6.0

    Sets the style sheet of the application to \a sheet. Nt keeps it, as QWidget::setStyleSheet() does, and sends
    a QEvent::StyleChange to the windows; widgets do not change how they look.
*/
void QApplication::setStyleSheet(const QString &sheet) {
    if (m_styleSheet == sheet)
        return;
    m_styleSheet = sheet;
    for (QWidget *top : topLevelWidgets()) {
        QEvent change(QEvent::StyleChange);
        QCoreApplication::sendEvent(top, &change);
    }
}

/*!
    \fn QString QApplication::styleSheet() const
    \since 6.0

    Returns the style sheet of the application.
*/

/*!
    \fn bool QApplication::autoSipEnabled() const
    \since 6.0

    Returns whether a software input panel opens for an editor that gets the focus. Nt keeps the value.
*/

/*!
    \fn void QApplication::setAutoSipEnabled(const bool enabled)
    \since 6.0

    Sets whether a software input panel opens for an editor that gets the focus to \a enabled.
*/

/*!
    \fn QWidget *QApplication::topLevelAt(int x, int y)
    \since 6.0
    \overload

    Returns the visible window at (\a x, \a y) on the screen.
*/

#include "moc_qapplication.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
