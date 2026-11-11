// MIT License - see LICENSE
#include <QtGui/qcursor.h>

#include <QtCore/qdebug.h>
#include <QtGui/qscreen.h>

#include "qpa/qplatformintegration_p.h"
#include "qwindow_p.h"

/*!
    \class QCursor
    \brief The QCursor class provides a mouse cursor with one of the standard shapes.

    A QCursor says what the mouse cursor looks like. Give one to
    QWidget::setCursor() or QWindow::setCursor() for the cursor over a
    widget or a window, or to QGuiApplication::setOverrideCursor() for the
    whole application, while it is busy for example:

    \code
    label->setCursor(Qt::PointingHandCursor);

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    loadEverything();
    QGuiApplication::restoreOverrideCursor();
    \endcode

    The static functions pos() and setPos() read and move the cursor of
    the screen.

    \differences
    A cursor has one of the shapes of Qt::CursorShape: there are no cursors
    made of a pixmap or a bitmap, and so no \c pixmap(), \c bitmap(),
    \c mask() or \c hotSpot(). On X11 the shapes are those of the cursor
    font of the X server, not those of the cursor theme of the desktop;
    shapes that the font does not have are shown as the nearest one that it
    has. With the \c offscreen platform, pos() is where setPos() or the
    last mouse event put the cursor.
    \enddifferences

    \sa QWidget::setCursor(), QGuiApplication::setOverrideCursor()
*/

/*!
    \fn QCursor::QCursor()

    Constructs a cursor with the shape of the standard arrow.
*/

/*!
    Constructs a cursor with the shape \a shape.
*/
QCursor::QCursor(Qt::CursorShape shape) : m_shape(shape) {}

/*!
    \fn Qt::CursorShape QCursor::shape() const

    Returns the shape of the cursor.

    \sa setShape()
*/

/*!
    Sets the shape of the cursor to \a newShape.

    \sa shape()
*/
void QCursor::setShape(Qt::CursorShape newShape) { m_shape = newShape; }

/*!
    Swaps this cursor with \a other.
*/
void QCursor::swap(QCursor &other) noexcept { std::swap(m_shape, other.m_shape); }

/*!
    Returns the position of the mouse cursor, in the coordinates of the
    desktop.

    \sa setPos(), QWidget::mapFromGlobal()
*/
QPoint QCursor::pos() {
    QPlatformIntegration *integration = QtPrivate::GuiAccess::integration();
    return integration ? integration->cursorPosition() : QPoint();
}

/*!
    \overload

    Returns the position of the mouse cursor. Nt has one desktop, so
    \a screen makes no difference.
*/
QPoint QCursor::pos(const QScreen *screen) {
    static_cast<void>(screen);
    return pos();
}

/*!
    Moves the mouse cursor to the position (\a x, \a y) of the desktop.
    Moving the cursor away from where the user put it is rarely welcome.

    \sa pos()
*/
void QCursor::setPos(int x, int y) {
    if (QPlatformIntegration *integration = QtPrivate::GuiAccess::integration())
        integration->setCursorPosition(QPoint(x, y));
}

/*!
    \fn void QCursor::setPos(const QPoint &p)
    \overload

    Moves the mouse cursor to the position \a p of the desktop.
*/

/*!
    \overload

    Moves the mouse cursor to (\a x, \a y). Nt has one desktop, so
    \a screen makes no difference.
*/
void QCursor::setPos(QScreen *screen, int x, int y) {
    static_cast<void>(screen);
    setPos(x, y);
}

/*!
    \fn void QCursor::setPos(QScreen *screen, const QPoint &p)
    \overload

    Moves the mouse cursor to \a p; \a screen makes no difference.
*/

QDebug operator<<(QDebug dbg, const QCursor &cursor) {
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "QCursor(Qt::CursorShape(" << static_cast<int>(cursor.shape()) << "))";
    return dbg;
}
