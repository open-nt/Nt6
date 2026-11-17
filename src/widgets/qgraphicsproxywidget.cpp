// MIT License - see LICENSE
#include <QtWidgets/qgraphicsproxywidget.h>

#include "qwidget_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qlogging.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qgraphicssceneevent.h>
#include <QtWidgets/qwidget.h>

#include <cmath>

using QtPrivate::WidgetAccess;

namespace {

// Sends `event` to `target` and then to its parents up to `root` until one accepts it. Returns the widget that
// accepted, or null.
template <typename MakeEvent> QWidget *deliver(QWidget *target, QWidget *root, const QPoint &rootPos, MakeEvent make) {
    for (QWidget *w = target; w; w = w->parentWidget()) {
        const QPoint local = w->mapFrom(root, rootPos);
        auto event = make(QPointF(local));
        event.setAccepted(true);
        QCoreApplication::sendEvent(w, &event);
        if (event.isAccepted())
            return w;
        if (w == root)
            break;
    }
    return nullptr;
}

} // namespace

/*!
    \class QGraphicsProxyWidget
    \brief The QGraphicsProxyWidget class shows a QWidget in a QGraphicsScene.

    The proxy owns the widget: it lays it out at the proxy's size, paints it into the scene and passes it the mouse,
    wheel and key events that reach the proxy.

    \code
    auto *proxy = new QGraphicsProxyWidget(parentItem);
    proxy->setWidget(new MapWidget);
    proxy->resize(400, 300);
    \endcode

    \differences
    The size hints are those of the widget, and its size policy is not taken over. The widget is painted into an image that the proxy draws, so it
    scales with the view but is resampled. Popups of the widget, such as the list of a combo box, open inside the
    widget. Drag and drop, context menus, tool tips and input methods are not passed on, and there is no
    \c createProxyForChildWidget().
    \enddifferences
*/

/*!
    Constructs a proxy without a widget, with the given \a parent. \a wFlags is accepted for compatibility.
*/
QGraphicsProxyWidget::QGraphicsProxyWidget(QGraphicsItem *parent, Qt::WindowFlags wFlags)
    : QGraphicsWidget(parent, wFlags) {
    setFlag(ItemIsFocusable);
    setAcceptHoverEvents(true);
}

/*!
    Destroys the proxy and its widget.
*/
QGraphicsProxyWidget::~QGraphicsProxyWidget() {
    if (m_widget) {
        WidgetAccess::data(m_widget.data()).proxyUpdate = nullptr;
        delete m_widget.data();
    }
}

/*!
    Shows \a widget in the proxy, taking ownership of it; the widget must be a window (have no parent). The previous
    widget is given back to the caller, hidden. The proxy takes the widget's size if it has none of its own.
*/
void QGraphicsProxyWidget::setWidget(QWidget *widget) {
    if (widget == m_widget)
        return;
    if (m_widget) {
        WidgetAccess::data(m_widget.data()).proxyUpdate = nullptr;
        m_widget->hide();
    }
    prepareGeometryChange();
    m_widget = widget;
    m_cacheValid = false;
    if (!widget)
        return;
    if (widget->parentWidget()) {
        qWarning("QGraphicsProxyWidget::setWidget: the widget must not have a parent");
        widget->setParent(nullptr);
    }
    WidgetAccess::data(widget).proxyUpdate = [this](const QRegion &region) { widgetDirty(region); };
    if (size().isEmpty()) {
        const QSize hint = widget->size().isEmpty() ? widget->sizeHint() : widget->size();
        resize(QSizeF(hint));
    }
    widget->resize(size().toSize());
    widget->show();
    updateGeometry();
    update();
}

/*!
    Returns the widget, or \c nullptr.
*/
QWidget *QGraphicsProxyWidget::widget() const { return m_widget; }

/*!
    Returns the rectangle of \a widget, the proxy's widget or one of its children, in proxy coordinates.
*/
QRectF QGraphicsProxyWidget::subWidgetRect(const QWidget *widget) const {
    if (!m_widget || !widget)
        return QRectF();
    if (widget == m_widget)
        return rect();
    if (!m_widget->isAncestorOf(widget))
        return QRectF();
    return QRectF(QRect(widget->mapTo(m_widget.data(), QPoint(0, 0)), widget->size()));
}

/*!
    \reimp

    Moves and resizes the proxy to \a rect, in parent coordinates, and resizes its widget to the same size.
*/
void QGraphicsProxyWidget::setGeometry(const QRectF &rect) {
    const QSizeF old = size();
    QGraphicsWidget::setGeometry(rect);
    if (size() == old)
        return;
    m_cacheValid = false;
    if (m_widget)
        m_widget->resize(size().toSize());
    update();
}

/*!
    \reimp

    Returns the size hints of the widget: its minimum size hint and minimum size, its size hint, and its maximum
    size. Without a widget the proxy has no size of its own.
*/
QSizeF QGraphicsProxyWidget::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    if (!m_widget)
        return QGraphicsWidget::sizeHint(which, constraint);
    switch (which) {
    case Qt::MinimumSize: {
        const QSize hint = m_widget->minimumSizeHint().isValid() ? m_widget->minimumSizeHint() : QSize(0, 0);
        return QSizeF(hint.expandedTo(m_widget->minimumSize()).boundedTo(m_widget->maximumSize()));
    }
    case Qt::PreferredSize: {
        const QSize hint = m_widget->sizeHint().isValid() ? m_widget->sizeHint() : m_widget->size();
        return QSizeF(hint.expandedTo(m_widget->minimumSize()).boundedTo(m_widget->maximumSize()));
    }
    case Qt::MaximumSize:
        return QSizeF(m_widget->maximumSize());
    default:
        return QSizeF(-1, -1);
    }
}

