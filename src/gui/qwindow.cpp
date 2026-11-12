// MIT License - see LICENSE
#include <QtGui/qwindow.h>

#include "qpa/qplatformintegration_p.h"
#include "qpa/qplatformwindow_p.h"
#include "qwindow_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtCore/qpointer.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qscreen.h>

namespace {
constexpr int kDefaultSize = 160;
}

struct QWindow::Data {
    QScreen *screen = nullptr;
    QPlatformWindow *platform = nullptr;
    QString title;
    QPointer<QWindow> transientParent;
    QRect geometry;
    bool visible = false;
    bool exposed = false;
    bool active = false;
    bool updateRequested = false;
    QCursor cursor;
    bool quitOnClose = true;
    QSurface::SurfaceType surfaceType = QSurface::RasterSurface;
    QSurfaceFormat requestedFormat = QSurfaceFormat::defaultFormat();
    QSurfaceFormat format; // what the window has, once an OpenGL context was made current on it
    bool hasFormat = false;
};

/*!
    \class QWindow
    \brief The QWindow class represents a window in the underlying windowing system.

    A window is a rectangle on a screen that the windowing system manages. It
    receives events, such as mouse and key events, and shows what an application
    paints into it. QWindow itself paints nothing: subclass QRasterWindow, or
    draw with a QBackingStore.

    \code
    QWindow window;
    window.setTitle("Hello");
    window.resize(320, 240);
    window.show();
    \endcode

    Handle input by reimplementing the event functions:

    \code
    class MyWindow : public QWindow {
    protected:
        void mousePressEvent(QMouseEvent *event) override { qDebug() << event->pos(); }
    };
    \endcode

    \differences
    Only top-level windows exist: there is no parent window, no window flags
    or modality, no \c setMinimumSize(), cursor, icon or opacity, no window
    states and no Vulkan, Metal or Direct3D surface. OpenGL surfaces work
    through EGL; see QOpenGLContext. \c close() destroys the platform
    window; showing the window again creates it anew. There is no
    \c requestUpdate() throttling to the display refresh rate.
    \enddifferences

    \sa QRasterWindow, QBackingStore, QScreen, QGuiApplication
*/

/*!
    Constructs a top-level window on \a screen, or on the primary screen if
    \a screen is \c nullptr. A QGuiApplication must exist. The window is not
    visible.
*/
QWindow::QWindow(QScreen *screen) : QSurface(QSurface::Window), m_d(std::make_unique<Data>()) {
    if (!QGuiApplication::instance())
        qFatal("QWindow: a QGuiApplication must be created before a window");
    m_d->screen = screen ? screen : QGuiApplication::primaryScreen();
    QtPrivate::GuiAccess::registerWindow(this);
}

/*!
    Destroys the window, which also hides it.
*/
QWindow::~QWindow() {
    destroy();
    QtPrivate::GuiAccess::unregisterWindow(this);
}

/*!
    Returns the identifier the windowing system uses for the window, or 0 if it
    has not been created.

    \sa create()
*/
quintptr QWindow::winId() const { return m_d->platform ? m_d->platform->winId() : 0; }

/*!
    Creates the platform window. show() does this when needed, so calling
    create() is only necessary to get a winId() before showing the window.
    A window with an empty geometry gets a size of 160 by 160.

    \sa destroy(), winId()
*/
void QWindow::create() {
    if (m_d->platform)
        return;
    QPlatformIntegration *integration = QtPrivate::GuiAccess::integration();
    if (!integration)
        return;
    if (m_d->geometry.isEmpty())
        QtPrivate::GuiAccess::applyGeometry(this, QRect(m_d->geometry.topLeft(), QSize(kDefaultSize, kDefaultSize)));
    m_d->platform = integration->createPlatformWindow(this);
    m_d->platform->setGeometry(m_d->geometry);
    if (!m_d->title.isEmpty())
        m_d->platform->setWindowTitle(m_d->title);
    if (m_d->transientParent && m_d->transientParent->m_d->platform)
        m_d->platform->setTransientParent(m_d->transientParent->m_d->platform);
    QtPrivate::GuiAccess::applyCursor(this);
}

/*!
    Returns the cursor that shows while the mouse is over the window: the
    standard arrow, unless setCursor() set another one.

    \sa setCursor(), unsetCursor()
*/
QCursor QWindow::cursor() const { return m_d->cursor; }

