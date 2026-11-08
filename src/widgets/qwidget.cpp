// MIT License - see LICENSE
#include <QtWidgets/qwidget.h>

#include "qwidget_p.h"
#include "qwidgetutil_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpainter.h>
#include <QtGui/qscreen.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qtooltip.h>

#include <algorithm>

namespace {

// The device that render() paints on, while it runs: the widget it started from and where that widget is on the device.
struct RenderContext {
    QPaintDevice *target = nullptr;
    const QWidget *root = nullptr;
    QPoint origin;
};
RenderContext g_render;

} // namespace

namespace QtPrivate {

std::vector<QWidget *> WidgetAccess::childWidgets(const QWidget *w) {
    std::vector<QWidget *> out;
    for (QObject *c : w->children())
        if (auto *cw = dynamic_cast<QWidget *>(c))
            out.push_back(cw);
    return out;
}

QPoint WidgetAccess::windowOffset(const QWidget *w) {
    QPoint offset;
    while (w->parentWidget()) {
        offset += w->pos();
        w = w->parentWidget();
    }
    return offset;
}

QRect WidgetAccess::visibleRect(const QWidget *w) {
    QRect r(windowOffset(w), w->size());
    for (const QWidget *p = w->parentWidget(); p; p = p->parentWidget())
        r = r.intersected(QRect(windowOffset(p), p->size()));
    return r;
}

bool WidgetAccess::shownSelf(const QWidget *w) {
    if (!w->parentWidget())
        return w->m_d->window && w->m_d->window->isVisible();
    return !w->m_d->hiddenExplicitly && !w->m_d->pendingShow;
}

QWidget *WidgetAccess::widgetAt(QWidget *root, const QPoint &p) {
    const std::vector<QWidget *> kids = childWidgets(root);
    for (auto it = kids.rbegin(); it != kids.rend(); ++it) {
        QWidget *child = *it;
        if (shownSelf(child) && !data(child).mouseTransparent && child->geometry().contains(p))
            return widgetAt(child, p - child->pos());
    }
    return root;
}

void WidgetAccess::paintRecursive(QWidget *w, const QPoint &origin, const QRect &parentVisible, const QRegion &dirty) {
    const QRect visible = QRect(origin, w->size()).intersected(parentVisible);
    if (visible.isEmpty() || !WidgetAccess::shownSelf(w))
        return;
    QWidget::Data &d = data(w);
    const QRegion region = dirty.intersected(visible);
    if (!region.isEmpty()) {
        d.painting = true;
        d.paintClip = region;
        if (w->isWindow() || d.autoFill) {
            QPainter painter(w);
            painter.fillRect(w->rect(), w->palette().color(w->isEnabled() ? QPalette::Active : QPalette::Disabled,
                                                           QPalette::Window));
        }
        QPaintEvent event(region.translated(-origin));
        QCoreApplication::sendEvent(w, &event);
        d.painting = false;
        d.paintClip = QRegion();
    }
    for (QWidget *child : childWidgets(w))
        paintRecursive(child, origin + child->pos(), visible, dirty);
}

void WidgetAccess::renderRecursive(QWidget *w, const QPoint &origin, const QRect &parentVisible, const QRegion &dirty) {
    const QRect visible = QRect(origin, w->size()).intersected(parentVisible);
    if (visible.isEmpty() || (w != g_render.root && data(w).hiddenExplicitly))
        return;
    QWidget::Data &d = data(w);
    const QRegion region = dirty.intersected(visible);
    if (!region.isEmpty()) {
        d.painting = true;
        d.paintClip = region;
        if (w->isWindow() || d.autoFill) {
            QPainter painter(w);
            painter.fillRect(w->rect(), w->palette().color(w->isEnabled() ? QPalette::Active : QPalette::Disabled,
                                                           QPalette::Window));
        }
        QPaintEvent event(region.translated(-origin));
        QCoreApplication::sendEvent(w, &event);
        d.painting = false;
        d.paintClip = QRegion();
    }
    for (QWidget *child : childWidgets(w))
        renderRecursive(child, origin + child->pos(), visible, dirty);
}

void WidgetAccess::paintTree(QWidget *top, const QRegion &region) {
    paintRecursive(top, QPoint(0, 0), QRect(QPoint(0, 0), top->size()), region);
}

void WidgetAccess::sendShowEvents(QWidget *w) {
    QShowEvent event;
    QCoreApplication::sendEvent(w, &event);
    for (QWidget *child : childWidgets(w))
        if (shownSelf(child))
            sendShowEvents(child);
}

void WidgetAccess::sendHideEvents(QWidget *w) {
    for (QWidget *child : childWidgets(w))
        if (shownSelf(child))
            sendHideEvents(child);
    QHideEvent event;
    QCoreApplication::sendEvent(w, &event);
}

bool WidgetAccess::focusable(const QWidget *w) {
    return shownSelf(w) && w->isEnabled() && (w->focusPolicy() & Qt::TabFocus);
}

static void collectChain(QWidget *w, std::vector<QWidget *> &out) {
    if (!WidgetAccess::shownSelf(w))
        return;
    if (WidgetAccess::focusable(w))
        out.push_back(w);
    for (QWidget *child : WidgetAccess::childWidgets(w))
        collectChain(child, out);
}

std::vector<QWidget *> WidgetAccess::tabChain(QWidget *top) {
    std::vector<QWidget *> chain;
    collectChain(top, chain);
    return chain;
}

QWidget *WidgetAccess::focusWidgetOf(QWidget *top) { return top->m_d->focusChild; }

void WidgetAccess::setFocusWidget(QWidget *top, QWidget *w) {
    QWidget *old = top->m_d->focusChild;
    if (old == w)
        return;
    top->m_d->focusChild = w;
    if (top->isActiveWindow()) {
        if (old) {
            QFocusEvent out(QEvent::FocusOut);
            QCoreApplication::sendEvent(old, &out);
        }
        if (w) {
            QFocusEvent in(QEvent::FocusIn);
            QCoreApplication::sendEvent(w, &in);
        }
    }
}

void WidgetAccess::windowActivated(QWidget *top, bool active) {
    QWidget *focus = top->m_d->focusChild;
    if (active) {
        if (!focus) {
            const std::vector<QWidget *> chain = tabChain(top);
            if (!chain.empty()) {
                focus = chain.front();
                top->m_d->focusChild = focus;
            }
        }
        QEvent activate(QEvent::WindowActivate);
        QCoreApplication::sendEvent(top, &activate);
        if (focus) {
            QFocusEvent in(QEvent::FocusIn);
            QCoreApplication::sendEvent(focus, &in);
        }
    } else {
        if (focus) {
            QFocusEvent out(QEvent::FocusOut);
            QCoreApplication::sendEvent(focus, &out);
        }
        QEvent deactivate(QEvent::WindowDeactivate);
        QCoreApplication::sendEvent(top, &deactivate);
    }
    top->update();
}

void WidgetAccess::setUnderMouse(QWidget *w, bool under) {
    if (w->m_d->underMouse == under)
        return;
    w->m_d->underMouse = under;
    w->update();
}

void WidgetAccess::clearReferences(QWidget *w) {
    QWidget *top = w;
    while (top->parentWidget())
        top = top->parentWidget();
    if (top->m_d->focusChild == w)
        top->m_d->focusChild = nullptr;
    if (top->m_d->window)
        top->m_d->window->forget(w);
}

void WidgetAccess::setPopup(QWidget *w, QWidget *popup) {
    QWidget *top = w->window();
    if (top->m_d->window)
        top->m_d->window->setPopup(popup);
}

void WidgetAccess::activateLayout(QWidget *w) {
    if (w->m_d->layout)
        w->m_d->layout->activate();
}

} // namespace QtPrivate

