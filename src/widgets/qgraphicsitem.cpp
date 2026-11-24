// MIT License - see LICENSE
#include <QtWidgets/qgraphicsitem.h>
#include <QtWidgets/qgraphicswidget.h>

#include "qgraphicsscene_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtGui/qevent.h>
#include <QtWidgets/qgraphicseffect.h>
#include <QtWidgets/qgraphicsscene.h>
#include <QtWidgets/qgraphicssceneevent.h>
#include <QtWidgets/qgraphicsview.h>
#include <QtWidgets/qstyleoption.h>

#include <QtWidgets/qgraphicstransform.h>

#include <algorithm>

using Access = QGraphicsItemAccess;

QTransform QGraphicsItem::Private::localTransform() const {
    QTransform t = transform;
    if (!transformations.isEmpty()) {
        QMatrix4x4 m;
        for (const QGraphicsTransform *transformation : transformations)
            transformation->applyTo(&m);
        t *= m.toTransform();
    }
    if (rotation != 0.0 || scale != 1.0) {
        t *= QTransform::fromTranslate(-origin.x(), -origin.y());
        t *= QTransform::fromScale(scale, scale);
        QTransform r;
        r.rotate(rotation);
        t *= r;
        t *= QTransform::fromTranslate(origin.x(), origin.y());
    }
    t *= QTransform::fromTranslate(pos.x(), pos.y());
    return t;
}

QList<QGraphicsItem *> QGraphicsItemAccess::sortedChildren(const QList<QGraphicsItem *> &children) {
    QList<QGraphicsItem *> sorted = children;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const QGraphicsItem *a, const QGraphicsItem *b) { return a->zValue() < b->zValue(); });
    return sorted;
}

QRectF QGraphicsItemAccess::subtreeSceneRect(const QGraphicsItem *item) {
    if (!item->isVisible())
        return QRectF();
    QRectF r = item->sceneBoundingRect();
    for (const QGraphicsItem *child : d(item)->children)
        r = r.united(subtreeSceneRect(child));
    return r;
}

bool QGraphicsItemAccess::passesAncestorClips(const QGraphicsItem *item, const QPointF &scenePos) {
    for (const QGraphicsItem *a = d(item)->parent; a; a = d(a)->parent) {
        if ((a->flags() & QGraphicsItem::ItemClipsChildrenToShape) && !a->contains(a->mapFromScene(scenePos)))
            return false;
    }
    return true;
}

namespace {

// Marks what `item` and its descendants cover as needing a redraw.
void dirtySubtree(const QGraphicsItem *item) {
    if (QGraphicsScene *scene = item->scene())
        QGraphicsSceneAccess::d(scene)->markDirty(QGraphicsItemAccess::subtreeSceneRect(item));
}

} // namespace

void QGraphicsItemAccess::transformationChanged(QGraphicsItem *item) {
    dirtySubtree(item);
    item->prepareGeometryChange();
    dirtySubtree(item);
}

void QGraphicsItemAccess::forgetTransformation(QGraphicsItem *item, QGraphicsTransform *transformation) {
    item->d_ptr->transformations.removeAll(transformation);
    transformationChanged(item);
}

/*!
    \class QGraphicsItem
    \brief The QGraphicsItem class is the base of the things a QGraphicsScene holds.

    An item has a boundingRect() and paints itself in paint(), in its own coordinates. It has a position in its
    parent (or in the scene, for a top-level item), a z value that orders it among its siblings, and a transformation
    of its own; children follow their parent. Items receive mouse, hover, wheel, key and focus events from the scene
    through the handlers below.

    \code
    class Dial : public QGraphicsItem {
    public:
        QRectF boundingRect() const override { return QRectF(-50, -50, 100, 100); }
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
            painter->drawEllipse(boundingRect());
        }
    };
    \endcode

    \differences
    Clipping to a shape (ItemClipsToShape, ItemClipsChildrenToShape) clips to the bounding rectangle of the shape on
    the device, since Nt clips by rectangles. ItemIgnoresTransformations, the cache modes, touch events, gestures,
    input methods, panels and modality are accepted but have no effect, and there is no graphics effect,
    transformations() list, \c setFocusProxy() or tool tip display. hasFocus() does not depend on whether the scene
    is active. scroll() redraws the rectangle instead of scrolling it.
    \enddifferences

    The type() of each item class is its \c Type enumerator (QGraphicsItem::Type is 1); items of the application
    return values from \c QGraphicsItem::UserType, so that qgraphicsitem_cast() can tell them apart.

    \sa QGraphicsScene, QGraphicsView, QGraphicsObject
*/

/*!
    \enum QGraphicsItem::GraphicsItemFlag

    Flags that change how an item behaves.

    \value ItemIsMovable The item follows the mouse when it is dragged with the left button.
    \value ItemIsSelectable The item can be selected by clicking it.
    \value ItemIsFocusable The item can take the keyboard focus.
    \value ItemClipsToShape The item is clipped to its shape (see the differences).
    \value ItemClipsChildrenToShape The children are clipped to the shape of the item (see the differences).
    \value ItemIgnoresTransformations Accepted; has no effect.
    \value ItemIgnoresParentOpacity The opacity of the parent does not dim the item.
    \value ItemDoesntPropagateOpacityToChildren The opacity of the item does not dim its children.
    \value ItemStacksBehindParent The item is drawn behind its parent.
    \value ItemUsesExtendedStyleOption The style option carries the exposed rectangle.
    \value ItemHasNoContents paint() is not called.
    \value ItemSendsGeometryChanges itemChange() is called for changes of position and transformation.
    \value ItemAcceptsInputMethod Accepted; has no effect.
    \value ItemNegativeZStacksBehindParent An item with a negative z value is drawn behind its parent.
    \value ItemIsPanel Accepted; has no effect.
    \value ItemIsFocusScope Accepted; has no effect.
    \value ItemSendsScenePositionChanges itemChange() is called when the scene position changes.
    \value ItemStopsClickFocusPropagation A click does not give the focus to an ancestor.
    \value ItemStopsFocusHandling Accepted; has no effect.
    \value ItemContainsChildrenInShape Accepted; has no effect.
*/

/*!
    \enum QGraphicsItem::GraphicsItemChange

    What is about to change, or has changed, when itemChange() is called.

    \value ItemPositionChange The position changes; the value is the new position (with ItemSendsGeometryChanges).
    \value ItemVisibleChange The visibility changes.
    \value ItemEnabledChange The enabled state changes.
    \value ItemSelectedChange The selection state changes.
    \value ItemParentChange The parent changes; the value is the new parent.
    \value ItemChildAddedChange A child was added; the value is the child.
    \value ItemChildRemovedChange A child is removed; the value is the child.
    \value ItemTransformChange The transformation changes (with ItemSendsGeometryChanges).
    \value ItemPositionHasChanged The position has changed (with ItemSendsGeometryChanges).
    \value ItemTransformHasChanged The transformation has changed (with ItemSendsGeometryChanges).
    \value ItemSceneChange The item moves to another scene; the value is the new scene.
    \value ItemVisibleHasChanged The visibility has changed.
    \value ItemEnabledHasChanged The enabled state has changed.
    \value ItemSelectedHasChanged The selection state has changed.
    \value ItemParentHasChanged The parent has changed.
    \value ItemSceneHasChanged The scene has changed.
    \value ItemCursorChange The cursor changes.
    \value ItemCursorHasChanged The cursor has changed.
    \value ItemToolTipChange The tool tip changes.
    \value ItemToolTipHasChanged The tool tip has changed.
    \value ItemFlagsChange The flags change.
    \value ItemFlagsHaveChanged The flags have changed.
    \value ItemZValueChange The z value changes.
    \value ItemZValueHasChanged The z value has changed.
    \value ItemOpacityChange The opacity changes.
    \value ItemOpacityHasChanged The opacity has changed.
    \value ItemScenePositionHasChanged The scene position has changed (with ItemSendsScenePositionChanges).
    \value ItemRotationChange The rotation changes (with ItemSendsGeometryChanges).
    \value ItemRotationHasChanged The rotation has changed (with ItemSendsGeometryChanges).
    \value ItemScaleChange The scale changes (with ItemSendsGeometryChanges).
    \value ItemScaleHasChanged The scale has changed (with ItemSendsGeometryChanges).
    \value ItemTransformOriginPointChange The transform origin changes (with ItemSendsGeometryChanges).
    \value ItemTransformOriginPointHasChanged The transform origin has changed (with ItemSendsGeometryChanges).
*/

/*!
    \enum QGraphicsItem::CacheMode

    How an item would be cached. Nt draws every item each time; the value is only kept.

    \value NoCache No cache.
    \value ItemCoordinateCache A cache in item coordinates.
    \value DeviceCoordinateCache A cache in device coordinates.
*/

