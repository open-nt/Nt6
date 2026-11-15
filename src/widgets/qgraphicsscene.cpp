// MIT License - see LICENSE
#include <QtWidgets/qgraphicsscene.h>

#include "qgraphicsscene_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qgraphicseffect.h>
#include <QtWidgets/qgraphicsproxywidget.h>
#include <QtWidgets/qgraphicssceneevent.h>
#include <QtWidgets/qgraphicsview.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>
#include <cmath>
#include <vector>

using ItemAccess = QGraphicsItemAccess;

// ---- the private side ----------------------------------------------------------------------------------------

void QGraphicsScene::Private::markDirty(const QRectF &rect) {
    if (rect.isEmpty())
        return;
    const QRectF r = rect.adjusted(-1, -1, 1, 1);
    for (QGraphicsView *view : views)
        QGraphicsViewAccess::d(view)->sceneDirty(r);
    dirty.append(r);
    queueUpdate();
}

void QGraphicsScene::Private::queueUpdate() {
    if (!updateQueued) {
        updateQueued = true;
        QMetaObject::invokeMethod(q, [this] { processDirty(); }, Qt::QueuedConnection);
    }
}

void QGraphicsScene::Private::addPending(QGraphicsItem *item) {
    pending.append(item);
    queueUpdate();
}

void QGraphicsScene::Private::processDirty() {
    updateQueued = false;
    const QList<QGraphicsItem *> born = pending;
    pending.clear();
    for (QGraphicsItem *item : born) {
        if (item->scene() == q)
            markDirty(ItemAccess::subtreeSceneRect(item));
    }
    updateQueued = false;
    const QList<QRectF> rects = dirty;
    dirty.clear();
    const QRectF current = q->sceneRect();
    if (current != lastSceneRect) {
        lastSceneRect = current;
        for (QGraphicsView *view : views)
            view->updateSceneRect(current);
        emit q->sceneRectChanged(current);
    }
    if (!rects.isEmpty())
        emit q->changed(rects);
}

void QGraphicsScene::Private::adopt(QGraphicsItem *item) {
    QGraphicsItem::Private *id = ItemAccess::d(item);
    QGraphicsScene *old = id->scene;
    if (old == q)
        return;
    ItemAccess::itemChange(item, QGraphicsItem::ItemSceneChange, QVariant::fromValue(q));
    id->scene = q;
    if (!id->parent && !topLevel.contains(item))
        topLevel.append(item);
    for (QGraphicsItem *child : id->children)
        adopt(child);
    if (id->wantsFocus) {
        id->wantsFocus = false;
        q->setFocusItem(item);
    }
    markDirty(ItemAccess::subtreeSceneRect(item));
    ItemAccess::itemChange(item, QGraphicsItem::ItemSceneHasChanged, QVariant::fromValue(q));
}

void QGraphicsScene::Private::release(QGraphicsItem *item) {
    QGraphicsItem::Private *id = ItemAccess::d(item);
    if (id->scene != q)
        return;
    ItemAccess::itemChange(item, QGraphicsItem::ItemSceneChange,
                           QVariant::fromValue(static_cast<QGraphicsScene *>(nullptr)));
    markDirty(ItemAccess::subtreeSceneRect(item));
    forget(item);
    for (QGraphicsItem *child : id->children)
        release(child);
    topLevel.removeAll(item);
    id->scene = nullptr;
    ItemAccess::itemChange(item, QGraphicsItem::ItemSceneHasChanged,
                           QVariant::fromValue(static_cast<QGraphicsScene *>(nullptr)));
}

void QGraphicsScene::Private::forget(QGraphicsItem *item) {
    const auto inside = [item](QGraphicsItem *other) { return other && (other == item || item->isAncestorOf(other)); };
    if (inside(focusItem)) {
        QGraphicsItem *old = focusItem;
        focusItem = nullptr;
        if (!ItemAccess::d(old)->destroying) {
            QFocusEvent out(QEvent::FocusOut);
            ItemAccess::sceneEvent(old, &out);
        }
        emit q->focusItemChanged(nullptr, old, Qt::OtherFocusReason);
    }
    if (inside(grabber))
        grabber = nullptr;
    pending.erase(std::remove_if(pending.begin(), pending.end(), inside), pending.end());
    if (inside(keyboardGrabber))
        keyboardGrabber = nullptr;
    hoverItems.erase(std::remove_if(hoverItems.begin(), hoverItems.end(),
                                    [&](QGraphicsItem *h) {
                                        if (!inside(h))
                                            return false;
                                        ItemAccess::d(h)->underMouse = false;
                                        return true;
                                    }),
                     hoverItems.end());
    for (QGraphicsItem *other : paintOrder())
        ItemAccess::d(other)->filters.removeAll(item);
}

void QGraphicsScene::Private::collect(QGraphicsItem *item, QList<QGraphicsItem *> &out) const {
    const QList<QGraphicsItem *> children = ItemAccess::sortedChildren(ItemAccess::d(item)->children);
    const auto behind = [](const QGraphicsItem *c) {
        return (c->flags() & QGraphicsItem::ItemStacksBehindParent) ||
               (c->zValue() < 0 && (c->flags() & QGraphicsItem::ItemNegativeZStacksBehindParent));
    };
    for (QGraphicsItem *child : children) {
        if (behind(child))
            collect(child, out);
    }
    out.append(item);
    for (QGraphicsItem *child : children) {
        if (!behind(child))
            collect(child, out);
    }
}

QList<QGraphicsItem *> QGraphicsScene::Private::paintOrder() const {
    QList<QGraphicsItem *> out;
    for (QGraphicsItem *item : ItemAccess::sortedChildren(topLevel))
        collect(item, out);
    return out;
}