using QtPrivate::WidgetAccess;

/*!
    \class QWidget
    \brief The QWidget class is the base class of all user interface objects.

    A widget is a rectangle that paints itself and receives events from the window system:
    mouse, keyboard and focus events, and paint events when part of it needs to be drawn. A widget
    without a parent is a window; a widget with a parent is a child that is drawn inside it.

    \code
    QWidget window;
    window.setWindowTitle("Hello");
    QVBoxLayout *layout = new QVBoxLayout(&window);
    layout->addWidget(new QLabel("Hello, widgets"));
    layout->addWidget(new QPushButton("OK"));
    window.show();
    \endcode

    To make your own widget, subclass QWidget and reimplement paintEvent(), which paints with a
    QPainter on the widget, and the event handlers you need. Call update() when the widget has to be
    painted again.

    The widgets of a window are painted into the backing store of one native window, so child widgets
    are not native windows of their own.

    \differences
    The widget has no window flags (\c Qt::Window, \c Qt::Dialog, \c Qt::Tool and so on), no widget
    attributes (\c setAttribute()), no \c windowIcon(), cursor, tool tip, context menu, actions,
    shortcuts, \c grab(), \c setFocusProxy(), \c toolTipDuration(), \c statusTip(), \c whatsThis(), \c setTabOrder(), \c stackUnder(),
    \c setWindowOpacity() or transparent windows. Only top-level widgets are native windows, and
    a widget that is a window has its geometry set on the QWindow without a window frame, so
    \c frameGeometry() is not provided. \c raise() and \c lower() reorder the children of a parent, and
    ask the window system for a top-level widget. Mouse events go to the deepest widget under the mouse and
    to the widget that was pressed until the buttons are released. Focus is per top-level widget and
    tab navigation goes through the children in the order they were created.
    \enddifferences

    \sa QWindow, QPainter, QLayout, QApplication
*/

/*!
    Constructs a widget that is a child of \a parent, or a window if \a parent is \c nullptr. A
    QApplication must exist. The widget is hidden until show() is called, or until its parent is shown.
*/
QWidget::QWidget(QWidget *parent) : QObject(parent), m_d(std::make_unique<Data>()) {
    if (!QApplication::instance())
        qFatal("QWidget: a QApplication must be created before a widget");
    m_d->font = QFont();
    if (parent) {
        m_d->geometry = QRect(0, 0, 100, 30);
        m_d->pendingShow = parent->isVisible();
    } else {
        m_d->geometry = QRect(0, 0, 640, 480);
        WidgetAccess::registerTopLevel(this);
    }
}

/*!
    Destroys the widget, its child widgets and its layout. A window is closed without a close event.
*/
QWidget::~QWidget() {
    // Deleting a child may delete other children, such as the popup of a combo box, so look at the list afresh each time.
    for (bool found = true; found;) {
        found = false;
        for (QObject *c : children()) {
            if (dynamic_cast<QWidget *>(c)) {
                delete c;
                found = true;
                break;
            }
        }
    }
    if (QWidget *p = parentWidget()) {
        if (p->m_d->layout)
            p->m_d->layout->removeWidget(this);
    }
    WidgetAccess::clearReferences(this);
    if (m_d->layout) {
        delete m_d->layout;
        m_d->layout = nullptr;
    }
    if (m_d->window) {
        m_d->window->detach();
        m_d->window.reset();
    }
    if (!parentWidget())
        WidgetAccess::unregisterTopLevel(this);
}

/*!
    \reimp
*/
QPaintEngine *QWidget::paintEngine() const {
    const QWidget *top = window();
    return top->m_d->window ? top->m_d->window->paintEngine() : nullptr;
}

/*!
    \reimp
*/
int QWidget::metric(PaintDeviceMetric m) const {
    switch (m) {
    case PdmWidth:
        return width();
    case PdmHeight:
        return height();
    case PdmDevicePixelRatio:
    case PdmDevicePixelRatioScaled: {
        const QWindow *handle = window()->windowHandle();
        const qreal ratio =
            handle ? handle->devicePixelRatio()
                   : (QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0);
        return m == PdmDevicePixelRatio ? static_cast<int>(ratio) : qRound(ratio * 65536.0);
    }
    default:
        return QPaintDevice::metric(m);
    }
}