/*!
    Constructs an item with the given \a parent item, in the parent's scene if it has one.
*/
QGraphicsItem::QGraphicsItem(QGraphicsItem *parent) : d_ptr(std::make_unique<Private>()) {
    d_ptr->q = this;
    if (!parent)
        return;
    // boundingRect() is pure virtual until the subclass is constructed, so the item is linked without asking for
    // it; the scene draws it on its next update pass.
    d_ptr->parent = parent;
    parent->d_ptr->children.append(this);
    parent->itemChange(ItemChildAddedChange, QVariant::fromValue(this));
    parent->notifyChange(ItemChildAddedChange);
    if (QGraphicsScene *scene = parent->d_ptr->scene) {
        d_ptr->scene = scene;
        QGraphicsSceneAccess::d(scene)->addPending(this);
    }
}

/*!
    Destroys the item and its children, and takes it out of its scene.
*/
QGraphicsItem::~QGraphicsItem() {
    d_ptr->destroying = true;
    for (QGraphicsTransform *transformation : std::as_const(d_ptr->transformations))
        transformation->m_items.removeAll(this);
    if (QGraphicsEffect *effect = d_ptr->effect) {
        d_ptr->effect = nullptr;
        effect->m_item = nullptr;
        delete effect;
    }
    for (QGraphicsItem *child : QList<QGraphicsItem *>(d_ptr->children))
        delete child;
    if (d_ptr->scene) {
        QGraphicsScene::Private *sd = QGraphicsSceneAccess::d(d_ptr->scene);
        sd->pending.removeAll(this);
        sd->markDirty(d_ptr->paintedRect); // boundingRect() cannot be called from here
        sd->forget(this);
        if (!d_ptr->parent)
            sd->topLevel.removeAll(this);
    }
    if (d_ptr->parent)
        Access::d(d_ptr->parent)->children.removeAll(this);
}

/*!
    Returns the scene the item is in, or \c nullptr.
*/
QGraphicsScene *QGraphicsItem::scene() const { return d_ptr->scene; }

/*!
    Returns the parent item, or \c nullptr for a top-level item.
*/
QGraphicsItem *QGraphicsItem::parentItem() const { return d_ptr->parent; }

/*!
    Returns the outermost ancestor of the item, or the item itself if it has no parent.
*/
QGraphicsItem *QGraphicsItem::topLevelItem() const {
    const QGraphicsItem *item = this;
    while (item->d_ptr->parent)
        item = item->d_ptr->parent;
    return const_cast<QGraphicsItem *>(item);
}

/*!
    Returns the nearest ancestor that is a QGraphicsObject, or \c nullptr.
*/
QGraphicsObject *QGraphicsItem::parentObject() const {
    for (QGraphicsItem *p = d_ptr->parent; p; p = p->d_ptr->parent) {
        if (p->d_ptr->isObject)
            return static_cast<QGraphicsObject *>(p);
    }
    return nullptr;
}

/*!
    Returns the nearest ancestor that is a QGraphicsWidget: always \c nullptr, as Nt has no QGraphicsWidget yet.
*/
QGraphicsWidget *QGraphicsItem::parentWidget() const { return nullptr; }

/*!
    Makes \a parent the parent of the item, keeping the item's position in parent coordinates. The item moves to the
    parent's scene. A null \a parent makes it a top-level item of its scene.
*/
void QGraphicsItem::setParentItem(QGraphicsItem *parent) {
    if (parent == d_ptr->parent || parent == this)
        return;
    for (QGraphicsItem *p = parent; p; p = p->d_ptr->parent) {
        if (p == this) {
            qWarning("QGraphicsItem::setParentItem: an item cannot be the parent of its ancestor");
            return;
        }
    }
    const QVariant v = itemChange(ItemParentChange, QVariant::fromValue(parent));
    parent = v.value<QGraphicsItem *>();
    if (parent == d_ptr->parent)
        return;
    dirtySubtree(this);
    QGraphicsItem *old = d_ptr->parent;
    if (old) {
        old->itemChange(ItemChildRemovedChange, QVariant::fromValue(this));
        old->d_ptr->children.removeAll(this);
        old->notifyChange(ItemChildRemovedChange);
    } else if (d_ptr->scene) {
        QGraphicsSceneAccess::d(d_ptr->scene)->topLevel.removeAll(this);
    }
    d_ptr->parent = parent;
    QGraphicsScene *newScene = parent ? parent->d_ptr->scene : d_ptr->scene;
    if (parent) {
        parent->d_ptr->children.append(this);
        parent->itemChange(ItemChildAddedChange, QVariant::fromValue(this));
        parent->notifyChange(ItemChildAddedChange);
    }
    if (newScene != d_ptr->scene) {
        if (d_ptr->scene)
            QGraphicsSceneAccess::d(d_ptr->scene)->release(this);
        if (newScene)
            QGraphicsSceneAccess::d(newScene)->adopt(this);
    } else if (!parent && d_ptr->scene) {
        QGraphicsSceneAccess::d(d_ptr->scene)->topLevel.append(this);
    }
    dirtySubtree(this);
    itemChange(ItemParentHasChanged, QVariant::fromValue(parent));
    notifyChange(ItemParentHasChanged);
}

/*!
    Returns the children, in the order they were added.
*/
QList<QGraphicsItem *> QGraphicsItem::childItems() const { return d_ptr->children; }

/*!
    Returns \c true if the item is a QGraphicsWidget: always \c false in Nt.
*/
bool QGraphicsItem::isWidget() const { return false; }

/*!
    Returns \c true if the item is a window: always \c false in Nt.
*/
bool QGraphicsItem::isWindow() const { return false; }

/*!
    Returns \c true if ItemIsPanel is set.
*/
bool QGraphicsItem::isPanel() const { return d_ptr->flags & ItemIsPanel; }

/*!
    Returns the item as a QGraphicsObject, or \c nullptr if it is not one.
*/
QGraphicsObject *QGraphicsItem::toGraphicsObject() {
    return d_ptr->isObject ? static_cast<QGraphicsObject *>(this) : nullptr;
}

/*!
    \overload
*/
const QGraphicsObject *QGraphicsItem::toGraphicsObject() const {
    return d_ptr->isObject ? static_cast<const QGraphicsObject *>(this) : nullptr;
}

/*!
    Returns the group the item belongs to, or \c nullptr.
*/
QGraphicsItemGroup *QGraphicsItem::group() const { return d_ptr->group; }

/*!
    Adds the item to \a group, or removes it from its group if \a group is \c nullptr.
*/
void QGraphicsItem::setGroup(QGraphicsItemGroup *group) {
    if (group == d_ptr->group)
        return;
    if (d_ptr->group)
        d_ptr->group->removeFromGroup(this);
    if (group)
        group->addToGroup(this);
}

/*!
    Returns the flags of the item.
*/
QGraphicsItem::GraphicsItemFlags QGraphicsItem::flags() const { return d_ptr->flags; }

/*!
    Sets \a flag on the item if \a enabled, or clears it.
*/
void QGraphicsItem::setFlag(GraphicsItemFlag flag, bool enabled) {
    setFlags(enabled ? d_ptr->flags | flag : d_ptr->flags & ~GraphicsItemFlags(flag));
}

/*!
    Sets the flags of the item to \a flags.
*/
void QGraphicsItem::setFlags(GraphicsItemFlags flags) {
    flags = GraphicsItemFlags::fromInt(
        static_cast<int>(itemChange(ItemFlagsChange, QVariant(static_cast<uint>(flags.toInt()))).toUInt()));
    if (flags == d_ptr->flags)
        return;
    const GraphicsItemFlags old = d_ptr->flags;
    d_ptr->flags = flags;
    if ((old & ItemIsFocusable) && !(flags & ItemIsFocusable) && hasFocus())
        clearFocus();
    if ((old & ItemIsSelectable) && !(flags & ItemIsSelectable) && d_ptr->selected)
        setSelected(false);
    dirtySubtree(this);
    itemChange(ItemFlagsHaveChanged, QVariant(static_cast<uint>(flags.toInt())));
}

/*!
    Returns the effect the item is drawn with, or \c nullptr.
*/
QGraphicsEffect *QGraphicsItem::graphicsEffect() const { return d_ptr->effect; }