QList<QGraphicsItem *> QGraphicsScene::Private::itemsAt(const QPointF &pos) const {
    QList<QGraphicsItem *> out;
    const QList<QGraphicsItem *> order = paintOrder();
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        QGraphicsItem *item = *it;
        if (!item->isVisible())
            continue;
        if (item->contains(item->mapFromScene(pos)) && ItemAccess::passesAncestorClips(item, pos))
            out.append(item);
    }
    return out;
}

void QGraphicsScene::Private::mapMouseEvent(QGraphicsSceneMouseEvent *event, const QGraphicsItem *item) {
    event->setPos(item->mapFromScene(event->scenePos()));
    event->setLastPos(item->mapFromScene(event->lastScenePos()));
    for (Qt::MouseButton b : {Qt::LeftButton, Qt::RightButton, Qt::MiddleButton, Qt::BackButton, Qt::ForwardButton})
        event->setButtonDownPos(b, item->mapFromScene(event->buttonDownScenePos(b)));
}

void QGraphicsScene::Private::updateHover(const QPointF &scenePos, const QPointF &screenPos, QWidget *widget,
                                          Qt::KeyboardModifiers modifiers) {
    QList<QGraphicsItem *> chain; // outermost first
    for (QGraphicsItem *item : itemsAt(scenePos)) {
        if (!item->isEnabled())
            break;
        bool found = false;
        for (QGraphicsItem *a = item; a; a = a->parentItem()) {
            if (a->acceptHoverEvents()) {
                chain.prepend(a);
                found = true;
            }
        }
        if (found || item->acceptHoverEvents())
            break;
    }
    const auto send = [&](QGraphicsItem *item, QEvent::Type type) {
        QGraphicsSceneHoverEvent event(type);
        event.setWidget(widget);
        event.setScenePos(scenePos);
        event.setScreenPos(screenPos);
        event.setPos(item->mapFromScene(scenePos));
        event.setLastScenePos(scenePos);
        event.setLastPos(item->mapFromScene(scenePos));
        event.setModifiers(modifiers);
        q->sendEvent(item, &event);
    };
    for (auto it = hoverItems.rbegin(); it != hoverItems.rend(); ++it) {
        if (!chain.contains(*it)) {
            ItemAccess::d(*it)->underMouse = false;
            send(*it, QEvent::GraphicsSceneHoverLeave);
        }
    }
    const QList<QGraphicsItem *> old = hoverItems;
    hoverItems = chain;
    for (QGraphicsItem *item : chain) {
        if (!old.contains(item)) {
            ItemAccess::d(item)->underMouse = true;
            send(item, QEvent::GraphicsSceneHoverEnter);
        } else {
            send(item, QEvent::GraphicsSceneHoverMove);
        }
    }
}

void QGraphicsScene::Private::clearHover() {
    const QList<QGraphicsItem *> old = hoverItems;
    hoverItems.clear();
    for (auto it = old.rbegin(); it != old.rend(); ++it) {
        ItemAccess::d(*it)->underMouse = false;
        QGraphicsSceneHoverEvent event(QEvent::GraphicsSceneHoverLeave);
        q->sendEvent(*it, &event);
    }
}

QGraphicsItem *QGraphicsScene::Private::cursorItemAt(const QPointF &pos) const {
    for (QGraphicsItem *item : itemsAt(pos)) {
        for (QGraphicsItem *a = item; a; a = a->parentItem()) {
            if (a->hasCursor())
                return a;
        }
    }
    return nullptr;
}

QGraphicsItem *QGraphicsScene::Private::focusableAncestor(QGraphicsItem *item) {
    for (QGraphicsItem *a = item; a; a = a->parentItem()) {
        if ((a->flags() & QGraphicsItem::ItemIsFocusable) && a->isEnabled())
            return a;
        if (a->flags() & QGraphicsItem::ItemStopsClickFocusPropagation)
            break;
    }
    return nullptr;
}

void QGraphicsScene::Private::drawItems(QPainter *painter, const QTransform &viewTransform, const QRectF &exposed,
                                        QWidget *widget) {
    const QTransform saved = painter->worldTransform();
    for (QGraphicsItem *item : ItemAccess::sortedChildren(topLevel))
        drawItem(painter, item, viewTransform, exposed, widget, 1.0, true, QRect(), false);
    painter->setWorldTransform(saved);
}