/*!
    \reimp
*/
QPaintDevice *QWidget::redirected(QPoint *offset) const {
    // While render() paints a tree of widgets, they paint on the device that it was given.
    if (g_render.target) {
        for (const QWidget *w = this; w; w = w->parentWidget()) {
            if (w == g_render.root) {
                *offset +=
                    WidgetAccess::windowOffset(this) - WidgetAccess::windowOffset(g_render.root) + g_render.origin;
                return g_render.target;
            }
        }
    }
    const QWidget *top = window();
    if (!top->m_d->window)
        return nullptr;
    *offset += WidgetAccess::windowOffset(this);
    return top->m_d->window.get();
}

/*!
    \reimp
*/
bool QWidget::systemClip(QRegion *region) const {
    if (!m_d->painting)
        return false;
    *region = m_d->paintClip;
    return true;
}

/*!
    Returns the tool tip of the widget: the text that appears next to the mouse when it rests on the widget for a
    moment. It is empty by default, and then the tool tip of the parent applies, if it has one.

    \sa setToolTip(), QToolTip
*/
QString QWidget::toolTip() const { return m_d->toolTip; }

/*!
    Sets the tool tip of the widget to \a text, which may have several lines. An empty text removes it. The tool tip
    is shown for a disabled widget too. A widget can show a tool tip that depends on the position of the mouse if it
    handles the QEvent::ToolTip event.

    \sa toolTip(), QHelpEvent
*/
void QWidget::setToolTip(const QString &text) { m_d->toolTip = text; }

/*!
    Paints the widget and its visible children on \a target, with the top left corner of the widget at
    \a targetOffset. Only \a sourceRegion of the widget is painted if it is not empty. The widget need not be
    shown: it is painted as it would look, at its current size, with its layout done, and a widget that is hidden
    with hide() is left out. A window has its background painted first.

    The pixels of \a target are those of the device, so a QImage with a device pixel ratio of 2 gets a picture with
    twice as many pixels. Use it to take a picture of a widget:

    \code
    QImage image(widget->size() * 2, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(2.0);
    widget->render(&image);
    \endcode

    \sa QPainter, QImage::setDevicePixelRatio()
*/
void QWidget::render(QPaintDevice *target, const QPoint &targetOffset, const QRegion &sourceRegion) {
    if (!target || target->paintingActive() || g_render.target)
        return;
    WidgetAccess::activateLayout(this);
    const QRegion source = sourceRegion.isEmpty() ? QRegion(rect()) : sourceRegion.intersected(rect());
    g_render = {target, this, targetOffset};
    WidgetAccess::renderRecursive(this, targetOffset, QRect(targetOffset, size()), source.translated(targetOffset));
    g_render = RenderContext();
}

/*!
    Returns the geometry of the widget. For a child widget it is relative to the parent; for a window it is
    in screen coordinates and excludes the window frame.

    \sa setGeometry(), rect()
*/
QRect QWidget::geometry() const { return m_d->geometry; }

/*!
    \fn QRect QWidget::rect() const

    Returns the rectangle of the widget in its own coordinates: its size, with the top left corner at the origin.
*/

/*!
    \fn QPoint QWidget::pos() const

    Returns the position of the widget in its parent, or on the screen for a window.

    \sa move()
*/

/*!
    \fn QSize QWidget::size() const

    Returns the size of the widget.

    \sa resize()
*/

/*!
    \fn int QWidget::x() const

    Returns the x coordinate of the widget in its parent.
*/

/*!
    \fn int QWidget::y() const

    Returns the y coordinate of the widget in its parent.
*/

/*!
    \fn int QWidget::width() const

    Returns the width of the widget.
*/

/*!
    \fn int QWidget::height() const

    Returns the height of the widget.
*/

namespace QtPrivate {

void WidgetAccess::applyLocalGeometry(QWidget *w, const QRect &rect) {
    QWidget::Data &d = *w->m_d;
    const QRect old = d.geometry;
    if (rect == old)
        return;
    d.geometry = rect;
    d.geometrySet = true;
    if (rect.topLeft() != old.topLeft()) {
        QMoveEvent move(rect.topLeft(), old.topLeft());
        QCoreApplication::sendEvent(w, &move);
    }
    if (rect.size() != old.size()) {
        QResizeEvent resize(rect.size(), old.size());
        QCoreApplication::sendEvent(w, &resize);
        if (QWidget *parent = w->parentWidget()) {
            if (parent->m_d->childChanged)
                parent->m_d->childChanged(w);
        }
    }
    if (QWidget *parent = w->parentWidget()) {
        if (shownSelf(w))
            parent->update(old.united(rect));
    }
}

} // namespace QtPrivate

/*!
    Sets the geometry of the widget to \a rect, in the coordinates of the parent, or on the screen for a
    window. The size is kept between minimumSize() and maximumSize(). The widget receives a move and
    a resize event for what changed.

    \sa geometry(), move(), resize()
*/
void QWidget::setGeometry(const QRect &rect) {
    QRect r = rect.normalized();
    r.setSize(QtPrivate::boundedTo(QtPrivate::expandedTo(r.size(), m_d->minimumSize), m_d->maximumSize));
    m_d->geometrySet = true;
    if (isWindow() && m_d->window) {
        m_d->window->setGeometry(r);
        return;
    }
    WidgetAccess::applyLocalGeometry(this, r);
}

/*!
    \fn void QWidget::setGeometry(int x, int y, int w, int h)
    \overload

    Sets the geometry to the rectangle at (\a x, \a y) of \a w by \a h pixels.
*/

/*!
    Moves the widget to \a pos in its parent, or on the screen for a window.

    \sa pos(), setGeometry()
*/
void QWidget::move(const QPoint &pos) { setGeometry(QRect(pos, size())); }

/*!
    \fn void QWidget::move(int x, int y)
    \overload

    Moves the widget to (\a x, \a y).
*/

/*!
    Resizes the widget to \a size, within its minimum and maximum size.

    \sa size(), setGeometry()
*/
void QWidget::resize(const QSize &size) { setGeometry(QRect(pos(), size)); }

/*!
    \fn void QWidget::resize(int w, int h)
    \overload

    Resizes the widget to \a w by \a h pixels.
*/

/*!
    Returns the minimum size that was set with setMinimumSize(), (0, 0) by default. A layout never makes
    the widget smaller than its minimum size.

    \sa minimumSizeHint()
*/
QSize QWidget::minimumSize() const { return m_d->minimumSize; }