/*!
    Draws the item with \a effect, which the item takes ownership of; the previous effect is deleted. \c nullptr
    removes the effect.
*/
void QGraphicsItem::setGraphicsEffect(QGraphicsEffect *effect) {
    if (effect == d_ptr->effect)
        return;
    update(d_ptr->effect ? d_ptr->effect->boundingRect() : QRectF());
    if (QGraphicsEffect *old = d_ptr->effect) {
        d_ptr->effect = nullptr;
        old->m_item = nullptr;
        delete old;
    }
    d_ptr->effect = effect;
    if (effect) {
        if (effect->m_item && effect->m_item != this)
            effect->m_item->setGraphicsEffect(nullptr);
        effect->m_item = this;
        update(effect->boundingRect());
    }
}

/*!
    Returns the cache mode.
*/
QGraphicsItem::CacheMode QGraphicsItem::cacheMode() const { return d_ptr->cacheMode; }

/*!
    Sets the cache mode to \a mode. \a cacheSize is ignored. Nt keeps the value but does not cache.
*/
void QGraphicsItem::setCacheMode(CacheMode mode, const QSize &cacheSize) {
    (void)cacheSize;
    d_ptr->cacheMode = mode;
}

/*!
    Returns the tool tip.
*/
QString QGraphicsItem::toolTip() const { return d_ptr->toolTip; }

/*!
    Sets the tool tip to \a toolTip. Nt keeps it but does not show it.
*/
void QGraphicsItem::setToolTip(const QString &toolTip) {
    const QString text = itemChange(ItemToolTipChange, QVariant(toolTip)).toString();
    d_ptr->toolTip = text;
    itemChange(ItemToolTipHasChanged, QVariant(text));
}

/*!
    Returns the cursor shown over the item, or an arrow if none was set.
*/
QCursor QGraphicsItem::cursor() const { return d_ptr->cursor; }

/*!
    Sets the cursor shown while the mouse is over the item to \a cursor.
*/
void QGraphicsItem::setCursor(const QCursor &cursor) {
    itemChange(ItemCursorChange, QVariant());
    d_ptr->cursor = cursor;
    d_ptr->hasCursor = true;
    itemChange(ItemCursorHasChanged, QVariant());
}

/*!
    Returns \c true if a cursor was set.
*/
bool QGraphicsItem::hasCursor() const { return d_ptr->hasCursor; }

/*!
    Removes the cursor of the item.
*/
void QGraphicsItem::unsetCursor() {
    d_ptr->hasCursor = false;
    d_ptr->cursor = QCursor();
}

/*!
    Returns \c true if the item and all its ancestors are visible.
*/
bool QGraphicsItem::isVisible() const {
    for (const QGraphicsItem *i = this; i; i = i->d_ptr->parent) {
        if (i->d_ptr->explicitlyHidden)
            return false;
    }
    return true;
}

/*!
    Returns \c true if the item would be visible when \a parent is visible: no item between them is hidden.
*/
bool QGraphicsItem::isVisibleTo(const QGraphicsItem *parent) const {
    for (const QGraphicsItem *i = this; i && i != parent; i = i->d_ptr->parent) {
        if (i->d_ptr->explicitlyHidden)
            return false;
    }
    return true;
}

/*!
    Shows the item if \a visible, or hides it and its children. A hidden item loses the focus and the mouse grab.
*/
void QGraphicsItem::setVisible(bool visible) {
    visible = itemChange(ItemVisibleChange, QVariant(visible)).toBool();
    if (visible == !d_ptr->explicitlyHidden)
        return;
    if (!visible)
        dirtySubtree(this);
    d_ptr->explicitlyHidden = !visible;
    if (visible)
        dirtySubtree(this);
    else if (d_ptr->scene)
        QGraphicsSceneAccess::d(d_ptr->scene)->forget(this);
    itemChange(ItemVisibleHasChanged, QVariant(visible));
    notifyChange(ItemVisibleHasChanged);
}

/*!
    Returns \c true if the item and all its ancestors are enabled.
*/
bool QGraphicsItem::isEnabled() const {
    for (const QGraphicsItem *i = this; i; i = i->d_ptr->parent) {
        if (i->d_ptr->explicitlyDisabled)
            return false;
    }
    return true;
}

/*!
    Enables the item if \a enabled, or disables it and its children. A disabled item gets no input.
*/
void QGraphicsItem::setEnabled(bool enabled) {
    enabled = itemChange(ItemEnabledChange, QVariant(enabled)).toBool();
    if (enabled == !d_ptr->explicitlyDisabled)
        return;
    d_ptr->explicitlyDisabled = !enabled;
    if (!enabled && d_ptr->scene)
        QGraphicsSceneAccess::d(d_ptr->scene)->forget(this);
    update();
    itemChange(ItemEnabledHasChanged, QVariant(enabled));
    notifyChange(ItemEnabledHasChanged);
}

/*!
    Returns \c true if the item is selected.
*/
bool QGraphicsItem::isSelected() const { return d_ptr->selected; }

/*!
    Selects the item if \a selected, or deselects it. Only an item with ItemIsSelectable can be selected.
*/
void QGraphicsItem::setSelected(bool selected) {
    if (selected && !(d_ptr->flags & ItemIsSelectable))
        return;
    selected = itemChange(ItemSelectedChange, QVariant(selected)).toBool();
    if (selected == d_ptr->selected)
        return;
    d_ptr->selected = selected;
    update();
    itemChange(ItemSelectedHasChanged, QVariant(selected));
    if (d_ptr->scene)
        emit d_ptr->scene->selectionChanged();
}

/*!
    Returns the opacity of the item, from 0.0 (invisible) to 1.0 (opaque).
*/
qreal QGraphicsItem::opacity() const { return d_ptr->opacity; }

/*!
    Returns the opacity the item is drawn with: its own times that of its ancestors, as the opacity flags allow.
*/
qreal QGraphicsItem::effectiveOpacity() const {
    qreal o = d_ptr->opacity;
    const QGraphicsItem *child = this;
    for (const QGraphicsItem *p = d_ptr->parent; p; child = p, p = p->d_ptr->parent) {
        if ((child->d_ptr->flags & ItemIgnoresParentOpacity) ||
            (p->d_ptr->flags & ItemDoesntPropagateOpacityToChildren))
            break;
        o *= p->d_ptr->opacity;
    }
    return o;
}

/*!
    Sets the opacity of the item to \a opacity, bounded to the range from 0.0 to 1.0.
*/
void QGraphicsItem::setOpacity(qreal opacity) {
    opacity = itemChange(ItemOpacityChange, QVariant(std::clamp(opacity, 0.0, 1.0))).toDouble();
    opacity = std::clamp(opacity, 0.0, 1.0);
    if (qFuzzyCompare(opacity, d_ptr->opacity))
        return;
    d_ptr->opacity = opacity;
    dirtySubtree(this);
    itemChange(ItemOpacityHasChanged, QVariant(opacity));
    notifyChange(ItemOpacityHasChanged);
}

/*!
    Returns the mouse buttons the item accepts presses of; all of them by default.
*/
Qt::MouseButtons QGraphicsItem::acceptedMouseButtons() const { return d_ptr->acceptedButtons; }

/*!
    Sets the mouse buttons the item accepts presses of to \a buttons. An item that accepts none lets presses through
    to the items below it.
*/
void QGraphicsItem::setAcceptedMouseButtons(Qt::MouseButtons buttons) {
    d_ptr->acceptedButtons = buttons;
    if (buttons == Qt::NoButton && d_ptr->scene && QGraphicsSceneAccess::d(d_ptr->scene)->grabber == this)
        ungrabMouse();
}

/*!
    \since 6.0

    Returns \c true if the item takes the mouse and hover events of its descendants: \c false by default, \c true
    for a QGraphicsItemGroup.

    \sa setHandlesChildEvents()
*/
bool QGraphicsItem::handlesChildEvents() const { return d_ptr->handlesChildEvents; }

/*!
    \since 6.0

    Makes the item take the mouse and hover events of all its descendants if \a enabled is \c true: a press on a
    child goes to this item instead, as if the item covered the child. With \c false, the default except for a
    QGraphicsItemGroup, the children get their own events.
*/
void QGraphicsItem::setHandlesChildEvents(bool enabled) { d_ptr->handlesChildEvents = enabled; }

/*!
    Returns \c true if the item receives hover events; \c false by default.
*/
bool QGraphicsItem::acceptHoverEvents() const { return d_ptr->acceptHover; }

/*!
    Sets whether the item receives hover events to \a enabled.
*/
void QGraphicsItem::setAcceptHoverEvents(bool enabled) { d_ptr->acceptHover = enabled; }

/*!
    Returns whether the item would receive touch events. Nt has none.
*/
bool QGraphicsItem::acceptTouchEvents() const { return d_ptr->acceptTouch; }

/*!
    Sets whether the item would receive touch events to \a enabled. Nt keeps the value; touches arrive as mouse
    events.
*/
void QGraphicsItem::setAcceptTouchEvents(bool enabled) { d_ptr->acceptTouch = enabled; }