void QGraphicsScene::Private::drawItem(QPainter *painter, QGraphicsItem *item, const QTransform &viewTransform,
                                       const QRectF &exposed, QWidget *widget, qreal parentOpacity,
                                       bool parentPropagates, const QRect &clip, bool clipped) {
    QGraphicsItem::Private *id = ItemAccess::d(item);
    if (id->explicitlyHidden)
        return;
    const qreal opacity =
        id->opacity *
        ((parentPropagates && !(id->flags & QGraphicsItem::ItemIgnoresParentOpacity)) ? parentOpacity : 1.0);
    const bool propagates = !(id->flags & QGraphicsItem::ItemDoesntPropagateOpacityToChildren);
    if (opacity <= 0.0 && propagates)
        return;
    const QTransform toDevice = item->sceneTransform() * viewTransform;

    // The clip of the children: the shape of an item that clips them, intersected with the clip so far.
    QRect childClip = clip;
    bool childClipped = clipped;
    if (id->flags & QGraphicsItem::ItemClipsChildrenToShape) {
        const QRect own = toDevice.mapRect(item->shape().boundingRect()).toAlignedRect();
        childClip = clipped ? clip.intersected(own) : own;
        childClipped = true;
    }

    const QList<QGraphicsItem *> children = ItemAccess::sortedChildren(id->children);
    const auto behind = [](const QGraphicsItem *c) {
        return (c->flags() & QGraphicsItem::ItemStacksBehindParent) ||
               (c->zValue() < 0 && (c->flags() & QGraphicsItem::ItemNegativeZStacksBehindParent));
    };
    for (QGraphicsItem *child : children) {
        if (behind(child))
            drawItem(painter, child, viewTransform, exposed, widget, opacity, propagates, childClip, childClipped);
    }

    const QRectF bounds = item->boundingRect();
    const QRectF sceneBounds = item->sceneTransform().mapRect(bounds);
    id->paintedRect = sceneBounds;
    if (!(id->flags & QGraphicsItem::ItemHasNoContents) && opacity > 0.0 && sceneBounds.intersects(exposed)) {
        QRect ownClip = clip;
        bool ownClipped = clipped;
        if (id->flags & QGraphicsItem::ItemClipsToShape) {
            const QRect shapeRect = toDevice.mapRect(item->shape().boundingRect()).toAlignedRect();
            ownClip = clipped ? clip.intersected(shapeRect) : shapeRect;
            ownClipped = true;
        }
        if (!ownClipped || !ownClip.isEmpty()) {
            painter->save();
            painter->resetTransform();
            if (ownClipped)
                painter->setClipRect(ownClip);
            painter->setWorldTransform(toDevice);
            painter->setOpacity(opacity);
            painter->setPen(QPen());
            painter->setBrush(Qt::NoBrush);
            if (widget)
                painter->setFont(widget->font());
            QStyleOptionGraphicsItem option;
            option.rect = bounds.toAlignedRect();
            option.exposedRect = bounds;
            if (id->flags & QGraphicsItem::ItemUsesExtendedStyleOption)
                option.exposedRect = item->mapRectFromScene(exposed).intersected(bounds);
            option.state = QStyle::State_None;
            if (item->isEnabled())
                option.state |= QStyle::State_Enabled;
            if (id->selected)
                option.state |= QStyle::State_Selected;
            if (item->hasFocus())
                option.state |= QStyle::State_HasFocus;
            if (id->underMouse)
                option.state |= QStyle::State_MouseOver;
            if (widget)
                option.palette = widget->palette();
            auto *shadow = qobject_cast<QGraphicsDropShadowEffect *>(id->effect);
            if (shadow && shadow->isEnabled())
                drawWithShadow(painter, item, shadow, toDevice, option, widget);
            else
                item->paint(painter, &option, widget);
            painter->restore();
        }
    }

    for (QGraphicsItem *child : children) {
        if (!behind(child))
            drawItem(painter, child, viewTransform, exposed, widget, opacity, propagates, childClip, childClipped);
    }
}