/*!
    Returns the maximum size that was set with setMaximumSize(). It is very large by default.
*/
QSize QWidget::maximumSize() const { return m_d->maximumSize; }

/*!
    Sets the minimum size of the widget to \a size. The widget grows if it is smaller.

    \sa minimumSize()
*/
void QWidget::setMinimumSize(const QSize &size) {
    m_d->minimumSize = size;
    if (width() < size.width() || height() < size.height())
        resize(QtPrivate::expandedTo(this->size(), size));
    updateGeometry();
}

/*!
    \fn void QWidget::setMinimumSize(int minw, int minh)
    \overload

    Sets the minimum size to \a minw by \a minh pixels.
*/

/*!
    Sets the maximum size of the widget to \a size. The widget shrinks if it is larger.

    \sa maximumSize()
*/
void QWidget::setMaximumSize(const QSize &size) {
    m_d->maximumSize = size;
    if (width() > size.width() || height() > size.height())
        resize(QtPrivate::boundedTo(this->size(), size));
    updateGeometry();
}

/*!
    \fn void QWidget::setMaximumSize(int maxw, int maxh)
    \overload

    Sets the maximum size to \a maxw by \a maxh pixels.
*/

/*!
    Sets both the minimum and the maximum size to \a size, so that the widget has exactly this size.
*/
void QWidget::setFixedSize(const QSize &size) {
    m_d->minimumSize = size;
    m_d->maximumSize = size;
    resize(size);
    updateGeometry();
}

/*!
    \fn void QWidget::setFixedSize(int w, int h)
    \overload

    Sets the size to exactly \a w by \a h pixels.
*/

/*!
    Sets the minimum and the maximum width to \a w.
*/
void QWidget::setFixedWidth(int w) {
    m_d->minimumSize.setWidth(w);
    m_d->maximumSize.setWidth(w);
    resize(w, height());
    updateGeometry();
}

/*!
    Sets the minimum and the maximum height to \a h.
*/
void QWidget::setFixedHeight(int h) {
    m_d->minimumSize.setHeight(h);
    m_d->maximumSize.setHeight(h);
    resize(width(), h);
    updateGeometry();
}

/*!
    Returns the size that the widget would like to have. Layouts use it, together with
    sizePolicy(). The default is the size hint of the widget's layout, or an invalid size if there
    is none. Reimplement it in a custom widget.

    \sa minimumSizeHint(), updateGeometry()
*/
QSize QWidget::sizeHint() const { return m_d->layout ? m_d->layout->sizeHint() : QSize(-1, -1); }

/*!
    Returns the smallest size at which the widget is still usable. The default is the minimum size of
    the widget's layout, or an invalid size if there is none.

    \sa sizeHint()
*/
QSize QWidget::minimumSizeHint() const { return m_d->layout ? m_d->layout->minimumSize() : QSize(-1, -1); }

/*!
    Returns the size policy, which tells layouts how the widget may be resized. The default is
    Preferred in both directions.

    \sa setSizePolicy()
*/
QSizePolicy QWidget::sizePolicy() const { return m_d->sizePolicy; }

/*!
    Sets the size policy to \a policy.

    \sa sizePolicy()
*/
void QWidget::setSizePolicy(QSizePolicy policy) {
    if (m_d->sizePolicy == policy)
        return;
    m_d->sizePolicy = policy;
    updateGeometry();
}

/*!
    \fn void QWidget::setSizePolicy(QSizePolicy::Policy horizontal, QSizePolicy::Policy vertical)
    \overload

    Sets the size policy to \a horizontal and \a vertical.
*/

/*!
    Resizes the widget to its size hint (at least its minimum size hint). Nothing happens if the widget has no size hint.

    \sa sizeHint()
*/
void QWidget::adjustSize() {
    QSize s = sizeHint();
    if (!s.isValid())
        return;
    const QSize minimum = minimumSizeHint();
    if (minimum.isValid())
        s = QtPrivate::expandedTo(s, minimum);
    resize(s);
}

/*!
    Tells the layout that contains the widget that its size hint, minimum size or size policy
    changed, so that the layout does its work again.
*/
void QWidget::updateGeometry() {
    if (QWidget *p = parentWidget()) {
        if (p->m_d->layout)
            p->m_d->layout->invalidate();
        if (p->m_d->childChanged)
            p->m_d->childChanged(this);
    }
}

/*!
    Returns \a pos, a point in the widget, as a point on the screen.

    \sa mapFromGlobal()
*/
QPoint QWidget::mapToGlobal(const QPoint &pos) const {
    const QWidget *top = window();
    return pos + WidgetAccess::windowOffset(this) + top->pos();
}

/*!
    Returns \a pos, a point on the screen, as a point in the widget.

    \sa mapToGlobal()
*/
QPoint QWidget::mapFromGlobal(const QPoint &pos) const {
    const QWidget *top = window();
    return pos - WidgetAccess::windowOffset(this) - top->pos();
}

/*!
    \fn QPoint QWidget::mapToParent(const QPoint &pos) const

    Returns \a pos, a point in the widget, as a point in its parent.
*/

/*!
    \fn QPoint QWidget::mapFromParent(const QPoint &pos) const

    Returns \a pos, a point in the parent, as a point in the widget.
*/

/*!
    Returns \a pos, a point in the widget, as a point in \a parent, which must be an ancestor.
*/
QPoint QWidget::mapTo(const QWidget *parent, const QPoint &pos) const {
    QPoint p = pos;
    for (const QWidget *w = this; w && w != parent; w = w->parentWidget())
        p += w->pos();
    return p;
}

/*!
    Returns \a pos, a point in \a parent, which must be an ancestor, as a point in the widget.
*/
QPoint QWidget::mapFrom(const QWidget *parent, const QPoint &pos) const {
    QPoint p = pos;
    for (const QWidget *w = this; w && w != parent; w = w->parentWidget())
        p -= w->pos();
    return p;
}