/*!
    Sets the cursor that shows while the mouse is over the window to
    \a cursor. An override cursor of the application shows instead for as
    long as there is one; see QGuiApplication::setOverrideCursor().

    \sa cursor(), unsetCursor()
*/
void QWindow::setCursor(const QCursor &cursor) {
    m_d->cursor = cursor;
    QtPrivate::GuiAccess::applyCursor(this);
}

/*!
    Gives the window the standard arrow cursor again.

    \sa setCursor()
*/
void QWindow::unsetCursor() { setCursor(QCursor()); }

/*!
    Hides the window and releases the platform window. The window object
    stays, and show() creates the platform window again.

    \sa create()
*/
void QWindow::destroy() {
    if (!m_d->platform)
        return;
    setVisible(false);
    QtPrivate::GuiAccess::surfaceAboutToBeDestroyed(this);
    delete m_d->platform;
    m_d->platform = nullptr;
    m_d->exposed = false;
}

/*!
    \fn bool QWindow::isTopLevel() const

    Returns \c true, because every window is a top-level window.
*/

/*!
    Returns the window title.

    \sa setTitle()
*/
QString QWindow::title() const { return m_d->title; }

/*!
    Sets the window title to \a title.

    \sa title()
*/
void QWindow::setTitle(const QString &title) {
    if (m_d->title == title)
        return;
    m_d->title = title;
    if (m_d->platform)
        m_d->platform->setWindowTitle(title);
    emit titleChanged(title);
}

/*!
    Returns the window that this window is transient for, or \c nullptr if it is a normal window.

    \sa setTransientParent()
*/
QWindow *QWindow::transientParent() const { return m_d->transientParent.data(); }

/*!
    Makes the window transient for \a parent: the window system keeps it above \a parent and treats it as a dialog of
    it. With \c nullptr the window is a normal window again.

    \sa transientParent()
*/
void QWindow::setTransientParent(QWindow *parent) {
    if (parent == this || m_d->transientParent == parent)
        return;
    m_d->transientParent = parent;
    if (m_d->platform)
        m_d->platform->setTransientParent(parent ? parent->m_d->platform : nullptr);
    emit transientParentChanged(parent);
}

/*!
    Returns \c true if the window is visible: shown, not hidden or closed.

    \sa setVisible(), isExposed()
*/
bool QWindow::isVisible() const { return m_d->visible; }

/*!
    Returns \c true if the window is visible and its contents are on the
    screen, so that painting makes sense. It becomes \c true when the first
    expose event arrives.

    \sa isVisible(), exposeEvent()
*/
bool QWindow::isExposed() const { return m_d->exposed; }

/*!
    Returns \c true if the window is the active window.

    \sa requestActivate()
*/
bool QWindow::isActive() const { return m_d->active; }

/*!
    Returns the screen the window is on.
*/
QScreen *QWindow::screen() const { return m_d->screen; }

/*!
    Returns the number of pixels of the screen that one unit of the window's coordinates
    covers; see QScreen::devicePixelRatio(). It is 1 if the window has no screen.
*/
qreal QWindow::devicePixelRatio() const { return m_d->screen ? m_d->screen->devicePixelRatio() : 1.0; }

/*!
    Returns the geometry of the window in screen coordinates.

    \sa setGeometry()
*/
QRect QWindow::geometry() const { return m_d->geometry; }

/*!
    Sets the geometry of the window to \a rect, in screen coordinates. The
    window receives a move and a resize event as far as the position and size
    change. The windowing system may adjust the geometry later.

    \sa geometry(), resize(), setPosition()
*/
void QWindow::setGeometry(const QRect &rect) {
    if (rect == m_d->geometry && (!m_d->platform || m_d->platform->geometry() == rect))
        return;
    QtPrivate::GuiAccess::applyGeometry(this, rect);
    if (m_d->platform)
        m_d->platform->setGeometry(rect);
}

/*!
    \fn void QWindow::setGeometry(int x, int y, int w, int h)
    \overload

    Sets the geometry to the rectangle at (\a x, \a y) of \a w by \a h.
*/

/*!
    \fn QPoint QWindow::position() const

    Returns the position of the top left corner of the window.

    \sa setPosition()
*/