/*!
    Returns \c true if the mouse is over the item, as far as hover events tell.
*/
bool QGraphicsItem::isUnderMouse() const { return d_ptr->underMouse; }

/*!
    Returns \c true if the item has the keyboard focus of its scene.
*/
bool QGraphicsItem::hasFocus() const {
    return d_ptr->scene && QGraphicsSceneAccess::d(d_ptr->scene)->focusItem == this;
}

/*!
    Gives the item the keyboard focus of its scene, for \a focusReason. The item needs ItemIsFocusable and must be
    visible and enabled. An item that is not in a scene yet takes the focus when it is added.
*/
void QGraphicsItem::setFocus(Qt::FocusReason focusReason) {
    if (d_ptr->focusProxy && d_ptr->focusProxy != this) {
        d_ptr->focusProxy->setFocus(focusReason);
        return;
    }
    if (!(d_ptr->flags & ItemIsFocusable))
        return;
    if (!d_ptr->scene) {
        d_ptr->wantsFocus = true;
        return;
    }
    d_ptr->scene->setFocusItem(this, focusReason);
}

/*!
    Takes the keyboard focus away from the item.
*/
void QGraphicsItem::clearFocus() {
    d_ptr->wantsFocus = false;
    if (hasFocus())
        d_ptr->scene->setFocusItem(nullptr);
}

/*!
    Returns the item that has the focus if it is this item or one of its descendants, or \c nullptr.
*/
QGraphicsItem *QGraphicsItem::focusItem() const {
    if (!d_ptr->scene)
        return nullptr;
    QGraphicsItem *f = QGraphicsSceneAccess::d(d_ptr->scene)->focusItem;
    return f && (f == this || isAncestorOf(f)) ? f : nullptr;
}

/*!
    Returns \c true if the item is in a scene that is shown in a view.
*/
bool QGraphicsItem::isActive() const { return d_ptr->scene && d_ptr->scene->isActive(); }

/*!
    Accepted for compatibility with \a active; Nt has no panels, so an item is active while its scene is.
*/
void QGraphicsItem::setActive(bool active) { (void)active; }

/*!
    Makes the item receive the mouse events of the scene until ungrabMouse(), wherever the mouse is.
*/
void QGraphicsItem::grabMouse() {
    if (d_ptr->scene && isVisible() && isEnabled())
        QGraphicsSceneAccess::d(d_ptr->scene)->grabber = this;
}

/*!
    Ends a mouse grab of the item.
*/
void QGraphicsItem::ungrabMouse() {
    if (d_ptr->scene && QGraphicsSceneAccess::d(d_ptr->scene)->grabber == this)
        QGraphicsSceneAccess::d(d_ptr->scene)->grabber = nullptr;
}

/*!
    Makes the item receive the key events of the scene until ungrabKeyboard().
*/
void QGraphicsItem::grabKeyboard() {
    if (d_ptr->scene && isVisible() && isEnabled())
        QGraphicsSceneAccess::d(d_ptr->scene)->keyboardGrabber = this;
}

/*!
    Ends a keyboard grab of the item.
*/
void QGraphicsItem::ungrabKeyboard() {
    if (d_ptr->scene && QGraphicsSceneAccess::d(d_ptr->scene)->keyboardGrabber == this)
        QGraphicsSceneAccess::d(d_ptr->scene)->keyboardGrabber = nullptr;
}

/*!
    Returns the position of the item in parent coordinates, or in scene coordinates for a top-level item.
*/
QPointF QGraphicsItem::pos() const { return d_ptr->pos; }

/*!
    Sets the x coordinate of the position to \a x.
*/
void QGraphicsItem::setX(qreal x) { setPos(QPointF(x, d_ptr->pos.y())); }

/*!
    Sets the y coordinate of the position to \a y.
*/
void QGraphicsItem::setY(qreal y) { setPos(QPointF(d_ptr->pos.x(), y)); }

/*!
    Returns the position of the item in scene coordinates.
*/
QPointF QGraphicsItem::scenePos() const { return mapToScene(QPointF(0, 0)); }

/*!
    Moves the item to \a pos in parent coordinates.
*/
void QGraphicsItem::setPos(const QPointF &pos) {
    QPointF p = pos;
    if (d_ptr->flags & ItemSendsGeometryChanges)
        p = itemChange(ItemPositionChange, QVariant(p)).toPointF();
    if (p == d_ptr->pos)
        return;
    const QPointF old = d_ptr->pos;
    dirtySubtree(this);
    d_ptr->pos = p;
    dirtySubtree(this);
    if (d_ptr->flags & ItemSendsGeometryChanges)
        itemChange(ItemPositionHasChanged, QVariant(p));
    if (d_ptr->flags & ItemSendsScenePositionChanges)
        itemChange(ItemScenePositionHasChanged, QVariant(scenePos()));
    if (old.x() != p.x())
        notifyChange(ItemPositionChange);
    if (old.y() != p.y())
        notifyChange(ItemPositionHasChanged);
}

/*!
    Scrolls the views of the scene so that \a rect of the item (its bounding rectangle if null) is visible with
    \a xmargin and \a ymargin pixels around it.
*/
void QGraphicsItem::ensureVisible(const QRectF &rect, int xmargin, int ymargin) {
    if (!d_ptr->scene)
        return;
    const QRectF r = mapRectToScene(rect.isNull() ? boundingRect() : rect);
    for (QGraphicsView *view : d_ptr->scene->views())
        view->ensureVisible(r, xmargin, ymargin);
}

/*!
    Returns the transformation set with setTransform().
*/
QTransform QGraphicsItem::transform() const { return d_ptr->transform; }

/*!
    Returns the transformation from item to scene coordinates.
*/
QTransform QGraphicsItem::sceneTransform() const {
    QTransform t = d_ptr->localTransform();
    for (const QGraphicsItem *p = d_ptr->parent; p; p = p->d_ptr->parent)
        t *= p->d_ptr->localTransform();
    return t;
}

/*!
    Returns the transformation from item to device coordinates for a view whose transformation is
    \a viewportTransform.
*/
QTransform QGraphicsItem::deviceTransform(const QTransform &viewportTransform) const {
    return sceneTransform() * viewportTransform;
}

/*!
    Returns the transformation from the coordinates of this item to those of \a other. \a ok, if given, is set to
    \c false if the transformation of \a other cannot be inverted.
*/
QTransform QGraphicsItem::itemTransform(const QGraphicsItem *other, bool *ok) const {
    bool invertible = true;
    const QTransform otherInverse = other ? other->sceneTransform().inverted(&invertible) : QTransform();
    if (ok)
        *ok = invertible;
    return sceneTransform() * otherInverse;
}

/*!
    Sets the transformation of the item to \a matrix, or combines it with the current one if \a combine.
*/
void QGraphicsItem::setTransform(const QTransform &matrix, bool combine) {
    QTransform t = combine ? matrix * d_ptr->transform : matrix;
    if (d_ptr->flags & ItemSendsGeometryChanges)
        t = itemChange(ItemTransformChange, QVariant::fromValue(t)).value<QTransform>();
    if (t == d_ptr->transform)
        return;
    dirtySubtree(this);
    d_ptr->transform = t;
    dirtySubtree(this);
    if (d_ptr->flags & ItemSendsGeometryChanges)
        itemChange(ItemTransformHasChanged, QVariant::fromValue(t));
}

/*!
    Resets the transformation to the identity.
*/
void QGraphicsItem::resetTransform() { setTransform(QTransform()); }

/*!
    \since 6.0

    Returns the transformations of the item, which setTransformations() set.
*/
QList<QGraphicsTransform *> QGraphicsItem::transformations() const { return d_ptr->transformations; }

/*!
    \since 6.0

    Sets the transformations of the item to \a transformations, applied in their order after transform() and before
    the rotation and the scale of the item. The item does not take ownership of them; one that is deleted leaves the
    list.

    \sa QGraphicsTransform
*/
void QGraphicsItem::setTransformations(const QList<QGraphicsTransform *> &transformations) {
    for (QGraphicsTransform *transformation : std::as_const(d_ptr->transformations))
        transformation->m_items.removeAll(this);
    dirtySubtree(this);
    prepareGeometryChange();
    d_ptr->transformations = transformations;
    for (QGraphicsTransform *transformation : std::as_const(d_ptr->transformations))
        if (transformation && !transformation->m_items.contains(this))
            transformation->m_items.append(this);
    dirtySubtree(this);
}