/*!
    Returns the visible child widget at \a pos, in the coordinates of the widget. It is the deepest one that
    contains the point. Returns \c nullptr if there is none.
*/
QWidget *QWidget::childAt(const QPoint &pos) const {
    QWidget *found = WidgetAccess::widgetAt(const_cast<QWidget *>(this), pos);
    return found == this ? nullptr : found;
}

/*!
    \fn QWidget *QWidget::childAt(int x, int y) const
    \overload

    Returns the child widget at (\a x, \a y).
*/

/*!
    Returns the parent widget, or \c nullptr for a window.

    \sa window()
*/
QWidget *QWidget::parentWidget() const { return dynamic_cast<QWidget *>(parent()); }

/*!
    Makes \a parent the parent of the widget, or makes the widget a window if \a parent is \c nullptr. The
    widget becomes hidden; call show() to show it again.
*/
void QWidget::setParent(QWidget *parent) {
    if (parent == parentWidget())
        return;
    const bool wasWindow = isWindow();
    if (QWidget *old = parentWidget()) {
        if (old->m_d->layout)
            old->m_d->layout->removeWidget(this);
    }
    WidgetAccess::clearReferences(this);
    if (m_d->window) {
        m_d->window->detach();
        m_d->window.reset();
    }
    if (wasWindow)
        WidgetAccess::unregisterTopLevel(this);
    bool wasShown = isVisible();
    QObject::setParent(parent);
    if (!parent)
        WidgetAccess::registerTopLevel(this);
    m_d->hiddenExplicitly = false;
    m_d->pendingShow = parent && parent->isVisible();
    if (wasShown)
        update();
    if (parent && parent->m_d->layout)
        parent->m_d->layout->invalidate();
    QEvent change(QEvent::ParentChange);
    QCoreApplication::sendEvent(this, &change);
}

/*!
    Returns the window that contains the widget: its top-level ancestor, or the widget itself if it is a window.

    \sa isWindow()
*/
QWidget *QWidget::window() const {
    const QWidget *w = this;
    while (w->parentWidget())
        w = w->parentWidget();
    return const_cast<QWidget *>(w);
}

/*!
    \fn bool QWidget::isWindow() const

    Returns \c true if the widget has no parent, so it is a window.
*/

/*!
    Returns the QWindow of the window that contains the widget, or \c nullptr if it has not been shown yet.
*/
QWindow *QWidget::windowHandle() const { return window()->m_d->window.get(); }

/*!
    Returns the window title of the widget, which is only used when the widget is a window.

    \sa setWindowTitle()
*/
QString QWidget::windowTitle() const { return m_d->title; }

/*!
    Sets the window title to \a title.

    \sa windowTitle()
*/
void QWidget::setWindowTitle(const QString &title) {
    if (m_d->title == title)
        return;
    m_d->title = title;
    if (m_d->window)
        m_d->window->setTitle(title);
    QEvent change(QEvent::WindowTitleChange);
    QCoreApplication::sendEvent(this, &change);
    emit windowTitleChanged(title);
}

/*!
    Returns \c true if the window that contains the widget is the active window.

    \sa activateWindow()
*/
bool QWidget::isActiveWindow() const {
    const QWidget *top = window();
    return top->m_d->window && top->m_d->window->isActive();
}

/*!
    Asks the window system to make the window that contains the widget the active window.

    \sa isActiveWindow()
*/
void QWidget::activateWindow() {
    QWidget *top = window();
    if (top->m_d->window)
        top->m_d->window->requestActivate();
}

/*!
    Returns \c true if the widget is shown: it, and all its ancestors, are shown, and the window is visible.

    \sa isHidden(), setVisible()
*/
bool QWidget::isVisible() const {
    if (!parentWidget())
        return m_d->window && m_d->window->isVisible();
    return !m_d->hiddenExplicitly && !m_d->pendingShow && parentWidget()->isVisible();
}

/*!
    Returns \c true if the widget is hidden: hide() was called, or it was not shown yet.
    This is not the same as the opposite of isVisible(), because a widget under a hidden parent is not visible but not hidden.

    \sa isVisible()
*/
bool QWidget::isHidden() const {
    if (!parentWidget())
        return !(m_d->window && m_d->window->isVisible());
    return m_d->hiddenExplicitly || m_d->pendingShow;
}

/*!
    Shows the widget if \a visible is \c true, and hides it otherwise. Showing a window creates the native
    window the first time and gives it its size hint as the size if no size was set.

    \sa show(), hide()
*/
void QWidget::setVisible(bool visible) {
    if (!parentWidget()) {
        if (visible) {
            m_d->hiddenExplicitly = false;
            if (!m_d->window) {
                m_d->window = std::make_unique<QWidgetWindow>(this);
                m_d->window->setTitle(m_d->title);
            }
            if (m_d->window->isVisible())
                return;
            if (!m_d->geometrySet)
                adjustSize();
            m_d->window->setGeometry(m_d->geometry);
            WidgetAccess::activateLayout(this);
            WidgetAccess::sendShowEvents(this);
            m_d->window->show();
        } else {
            m_d->hiddenExplicitly = true;
            if (m_d->window)
                m_d->window->hide();
        }
        return;
    }
    const bool wasVisible = isVisible();
    if (visible) {
        m_d->hiddenExplicitly = false;
        m_d->pendingShow = false;
        if (!wasVisible && isVisible())
            WidgetAccess::sendShowEvents(this);
        update();
    } else {
        m_d->hiddenExplicitly = true;
        if (wasVisible) {
            // A hidden widget cannot keep the keyboard focus.
            QWidget *top = window();
            for (QWidget *f = top->m_d->focusChild; f; f = f->parentWidget()) {
                if (f == this) {
                    WidgetAccess::setFocusWidget(top, nullptr);
                    break;
                }
            }
            WidgetAccess::sendHideEvents(this);
            parentWidget()->update(geometry());
        }
    }
    updateGeometry();
}

/*!
    Shows the widget. This is the same as \c{setVisible(true)}.

    \sa hide()
*/
void QWidget::show() { setVisible(true); }

/*!
    Hides the widget. This is the same as \c{setVisible(false)}.

    \sa show()
*/
void QWidget::hide() { setVisible(false); }

