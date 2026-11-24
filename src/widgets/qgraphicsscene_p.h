// MIT License - see LICENSE
//
// The private state of the Graphics View classes, shared by their sources.
#pragma once

#include <QtCore/qmap.h>
#include <QtCore/qpointer.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qgraphicsitem.h>
#include <QtWidgets/qgraphicsscene.h>
#include <QtWidgets/qgraphicsview.h>

class QGraphicsEffect;
class QGraphicsSceneMouseEvent;
class QStyleOptionGraphicsItem;

class QGraphicsTransform;

struct QGraphicsItem::Private {
    QGraphicsItem *q = nullptr;
    QGraphicsScene *scene = nullptr;
    QGraphicsItem *parent = nullptr;
    QList<QGraphicsItem *> children; // in the order they were added; stacking is by z, then by this order
    QGraphicsItemGroup *group = nullptr;
    QGraphicsEffect *effect = nullptr; // owned
    QPointF pos;
    qreal z = 0.0;
    qreal opacity = 1.0;
    QTransform transform;
    qreal rotation = 0.0;
    qreal scale = 1.0;
    QList<QGraphicsTransform *> transformations; // setTransformations(); not owned
    QPointF origin;
    GraphicsItemFlags flags;
    CacheMode cacheMode = NoCache;
    Qt::MouseButtons acceptedButtons =
        Qt::LeftButton | Qt::RightButton | Qt::MiddleButton | Qt::BackButton | Qt::ForwardButton;
    bool explicitlyHidden = false;
    bool explicitlyDisabled = false;
    bool selected = false;
    bool acceptHover = false;
    bool handlesChildEvents = false; // the item takes the mouse events of its descendants
    bool acceptDrops = false;
    bool acceptTouch = false;
    bool underMouse = false;
    bool wantsFocus = false; // setFocus() before the item was in a scene
    bool isObject = false;
    bool hasCursor = false;
    bool destroying = false;
    QCursor cursor;
    QRectF paintedRect; // where the item was last drawn, in scene coordinates, for redrawing it when it goes
    QString toolTip;
    QMap<int, QVariant> data;
    QList<QGraphicsItem *> filters;
    QGraphicsItem *focusProxy = nullptr; // setFocusProxy(); cleared when the proxy goes
    QGraphicsItem::PanelModality modality = QGraphicsItem::NonModal;
    bool filtersChildEvents = false;
    Qt::InputMethodHints inputMethodHints;
    qreal regionGranularity = 0.0;

    // The transformation from item to parent coordinates.
    QTransform localTransform() const;
};

// Reaches the private parts of items for the scene, the view and the items' own sources.
struct QGraphicsItemAccess {
    static QGraphicsItem::Private *d(const QGraphicsItem *item) { return item->d_ptr.get(); }
    // A transformation of the item changed, or was deleted.
    static void transformationChanged(QGraphicsItem *item);
    static void forgetTransformation(QGraphicsItem *item, QGraphicsTransform *transformation);
    static bool sceneEvent(QGraphicsItem *item, QEvent *event) { return item->sceneEvent(event); }
    static bool sceneEventFilter(QGraphicsItem *filter, QGraphicsItem *watched, QEvent *event) {
        return filter->sceneEventFilter(watched, event);
    }
    static QVariant itemChange(QGraphicsItem *item, QGraphicsItem::GraphicsItemChange change, const QVariant &value) {
        return item->itemChange(change, value);
    }
    static void notify(QGraphicsItem *item, QGraphicsItem::GraphicsItemChange change) { item->notifyChange(change); }
    // The children of `item` (or the top-level items of `scene` for a null item) in the order they are drawn.
    static QList<QGraphicsItem *> sortedChildren(const QList<QGraphicsItem *> &children);
    // The rectangle in scene coordinates that `item` and its visible descendants cover.
    static QRectF subtreeSceneRect(const QGraphicsItem *item);
    // Whether every ancestor that clips its children lets `scenePos` through to `item`.
    static bool passesAncestorClips(const QGraphicsItem *item, const QPointF &scenePos);
};