/*!
    Rotates the item by \a angle degrees, clockwise, about transformOriginPoint().
*/
void QGraphicsItem::setRotation(qreal angle) {
    if (d_ptr->flags & ItemSendsGeometryChanges)
        angle = itemChange(ItemRotationChange, QVariant(angle)).toDouble();
    if (qFuzzyCompare(angle, d_ptr->rotation) && (angle != 0.0) == (d_ptr->rotation != 0.0))
        return;
    dirtySubtree(this);
    d_ptr->rotation = angle;
    dirtySubtree(this);
    if (d_ptr->flags & ItemSendsGeometryChanges)
        itemChange(ItemRotationHasChanged, QVariant(angle));
    notifyChange(ItemRotationHasChanged);
}

/*!
    Returns the rotation in degrees.
*/
qreal QGraphicsItem::rotation() const { return d_ptr->rotation; }

/*!
    Scales the item by \a scale about transformOriginPoint().
*/
void QGraphicsItem::setScale(qreal scale) {
    if (d_ptr->flags & ItemSendsGeometryChanges)
        scale = itemChange(ItemScaleChange, QVariant(scale)).toDouble();
    if (qFuzzyCompare(scale, d_ptr->scale))
        return;
    dirtySubtree(this);
    d_ptr->scale = scale;
    dirtySubtree(this);
    if (d_ptr->flags & ItemSendsGeometryChanges)
        itemChange(ItemScaleHasChanged, QVariant(scale));
    notifyChange(ItemScaleHasChanged);
}

/*!
    Returns the scale factor, 1.0 by default.
*/
qreal QGraphicsItem::scale() const { return d_ptr->scale; }

/*!
    Returns the point, in item coordinates, that rotation and scale are about.
*/
QPointF QGraphicsItem::transformOriginPoint() const { return d_ptr->origin; }

/*!
    Sets the point that rotation and scale are about to \a origin, in item coordinates.
*/
void QGraphicsItem::setTransformOriginPoint(const QPointF &origin) {
    QPointF o = origin;
    if (d_ptr->flags & ItemSendsGeometryChanges)
        o = itemChange(ItemTransformOriginPointChange, QVariant(o)).toPointF();
    if (o == d_ptr->origin)
        return;
    dirtySubtree(this);
    d_ptr->origin = o;
    dirtySubtree(this);
    if (d_ptr->flags & ItemSendsGeometryChanges)
        itemChange(ItemTransformOriginPointHasChanged, QVariant(o));
}

/*!
    Returns the z value: items with higher values are drawn above their siblings.
*/
qreal QGraphicsItem::zValue() const { return d_ptr->z; }

/*!
    Sets the z value to \a z.
*/
void QGraphicsItem::setZValue(qreal z) {
    z = itemChange(ItemZValueChange, QVariant(z)).toDouble();
    if (z == d_ptr->z)
        return;
    d_ptr->z = z;
    dirtySubtree(this);
    itemChange(ItemZValueHasChanged, QVariant(z));
    notifyChange(ItemZValueHasChanged);
}

/*!
    Stacks the item just below \a sibling, which must have the same parent; their z values decide first.
*/
void QGraphicsItem::stackBefore(const QGraphicsItem *sibling) {
    if (!sibling || sibling == this || sibling->d_ptr->parent != d_ptr->parent)
        return;
    QList<QGraphicsItem *> *list = d_ptr->parent  ? &d_ptr->parent->d_ptr->children
                                   : d_ptr->scene ? &QGraphicsSceneAccess::d(d_ptr->scene)->topLevel
                                                  : nullptr;
    if (!list)
        return;
    list->removeAll(this);
    const qsizetype at = list->indexOf(const_cast<QGraphicsItem *>(sibling));
    list->insert(at < 0 ? list->size() : at, this);
    dirtySubtree(this);
}

/*!
    Returns the rectangle, in item coordinates, that the descendants of the item cover.
*/
QRectF QGraphicsItem::childrenBoundingRect() const {
    QRectF r;
    for (const QGraphicsItem *child : d_ptr->children) {
        const QRectF own = child->boundingRect().united(child->childrenBoundingRect());
        r = r.united(child->d_ptr->localTransform().mapRect(own));
    }
    return r;
}

/*!
    Returns the bounding rectangle in scene coordinates.
*/
QRectF QGraphicsItem::sceneBoundingRect() const { return sceneTransform().mapRect(boundingRect()); }