/*!
    Closes the widget. For a window this sends a close event, which the window can ignore, and returns
    whether the window closed. A child widget is hidden. The application quits when its last window is closed
    (see QGuiApplication::quitOnLastWindowClosed()).

    \sa closeEvent()
*/
bool QWidget::close() {
    if (parentWidget()) {
        hide();
        return true;
    }
    return m_d->window ? m_d->window->close() : true;
}

/*!
    Raises the widget above its siblings, or asks the window system to put a window in front of the others.

    \sa lower()
*/
void QWidget::raise() {
    if (QWidget *p = parentWidget()) {
        QObject::setParent(nullptr);
        QObject::setParent(p);
        update();
    } else if (m_d->window) {
        m_d->window->raise();
    }
}

/*!
    Lowers the widget below its siblings. Nothing happens for a window.

    \sa raise()
*/
void QWidget::lower() {
    QWidget *p = parentWidget();
    if (!p)
        return;
    const QObjectList siblings = p->children();
    for (QObject *c : siblings) {
        if (c != this && dynamic_cast<QWidget *>(c)) {
            c->setParent(nullptr);
            c->setParent(p);
        }
    }
    update();
}

/*!
    Returns \c true if the widget is enabled, which it is unless setEnabled(false) was called on it or
    on an ancestor. A disabled widget does not get mouse and keyboard events and is drawn with the disabled colors.

    \sa setEnabled()
*/
bool QWidget::isEnabled() const {
    for (const QWidget *w = this; w; w = w->parentWidget())
        if (w->m_d->disabled)
            return false;
    return true;
}

/*!
    Enables the widget if \a enabled is \c true, and disables it, with its children, otherwise.

    \sa isEnabled()
*/
void QWidget::setEnabled(bool enabled) {
    if (m_d->disabled == !enabled)
        return;
    m_d->disabled = !enabled;
    std::vector<QWidget *> all{this};
    for (std::size_t i = 0; i < all.size(); ++i)
        for (QWidget *c : WidgetAccess::childWidgets(all[i]))
            all.push_back(c);
    for (QWidget *w : all) {
        QEvent change(QEvent::EnabledChange);
        QCoreApplication::sendEvent(w, &change);
    }
}

/*!
    \fn void QWidget::setDisabled(bool disable)

    Disables the widget if \a disable is \c true, and enables it otherwise.

    \sa setEnabled()
*/

/*!
    Returns the palette of the widget: the one set with setPalette(), or else the one of its parent, or the
    application palette.

    \sa setPalette()
*/
const QPalette &QWidget::palette() const {
    for (const QWidget *w = this; w; w = w->parentWidget())
        if (w->m_d->paletteSet)
            return w->m_d->palette;
    m_d->palette = QApplication::palette();
    return m_d->palette;
}

/*!
    Sets the palette of the widget to \a palette. Children that have no palette of their own use it too.

    \sa palette()
*/
void QWidget::setPalette(const QPalette &palette) {
    m_d->palette = palette;
    m_d->paletteSet = true;
    std::vector<QWidget *> all{this};
    for (std::size_t i = 0; i < all.size(); ++i)
        for (QWidget *c : WidgetAccess::childWidgets(all[i]))
            if (!c->m_d->paletteSet)
                all.push_back(c);
    for (QWidget *w : all) {
        QEvent change(QEvent::PaletteChange);
        QCoreApplication::sendEvent(w, &change);
    }
}

/*!
    Returns the font of the widget: the one set with setFont(), or else the one of its parent, or the
    application font.

    \sa setFont()
*/
const QFont &QWidget::font() const {
    for (const QWidget *w = this; w; w = w->parentWidget())
        if (w->m_d->fontSet)
            return w->m_d->font;
    m_d->font = QGuiApplication::font();
    return m_d->font;
}

/*!
    Sets the font of the widget to \a font. Children that have no font of their own use it too.

    \sa font()
*/
void QWidget::setFont(const QFont &font) {
    m_d->font = font;
    m_d->fontSet = true;
    std::vector<QWidget *> all{this};
    for (std::size_t i = 0; i < all.size(); ++i)
        for (QWidget *c : WidgetAccess::childWidgets(all[i]))
            if (!c->m_d->fontSet)
                all.push_back(c);
    for (QWidget *w : all) {
        QEvent change(QEvent::FontChange);
        QCoreApplication::sendEvent(w, &change);
    }
}

/*!
    \fn QFontMetrics QWidget::fontMetrics() const

    Returns the metrics of the widget's font.
*/

/*!
    Returns the style that draws the widget: the one set with setStyle() or the application style.

    \sa setStyle()
*/
QStyle *QWidget::style() const { return m_d->style ? m_d->style : QApplication::style(); }

/*!
    Sets the style of the widget to \a style, which the widget does not own and which must outlive it. A
    \c nullptr restores the application style. The style of the children is not changed.

    \sa style()
*/
void QWidget::setStyle(QStyle *style) {
    m_d->style = style;
    QEvent change(QEvent::StyleChange);
    QCoreApplication::sendEvent(this, &change);
}

/*!
    Returns whether the widget paints its background with the Window color of its palette before painting itself. It
    is always done for windows. The default is \c false.

    \sa setAutoFillBackground()
*/
bool QWidget::autoFillBackground() const { return m_d->autoFill; }

/*!
    Sets whether the widget fills its background to \a enabled.

    \sa autoFillBackground()
*/
void QWidget::setAutoFillBackground(bool enabled) {
    m_d->autoFill = enabled;
    update();
}

/*!
    Returns the focus policy of the widget, which is Qt::NoFocus by default.

    \sa setFocusPolicy()
*/
Qt::FocusPolicy QWidget::focusPolicy() const { return m_d->focusPolicy; }

/*!
    Sets the focus policy to \a policy, which says how the widget gets the keyboard focus.

    \sa focusPolicy()
*/
void QWidget::setFocusPolicy(Qt::FocusPolicy policy) { m_d->focusPolicy = policy; }

/*!
    Returns \c true if the widget has the keyboard focus: it is the focus widget of an active window.

    \sa setFocus()
*/
bool QWidget::hasFocus() const {
    const QWidget *top = window();
    return top->m_d->focusChild == this && top->isActiveWindow();
}