/*!
    Moves the window to \a pt.

    \sa position(), setGeometry()
*/
void QWindow::setPosition(const QPoint &pt) { setGeometry(QRect(pt, size())); }

/*!
    \fn void QWindow::setPosition(int x, int y)
    \overload

    Moves the window to (\a x, \a y).
*/

/*!
    \fn QSize QWindow::size() const

    Returns the size of the window in pixels.

    \sa resize()
*/

/*!
    Resizes the window to \a newSize.

    \sa size(), setGeometry()
*/
void QWindow::resize(const QSize &newSize) { setGeometry(QRect(position(), newSize)); }

/*!
    \fn void QWindow::resize(int w, int h)
    \overload

    Resizes the window to \a w by \a h pixels.
*/

/*!
    \fn int QWindow::x() const

    Returns the x position of the window.

    \sa setX()
*/

/*!
    \fn int QWindow::y() const

    Returns the y position of the window.

    \sa setY()
*/

/*!
    \fn int QWindow::width() const

    Returns the width of the window in pixels.

    \sa setWidth()
*/

/*!
    \fn int QWindow::height() const

    Returns the height of the window in pixels.

    \sa setHeight()
*/

/*!
    Sets the x position of the window to \a arg.

    \sa x()
*/
void QWindow::setX(int arg) { setPosition(QPoint(arg, y())); }

/*!
    Sets the y position of the window to \a arg.

    \sa y()
*/
void QWindow::setY(int arg) { setPosition(QPoint(x(), arg)); }

/*!
    Sets the width of the window to \a arg pixels.

    \sa width()
*/
void QWindow::setWidth(int arg) { resize(QSize(arg, height())); }

/*!
    Sets the height of the window to \a arg pixels.

    \sa height()
*/
void QWindow::setHeight(int arg) { resize(QSize(width(), arg)); }

/*!
    \fn QPoint QWindow::mapToGlobal(const QPoint &pos) const

    Returns \a pos, a point in the window, as a point on the screen.

    \sa mapFromGlobal()
*/

/*!
    \fn QPoint QWindow::mapFromGlobal(const QPoint &pos) const

    Returns \a pos, a point on the screen, as a point in the window.

    \sa mapToGlobal()
*/

/*!
    Returns the platform window, or \c nullptr if the window has not been
    created. The type is private to Nt.

    \sa create()
*/
QPlatformWindow *QWindow::handle() const { return m_d->platform; }

/*!
    \since 6.0
    Sets the kind of surface of the window to \a surfaceType. Set it to
    QSurface::OpenGLSurface before the window is created to render into it with
    a QOpenGLContext; QOpenGLWindow does that itself.

    \sa surfaceType(), QOpenGLContext::makeCurrent()
*/
void QWindow::setSurfaceType(QSurface::SurfaceType surfaceType) { m_d->surfaceType = surfaceType; }

/*!
    \reimp

    Returns the kind of surface of the window. The default is
    QSurface::RasterSurface.

    \sa setSurfaceType()
*/
QSurface::SurfaceType QWindow::surfaceType() const { return m_d->surfaceType; }

/*!
    \since 6.0
    Sets the format the window asks for to \a format: its depth, stencil and
    multisample buffers, the OpenGL version and the swap interval. Set it before
    an OpenGL context is first made current on the window.

    \sa requestedFormat(), format()
*/
void QWindow::setFormat(const QSurfaceFormat &format) { m_d->requestedFormat = format; }

/*!
    \since 6.0
    Returns the format set with setFormat(), or QSurfaceFormat::defaultFormat()
    if none was set.

    \sa setFormat(), format()
*/
QSurfaceFormat QWindow::requestedFormat() const { return m_d->requestedFormat; }

/*!
    \reimp

    Returns the format the window has: the requested format until an OpenGL
    context is made current on the window, and then the format of the buffers
    the window system gave it.

    \sa requestedFormat()
*/
QSurfaceFormat QWindow::format() const { return m_d->hasFormat ? m_d->format : m_d->requestedFormat; }