/*!
    Returns the shape of the item, which hit tests and collisions use: its bounding rectangle by default.
*/
QPainterPath QGraphicsItem::shape() const {
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

/*!
    Returns \c true if the item or an ancestor clips it to a shape.
*/
bool QGraphicsItem::isClipped() const {
    if (d_ptr->flags & ItemClipsToShape)
        return true;
    for (const QGraphicsItem *p = d_ptr->parent; p; p = p->d_ptr->parent) {
        if (p->d_ptr->flags & ItemClipsChildrenToShape)
            return true;
    }
    return false;
}

/*!
    Returns the clip of the item in item coordinates, or an empty path if it is not clipped.
*/
QPainterPath QGraphicsItem::clipPath() const {
    if (!isClipped())
        return QPainterPath();
    QRectF r = (d_ptr->flags & ItemClipsToShape) ? shape().boundingRect() : boundingRect();
    for (const QGraphicsItem *p = d_ptr->parent; p; p = p->d_ptr->parent) {
        if (p->d_ptr->flags & ItemClipsChildrenToShape)
            r = r.intersected(mapRectFromItem(p, p->shape().boundingRect()));
    }
    QPainterPath path;
    path.addRect(r);
    return path;
}

/*!
    Returns \c true if \a point, in item coordinates, is inside the shape of the item.
*/
bool QGraphicsItem::contains(const QPointF &point) const {
    return boundingRect().contains(point) && shape().contains(point);
}

/*!
    Returns \c true if the item collides with \a other, as \a mode compares their shapes or bounding rectangles.
*/
bool QGraphicsItem::collidesWithItem(const QGraphicsItem *other, Qt::ItemSelectionMode mode) const {
    if (!other || other == this)
        return false;
    if (mode == Qt::IntersectsItemBoundingRect || mode == Qt::ContainsItemBoundingRect) {
        const QRectF mine = sceneBoundingRect();
        const QRectF theirs = other->sceneBoundingRect();
        return mode == Qt::IntersectsItemBoundingRect ? mine.intersects(theirs) : mine.contains(theirs);
    }
    return collidesWithPath(mapFromScene(other->mapToScene(other->shape())), mode);
}

/*!
    Returns \c true if the item collides with \a path, in item coordinates, as \a mode compares them.
*/
bool QGraphicsItem::collidesWithPath(const QPainterPath &path, Qt::ItemSelectionMode mode) const {
    if (path.isEmpty())
        return false;
    const QRectF rect = (mode == Qt::IntersectsItemBoundingRect || mode == Qt::ContainsItemBoundingRect)
                            ? boundingRect()
                            : shape().boundingRect();
    switch (mode) {
    case Qt::ContainsItemShape:
    case Qt::ContainsItemBoundingRect:
        return path.contains(rect);
    case Qt::IntersectsItemShape:
        return path.intersects(shape());
    case Qt::IntersectsItemBoundingRect:
        return path.intersects(rect);
    }
    return false;
}

/*!
    Returns the items of the scene that collide with this one, as \a mode compares them.
*/
QList<QGraphicsItem *> QGraphicsItem::collidingItems(Qt::ItemSelectionMode mode) const {
    return d_ptr->scene ? d_ptr->scene->collidingItems(this, mode) : QList<QGraphicsItem *>();
}

/*!
    Returns \c true if \a rect of the item (its bounding rectangle if null) is covered by opaque items above it.
*/
bool QGraphicsItem::isObscured(const QRectF &rect) const {
    if (!d_ptr->scene)
        return false;
    const QRectF r = mapRectToScene(rect.isNull() ? boundingRect() : rect);
    const QList<QGraphicsItem *> above = d_ptr->scene->items(r, Qt::IntersectsItemBoundingRect);
    for (QGraphicsItem *item : above) {
        if (item == this)
            break;
        if (item->isObscuredBy(this) || item->mapToScene(item->opaqueArea()).contains(r))
            return true;
    }
    return false;
}

/*!
    Returns \c true if the opaque area of \a item covers this item's shape. The default compares the bounding
    rectangle of this item with the opaque area of \a item.
*/
bool QGraphicsItem::isObscuredBy(const QGraphicsItem *item) const {
    if (!item)
        return false;
    return item->mapToScene(item->opaqueArea()).contains(sceneBoundingRect());
}

/*!
    Returns the area, in item coordinates, that the item paints opaque: none by default.
*/
QPainterPath QGraphicsItem::opaqueArea() const { return QPainterPath(); }

/*!
    \fn void QGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)

    Paints the item with \a painter, in item coordinates, with the state in \a option. \a widget is the viewport of
    the view being drawn, or \c nullptr when the scene is rendered elsewhere.
*/

/*!
    \fn QRectF QGraphicsItem::boundingRect() const

    Returns the rectangle, in item coordinates, that the item paints in. Call prepareGeometryChange() before it
    changes.
*/

/*!
    Schedules a redraw of \a rect of the item, or of its bounding rectangle if \a rect is null.
*/
void QGraphicsItem::update(const QRectF &rect) {
    if (!d_ptr->scene || !isVisible())
        return;
    QGraphicsSceneAccess::d(d_ptr->scene)->markDirty(mapRectToScene(rect.isNull() ? boundingRect() : rect));
}

/*!
    Scrolls \a rect of the item (its bounding rectangle if null) by \a dx and \a dy. Nt redraws the rectangle.
*/
void QGraphicsItem::scroll(qreal dx, qreal dy, const QRectF &rect) {
    (void)dx;
    (void)dy;
    update(rect);
}

/*!
    Maps \a point from this item's coordinates to those of \a item, or to the scene if \a item is \c nullptr.
*/
QPointF QGraphicsItem::mapToItem(const QGraphicsItem *item, const QPointF &point) const {
    return item ? item->mapFromScene(mapToScene(point)) : mapToScene(point);
}

/*!
    Maps \a point from item to parent coordinates.
*/
QPointF QGraphicsItem::mapToParent(const QPointF &point) const { return d_ptr->localTransform().map(point); }

/*!
    Maps \a point from item to scene coordinates.
*/
QPointF QGraphicsItem::mapToScene(const QPointF &point) const { return sceneTransform().map(point); }

/*!
    \overload

    Maps \a rect to the coordinates of \a item, or to the scene if \a item is \c nullptr.
*/
QPolygonF QGraphicsItem::mapToItem(const QGraphicsItem *item, const QRectF &rect) const {
    return item ? item->mapFromScene(mapToScene(rect)) : mapToScene(rect);
}

/*!
    \overload

    Maps \a rect to parent coordinates.
*/
QPolygonF QGraphicsItem::mapToParent(const QRectF &rect) const { return d_ptr->localTransform().map(QPolygonF(rect)); }

/*!
    \overload

    Maps \a rect to scene coordinates.
*/
QPolygonF QGraphicsItem::mapToScene(const QRectF &rect) const { return sceneTransform().map(QPolygonF(rect)); }

/*!
    \overload

    Maps \a polygon to scene coordinates.
*/
QPolygonF QGraphicsItem::mapToScene(const QPolygonF &polygon) const { return sceneTransform().map(polygon); }

/*!
    \overload

    Maps \a path to scene coordinates.
*/
QPainterPath QGraphicsItem::mapToScene(const QPainterPath &path) const { return sceneTransform().map(path); }

/*!
    Returns the bounding rectangle of \a rect mapped to the coordinates of \a item, or to the scene.
*/
QRectF QGraphicsItem::mapRectToItem(const QGraphicsItem *item, const QRectF &rect) const {
    return mapToItem(item, rect).boundingRect();
}

/*!
    Returns the bounding rectangle of \a rect mapped to parent coordinates.
*/
QRectF QGraphicsItem::mapRectToParent(const QRectF &rect) const { return d_ptr->localTransform().mapRect(rect); }

/*!
    Returns the bounding rectangle of \a rect mapped to scene coordinates.
*/
QRectF QGraphicsItem::mapRectToScene(const QRectF &rect) const { return sceneTransform().mapRect(rect); }

/*!
    Maps \a point from the coordinates of \a item (or the scene if it is \c nullptr) to this item's.
*/
QPointF QGraphicsItem::mapFromItem(const QGraphicsItem *item, const QPointF &point) const {
    return mapFromScene(item ? item->mapToScene(point) : point);
}

/*!
    Maps \a point from parent to item coordinates.
*/
QPointF QGraphicsItem::mapFromParent(const QPointF &point) const {
    return d_ptr->localTransform().inverted().map(point);
}

/*!
    Maps \a point from scene to item coordinates.
*/
QPointF QGraphicsItem::mapFromScene(const QPointF &point) const { return sceneTransform().inverted().map(point); }

/*!
    \overload

    Maps \a rect from the coordinates of \a item, or of the scene, to this item's.
*/
QPolygonF QGraphicsItem::mapFromItem(const QGraphicsItem *item, const QRectF &rect) const {
    return mapFromScene(item ? item->mapToScene(rect) : QPolygonF(rect));
}

/*!
    \overload

    Maps \a rect from parent coordinates.
*/
QPolygonF QGraphicsItem::mapFromParent(const QRectF &rect) const {
    return d_ptr->localTransform().inverted().map(QPolygonF(rect));
}

/*!
    \overload

    Maps \a rect from scene coordinates.
*/
QPolygonF QGraphicsItem::mapFromScene(const QRectF &rect) const {
    return sceneTransform().inverted().map(QPolygonF(rect));
}

/*!
    \overload

    Maps \a polygon from scene coordinates.
*/
QPolygonF QGraphicsItem::mapFromScene(const QPolygonF &polygon) const {
    return sceneTransform().inverted().map(polygon);
}

/*!
    \overload

    Maps \a path from scene coordinates.
*/
QPainterPath QGraphicsItem::mapFromScene(const QPainterPath &path) const {
    return sceneTransform().inverted().map(path);
}

/*!
    Returns the bounding rectangle of \a rect mapped from the coordinates of \a item, or of the scene.
*/
QRectF QGraphicsItem::mapRectFromItem(const QGraphicsItem *item, const QRectF &rect) const {
    return mapFromItem(item, rect).boundingRect();
}

/*!
    Returns the bounding rectangle of \a rect mapped from parent coordinates.
*/
QRectF QGraphicsItem::mapRectFromParent(const QRectF &rect) const {
    return d_ptr->localTransform().inverted().mapRect(rect);
}

/*!
    Returns the bounding rectangle of \a rect mapped from scene coordinates.
*/
QRectF QGraphicsItem::mapRectFromScene(const QRectF &rect) const { return sceneTransform().inverted().mapRect(rect); }

/*!
    Returns \c true if the item is an ancestor of \a child.
*/
bool QGraphicsItem::isAncestorOf(const QGraphicsItem *child) const {
    for (const QGraphicsItem *p = child ? child->d_ptr->parent : nullptr; p; p = p->d_ptr->parent) {
        if (p == this)
            return true;
    }
    return false;
}

/*!
    Returns the nearest item that is this item or an ancestor of it, and also \a other or an ancestor of it; or
    \c nullptr.
*/
QGraphicsItem *QGraphicsItem::commonAncestorItem(const QGraphicsItem *other) const {
    for (const QGraphicsItem *a = this; a; a = a->d_ptr->parent) {
        if (a == other || a->isAncestorOf(other))
            return const_cast<QGraphicsItem *>(a);
    }
    return nullptr;
}

/*!
    Returns the value stored under \a key with setData(), or an invalid QVariant.
*/
QVariant QGraphicsItem::data(int key) const { return d_ptr->data.value(key); }

/*!
    Stores \a value under \a key, for the application's own use.
*/
void QGraphicsItem::setData(int key, const QVariant &value) { d_ptr->data.insert(key, value); }

/*!
    Returns the type of the item, which qgraphicsitem_cast() compares: Type for a plain item. Subclasses of the
    application return a value from UserType.
*/
int QGraphicsItem::type() const { return Type; }

/*!
    Makes \a filterItem see the events of this item first, in its sceneEventFilter(). Both must be in the same scene.
*/
void QGraphicsItem::installSceneEventFilter(QGraphicsItem *filterItem) {
    if (!filterItem || filterItem == this || filterItem->scene() != scene() || !scene()) {
        qWarning("QGraphicsItem::installSceneEventFilter: the items must be different and in the same scene");
        return;
    }
    if (!d_ptr->filters.contains(filterItem))
        d_ptr->filters.append(filterItem);
}

/*!
    Removes \a filterItem from the filters of this item.
*/
void QGraphicsItem::removeSceneEventFilter(QGraphicsItem *filterItem) { d_ptr->filters.removeAll(filterItem); }

/*!
    Called with the \a event meant for \a watched when this item filters its events. Returns \c true to stop it.
    The default returns \c false.
*/
bool QGraphicsItem::sceneEventFilter(QGraphicsItem *watched, QEvent *event) {
    (void)watched;
    (void)event;
    return false;
}

/*!
    \since 6.0

    Returns \c true if drags can be dropped on the item.

    \sa setAcceptDrops()
*/
bool QGraphicsItem::acceptDrops() const { return d_ptr->acceptDrops; }

/*!
    \since 6.0

    Lets drags be dropped on the item if \a on is \c true: it then gets dragEnterEvent() and the events that
    follow, from the scene of a QGraphicsView.
*/
void QGraphicsItem::setAcceptDrops(bool on) { d_ptr->acceptDrops = on; }

/*!
    \since 6.0

    Handles a drag that comes over the item, \a event. The default ignores it; an item that takes drops accepts it.
*/
void QGraphicsItem::dragEnterEvent(QGraphicsSceneDragDropEvent *event) { event->ignore(); }

/*!
    \since 6.0

    Handles a drag that moves over the item, \a event, after the item accepted it. The default does nothing.
*/
void QGraphicsItem::dragMoveEvent(QGraphicsSceneDragDropEvent *event) { (void)event; }

/*!
    \since 6.0

    Handles a drag that left the item, \a event. The default does nothing.
*/
void QGraphicsItem::dragLeaveEvent(QGraphicsSceneDragDropEvent *event) { (void)event; }

/*!
    \since 6.0

    Handles the drop of a drag on the item, \a event. The default ignores it.
*/
void QGraphicsItem::dropEvent(QGraphicsSceneDragDropEvent *event) { event->ignore(); }

/*!
    \since 6.0

    Handles a request for a context menu over the item, \a event. Reimplement it to open a menu at
    QGraphicsSceneContextMenuEvent::screenPos(); the default ignores the event, which then goes to the item below.
*/
void QGraphicsItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) { event->ignore(); }