/*!
    Returns the widget that has the focus in the window that contains this widget, or \c nullptr.
*/
QWidget *QWidget::focusWidget() const { return window()->m_d->focusChild; }

/*!
    Gives the keyboard focus to the widget. If the window is not active, the widget gets the focus when the window is
    activated. The widget receives a focus-in event, and the widget that had the focus a focus-out event.

    \sa hasFocus(), clearFocus()
*/
void QWidget::setFocus() { WidgetAccess::setFocusWidget(window(), this); }

/*!
    Takes the keyboard focus away from the widget, if it has it.

    \sa setFocus()
*/
void QWidget::clearFocus() {
    QWidget *top = window();
    if (top->m_d->focusChild == this)
        WidgetAccess::setFocusWidget(top, nullptr);
}

/*!
    Moves the focus to the next widget in the window that accepts focus by tabbing, or to the previous one if \a next is
    \c false, and returns \c true if it did. The order is the one in which the widgets were created.
    The focus wraps around.
*/
bool QWidget::focusNextPrevChild(bool next) {
    QWidget *top = window();
    const std::vector<QWidget *> chain = WidgetAccess::tabChain(top);
    if (chain.empty())
        return false;
    QWidget *current = top->m_d->focusChild ? top->m_d->focusChild.data() : this;
    const auto it = std::find(chain.begin(), chain.end(), current);
    std::size_t index = 0;
    if (it != chain.end()) {
        const std::size_t at = static_cast<std::size_t>(it - chain.begin());
        index = next ? (at + 1) % chain.size() : (at + chain.size() - 1) % chain.size();
    } else if (!next) {
        index = chain.size() - 1;
    }
    if (chain[index] == current)
        return false;
    chain[index]->setFocus();
    return true;
}

/*!
    \fn bool QWidget::focusNextChild()

    Moves the focus to the next widget; see focusNextPrevChild().
*/

/*!
    \fn bool QWidget::focusPreviousChild()

    Moves the focus to the previous widget; see focusNextPrevChild().
*/

/*!
    Sets whether the widget gets mouse move events when no mouse button is down to \a enable. Without it, the
    widget only gets them while a button is pressed.

    \sa hasMouseTracking()
*/
void QWidget::setMouseTracking(bool enable) {
    m_d->mouseTracking = enable;
    QEvent change(QEvent::MouseTrackingChange);
    QCoreApplication::sendEvent(this, &change);
}

/*!
    Returns whether mouse tracking is on.

    \sa setMouseTracking()
*/
bool QWidget::hasMouseTracking() const { return m_d->mouseTracking; }

/*!
    Returns \c true if the mouse is over the widget and no other visible widget on top of it.
*/
bool QWidget::underMouse() const { return m_d->underMouse; }

/*!
    Returns the layout that manages the children of the widget, or \c nullptr.

    \sa setLayout()
*/
QLayout *QWidget::layout() const { return m_d->layout; }

/*!
    Makes \a layout the layout of the widget, which takes ownership of it. The widgets in the layout become
    children of this widget. A widget can have one layout; setting another when there is one does nothing but print a warning.

    \sa layout()
*/
void QWidget::setLayout(QLayout *layout) {
    if (!layout)
        return;
    if (m_d->layout && m_d->layout != layout) {
        qWarning("QWidget::setLayout: attempting to set a layout on a widget that already has one");
        return;
    }
    if (layout->parent() && layout->parent() != this) {
        qWarning("QWidget::setLayout: the layout already has a parent");
        return;
    }
    layout->QObject::setParent(this);
    m_d->layout = layout;
    // The widgets that were added before the layout had a parent widget are adopted now.
    layout->reparentChildWidgets();
    layout->invalidate();
}

/*!
    Updates the whole widget. The paint event is delivered from the event loop, and several calls are merged into one.

    \sa repaint()
*/
void QWidget::update() { update(QRegion(rect())); }

/*!
    \overload

    Updates \a rect of the widget.
*/
void QWidget::update(const QRect &rect) { update(QRegion(rect)); }

/*!
    \overload

    Updates \a region of the widget.
*/
void QWidget::update(const QRegion &region) {
    if (!isVisible())
        return;
    QWidget *top = window();
    if (!top->m_d->window)
        return;
    const QRegion mapped =
        region.translated(WidgetAccess::windowOffset(this)).intersected(WidgetAccess::visibleRect(this));
    if (!mapped.isEmpty())
        top->m_d->window->update(mapped);
}

/*!
    Paints the whole widget now, instead of from the event loop.

    \sa update()
*/
void QWidget::repaint() { repaint(rect()); }

/*!
    \overload

    Paints \a rect of the widget now.
*/
void QWidget::repaint(const QRect &rect) {
    update(rect);
    QWidget *top = window();
    if (top->m_d->window)
        QCoreApplication::sendPostedEvents(top->m_d->window.get(), QEvent::UpdateRequest);
}