/*!
    Shows the window if \a visible is \c true and hides it otherwise. Showing
    creates the platform window if needed.

    \sa show(), hide()
*/
void QWindow::setVisible(bool visible) {
    if (m_d->visible == visible)
        return;
    if (visible) {
        create();
        if (!m_d->platform)
            return;
        m_d->visible = true;
        QShowEvent showEvent;
        QCoreApplication::sendEvent(this, &showEvent);
        m_d->platform->setVisible(true);
    } else {
        m_d->visible = false;
        m_d->exposed = false;
        QHideEvent hideEvent;
        QCoreApplication::sendEvent(this, &hideEvent);
        if (m_d->platform)
            m_d->platform->setVisible(false);
    }
    emit visibleChanged(m_d->visible);
    if (!visible)
        QtPrivate::GuiAccess::maybeQuitOnLastWindowClosed(this);
}

/*!
    Shows the window. This is the same as \c{setVisible(true)}.

    \sa hide()
*/
void QWindow::show() { setVisible(true); }

/*!
    Hides the window. This is the same as \c{setVisible(false)}.

    \sa show()
*/
void QWindow::hide() { setVisible(false); }

/*!
    Sends the window a close event. If the window accepts it, which it does by
    default, the window is destroyed and the function returns \c true.

    The application quits when the last window closes and
    QGuiApplication::quitOnLastWindowClosed() is \c true.

    \sa destroy(), closeEvent()
*/
bool QWindow::close() {
    if (!m_d->platform)
        return true;
    QCloseEvent closeEvent;
    QCoreApplication::sendEvent(this, &closeEvent);
    if (!closeEvent.isAccepted())
        return false;
    destroy();
    return true;
}

/*!
    Asks the window system to put the window in front of the others.
*/
void QWindow::raise() {
    if (m_d->platform && m_d->visible)
        m_d->platform->requestActivateWindow();
}

/*!
    Asks the window system to give the window the keyboard focus. The window is
    active once isActive() returns \c true.

    \sa isActive()
*/
void QWindow::requestActivate() {
    if (m_d->platform && m_d->visible)
        m_d->platform->requestActivateWindow();
}

/*!
    Posts an \c UpdateRequest event to the window, once, however often this is
    called before the event is delivered.
*/
void QWindow::requestUpdate() {
    if (m_d->updateRequested)
        return;
    m_d->updateRequested = true;
    QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
}