/*!
    Receives every \a event for the item and passes it to the handler for its type. Returns \c true if the event was
    recognized.
*/
bool QGraphicsItem::sceneEvent(QEvent *event) {
    switch (event->type()) {
    case QEvent::InputMethod:
        inputMethodEvent(static_cast<QInputMethodEvent *>(event));
        return true;
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
    case QEvent::GraphicsSceneHoverEnter:
        hoverEnterEvent(static_cast<QGraphicsSceneHoverEvent *>(event));
        return true;
    case QEvent::GraphicsSceneHoverMove:
        hoverMoveEvent(static_cast<QGraphicsSceneHoverEvent *>(event));
        return true;
    case QEvent::GraphicsSceneHoverLeave:
        hoverLeaveEvent(static_cast<QGraphicsSceneHoverEvent *>(event));
        return true;
    case QEvent::GraphicsSceneWheel:
        wheelEvent(static_cast<QGraphicsSceneWheelEvent *>(event));
        return true;
    case QEvent::GraphicsSceneContextMenu:
        contextMenuEvent(static_cast<QGraphicsSceneContextMenuEvent *>(event));
        return true;
    case QEvent::GraphicsSceneDragEnter:
        dragEnterEvent(static_cast<QGraphicsSceneDragDropEvent *>(event));
        return true;
    case QEvent::GraphicsSceneDragMove:
        dragMoveEvent(static_cast<QGraphicsSceneDragDropEvent *>(event));
        return true;
    case QEvent::GraphicsSceneDragLeave:
        dragLeaveEvent(static_cast<QGraphicsSceneDragDropEvent *>(event));
        return true;
    case QEvent::GraphicsSceneDrop:
        dropEvent(static_cast<QGraphicsSceneDragDropEvent *>(event));
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
    default:
        return false;
    }
}

/*!
    Receives the focus-in \a event. The default redraws the item.
*/
void QGraphicsItem::focusInEvent(QFocusEvent *event) {
    (void)event;
    update();
}

/*!
    Receives the focus-out \a event. The default redraws the item.
*/
void QGraphicsItem::focusOutEvent(QFocusEvent *event) {
    (void)event;
    update();
}

/*!
    Receives the hover-enter \a event when the mouse moves onto an item that accepts hover events. The default
    redraws the item.
*/
void QGraphicsItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    (void)event;
    update();
}

/*!
    Receives the hover-move \a event. The default does nothing.
*/
void QGraphicsItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) { (void)event; }

/*!
    Receives the hover-leave \a event. The default redraws the item.
*/
void QGraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    (void)event;
    update();
}

/*!
    Receives the key-press \a event while the item has the focus. The default ignores it, so it goes to the parent.
*/
void QGraphicsItem::keyPressEvent(QKeyEvent *event) { event->ignore(); }

/*!
    Receives the key-release \a event while the item has the focus. The default ignores it.
*/
void QGraphicsItem::keyReleaseEvent(QKeyEvent *event) { event->ignore(); }

/*!
    Receives the mouse-press \a event. The default accepts it, so that the item grabs the mouse, if the item is
    movable or selectable, selecting it; otherwise it ignores the event, which goes to the items below.
*/
void QGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton && (d_ptr->flags & ItemIsSelectable)) {
        const bool multi = event->modifiers() & Qt::ControlModifier;
        if (!multi && d_ptr->scene && !d_ptr->selected)
            d_ptr->scene->clearSelection();
        setSelected(multi ? !d_ptr->selected : true);
    }
    if (d_ptr->flags & (ItemIsMovable | ItemIsSelectable))
        event->accept();
    else
        event->ignore();
}

/*!
    Receives the mouse-move \a event while the item grabs the mouse. The default drags a movable item, with the other
    selected movable items, while the left button is down.
*/
void QGraphicsItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (!(event->buttons() & Qt::LeftButton) || !(d_ptr->flags & ItemIsMovable))
        return;
    QList<QGraphicsItem *> moving{this};
    if (d_ptr->scene && d_ptr->selected) {
        for (QGraphicsItem *item : d_ptr->scene->selectedItems()) {
            if (item != this && (item->flags() & ItemIsMovable) && !moving.contains(item))
                moving.append(item);
        }
    }
    for (QGraphicsItem *item : moving) {
        QGraphicsItem *p = item->parentItem();
        const QPointF from = p ? p->mapFromScene(event->lastScenePos()) : event->lastScenePos();
        const QPointF to = p ? p->mapFromScene(event->scenePos()) : event->scenePos();
        item->setPos(item->pos() + (to - from));
    }
}

/*!
    Receives the mouse-release \a event. The default does nothing.
*/
void QGraphicsItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) { (void)event; }

/*!
    Receives the double-click \a event. The default treats it as a press.
*/
void QGraphicsItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) { mousePressEvent(event); }

/*!
    Receives the wheel \a event. The default ignores it, so the view scrolls.
*/
void QGraphicsItem::wheelEvent(QGraphicsSceneWheelEvent *event) { event->ignore(); }

/*!
    Called when \a change is about to happen, or has happened, with \a value describing it. For the changes that are
    about to happen, the returned value is applied instead, so a subclass can adjust or veto them. The default
    returns \a value.
*/
QVariant QGraphicsItem::itemChange(GraphicsItemChange change, const QVariant &value) {
    (void)change;
    return value;
}

/*!
    Tells the scene that boundingRect() is about to change, so that the area it covers now is redrawn. Call it
    before the change.
*/
void QGraphicsItem::prepareGeometryChange() {
    if (d_ptr->scene && isVisible())
        QGraphicsSceneAccess::d(d_ptr->scene)->markDirty(QGraphicsItemAccess::subtreeSceneRect(this));
}

/*!
    Accepted for compatibility; Nt has no input methods.
*/
void QGraphicsItem::updateMicroFocus() {}

void QGraphicsItem::notifyChange(GraphicsItemChange change) { (void)change; }

// ---- QGraphicsObject -----------------------------------------------------------------------------------------

/*!
    \class QGraphicsObject
    \brief The QGraphicsObject class is a QGraphicsItem that is also a QObject, with signals and properties.

    Derive from it for an item that needs signals, slots or properties. The position, opacity, z value, rotation and
    scale are properties with change signals.

    \differences
    There are no gestures. widthChanged() and heightChanged() are for subclasses to emit.
    \enddifferences
*/

/*!
    Constructs an object with the given \a parent item.
*/
QGraphicsObject::QGraphicsObject(QGraphicsItem *parent) : QObject(nullptr), QGraphicsItem(parent) {
    QGraphicsItemAccess::d(this)->isObject = true;
}

/*!
    Destroys the object.
*/
QGraphicsObject::~QGraphicsObject() = default;

/*!
    Subscribes the item to the gestures of the \a type, with the \a flags. Nt does not recognize gestures itself; the
    subscription is only kept.

    \sa ungrabGesture()
*/
void QGraphicsObject::grabGesture(Qt::GestureType type, Qt::GestureFlags flags) {
    (void)flags;
    if (!m_gestures.contains(type))
        m_gestures.append(type);
}

/*!
    Unsubscribes the item from the gestures of the \a type.

    \sa grabGesture()
*/
void QGraphicsObject::ungrabGesture(Qt::GestureType type) { m_gestures.removeAll(type); }

