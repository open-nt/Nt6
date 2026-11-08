// MIT License - see LICENSE
#include "qwidgetwindow_p.h"

#include "qtooltip_p.h"
#include "qwidget_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtGui/qevent.h>
#include <QtWidgets/qtooltip.h>
#include <QtWidgets/qwidget.h>

#include <algorithm>

using QtPrivate::WidgetAccess;

QWidgetWindow::QWidgetWindow(QWidget *widget) : m_widget(widget) {}

QWidgetWindow::~QWidgetWindow() = default;

void QWidgetWindow::forget(QWidget *widget) {
    if (m_grab == widget)
        m_grab = nullptr;
    if (m_under == widget)
        m_under = nullptr;
    if (m_popup == widget)
        m_popup = nullptr;
}

void QWidgetWindow::paintEvent(QPaintEvent *event) {
    if (m_widget)
        WidgetAccess::paintTree(m_widget, event->region());
}

void QWidgetWindow::resizeEvent(QResizeEvent *event) {
    QPaintDeviceWindow::resizeEvent(event);
    if (!m_widget)
        return;
    QWidget::Data &d = WidgetAccess::data(m_widget);
    d.geometry = geometry();
    d.geometrySet = true;
    QResizeEvent resize(event->size(), event->oldSize());
    QCoreApplication::sendEvent(m_widget, &resize);
}

void QWidgetWindow::moveEvent(QMoveEvent *event) {
    if (!m_widget)
        return;
    QWidget::Data &d = WidgetAccess::data(m_widget);
    d.geometry = geometry();
    d.geometrySet = true;
    QMoveEvent move(event->pos(), event->oldPos());
    QCoreApplication::sendEvent(m_widget, &move);
}

void QWidgetWindow::showEvent(QShowEvent *event) { (void)event; }

void QWidgetWindow::hideEvent(QHideEvent *event) {
    (void)event;
    cancelToolTip();
    m_grab = nullptr;
    m_popup = nullptr;
    if (m_under) {
        QEvent leave(QEvent::Leave);
        for (QWidget *w = m_under; w; w = w->parentWidget()) {
            WidgetAccess::setUnderMouse(w, false);
            QCoreApplication::sendEvent(w, &leave);
        }
        m_under = nullptr;
    }
    if (m_widget)
        WidgetAccess::sendHideEvents(m_widget);
}

void QWidgetWindow::closeEvent(QCloseEvent *event) {
    if (!m_widget)
        return;
    QCloseEvent close;
    QCoreApplication::sendEvent(m_widget, &close);
    event->setAccepted(close.isAccepted());
}

void QWidgetWindow::focusInEvent(QFocusEvent *event) {
    (void)event;
    if (m_widget)
        WidgetAccess::windowActivated(m_widget, true);
}

void QWidgetWindow::focusOutEvent(QFocusEvent *event) {
    (void)event;
    if (m_widget)
        WidgetAccess::windowActivated(m_widget, false);
}

namespace {

// Sends the event that make() builds to `target` and then to its ancestors until a widget accepts it,
// with the position `startPos` translated for each of them. A disabled widget swallows the event.
template <typename Make> bool propagate(QWidget *target, const QPointF &startPos, Make make) {
    QPointF pos = startPos;
    for (QWidget *w = target; w; w = w->parentWidget()) {
        if (!w->isEnabled())
            return true;
        auto event = make(pos);
        event.accept();
        QCoreApplication::sendEvent(w, &event);
        if (event.isAccepted())
            return true;
        pos += QPointF(w->pos());
    }
    return false;
}

} // namespace

void QWidgetWindow::updateUnderMouse(QWidget *target, const QPointF &local, const QPointF &global) {
    if (target == m_under)
        return;
    QWidget *old = m_under;
    m_under = target;
    // Leave the widgets that the mouse is no longer over, bottom up, then enter the new ones, top down.
    std::vector<QWidget *> newChain;
    for (QWidget *w = target; w; w = w->parentWidget())
        newChain.push_back(w);
    for (QWidget *w = old; w; w = w->parentWidget()) {
        if (std::find(newChain.begin(), newChain.end(), w) != newChain.end())
            break;
        WidgetAccess::setUnderMouse(w, false);
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(w, &leave);
    }
    std::vector<QWidget *> oldChain;
    for (QWidget *w = old; w; w = w->parentWidget())
        oldChain.push_back(w);
    for (auto it = newChain.rbegin(); it != newChain.rend(); ++it) {
        QWidget *w = *it;
        if (std::find(oldChain.begin(), oldChain.end(), w) != oldChain.end())
            continue;
        WidgetAccess::setUnderMouse(w, true);
        const QPoint offset = w->mapTo(m_widget, QPoint(0, 0));
        QEnterEvent enter(local - QPointF(offset), global);
        QCoreApplication::sendEvent(w, &enter);
    }
}

void QWidgetWindow::deliverMouse(QMouseEvent *event, QWidget *target) {
    if (!target)
        return;
    const QPointF local = event->position() - QPointF(target->mapTo(m_widget, QPoint(0, 0)));
    const bool handled = propagate(target, local, [&](const QPointF &pos) {
        return QMouseEvent(event->type(), pos, event->globalPosition(), event->button(), event->buttons(),
                           event->modifiers());
    });
    event->setAccepted(handled);
}