/*!
    Handles \a event and returns \c true if it was recognized. It calls the
    event handlers such as mousePressEvent() and keyPressEvent().
*/
bool QWindow::event(QEvent *event) {
    switch (event->type()) {
    case QEvent::Expose:
        exposeEvent(static_cast<QExposeEvent *>(event));
        return true;
    case QEvent::Resize:
        resizeEvent(static_cast<QResizeEvent *>(event));
        return true;
    case QEvent::Move:
        moveEvent(static_cast<QMoveEvent *>(event));
        return true;
    case QEvent::FocusIn:
        focusInEvent(static_cast<QFocusEvent *>(event));
        return true;
    case QEvent::FocusOut:
        focusOutEvent(static_cast<QFocusEvent *>(event));
        return true;
    case QEvent::Show:
        showEvent(static_cast<QShowEvent *>(event));
        return true;
    case QEvent::Hide:
        hideEvent(static_cast<QHideEvent *>(event));
        return true;
    case QEvent::Close:
        closeEvent(static_cast<QCloseEvent *>(event));
        return true;
    case QEvent::KeyPress:
        keyPressEvent(static_cast<QKeyEvent *>(event));
        return true;
    case QEvent::KeyRelease:
        keyReleaseEvent(static_cast<QKeyEvent *>(event));
        return true;
    case QEvent::MouseButtonPress:
        mousePressEvent(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::MouseButtonRelease:
        mouseReleaseEvent(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::MouseButtonDblClick:
        mouseDoubleClickEvent(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::MouseMove:
        mouseMoveEvent(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::Wheel:
        wheelEvent(static_cast<QWheelEvent *>(event));
        return true;
    case QEvent::UpdateRequest:
        m_d->updateRequested = false;
        return true;
    default:
        return QObject::event(event);
    }
}

/*!
    Handles an expose event: part of the window became visible, and the
    window has to draw it. \a event holds the region. The default does nothing.

    \sa isExposed()
*/
void QWindow::exposeEvent(QExposeEvent *event) { (void)event; }

/*!
    Handles the resize event \a event. The default does nothing.

    \sa resize()
*/
void QWindow::resizeEvent(QResizeEvent *event) { (void)event; }

/*!
    Handles the move event \a event. The default does nothing.

    \sa setPosition()
*/
void QWindow::moveEvent(QMoveEvent *event) { (void)event; }

/*!
    Handles the focus-in event \a event: the window got the keyboard focus. The default
    does nothing.
*/
void QWindow::focusInEvent(QFocusEvent *event) { (void)event; }

/*!
    Handles the focus-out event \a event: the window lost the keyboard focus. The default
    does nothing.
*/
void QWindow::focusOutEvent(QFocusEvent *event) { (void)event; }

/*!
    Handles the show event \a event: the window is about to become visible. The default
    does nothing.
*/
void QWindow::showEvent(QShowEvent *event) { (void)event; }

/*!
    Handles the hide event \a event: the window was hidden. The default does nothing.
*/
void QWindow::hideEvent(QHideEvent *event) { (void)event; }

/*!
    Handles the close event \a event. Call \c{event->ignore()} to keep the window open.
    The default accepts the event.

    \sa close()
*/
void QWindow::closeEvent(QCloseEvent *event) { event->accept(); }

/*!
    Handles the key press \a event. The default ignores the event.

    \sa keyReleaseEvent()
*/
void QWindow::keyPressEvent(QKeyEvent *event) { event->ignore(); }

/*!
    Handles the key release \a event. The default ignores the event.

    \sa keyPressEvent()
*/
void QWindow::keyReleaseEvent(QKeyEvent *event) { event->ignore(); }

/*!
    Handles the mouse button press \a event. The default ignores the event.

    \sa mouseReleaseEvent()
*/
void QWindow::mousePressEvent(QMouseEvent *event) { event->ignore(); }

/*!
    Handles the mouse button release \a event. The default ignores the event.

    \sa mousePressEvent()
*/
void QWindow::mouseReleaseEvent(QMouseEvent *event) { event->ignore(); }

/*!
    Handles the double click \a event. The default ignores the event.
*/
void QWindow::mouseDoubleClickEvent(QMouseEvent *event) { event->ignore(); }

/*!
    Handles the mouse move \a event. The default ignores the event.
*/
void QWindow::mouseMoveEvent(QMouseEvent *event) { event->ignore(); }

/*!
    Handles the mouse wheel turn \a event. The default ignores the event.
*/
void QWindow::wheelEvent(QWheelEvent *event) { event->ignore(); }

namespace QtPrivate {

QPlatformWindow *GuiAccess::platformWindow(const QWindow *window) { return window->m_d->platform; }

void GuiAccess::setQuitOnClose(QWindow *window, bool quit) { window->m_d->quitOnClose = quit; }

bool GuiAccess::quitOnClose(const QWindow *window) { return window->m_d->quitOnClose; }

void GuiAccess::applyGeometry(QWindow *window, const QRect &rect) {
    QWindow::Data &d = *window->m_d;
    const QRect old = d.geometry;
    if (rect == old)
        return;
    d.geometry = rect;
    if (rect.topLeft() != old.topLeft()) {
        QMoveEvent move(rect.topLeft(), old.topLeft());
        QCoreApplication::sendEvent(window, &move);
        if (rect.x() != old.x())
            emit window->xChanged(rect.x());
        if (rect.y() != old.y())
            emit window->yChanged(rect.y());
    }
    if (rect.size() != old.size()) {
        QResizeEvent resize(rect.size(), old.size());
        QCoreApplication::sendEvent(window, &resize);
        if (rect.width() != old.width())
            emit window->widthChanged(rect.width());
        if (rect.height() != old.height())
            emit window->heightChanged(rect.height());
    }
}

void GuiAccess::setExposed(QWindow *window, bool exposed) { window->m_d->exposed = exposed; }

void GuiAccess::setActive(QWindow *window, bool active) {
    if (window->m_d->active == active)
        return;
    window->m_d->active = active;
    emit window->activeChanged(active);
}

} // namespace QtPrivate

void QtPrivate::GuiAccess::setActualFormat(QWindow *window, const QSurfaceFormat &format) {
    window->m_d->format = format;
    window->m_d->hasFormat = true;
}

#include "moc_qwindow.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