void QGraphicsScene::Private::drawWithShadow(QPainter *painter, QGraphicsItem *item, QGraphicsDropShadowEffect *shadow,
                                             const QTransform &toDevice, const QStyleOptionGraphicsItem &option,
                                             QWidget *widget) {
    // The item is painted into an image of its device rectangle; the shadow is that image's alpha, in the shadow
    // color, blurred and offset, drawn first.
    const qreal scale = std::sqrt(std::abs(toDevice.determinant()));
    const int blur = static_cast<int>(std::ceil(shadow->blurRadius()));
    const QRect area = toDevice.mapRect(item->boundingRect()).toAlignedRect().adjusted(-1, -1, 1, 1);
    if (area.isEmpty())
        return;
    QImage content(area.size(), QImage::Format_ARGB32_Premultiplied);
    content.fill(Qt::transparent);
    {
        QPainter p(&content);
        p.setRenderHints(painter->renderHints());
        p.setTransform(toDevice * QTransform::fromTranslate(-area.left(), -area.top()));
        p.setPen(QPen());
        p.setBrush(Qt::NoBrush);
        if (widget)
            p.setFont(widget->font());
        item->paint(&p, &option, widget);
    }
    QImage mask(area.width() + 2 * blur, area.height() + 2 * blur, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    const QColor c = shadow->color();
    for (int y = 0; y < area.height(); ++y) {
        const auto *src = reinterpret_cast<const QRgb *>(content.constScanLine(y));
        auto *dst = reinterpret_cast<QRgb *>(mask.scanLine(y + blur)) + blur;
        for (int x = 0; x < area.width(); ++x) {
            const int a = qAlpha(src[x]) * c.alpha() / 255;
            dst[x] = qRgba(c.red() * a / 255, c.green() * a / 255, c.blue() * a / 255, a);
        }
    }
    if (blur > 0) {
        // A separable box blur of the premultiplied pixels.
        const int w = mask.width();
        const int h = mask.height();
        QImage tmp = mask;
        const int n = 2 * blur + 1;
        for (int y = 0; y < h; ++y) {
            const auto *src = reinterpret_cast<const QRgb *>(mask.constScanLine(y));
            auto *dst = reinterpret_cast<QRgb *>(tmp.scanLine(y));
            for (int x = 0; x < w; ++x) {
                int r = 0, g = 0, b = 0, a = 0;
                for (int k = -blur; k <= blur; ++k) {
                    const int xx = std::clamp(x + k, 0, w - 1);
                    r += qRed(src[xx]);
                    g += qGreen(src[xx]);
                    b += qBlue(src[xx]);
                    a += qAlpha(src[xx]);
                }
                dst[x] = qRgba(r / n, g / n, b / n, a / n);
            }
        }
        for (int y = 0; y < h; ++y) {
            auto *dst = reinterpret_cast<QRgb *>(mask.scanLine(y));
            for (int x = 0; x < w; ++x) {
                int r = 0, g = 0, b = 0, a = 0;
                for (int k = -blur; k <= blur; ++k) {
                    const int yy = std::clamp(y + k, 0, h - 1);
                    const QRgb p = reinterpret_cast<const QRgb *>(tmp.constScanLine(yy))[x];
                    r += qRed(p);
                    g += qGreen(p);
                    b += qBlue(p);
                    a += qAlpha(p);
                }
                dst[x] = qRgba(r / n, g / n, b / n, a / n);
            }
        }
    }
    painter->save();
    painter->resetTransform();
    const QPointF offset = shadow->offset() * scale;
    painter->drawImage(QPointF(area.left() - blur + offset.x(), area.top() - blur + offset.y()), mask);
    painter->drawImage(QPointF(area.topLeft()), content);
    painter->restore();
}

// ---- QGraphicsScene ------------------------------------------------------------------------------------------

/*!
    \class QGraphicsScene
    \brief The QGraphicsScene class holds items and passes them input; QGraphicsView shows it.

    Add items with addItem() or the addRect() family. The scene keeps them in a tree, finds them by position with
    items() and itemAt(), and sends them the mouse, hover, wheel and key events that views pass on. Any number of
    views can show the same scene; each redraws what changed.

    \code
    QGraphicsScene scene(0, 0, 1280, 720);
    scene.addRect(QRectF(10, 10, 100, 50), QPen(Qt::white), QBrush(Qt::darkGreen));
    QGraphicsView view(&scene);
    view.show();
    \endcode

    \differences
    Items are found by walking them; there is no BSP index, so itemIndexMethod() is only kept. There is no
    \c addWidget() and no QGraphicsProxyWidget yet, no style, no drag and drop, no context menus, no help events,
    no gestures and no touch events, and the scene has no panels or modality. selectionArea() and
    setSelectionArea() compare bounding rectangles for paths. A scene is active while it is in a view.
    \enddifferences

    \sa QGraphicsView, QGraphicsItem
*/

/*!
    \enum QGraphicsScene::ItemIndexMethod

    How the scene would index its items. Nt walks the items either way.

    \value BspTreeIndex A binary space partitioning tree.
    \value NoIndex No index.
*/

/*!
    \enum QGraphicsScene::SceneLayer

    The layers of a scene.

    \value ItemLayer The items.
    \value BackgroundLayer The background.
    \value ForegroundLayer The foreground.
    \value AllLayers All of them.
*/

/*!
    Constructs a scene with the given \a parent, whose scene rectangle grows with its items.
*/
QGraphicsScene::QGraphicsScene(QObject *parent) : QObject(parent), d(std::make_unique<Private>()) { d->q = this; }

/*!
    Constructs a scene with the scene rectangle \a sceneRect and the given \a parent.
*/
QGraphicsScene::QGraphicsScene(const QRectF &sceneRect, QObject *parent) : QGraphicsScene(parent) {
    setSceneRect(sceneRect);
}

/*!
    Constructs a scene whose scene rectangle is at (\a x, \a y), \a width by \a height, with the given \a parent.
*/
QGraphicsScene::QGraphicsScene(qreal x, qreal y, qreal width, qreal height, QObject *parent)
    : QGraphicsScene(QRectF(x, y, width, height), parent) {}

/*!
    Destroys the scene and its items. The views that showed it show nothing.
*/
QGraphicsScene::~QGraphicsScene() {
    for (QGraphicsView *view : QList<QGraphicsView *>(d->views))
        view->setScene(nullptr);
    clear();
}

/*!
    Returns the scene rectangle: what setSceneRect() set, or the largest area the items have covered.
*/
QRectF QGraphicsScene::sceneRect() const {
    if (d->sceneRectSet)
        return d->sceneRect;
    d->growingRect = d->growingRect.united(itemsBoundingRect());
    return d->growingRect;
}

/*!
    Sets the scene rectangle to \a rect. A null rectangle makes it grow with the items again.
*/
void QGraphicsScene::setSceneRect(const QRectF &rect) {
    d->sceneRectSet = !rect.isNull();
    d->sceneRect = rect;
    d->growingRect = QRectF();
    const QRectF current = sceneRect();
    d->lastSceneRect = current;
    for (QGraphicsView *view : d->views)
        view->updateSceneRect(current);
    emit sceneRectChanged(current);
}

/*!
    Draws \a source of the scene (the scene rectangle if null) into \a target of the painter's device (the whole
    device if null) with \a painter, fitting it as \a aspectRatioMode says.
*/
void QGraphicsScene::render(QPainter *painter, const QRectF &target, const QRectF &source,
                            Qt::AspectRatioMode aspectRatioMode) {
    if (!painter)
        return;
    const QRectF src = source.isNull() ? sceneRect() : source;
    QRectF dst = target;
    if (dst.isNull() && painter->device())
        dst = QRectF(0, 0, painter->device()->width(), painter->device()->height());
    if (src.isEmpty() || dst.isEmpty())
        return;
    qreal sx = dst.width() / src.width();
    qreal sy = dst.height() / src.height();
    if (aspectRatioMode == Qt::KeepAspectRatio)
        sx = sy = std::min(sx, sy);
    else if (aspectRatioMode == Qt::KeepAspectRatioByExpanding)
        sx = sy = std::max(sx, sy);
    const qreal dx = dst.left() + (dst.width() - src.width() * sx) / 2.0;
    const qreal dy = dst.top() + (dst.height() - src.height() * sy) / 2.0;
    const QTransform t = QTransform::fromTranslate(-src.left(), -src.top()) * QTransform::fromScale(sx, sy) *
                         QTransform::fromTranslate(dx, dy) * painter->worldTransform();
    painter->save();
    painter->setWorldTransform(t);
    drawBackground(painter, src);
    d->drawItems(painter, t, src, nullptr);
    painter->setWorldTransform(t);
    drawForeground(painter, src);
    painter->restore();
}

/*!
    Returns the index method.
*/
QGraphicsScene::ItemIndexMethod QGraphicsScene::itemIndexMethod() const { return d->indexMethod; }

/*!
    Sets the index method to \a method. Nt keeps the value.
*/
void QGraphicsScene::setItemIndexMethod(ItemIndexMethod method) { d->indexMethod = method; }

/*!
    Returns the rectangle the visible items cover, in scene coordinates.
*/
QRectF QGraphicsScene::itemsBoundingRect() const {
    QRectF r;
    for (QGraphicsItem *item : d->topLevel)
        r = r.united(ItemAccess::subtreeSceneRect(item));
    return r;
}

/*!
    Returns every item, in \a order: Qt::DescendingOrder lists the topmost first.
*/
QList<QGraphicsItem *> QGraphicsScene::items(Qt::SortOrder order) const {
    QList<QGraphicsItem *> all = d->paintOrder();
    if (order == Qt::DescendingOrder)
        std::reverse(all.begin(), all.end());
    return all;
}

/*!
    Returns the visible items at \a pos, compared as \a mode says, in \a order. \a deviceTransform is accepted for
    compatibility.
*/
QList<QGraphicsItem *> QGraphicsScene::items(const QPointF &pos, Qt::ItemSelectionMode mode, Qt::SortOrder order,
                                             const QTransform &deviceTransform) const {
    (void)deviceTransform;
    QList<QGraphicsItem *> out;
    for (QGraphicsItem *item : items(order)) {
        if (!item->isVisible() || !ItemAccess::passesAncestorClips(item, pos))
            continue;
        const QPointF local = item->mapFromScene(pos);
        const bool hit = (mode == Qt::IntersectsItemBoundingRect || mode == Qt::ContainsItemBoundingRect)
                             ? item->boundingRect().contains(local)
                             : item->contains(local);
        if (hit)
            out.append(item);
    }
    return out;
}

/*!
    \overload

    Returns the visible items that \a rect touches or contains, as \a mode says, in \a order.
*/
QList<QGraphicsItem *> QGraphicsScene::items(const QRectF &rect, Qt::ItemSelectionMode mode, Qt::SortOrder order,
                                             const QTransform &deviceTransform) const {
    QPainterPath path;
    path.addRect(rect);
    return items(path, mode, order, deviceTransform);
}

/*!
    \overload

    Returns the visible items that \a path touches or contains, as \a mode says, in \a order.
*/
QList<QGraphicsItem *> QGraphicsScene::items(const QPainterPath &path, Qt::ItemSelectionMode mode, Qt::SortOrder order,
                                             const QTransform &deviceTransform) const {
    (void)deviceTransform;
    QList<QGraphicsItem *> out;
    const QRectF area = path.boundingRect();
    for (QGraphicsItem *item : items(order)) {
        if (!item->isVisible())
            continue;
        const QRectF itemRect = (mode == Qt::IntersectsItemBoundingRect || mode == Qt::ContainsItemBoundingRect)
                                    ? item->sceneBoundingRect()
                                    : item->mapToScene(item->shape()).boundingRect();
        const bool contained = mode == Qt::ContainsItemShape || mode == Qt::ContainsItemBoundingRect;
        if (contained ? area.contains(itemRect) : area.intersects(itemRect))
            out.append(item);
    }
    return out;
}

/*!
    Returns the items that collide with \a item, as \a mode compares them, the topmost first.
*/
QList<QGraphicsItem *> QGraphicsScene::collidingItems(const QGraphicsItem *item, Qt::ItemSelectionMode mode) const {
    QList<QGraphicsItem *> out;
    if (!item)
        return out;
    for (QGraphicsItem *other : items()) {
        if (other != item && other->isVisible() && item->collidesWithItem(other, mode))
            out.append(other);
    }
    return out;
}

/*!
    Returns the topmost visible item at \a pos, or \c nullptr. \a deviceTransform is accepted for compatibility.
*/
QGraphicsItem *QGraphicsScene::itemAt(const QPointF &pos, const QTransform &deviceTransform) const {
    const QList<QGraphicsItem *> found = items(pos, Qt::IntersectsItemShape, Qt::DescendingOrder, deviceTransform);
    return found.isEmpty() ? nullptr : found.first();
}

/*!
    Returns the selected items.
*/
QList<QGraphicsItem *> QGraphicsScene::selectedItems() const {
    QList<QGraphicsItem *> out;
    for (QGraphicsItem *item : d->paintOrder()) {
        if (item->isSelected())
            out.append(item);
    }
    return out;
}

/*!
    Returns an empty path: Nt does not keep the selection area.
*/
QPainterPath QGraphicsScene::selectionArea() const { return QPainterPath(); }

/*!
    Selects the selectable items inside \a path and deselects the others. \a deviceTransform is accepted for
    compatibility.
*/
void QGraphicsScene::setSelectionArea(const QPainterPath &path, const QTransform &deviceTransform) {
    const QList<QGraphicsItem *> inside = items(path, Qt::IntersectsItemShape, Qt::DescendingOrder, deviceTransform);
    for (QGraphicsItem *item : d->paintOrder())
        item->setSelected(inside.contains(item));
}

/*!
    Puts \a items into a new group and returns it.
*/
QGraphicsItemGroup *QGraphicsScene::createItemGroup(const QList<QGraphicsItem *> &items) {
    auto *group = new QGraphicsItemGroup;
    addItem(group);
    for (QGraphicsItem *item : items)
        group->addToGroup(item);
    return group;
}

/*!
    Takes the items out of \a group and deletes the group.
*/
void QGraphicsScene::destroyItemGroup(QGraphicsItemGroup *group) {
    if (!group)
        return;
    for (QGraphicsItem *item : group->childItems())
        group->removeFromGroup(item);
    delete group;
}

/*!
    Adds \a item, with its children, to the scene, taking ownership of it. An item in another scene moves; a child
    item is first made a top-level item.
*/
void QGraphicsScene::addItem(QGraphicsItem *item) {
    if (!item)
        return;
    if (item->scene() == this && !item->parentItem())
        return;
    if (item->parentItem())
        item->setParentItem(nullptr);
    if (QGraphicsScene *old = item->scene())
        QGraphicsSceneAccess::d(old)->release(item);
    d->adopt(item);
}

/*!
    Adds an ellipse in \a rect with \a pen and \a brush, and returns it.
*/
QGraphicsEllipseItem *QGraphicsScene::addEllipse(const QRectF &rect, const QPen &pen, const QBrush &brush) {
    auto *item = new QGraphicsEllipseItem(rect);
    item->setPen(pen);
    item->setBrush(brush);
    addItem(item);
    return item;
}

/*!
    Adds \a line drawn with \a pen, and returns it.
*/
QGraphicsLineItem *QGraphicsScene::addLine(const QLineF &line, const QPen &pen) {
    auto *item = new QGraphicsLineItem(line);
    item->setPen(pen);
    addItem(item);
    return item;
}

/*!
    Adds \a path with \a pen and \a brush, and returns it.
*/
QGraphicsPathItem *QGraphicsScene::addPath(const QPainterPath &path, const QPen &pen, const QBrush &brush) {
    auto *item = new QGraphicsPathItem(path);
    item->setPen(pen);
    item->setBrush(brush);
    addItem(item);
    return item;
}

/*!
    Adds \a pixmap at the origin, and returns its item.
*/
QGraphicsPixmapItem *QGraphicsScene::addPixmap(const QPixmap &pixmap) {
    auto *item = new QGraphicsPixmapItem(pixmap);
    addItem(item);
    return item;
}

/*!
    Adds \a polygon with \a pen and \a brush, and returns it.
*/
QGraphicsPolygonItem *QGraphicsScene::addPolygon(const QPolygonF &polygon, const QPen &pen, const QBrush &brush) {
    auto *item = new QGraphicsPolygonItem(polygon);
    item->setPen(pen);
    item->setBrush(brush);
    addItem(item);
    return item;
}

/*!
    Adds \a rect with \a pen and \a brush, and returns it.
*/
QGraphicsRectItem *QGraphicsScene::addRect(const QRectF &rect, const QPen &pen, const QBrush &brush) {
    auto *item = new QGraphicsRectItem(rect);
    item->setPen(pen);
    item->setBrush(brush);
    addItem(item);
    return item;
}

/*!
    Adds the rich \a text in \a font, and returns its item.
*/
QGraphicsTextItem *QGraphicsScene::addText(const QString &text, const QFont &font) {
    auto *item = new QGraphicsTextItem(text);
    item->setFont(font);
    addItem(item);
    return item;
}

/*!
    Embeds the top-level \a widget in a new QGraphicsProxyWidget made with \a wFlags, adds the proxy to the
    scene and returns it.

    \sa QGraphicsProxyWidget::setWidget()
*/
QGraphicsProxyWidget *QGraphicsScene::addWidget(QWidget *widget, Qt::WindowFlags wFlags) {
    auto *proxy = new QGraphicsProxyWidget(nullptr, wFlags);
    proxy->setWidget(widget);
    addItem(proxy);
    return proxy;
}

/*!
    Adds the plain \a text in \a font, and returns its item.
*/
QGraphicsSimpleTextItem *QGraphicsScene::addSimpleText(const QString &text, const QFont &font) {
    auto *item = new QGraphicsSimpleTextItem(text);
    item->setFont(font);
    addItem(item);
    return item;
}

/*!
    Takes \a item, with its children, out of the scene. The caller owns it afterwards; a child item is made a
    top-level item first.
*/
void QGraphicsScene::removeItem(QGraphicsItem *item) {
    if (!item || item->scene() != this)
        return;
    if (item->parentItem())
        item->setParentItem(nullptr);
    d->release(item);
}

/*!
    Returns the item that has the keyboard focus, or \c nullptr.
*/
QGraphicsItem *QGraphicsScene::focusItem() const { return d->focusItem; }

/*!
    Gives \a item the keyboard focus, for \a focusReason, or clears it if \a item is \c nullptr. The item must be in
    this scene, focusable, visible and enabled.
*/
void QGraphicsScene::setFocusItem(QGraphicsItem *item, Qt::FocusReason focusReason) {
    if (item && (item->scene() != this || !(item->flags() & QGraphicsItem::ItemIsFocusable) || !item->isVisible() ||
                 !item->isEnabled()))
        return;
    if (item == d->focusItem)
        return;
    QGraphicsItem *old = d->focusItem;
    d->focusItem = item;
    if (old) {
        QFocusEvent out(QEvent::FocusOut);
        sendEvent(old, &out);
    }
    if (item) {
        QFocusEvent in(QEvent::FocusIn);
        sendEvent(item, &in);
    }
    emit focusItemChanged(item, old, focusReason);
}

/*!
    Returns \c true if the scene has the keyboard focus of its view.
*/
bool QGraphicsScene::hasFocus() const { return d->hasFocus; }

/*!
    Gives the scene the keyboard focus, for \a focusReason.
*/
void QGraphicsScene::setFocus(Qt::FocusReason focusReason) {
    if (d->hasFocus)
        return;
    QFocusEvent event(QEvent::FocusIn);
    (void)focusReason;
    focusInEvent(&event);
}

/*!
    Takes the keyboard focus away from the scene; the focus item keeps its place for when it returns.
*/
void QGraphicsScene::clearFocus() {
    if (!d->hasFocus)
        return;
    QFocusEvent event(QEvent::FocusOut);
    focusOutEvent(&event);
}

/*!
    Sets whether a click on the background keeps the focus item to \a enabled.
*/
void QGraphicsScene::setStickyFocus(bool enabled) { d->stickyFocus = enabled; }

/*!
    Returns \c true if a click on the background keeps the focus item; \c false by default.
*/
bool QGraphicsScene::stickyFocus() const { return d->stickyFocus; }

/*!
    Returns the item that grabs the mouse, or \c nullptr.
*/
QGraphicsItem *QGraphicsScene::mouseGrabberItem() const { return d->grabber; }

/*!
    Returns the brush the background is filled with; none by default.
*/
QBrush QGraphicsScene::backgroundBrush() const { return d->backgroundBrush; }

/*!
    Sets the background brush to \a brush.
*/
void QGraphicsScene::setBackgroundBrush(const QBrush &brush) {
    d->backgroundBrush = brush;
    update();
}

/*!
    Returns the brush the foreground is filled with; none by default.
*/
QBrush QGraphicsScene::foregroundBrush() const { return d->foregroundBrush; }

/*!
    Sets the foreground brush to \a brush.
*/
void QGraphicsScene::setForegroundBrush(const QBrush &brush) {
    d->foregroundBrush = brush;
    update();
}

/*!
    Returns the views that show the scene.
*/
QList<QGraphicsView *> QGraphicsScene::views() const { return d->views; }

/*!
    Returns the default font of the scene.
*/
QFont QGraphicsScene::font() const { return d->font; }

/*!
    Sets the default font to \a font. Nt keeps it; items choose their own fonts.
*/
void QGraphicsScene::setFont(const QFont &font) { d->font = font; }

/*!
    Returns the palette of the scene.
*/
QPalette QGraphicsScene::palette() const { return d->palette; }

/*!
    Sets the palette to \a palette. Nt keeps it; the style option of an item carries the view's palette.
*/
void QGraphicsScene::setPalette(const QPalette &palette) { d->palette = palette; }

/*!
    Returns \c true while the scene is shown in a view.
*/
bool QGraphicsScene::isActive() const { return !d->views.isEmpty(); }

/*!
    Returns \c nullptr: Nt has no panels.
*/
QGraphicsItem *QGraphicsScene::activePanel() const { return nullptr; }

/*!
    Sends \a event to \a item through its scene event filters. Returns \c true if a filter or the item handled it.
*/
bool QGraphicsScene::sendEvent(QGraphicsItem *item, QEvent *event) {
    if (!item || item->scene() != this || !event)
        return false;
    for (QGraphicsItem *filter : QList<QGraphicsItem *>(ItemAccess::d(item)->filters)) {
        if (ItemAccess::sceneEventFilter(filter, item, event))
            return true;
    }
    return ItemAccess::sceneEvent(item, event);
}

/*!
    Schedules a redraw of \a rect, or of the whole scene if \a rect is null.
*/
void QGraphicsScene::update(const QRectF &rect) {
    if (rect.isNull()) {
        for (QGraphicsView *view : d->views)
            view->viewport()->update();
        d->markDirty(sceneRect());
        return;
    }
    d->markDirty(rect);
}

/*!
    Redraws \a rect (all of it if null) of \a layers. Nt redraws all layers.
*/
void QGraphicsScene::invalidate(const QRectF &rect, SceneLayers layers) {
    (void)layers;
    update(rect);
}

/*!
    Advances the scene. Nt has no \c QGraphicsItem::advance(), so this does nothing.
*/
void QGraphicsScene::advance() {}

/*!
    Deselects every item.
*/
void QGraphicsScene::clearSelection() {
    bool changed = false;
    for (QGraphicsItem *item : d->paintOrder()) {
        if (item->isSelected()) {
            ItemAccess::d(item)->selected = false;
            item->update();
            changed = true;
        }
    }
    if (changed)
        emit selectionChanged();
}

/*!
    Removes and deletes every item.
*/
void QGraphicsScene::clear() {
    d->hoverItems.clear();
    d->grabber = nullptr;
    d->keyboardGrabber = nullptr;
    while (!d->topLevel.isEmpty())
        delete d->topLevel.first();
    d->growingRect = QRectF();
}

/*!
    \reimp

    Passes \a event to the handler for its type.
*/
bool QGraphicsScene::event(QEvent *event) {
    switch (event->type()) {
    case QEvent::GraphicsSceneMousePress:
        mousePressEvent(static_cast<QGraphicsSceneMouseEvent *>(event));
        return true;
    case QEvent::GraphicsSceneMouseMove:
        mouseMoveEvent(static_cast<QGraphicsSceneMouseEvent *>(event));
        return true;
    case QEvent::GraphicsSceneMouseRelease:
        mouseReleaseEvent(static_cast<QGraphicsSceneMouseEvent *>(event));
        return true;
    case QEvent::GraphicsSceneMouseDoubleClick:
        mouseDoubleClickEvent(static_cast<QGraphicsSceneMouseEvent *>(event));
        return true;
    case QEvent::GraphicsSceneWheel:
        wheelEvent(static_cast<QGraphicsSceneWheelEvent *>(event));
        return true;
    case QEvent::KeyPress:
        keyPressEvent(static_cast<QKeyEvent *>(event));
        return true;
    case QEvent::KeyRelease:
        keyReleaseEvent(static_cast<QKeyEvent *>(event));
        return true;
    case QEvent::FocusIn:
        focusInEvent(static_cast<QFocusEvent *>(event));
        return true;
    case QEvent::FocusOut:
        focusOutEvent(static_cast<QFocusEvent *>(event));
        return true;
    case QEvent::Leave:
        d->clearHover();
        return true;
    default:
        return QObject::event(event);
    }
}

/*!
    Receives the focus-in \a event of the scene; the focus item gets a focus-in event too.
*/
void QGraphicsScene::focusInEvent(QFocusEvent *event) {
    (void)event;
    d->hasFocus = true;
    if (d->focusItem) {
        QFocusEvent in(QEvent::FocusIn);
        sendEvent(d->focusItem, &in);
    }
}

/*!
    Receives the focus-out \a event of the scene; the focus item gets a focus-out event too.
*/
void QGraphicsScene::focusOutEvent(QFocusEvent *event) {
    (void)event;
    d->hasFocus = false;
    if (d->focusItem) {
        QFocusEvent out(QEvent::FocusOut);
        sendEvent(d->focusItem, &out);
    }
}

/*!
    Receives the key-press \a event and sends it to the focus item, then to its ancestors until one accepts it.
*/
void QGraphicsScene::keyPressEvent(QKeyEvent *event) {
    QGraphicsItem *target = d->keyboardGrabber ? d->keyboardGrabber : d->focusItem;
    for (QGraphicsItem *item = target; item; item = item->parentItem()) {
        if (!item->isEnabled())
            continue;
        event->accept();
        sendEvent(item, event);
        if (event->isAccepted())
            return;
    }
    event->ignore();
}

/*!
    Receives the key-release \a event and sends it to the focus item, then to its ancestors until one accepts it.
*/
void QGraphicsScene::keyReleaseEvent(QKeyEvent *event) {
    QGraphicsItem *target = d->keyboardGrabber ? d->keyboardGrabber : d->focusItem;
    for (QGraphicsItem *item = target; item; item = item->parentItem()) {
        if (!item->isEnabled())
            continue;
        event->accept();
        sendEvent(item, event);
        if (event->isAccepted())
            return;
    }
    event->ignore();
}

/*!
    Receives the mouse-press \a event. The topmost item under the mouse that accepts the button and accepts the
    event grabs the mouse; a click also moves the focus and, on the background, clears the selection.
*/
void QGraphicsScene::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (d->grabber) {
        Private::mapMouseEvent(event, d->grabber);
        event->accept();
        sendEvent(d->grabber, event);
        return;
    }
    const QList<QGraphicsItem *> under = d->itemsAt(event->scenePos());
    QGraphicsItem *focusTarget = nullptr;
    for (QGraphicsItem *item : under) {
        focusTarget = Private::focusableAncestor(item);
        if (focusTarget)
            break;
    }
    if (focusTarget)
        setFocusItem(focusTarget, Qt::MouseFocusReason);
    else if (!d->stickyFocus)
        setFocusItem(nullptr, Qt::MouseFocusReason);
    for (QGraphicsItem *item : under) {
        if (!item->isEnabled())
            break;
        if (!(item->acceptedMouseButtons() & event->button()))
            continue;
        Private::mapMouseEvent(event, item);
        event->accept();
        sendEvent(item, event);
        if (event->isAccepted()) {
            if (item->scene() == this && item->isVisible())
                d->grabber = item;
            return;
        }
    }
    if (!(event->modifiers() & Qt::ControlModifier))
        clearSelection();
    event->ignore();
}

