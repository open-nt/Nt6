// MIT License - see LICENSE
#pragma once

#include <QtGui/qtestsupport_gui.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qwidget.h>

namespace QTest {

/*!
    Waits until the window of \a widget is exposed (see QWindow::isExposed()), processing events meanwhile, for at
    most \a timeout milliseconds. Returns \c true if it became exposed in time, and \c false at once if \a widget is
    null or has no window yet (it was never shown).

    \since 6.0
    \sa qWaitForWindowActive()
*/
inline bool qWaitForWindowExposed(QWidget *widget, int timeout = 5000) {
    QWindow *window = widget ? widget->window()->windowHandle() : nullptr;
    return window && qWaitForWindowExposed(window, timeout);
}

/*!
    Waits until the window of \a widget is the active window (see QWindow::isActive()), processing events meanwhile,
    for at most \a timeout milliseconds. Returns \c true if it became active in time, and \c false at once if
    \a widget is null or has no window yet.

    \since 6.0
    \sa qWaitForWindowExposed()
*/
inline bool qWaitForWindowActive(QWidget *widget, int timeout = 5000) {
    QWindow *window = widget ? widget->window()->windowHandle() : nullptr;
    return window && qWaitForWindowActive(window, timeout);
}

} // namespace QTest