struct QGraphicsScene::Private {
    QGraphicsScene *q = nullptr;
    QList<QGraphicsItem *> topLevel;
    QRectF sceneRect;
    bool sceneRectSet = false;
    mutable QRectF growingRect;
    QBrush backgroundBrush;
    QBrush foregroundBrush;
    QFont font;
    QPalette palette;
    int bspTreeDepth = 0;
    qreal minimumRenderSize = 0;
    bool focusOnTouch = true;
    QPointer<QStyle> style;
    QPointer<QGraphicsWidget> activeWindow;
    bool stickyFocus = false;
    bool hasFocus = false;
    ItemIndexMethod indexMethod = BspTreeIndex;
    QGraphicsItem *focusItem = nullptr;
    QGraphicsItem *grabber = nullptr;
    QGraphicsItem *dropItem = nullptr; // the item a drag is over, which accepted it
    QGraphicsItem *keyboardGrabber = nullptr;
    QList<QGraphicsItem *> hoverItems; // from the outermost to the innermost
    QList<QGraphicsView *> views;
    QList<QRectF> dirty;
    QList<QGraphicsItem *> pending; // made with a parent in this scene; drawn on the next update pass
    bool updateQueued = false;
    QRectF lastSceneRect;

    // Records that `rect` (scene coordinates) has to be drawn again, and tells the views.
    void markDirty(const QRectF &rect);
    void addPending(QGraphicsItem *item);
    void queueUpdate();
    void processDirty();
    // Adds `item` and its descendants to the scene, or takes them out.
    void adopt(QGraphicsItem *item);
    void release(QGraphicsItem *item);
    // Every item, in the order they are drawn.
    QList<QGraphicsItem *> paintOrder() const;
    void collect(QGraphicsItem *item, QList<QGraphicsItem *> &out) const;
    // The visible items whose shape (or bounding rectangle) contains `pos`, the topmost first.
    QList<QGraphicsItem *> itemsAt(const QPointF &pos) const;
    // Sets the item-coordinate positions of a mouse event for `item`.
    static void mapMouseEvent(QGraphicsSceneMouseEvent *event, const QGraphicsItem *item);
    // Moves the hover to the items under `scenePos`, sending them enter, move and leave events.
    void updateHover(const QPointF &scenePos, const QPointF &screenPos, QWidget *widget,
                     Qt::KeyboardModifiers modifiers);
    void clearHover();
    // The item whose cursor shows at `pos`, or null.
    QGraphicsItem *cursorItemAt(const QPointF &pos) const;
    // Draws the items that touch `exposed` (scene coordinates) with `viewTransform` from scene to device.
    void drawItems(QPainter *painter, const QTransform &viewTransform, const QRectF &exposed, QWidget *widget);
    void drawItem(QPainter *painter, QGraphicsItem *item, const QTransform &viewTransform, const QRectF &exposed,
                  QWidget *widget, qreal parentOpacity, bool parentPropagates, const QRect &clip, bool clipped);
    // Draws `item` through its graphics `effect`: painted into an image of its device rectangle first.
    void drawWithEffect(QPainter *painter, QGraphicsItem *item, QGraphicsEffect *effect, const QTransform &toDevice,
                        const QStyleOptionGraphicsItem &option, QWidget *widget);
    // The item, or its first ancestor, that takes the focus when it is clicked.
    static QGraphicsItem *focusableAncestor(QGraphicsItem *item);
    // Called when an item leaves the scene or becomes invisible or disabled.
    void forget(QGraphicsItem *item);
};

struct QGraphicsSceneAccess {
    static QGraphicsScene::Private *d(const QGraphicsScene *scene) { return scene->d.get(); }
};

struct QGraphicsView::Private {
    QGraphicsView *q = nullptr;
    QPointer<QGraphicsScene> scene;
    QTransform matrix;
    QRectF sceneRect;
    bool sceneRectSet = false;
    Qt::Alignment alignment = Qt::AlignCenter;
    ViewportAnchor transformationAnchor = AnchorViewCenter;
    ViewportAnchor resizeAnchor = NoAnchor;
    ViewportUpdateMode updateMode = MinimalViewportUpdate;
    OptimizationFlags optimizationFlags;
    DragMode dragMode = NoDrag;
    CacheMode cacheMode = CacheNone;
    bool interactive = true;
    QPainter::RenderHints renderHints = QPainter::TextAntialiasing;
    QBrush backgroundBrush;
    QBrush foregroundBrush;
    QMap<int, QPointF> downScenePos;
    QMap<int, QPointF> downScreenPos;
    QPointF lastScenePos;
    QPointF lastScreenPos;
    bool haveLastPos = false;
    Qt::ItemSelectionMode rubberBandMode = Qt::IntersectsItemShape;
    bool rubberBanding = false; // a RubberBandDrag is on
    QPoint rubberOrigin;        // where it started, in the viewport
    QRect rubberRect;           // the band, in the viewport
    bool handScrolling = false; // a ScrollHandDrag is on
    QPoint handLast;            // the last position of the mouse while it is

    // Asks the viewport to draw `rect` (scene coordinates) again.
    void sceneDirty(const QRectF &rect);
};

struct QGraphicsViewAccess {
    static QGraphicsView::Private *d(const QGraphicsView *view) { return view->d.get(); }
};