/*!
    \reimp

    Draws the widget, rendered again if it changed since the last time.
*/
void QGraphicsProxyWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    (void)option;
    (void)widget;
    if (!m_widget || m_widget->isHidden())
        return;
    const QSize size = m_widget->size();
    if (size.isEmpty())
        return;
    if (!m_cacheValid || m_cache.size() != size) {
        m_cache = QImage(size, QImage::Format_ARGB32_Premultiplied);
        m_cache.fill(Qt::transparent);
        m_widget->render(&m_cache);
        m_cacheValid = true;
    }
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->drawImage(QPointF(0, 0), m_cache);
}

/*!
    \reimp
*/
int QGraphicsProxyWidget::type() const { return Type; }

void QGraphicsProxyWidget::widgetDirty(const QRegion &region) {
    m_cacheValid = false;
    update(QRectF(region.boundingRect()));
}

/*!
    \reimp
*/
bool QGraphicsProxyWidget::sceneEvent(QEvent *event) { return QGraphicsWidget::sceneEvent(event); }

void QGraphicsProxyWidget::sendMouse(QGraphicsSceneMouseEvent *event, QEvent::Type type) {
    if (!m_widget) {
        event->ignore();
        return;
    }
    const QPoint rootPos = event->pos().toPoint();
    QWidget *target = m_grab && (type == QEvent::MouseMove || type == QEvent::MouseButtonRelease)
                          ? m_grab.data()
                          : WidgetAccess::widgetAt(m_widget.data(), rootPos);
    if (!target)
        target = m_widget;
    const QPointF global = event->screenPos();
    QWidget *accepted = deliver(target, m_widget.data(), rootPos, [&](const QPointF &local) {
        return QMouseEvent(type, local, global, event->button(), event->buttons(), event->modifiers());
    });
    if (type == QEvent::MouseButtonPress || type == QEvent::MouseButtonDblClick)
        m_grab = accepted ? accepted : target;
    if (type == QEvent::MouseButtonRelease && event->buttons() == Qt::NoButton)
        m_grab = nullptr;
    event->setAccepted(accepted != nullptr || type != QEvent::MouseButtonPress);
}

/*!
    \reimp

    Passes the press to the widget under the mouse, which receives the moves and the release until the buttons are
    up.
*/
void QGraphicsProxyWidget::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    sendMouse(event, QEvent::MouseButtonPress);
}

/*!
    \reimp
*/
void QGraphicsProxyWidget::mouseMoveEvent(QGraphicsSceneMouseEvent *event) { sendMouse(event, QEvent::MouseMove); }

/*!
    \reimp
*/
void QGraphicsProxyWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    sendMouse(event, QEvent::MouseButtonRelease);
}

/*!
    \reimp
*/
void QGraphicsProxyWidget::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    sendMouse(event, QEvent::MouseButtonDblClick);
}

/*!
    \reimp

    Passes the move to the widget under the mouse if it tracks the mouse.
*/
void QGraphicsProxyWidget::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    if (!m_widget)
        return;
    const QPoint rootPos = event->pos().toPoint();
    QWidget *target = WidgetAccess::widgetAt(m_widget.data(), rootPos);
    if (!target || !target->hasMouseTracking())
        return;
    QMouseEvent move(QEvent::MouseMove, QPointF(target->mapFrom(m_widget.data(), rootPos)), event->screenPos(),
                     Qt::NoButton, Qt::NoButton, event->modifiers());
    QCoreApplication::sendEvent(target, &move);
}

/*!
    \reimp

    Passes the wheel event to the widget under the mouse and its parents until one accepts it.
*/
void QGraphicsProxyWidget::wheelEvent(QGraphicsSceneWheelEvent *event) {
    if (!m_widget) {
        event->ignore();
        return;
    }
    const QPoint rootPos = event->pos().toPoint();
    QWidget *target = WidgetAccess::widgetAt(m_widget.data(), rootPos);
    const QPoint angle = event->orientation() == Qt::Vertical ? QPoint(0, event->delta()) : QPoint(event->delta(), 0);
    QWidget *accepted = deliver(target ? target : m_widget.data(), m_widget.data(), rootPos, [&](const QPointF &local) {
        return QWheelEvent(local, event->screenPos(), event->pixelDelta(), angle, event->buttons(), event->modifiers());
    });
    event->setAccepted(accepted != nullptr);
}

/*!
    \reimp

    Passes the key to the widget that has the focus in the proxy's widget.
*/
void QGraphicsProxyWidget::keyPressEvent(QKeyEvent *event) {
    QWidget *target = m_widget ? WidgetAccess::focusWidgetOf(m_widget.data()) : nullptr;
    if (!target)
        target = m_widget;
    if (!target) {
        event->ignore();
        return;
    }
    event->setAccepted(true);
    QCoreApplication::sendEvent(target, event);
}

/*!
    \reimp
*/
void QGraphicsProxyWidget::keyReleaseEvent(QKeyEvent *event) {
    QWidget *target = m_widget ? WidgetAccess::focusWidgetOf(m_widget.data()) : nullptr;
    if (!target)
        target = m_widget;
    if (!target) {
        event->ignore();
        return;
    }
    event->setAccepted(true);
    QCoreApplication::sendEvent(target, event);
}

/*!
    \reimp
*/
void QGraphicsProxyWidget::focusInEvent(QFocusEvent *event) {
    QGraphicsWidget::focusInEvent(event);
    if (m_widget && !WidgetAccess::focusWidgetOf(m_widget.data()))
        m_widget->setFocus();
}

#include "moc_qgraphicsproxywidget.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