void QWidgetWindow::mousePressEvent(QMouseEvent *event) {
    if (!m_widget)
        return;
    cancelToolTip();
    if (m_popup && !m_grab) {
        // A press outside the popup closes it, and does nothing else.
        bool inside = false;
        for (QWidget *w = WidgetAccess::widgetAt(m_widget, event->pos()); w; w = w->parentWidget())
            inside = inside || w == m_popup;
        if (!inside) {
            QWidget *popup = m_popup;
            m_popup = nullptr;
            popup->hide();
            event->accept();
            return;
        }
    }
    QWidget *target = m_grab ? m_grab.data() : WidgetAccess::widgetAt(m_widget, event->pos());
    if (!m_grab) {
        m_grab = target;
        updateUnderMouse(target, event->position(), event->globalPosition());
        if (target->isEnabled() && (target->focusPolicy() & Qt::ClickFocus))
            target->setFocus();
    }
    deliverMouse(event, target);
}

void QWidgetWindow::mouseDoubleClickEvent(QMouseEvent *event) {
    if (!m_widget)
        return;
    QWidget *target = m_grab ? m_grab.data() : WidgetAccess::widgetAt(m_widget, event->pos());
    if (!m_grab)
        m_grab = target;
    deliverMouse(event, target);
}

void QWidgetWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (!m_widget)
        return;
    QWidget *target = m_grab ? m_grab.data() : WidgetAccess::widgetAt(m_widget, event->pos());
    deliverMouse(event, target);
    if (event->buttons() == Qt::NoButton) {
        m_grab = nullptr;
        // The mouse may have moved to another widget while the button was down.
        QWidget *under = WidgetAccess::widgetAt(m_widget, event->pos());
        updateUnderMouse(under, event->position(), event->globalPosition());
    }
}

void QWidgetWindow::mouseMoveEvent(QMouseEvent *event) {
    if (!m_widget)
        return;
    if (m_grab && event->buttons() != Qt::NoButton) {
        cancelToolTip();
        deliverMouse(event, m_grab);
        return;
    }
    QWidget *under = WidgetAccess::widgetAt(m_widget, event->pos());
    updateUnderMouse(under, event->position(), event->globalPosition());
    toolTipMoved(under, event->pos(), event->globalPos());
    if (under->hasMouseTracking())
        deliverMouse(event, under);
}

void QWidgetWindow::wheelEvent(QWheelEvent *event) {
    if (!m_widget)
        return;
    cancelToolTip();
    QWidget *target = WidgetAccess::widgetAt(m_widget, event->position().toPoint());
    const QPointF local = event->position() - QPointF(target->mapTo(m_widget, QPoint(0, 0)));
    const bool handled = propagate(target, local, [&](const QPointF &pos) {
        return QWheelEvent(pos, event->globalPosition(), event->pixelDelta(), event->angleDelta(), event->buttons(),
                           event->modifiers());
    });
    event->setAccepted(handled);
}

void QWidgetWindow::keyPressEvent(QKeyEvent *event) {
    if (!m_widget)
        return;
    cancelToolTip();
    QWidget *target = WidgetAccess::focusWidgetOf(m_widget);
    if (!target)
        target = m_widget;
    const bool handled = propagate(target, QPointF(), [&](const QPointF &) {
        return QKeyEvent(event->type(), event->key(), event->modifiers(), event->text(), event->isAutoRepeat(),
                         static_cast<ushort>(event->count()));
    });
    event->setAccepted(handled);
}

void QWidgetWindow::keyReleaseEvent(QKeyEvent *event) {
    if (!m_widget)
        return;
    QWidget *target = WidgetAccess::focusWidgetOf(m_widget);
    if (!target)
        target = m_widget;
    const bool handled = propagate(target, QPointF(), [&](const QPointF &) {
        return QKeyEvent(event->type(), event->key(), event->modifiers(), event->text(), event->isAutoRepeat(),
                         static_cast<ushort>(event->count()));
    });
    event->setAccepted(handled);
}

bool QWidgetWindow::event(QEvent *event) {
    if (event->type() == QEvent::Enter && m_widget) {
        auto *e = static_cast<QEnterEvent *>(event);
        QWidget *under = WidgetAccess::widgetAt(m_widget, e->pos());
        updateUnderMouse(under, e->position(), e->globalPosition());
        return true;
    }
    if (event->type() == QEvent::Leave && m_widget) {
        cancelToolTip();
        updateUnderMouse(nullptr, QPointF(), QPointF());
        return true;
    }
    return QPaintDeviceWindow::event(event);
}

// The mouse is at `pos` in the window, over `under`: a tool tip that is shown stays while the mouse is on its widget,
// and otherwise the mouse has to rest before the next one is asked for.
void QWidgetWindow::toolTipMoved(QWidget *under, const QPoint &pos, const QPoint &globalPos) {
    const QPoint inWidget = pos - WidgetAccess::windowOffset(under);
    if (QtPrivate::toolTipFollowsMouse(under, inWidget))
        return;
    if (m_tipTimer)
        killTimer(m_tipTimer);
    m_tipWidget = under;
    m_tipPos = pos;
    m_tipGlobalPos = globalPos;
    m_tipTimer = startTimer(QtPrivate::toolTipDelay());
}

void QWidgetWindow::cancelToolTip() {
    if (m_tipTimer) {
        killTimer(m_tipTimer);
        m_tipTimer = 0;
    }
    QToolTip::hideText();
}

void QWidgetWindow::timerEvent(QTimerEvent *event) {
    if (event->timerId() != m_tipTimer) {
        QPaintDeviceWindow::timerEvent(event);
        return;
    }
    killTimer(m_tipTimer);
    m_tipTimer = 0;
    // Ask the widget under the mouse, and its parents in turn, until one has a tool tip.
    QPoint pos = m_tipPos - WidgetAccess::windowOffset(m_tipWidget);
    for (QWidget *w = m_tipWidget; w; w = w->parentWidget()) {
        QHelpEvent help(QEvent::ToolTip, pos, m_tipGlobalPos);
        help.accept();
        QCoreApplication::sendEvent(w, &help);
        if (help.isAccepted())
            return;
        pos += w->pos();
    }
}