/*!
    Accepted for compatibility; Nt has no input methods.
*/
void QGraphicsObject::updateMicroFocus() {}

/*!
    \reimp
*/
bool QGraphicsObject::event(QEvent *ev) { return QObject::event(ev); }

void QGraphicsObject::notifyChange(GraphicsItemChange change) {
    switch (change) {
    case ItemPositionChange:
        emit xChanged();
        break;
    case ItemPositionHasChanged:
        emit yChanged();
        break;
    case ItemOpacityHasChanged:
        emit opacityChanged();
        break;
    case ItemVisibleHasChanged:
        emit visibleChanged();
        break;
    case ItemEnabledHasChanged:
        emit enabledChanged();
        break;
    case ItemZValueHasChanged:
        emit zChanged();
        break;
    case ItemRotationHasChanged:
        emit rotationChanged();
        break;
    case ItemScaleHasChanged:
        emit scaleChanged();
        break;
    case ItemParentHasChanged:
        emit parentChanged();
        break;
    case ItemChildAddedChange:
    case ItemChildRemovedChange:
        emit childrenChanged();
        break;
    default:
        break;
    }
}

/*!
    \enum QGraphicsItem::PanelModality
    \since 6.0

    How a panel blocks the input of other items while it is shown.

    \value NonModal It does not.
    \value PanelModal It blocks its ancestor panels.
    \value SceneModal It blocks every other panel of the scene.
*/

/*!
    \enum QGraphicsItem::Extension
    \since 6.0

    Extensions of an item, which a subclass can support.

    \value UserExtension The first value for extensions of an application.
*/

/*!
    \since 6.0

    Returns the item that takes the focus for this one, or \c nullptr.
*/
QGraphicsItem *QGraphicsItem::focusProxy() const { return d_ptr->focusProxy; }

/*!
    \since 6.0

    Makes \a item take the focus whenever this item is given it; \c nullptr removes the proxy. An item of another
    scene, or one that would make a loop, is refused.
*/
void QGraphicsItem::setFocusProxy(QGraphicsItem *item) {
    if (item == this)
        return;
    if (item && item->scene() && d_ptr->scene && item->scene() != d_ptr->scene)
        return;
    for (QGraphicsItem *p = item; p; p = p->d_ptr->focusProxy)
        if (p == this)
            return;
    d_ptr->focusProxy = item;
}

/*!
    \since 6.0

    Returns the item that has the focus within this focus scope; Nt has no focus scopes, so this is \c nullptr.
*/
QGraphicsItem *QGraphicsItem::focusScopeItem() const { return nullptr; }

/*!
    \since 6.0

    Returns the nearest panel among the item and its ancestors, or \c nullptr.

    \sa isPanel()
*/
QGraphicsItem *QGraphicsItem::panel() const {
    for (const QGraphicsItem *item = this; item; item = item->parentItem())
        if (item->isPanel())
            return const_cast<QGraphicsItem *>(item); // NOLINT(cppcoreguidelines-pro-type-const-cast): Qt's signature
    return nullptr;
}

/*!
    \since 6.0

    Returns the outermost widget among the item and its ancestors, or \c nullptr.
*/
QGraphicsWidget *QGraphicsItem::topLevelWidget() const {
    QGraphicsWidget *found = nullptr;
    for (const QGraphicsItem *item = this; item; item = item->parentItem())
        if (item->isWidget())
            found = static_cast<QGraphicsWidget *>(const_cast<QGraphicsItem *>(item)); // NOLINT
    return found;
}

/*!
    \since 6.0

    Returns the nearest window among the item and its ancestors, or \c nullptr.
*/
QGraphicsWidget *QGraphicsItem::window() const {
    for (const QGraphicsItem *item = this; item; item = item->parentItem())
        if (item->isWindow())
            return static_cast<QGraphicsWidget *>(const_cast<QGraphicsItem *>(item)); // NOLINT
    return nullptr;
}

/*!
    \since 6.0

    Returns how the item, if it is a panel, blocks other panels.
*/
QGraphicsItem::PanelModality QGraphicsItem::panelModality() const { return d_ptr->modality; }

/*!
    \since 6.0

    Sets how the item, if it is a panel, blocks other panels to \a panelModality.
*/
void QGraphicsItem::setPanelModality(PanelModality panelModality) { d_ptr->modality = panelModality; }

/*!
    \since 6.0

    Returns \c true if a modal panel that is shown blocks the item, and puts it in \a blockingPanel if that is not
    \c nullptr. A scene modal panel blocks every item that is not in it; a panel modal one blocks its ancestors.
*/
bool QGraphicsItem::isBlockedByModalPanel(QGraphicsItem **blockingPanel) const {
    if (!d_ptr->scene)
        return false;
    const QGraphicsItem *own = panel();
    for (QGraphicsItem *item : d_ptr->scene->items()) {
        if (!item->isPanel() || !item->isVisible() || item->d_ptr->modality == NonModal || item == own)
            continue;
        bool blocks = false;
        if (item->d_ptr->modality == SceneModal)
            blocks = !item->isAncestorOf(this);
        else
            blocks = own && own->isAncestorOf(item);
        if (blocks) {
            if (blockingPanel)
                *blockingPanel = item;
            return true;
        }
    }
    return false;
}

/*!
    \since 6.0

    Returns \c true if the item filters the events of its children.
*/
bool QGraphicsItem::filtersChildEvents() const { return d_ptr->filtersChildEvents; }

/*!
    \since 6.0

    Makes the item see the events of its children in sceneEventFilter() before they do, if \a enabled is \c true.
*/
void QGraphicsItem::setFiltersChildEvents(bool enabled) { d_ptr->filtersChildEvents = enabled; }

/*!
    \since 6.0

    Returns the input method hints of the item.
*/
Qt::InputMethodHints QGraphicsItem::inputMethodHints() const { return d_ptr->inputMethodHints; }

/*!
    \since 6.0

    Sets the input method hints of the item to \a hints.
*/
void QGraphicsItem::setInputMethodHints(Qt::InputMethodHints hints) { d_ptr->inputMethodHints = hints; }

/*!
    \since 6.0

    Returns the region of the device that the item covers, its bounding rectangle mapped by
    \a itemToDeviceTransform.
*/
QRegion QGraphicsItem::boundingRegion(const QTransform &itemToDeviceTransform) const {
    return QRegion(itemToDeviceTransform.mapRect(boundingRect()).toAlignedRect());
}

/*!
    \since 6.0

    Returns how finely boundingRegion() follows the shape, from 0 to 1. Nt returns the bounding rectangle whatever
    it is.
*/
qreal QGraphicsItem::boundingRegionGranularity() const { return d_ptr->regionGranularity; }

/*!
    \since 6.0

    Sets how finely boundingRegion() follows the shape to \a granularity, from 0 to 1.
*/
void QGraphicsItem::setBoundingRegionGranularity(qreal granularity) {
    d_ptr->regionGranularity = std::clamp(granularity, 0.0, 1.0);
}

/*!
    \since 6.0

    Is called twice by QGraphicsScene::advance(): with \a phase 0 to prepare a step of an animation, then with 1 to
    take it. The default does nothing.
*/
void QGraphicsItem::advance(int phase) { (void)phase; }

/*!
    \since 6.0

    Handles \a event, text from an input method. The default ignores it.
*/
void QGraphicsItem::inputMethodEvent(QInputMethodEvent *event) { event->ignore(); }

/*!
    \since 6.0

    Returns the input method property \a query of the item. The default returns an invalid QVariant.
*/
QVariant QGraphicsItem::inputMethodQuery(Qt::InputMethodQuery query) const {
    (void)query;
    return QVariant();
}

/*!
    \since 6.0

    Returns \c true if the item supports \a extension. The default returns \c false.
*/
bool QGraphicsItem::supportsExtension(Extension extension) const {
    (void)extension;
    return false;
}

/*!
    \since 6.0

    Sets the \a extension of the item to \a variant. The default does nothing.
*/
void QGraphicsItem::setExtension(Extension extension, const QVariant &variant) {
    (void)extension;
    (void)variant;
}

/*!
    \since 6.0

    Returns the extension that \a variant asks for. The default returns an invalid QVariant.
*/
QVariant QGraphicsItem::extension(const QVariant &variant) const {
    (void)variant;
    return QVariant();
}

/*!
    \since 6.0

    Adds the item to the index of its scene. The scene of Nt keeps no index, so this does nothing.
*/
void QGraphicsItem::addToIndex() {}

/*!
    \since 6.0

    Takes the item out of the index of its scene. The scene of Nt keeps no index, so this does nothing.
*/
void QGraphicsItem::removeFromIndex() {}

#include "moc_qgraphicsitem.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