/*!
    \reimp
*/
bool QWidget::event(QEvent *e) {
    switch (e->type()) {
    case QEvent::Paint:
        paintEvent(static_cast<QPaintEvent *>(e));
        return true;
    case QEvent::Resize:
        resizeEvent(static_cast<QResizeEvent *>(e));
        if (m_d->layout)
            m_d->layout->setGeometry(rect());
        return true;
    case QEvent::Move:
        moveEvent(static_cast<QMoveEvent *>(e));
        return true;
    case QEvent::Show:
        showEvent(static_cast<QShowEvent *>(e));
        return true;
    case QEvent::Hide:
        hideEvent(static_cast<QHideEvent *>(e));
        return true;
    case QEvent::Close:
        closeEvent(static_cast<QCloseEvent *>(e));
        return true;
    case QEvent::MouseButtonPress:
        mousePressEvent(static_cast<QMouseEvent *>(e));
        return true;
    case QEvent::MouseButtonRelease:
        mouseReleaseEvent(static_cast<QMouseEvent *>(e));
        return true;
    case QEvent::MouseButtonDblClick:
        mouseDoubleClickEvent(static_cast<QMouseEvent *>(e));
        return true;
    case QEvent::MouseMove:
        mouseMoveEvent(static_cast<QMouseEvent *>(e));
        return true;
    case QEvent::Wheel:
        wheelEvent(static_cast<QWheelEvent *>(e));
        return true;
    case QEvent::KeyPress: {
        auto *k = static_cast<QKeyEvent *>(e);
        if (!(k->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
            bool moved = false;
            if (k->key() == Qt::Key_Backtab || (k->key() == Qt::Key_Tab && (k->modifiers() & Qt::ShiftModifier)))
                moved = focusNextPrevChild(false);
            else if (k->key() == Qt::Key_Tab)
                moved = focusNextPrevChild(true);
            if (moved) {
                k->accept();
                return true;
            }
        }
        keyPressEvent(k);
        return true;
    }
    case QEvent::KeyRelease:
        keyReleaseEvent(static_cast<QKeyEvent *>(e));
        return true;
    case QEvent::FocusIn:
        focusInEvent(static_cast<QFocusEvent *>(e));
        update();
        return true;
    case QEvent::FocusOut:
        focusOutEvent(static_cast<QFocusEvent *>(e));
        update();
        return true;
    case QEvent::Enter:
        enterEvent(static_cast<QEnterEvent *>(e));
        return true;
    case QEvent::Leave:
        leaveEvent(e);
        return true;
    case QEvent::ToolTip:
        // The tool tip of the widget; a widget that has none leaves the event to its parent.
        if (!m_d->toolTip.isEmpty()) {
            QToolTip::showText(static_cast<QHelpEvent *>(e)->globalPos(), m_d->toolTip, this);
            e->accept();
        } else {
            e->ignore();
        }
        return true;
    case QEvent::LayoutRequest:
        if (m_d->layout) {
            m_d->layout->clearPendingRequest();
            m_d->layout->activate();
            if (parentWidget() && parentWidget()->m_d->childChanged)
                parentWidget()->m_d->childChanged(this);
        }
        return true;
    case QEvent::EnabledChange:
    case QEvent::FontChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
    case QEvent::ParentChange:
    case QEvent::WindowTitleChange:
    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
    case QEvent::MouseTrackingChange:
        changeEvent(e);
        return true;
    default:
        return QObject::event(e);
    }
}

/*!
    Handles a paint event: paint the widget with a QPainter, at least the part that \a event names. Painting
    outside the widget or its visible part has no effect. The default does nothing.

    \sa update()
*/
void QWidget::paintEvent(QPaintEvent *event) { (void)event; }

/*!
    Handles the resize \a event. The default does nothing; layouts are updated by the widget itself.

    \sa resize()
*/
void QWidget::resizeEvent(QResizeEvent *event) { (void)event; }

/*!
    Handles the move \a event. The default does nothing.

    \sa move()
*/
void QWidget::moveEvent(QMoveEvent *event) { (void)event; }

/*!
    Handles the show \a event: the widget has been, or is about to be, shown. The default does nothing.
*/
void QWidget::showEvent(QShowEvent *event) { (void)event; }

/*!
    Handles the hide \a event: the widget has been hidden. The default does nothing.
*/
void QWidget::hideEvent(QHideEvent *event) { (void)event; }

/*!
    Handles the close event \a event, sent when a window is asked to close. Call
    \c{event->ignore()} to keep it open. The default accepts the event.

    \sa close()
*/
void QWidget::closeEvent(QCloseEvent *event) { event->accept(); }

/*!
    Handles the mouse press \a event. If the widget does not use the event, it should ignore it, which the default does,
    so that the parent gets it.
*/
void QWidget::mousePressEvent(QMouseEvent *event) { event->ignore(); }

/*!
    Handles the mouse release \a event. The default ignores it.
*/
void QWidget::mouseReleaseEvent(QMouseEvent *event) { event->ignore(); }

/*!
    Handles the double click \a event. The default calls mousePressEvent().
*/
void QWidget::mouseDoubleClickEvent(QMouseEvent *event) { mousePressEvent(event); }

/*!
    Handles the mouse move \a event. The default ignores it. The widget gets it when a button is down, and when
    mouse tracking is on.

    \sa setMouseTracking()
*/
void QWidget::mouseMoveEvent(QMouseEvent *event) { event->ignore(); }

/*!
    Handles the wheel \a event. The default ignores it.
*/
void QWidget::wheelEvent(QWheelEvent *event) { event->ignore(); }

/*!
    Handles the key press \a event. Only the widget that has the focus gets it. The default ignores the
    event, so that the parent gets it.
*/
void QWidget::keyPressEvent(QKeyEvent *event) { event->ignore(); }

/*!
    Handles the key release \a event. The default ignores it.
*/
void QWidget::keyReleaseEvent(QKeyEvent *event) { event->ignore(); }

/*!
    Handles the focus-in \a event: the widget got the keyboard focus. The default does nothing but repaint.
*/
void QWidget::focusInEvent(QFocusEvent *event) { (void)event; }

/*!
    Handles the focus-out \a event: the widget lost the keyboard focus. The default does nothing but repaint.
*/
void QWidget::focusOutEvent(QFocusEvent *event) { (void)event; }

/*!
    Handles the enter \a event: the mouse entered the widget. The default does nothing but repaint.

    \sa leaveEvent()
*/
void QWidget::enterEvent(QEnterEvent *event) {
    (void)event;
    update();
}

/*!
    Handles the leave \a event: the mouse left the widget. The default does nothing but repaint.

    \sa enterEvent()
*/
void QWidget::leaveEvent(QEvent *event) {
    (void)event;
    update();
}

/*!
    Handles the change \a event, which is of the type EnabledChange, FontChange, PaletteChange, StyleChange,
    ParentChange, WindowTitleChange, WindowActivate, WindowDeactivate or MouseTrackingChange. The default repaints the
    widget, and lays it out again if its size hint may have changed.
*/
void QWidget::changeEvent(QEvent *event) {
    switch (event->type()) {
    case QEvent::FontChange:
    case QEvent::StyleChange:
        updateGeometry();
        update();
        break;
    case QEvent::EnabledChange:
    case QEvent::PaletteChange:
        update();
        break;
    default:
        break;
    }
}