/*!
    Receives the mouse-move \a event: the grabber gets it, or, without one, the hover moves.
*/
void QGraphicsScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (d->grabber) {
        Private::mapMouseEvent(event, d->grabber);
        event->accept();
        sendEvent(d->grabber, event);
        return;
    }
    d->updateHover(event->scenePos(), event->screenPos(), event->widget(), event->modifiers());
    event->ignore();
}

/*!
    Receives the mouse-release \a event: the grabber gets it, and the grab ends once no button is down.
*/
void QGraphicsScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsItem *grabber = d->grabber;
    if (!grabber) {
        event->ignore();
        return;
    }
    Private::mapMouseEvent(event, grabber);
    event->accept();
    sendEvent(grabber, event);
    if (event->buttons() == Qt::NoButton && d->grabber == grabber)
        d->grabber = nullptr;
    if (!d->grabber)
        d->updateHover(event->scenePos(), event->screenPos(), event->widget(), event->modifiers());
}

/*!
    Receives the double-click \a event and sends it as the press is sent; the item that accepts it grabs the mouse.
*/
void QGraphicsScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    if (d->grabber) {
        Private::mapMouseEvent(event, d->grabber);
        event->accept();
        sendEvent(d->grabber, event);
        return;
    }
    for (QGraphicsItem *item : d->itemsAt(event->scenePos())) {
        if (!item->isEnabled())
            break;
        if (!(item->acceptedMouseButtons() & event->button()))
            continue;
        Private::mapMouseEvent(event, item);
        event->accept();
        sendEvent(item, event);
        if (event->isAccepted()) {
            d->grabber = item;
            return;
        }
    }
    event->ignore();
}

/*!
    Receives the wheel \a event and sends it to the items under the mouse, the topmost first, until one accepts it.
*/
void QGraphicsScene::wheelEvent(QGraphicsSceneWheelEvent *event) {
    for (QGraphicsItem *item : d->itemsAt(event->scenePos())) {
        if (!item->isEnabled())
            break;
        event->setPos(item->mapFromScene(event->scenePos()));
        event->accept();
        sendEvent(item, event);
        if (event->isAccepted())
            return;
    }
    event->ignore();
}

/*!
    Draws the background of \a rect (scene coordinates) with \a painter: fills it with backgroundBrush().
*/
void QGraphicsScene::drawBackground(QPainter *painter, const QRectF &rect) {
    if (d->backgroundBrush.style() != Qt::NoBrush)
        painter->fillRect(rect, d->backgroundBrush);
}

/*!
    Draws the foreground of \a rect (scene coordinates) with \a painter: fills it with foregroundBrush().
*/
void QGraphicsScene::drawForeground(QPainter *painter, const QRectF &rect) {
    if (d->foregroundBrush.style() != Qt::NoBrush)
        painter->fillRect(rect, d->foregroundBrush);
}

#include "moc_qgraphicsscene.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
