// MIT License - see LICENSE
#pragma once

#include <QtCore/qline.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qvariant.h>
#include <QtGui/qbrush.h>
#include <QtGui/qcursor.h>
#include <QtGui/qfont.h>
#include <QtGui/qpainterpath.h>
#include <QtGui/qpen.h>
#include <QtGui/qpixmap.h>
#include <QtGui/qpolygon.h>
#include <QtGui/qtransform.h>

#include <memory>

class QFocusEvent;
class QGraphicsEffect;
class QGraphicsItemGroup;
class QGraphicsObject;
class QGraphicsScene;
class QGraphicsSceneContextMenuEvent;
class QInputMethodEvent;
class QGraphicsSceneDragDropEvent;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneWheelEvent;
class QGraphicsTransform;
class QGraphicsWidget;
class QKeyEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QTextDocument;
class QTextCursor;
class QWidget;

class QGraphicsItem {
public:
    enum GraphicsItemFlag {
        ItemIsMovable = 0x1,
        ItemIsSelectable = 0x2,
        ItemIsFocusable = 0x4,
        ItemClipsToShape = 0x8,
        ItemClipsChildrenToShape = 0x10,
        ItemIgnoresTransformations = 0x20,
        ItemIgnoresParentOpacity = 0x40,
        ItemDoesntPropagateOpacityToChildren = 0x80,
        ItemStacksBehindParent = 0x100,
        ItemUsesExtendedStyleOption = 0x200,
        ItemHasNoContents = 0x400,
        ItemSendsGeometryChanges = 0x800,
        ItemAcceptsInputMethod = 0x1000,
        ItemNegativeZStacksBehindParent = 0x2000,
        ItemIsPanel = 0x4000,
        ItemIsFocusScope = 0x8000,
        ItemSendsScenePositionChanges = 0x10000,
        ItemStopsClickFocusPropagation = 0x20000,
        ItemStopsFocusHandling = 0x40000,
        ItemContainsChildrenInShape = 0x80000
    };
    Q_DECLARE_FLAGS(GraphicsItemFlags, GraphicsItemFlag)

    enum GraphicsItemChange {
        ItemPositionChange = 0,
        ItemVisibleChange = 2,
        ItemEnabledChange = 3,
        ItemSelectedChange = 4,
        ItemParentChange = 5,
        ItemChildAddedChange = 6,
        ItemChildRemovedChange = 7,
        ItemTransformChange = 8,
        ItemPositionHasChanged = 9,
        ItemTransformHasChanged = 10,
        ItemSceneChange = 11,
        ItemVisibleHasChanged = 12,
        ItemEnabledHasChanged = 13,
        ItemSelectedHasChanged = 14,
        ItemParentHasChanged = 15,
        ItemSceneHasChanged = 16,
        ItemCursorChange = 17,
        ItemCursorHasChanged = 18,
        ItemToolTipChange = 19,
        ItemToolTipHasChanged = 20,
        ItemFlagsChange = 21,
        ItemFlagsHaveChanged = 22,
        ItemZValueChange = 23,
        ItemZValueHasChanged = 24,
        ItemOpacityChange = 25,
        ItemOpacityHasChanged = 26,
        ItemScenePositionHasChanged = 27,
        ItemRotationChange = 28,
        ItemRotationHasChanged = 29,
        ItemScaleChange = 30,
        ItemScaleHasChanged = 31,
        ItemTransformOriginPointChange = 32,
        ItemTransformOriginPointHasChanged = 33
    };

    enum CacheMode { NoCache, ItemCoordinateCache, DeviceCoordinateCache };
    enum PanelModality { NonModal, PanelModal, SceneModal };
    enum Extension : unsigned { UserExtension = 0x80000000 };

    enum { Type = 1, UserType = 65536 };

    explicit QGraphicsItem(QGraphicsItem *parent = nullptr);
    virtual ~QGraphicsItem();
    QGraphicsItem(const QGraphicsItem &) = delete;
    QGraphicsItem &operator=(const QGraphicsItem &) = delete;

    QGraphicsScene *scene() const;
    QGraphicsItem *parentItem() const;
    QGraphicsItem *topLevelItem() const;
    QGraphicsObject *parentObject() const;
    QGraphicsWidget *parentWidget() const;
    void setParentItem(QGraphicsItem *parent);
    QList<QGraphicsItem *> childItems() const;
    bool isWidget() const;
    bool isWindow() const;
    bool isPanel() const;
    QGraphicsObject *toGraphicsObject();
    const QGraphicsObject *toGraphicsObject() const;
    QGraphicsItemGroup *group() const;
    void setGroup(QGraphicsItemGroup *group);

    GraphicsItemFlags flags() const;
    void setFlag(GraphicsItemFlag flag, bool enabled = true);
    void setFlags(GraphicsItemFlags flags);

    QGraphicsEffect *graphicsEffect() const;
    void setGraphicsEffect(QGraphicsEffect *effect);

    CacheMode cacheMode() const;
    void setCacheMode(CacheMode mode, const QSize &cacheSize = QSize());

    QString toolTip() const;
    void setToolTip(const QString &toolTip);
    QCursor cursor() const;
    void setCursor(const QCursor &cursor);
    bool hasCursor() const;
    void unsetCursor();

    bool isVisible() const;
    bool isVisibleTo(const QGraphicsItem *parent) const;
    void setVisible(bool visible);
    /*! Shows the item; the same as setVisible(true). */
    void hide() { setVisible(false); }
    /*! Hides the item; the same as setVisible(false). */
    void show() { setVisible(true); }

    bool isEnabled() const;
    void setEnabled(bool enabled);

    bool isSelected() const;
    void setSelected(bool selected);

    qreal opacity() const;
    qreal effectiveOpacity() const;
    void setOpacity(qreal opacity);

    Qt::MouseButtons acceptedMouseButtons() const;
    void setAcceptedMouseButtons(Qt::MouseButtons buttons);
    bool acceptDrops() const;
    void setAcceptDrops(bool on);
    bool acceptHoverEvents() const;
    void setAcceptHoverEvents(bool enabled);
    bool handlesChildEvents() const;
    void setHandlesChildEvents(bool enabled);
    bool acceptTouchEvents() const;
    void setAcceptTouchEvents(bool enabled);
    bool isUnderMouse() const;

    bool hasFocus() const;
    void setFocus(Qt::FocusReason focusReason = Qt::OtherFocusReason);
    void clearFocus();
    QGraphicsItem *focusItem() const;
    QGraphicsItem *focusProxy() const;
    void setFocusProxy(QGraphicsItem *item);
    QGraphicsItem *focusScopeItem() const;
    QGraphicsItem *panel() const;
    QGraphicsWidget *topLevelWidget() const;
    QGraphicsWidget *window() const;
    PanelModality panelModality() const;
    void setPanelModality(PanelModality panelModality);
    bool isBlockedByModalPanel(QGraphicsItem **blockingPanel = nullptr) const;
    bool filtersChildEvents() const;
    void setFiltersChildEvents(bool enabled);
    Qt::InputMethodHints inputMethodHints() const;
    void setInputMethodHints(Qt::InputMethodHints hints);
    QRegion boundingRegion(const QTransform &itemToDeviceTransform) const;
    qreal boundingRegionGranularity() const;
    void setBoundingRegionGranularity(qreal granularity);
    virtual void advance(int phase);
    bool isActive() const;
    void setActive(bool active);

    void grabMouse();
    void ungrabMouse();
    void grabKeyboard();
    void ungrabKeyboard();

    QPointF pos() const;
    /*! Returns the x coordinate of pos(). */
    qreal x() const { return pos().x(); }
    void setX(qreal x);
    /*! Returns the y coordinate of pos(). */
    qreal y() const { return pos().y(); }
    void setY(qreal y);
    QPointF scenePos() const;
    void setPos(const QPointF &pos);
    /*! \overload Moves the item to (\a x, \a y) in parent coordinates. */
    void setPos(qreal x, qreal y) { setPos(QPointF(x, y)); }
    /*! Moves the item by \a dx and \a dy in parent coordinates. */
    void moveBy(qreal dx, qreal dy) { setPos(pos() + QPointF(dx, dy)); }
    void ensureVisible(const QRectF &rect = QRectF(), int xmargin = 50, int ymargin = 50);

    QTransform transform() const;
    QTransform sceneTransform() const;
    QTransform deviceTransform(const QTransform &viewportTransform) const;
    QTransform itemTransform(const QGraphicsItem *other, bool *ok = nullptr) const;
    void setTransform(const QTransform &matrix, bool combine = false);
    void resetTransform();
    QList<QGraphicsTransform *> transformations() const;
    void setTransformations(const QList<QGraphicsTransform *> &transformations);
    void setRotation(qreal angle);
    qreal rotation() const;
    void setScale(qreal scale);
    qreal scale() const;
    QPointF transformOriginPoint() const;
    void setTransformOriginPoint(const QPointF &origin);
    /*! \overload Sets the transform origin to (\a ax, \a ay). */
    void setTransformOriginPoint(qreal ax, qreal ay) { setTransformOriginPoint(QPointF(ax, ay)); }

    qreal zValue() const;
    void setZValue(qreal z);
    void stackBefore(const QGraphicsItem *sibling);

    virtual QRectF boundingRect() const = 0;
    QRectF childrenBoundingRect() const;
    QRectF sceneBoundingRect() const;
    virtual QPainterPath shape() const;
    bool isClipped() const;
    QPainterPath clipPath() const;
    virtual bool contains(const QPointF &point) const;
    virtual bool collidesWithItem(const QGraphicsItem *other,
                                  Qt::ItemSelectionMode mode = Qt::IntersectsItemShape) const;
    virtual bool collidesWithPath(const QPainterPath &path, Qt::ItemSelectionMode mode = Qt::IntersectsItemShape) const;
    QList<QGraphicsItem *> collidingItems(Qt::ItemSelectionMode mode = Qt::IntersectsItemShape) const;
    bool isObscured(const QRectF &rect = QRectF()) const;
    virtual bool isObscuredBy(const QGraphicsItem *item) const;
    virtual QPainterPath opaqueArea() const;

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) = 0;

    void update(const QRectF &rect = QRectF());
    /*! \overload Schedules a redraw of the rectangle at (\a x, \a y) that is \a width by \a height. */
    void update(qreal x, qreal y, qreal width, qreal height) { update(QRectF(x, y, width, height)); }
    void scroll(qreal dx, qreal dy, const QRectF &rect = QRectF());

    QPointF mapToItem(const QGraphicsItem *item, const QPointF &point) const;
    QPointF mapToParent(const QPointF &point) const;
    QPointF mapToScene(const QPointF &point) const;
    QPolygonF mapToItem(const QGraphicsItem *item, const QRectF &rect) const;
    QPolygonF mapToParent(const QRectF &rect) const;
    QPolygonF mapToScene(const QRectF &rect) const;
    QPolygonF mapToScene(const QPolygonF &polygon) const;
    QPainterPath mapToScene(const QPainterPath &path) const;
    QRectF mapRectToItem(const QGraphicsItem *item, const QRectF &rect) const;
    QRectF mapRectToParent(const QRectF &rect) const;
    QRectF mapRectToScene(const QRectF &rect) const;
    QPointF mapFromItem(const QGraphicsItem *item, const QPointF &point) const;
    QPointF mapFromParent(const QPointF &point) const;
    QPointF mapFromScene(const QPointF &point) const;
    QPolygonF mapFromItem(const QGraphicsItem *item, const QRectF &rect) const;
    QPolygonF mapFromParent(const QRectF &rect) const;
    QPolygonF mapFromScene(const QRectF &rect) const;
    QPolygonF mapFromScene(const QPolygonF &polygon) const;
    QPainterPath mapFromScene(const QPainterPath &path) const;
    QRectF mapRectFromItem(const QGraphicsItem *item, const QRectF &rect) const;
    QRectF mapRectFromParent(const QRectF &rect) const;
    QRectF mapRectFromScene(const QRectF &rect) const;
    /*! \overload Maps (\a x, \a y) to scene coordinates. */
    QPointF mapToScene(qreal x, qreal y) const { return mapToScene(QPointF(x, y)); }
    /*! \overload Maps (\a x, \a y) from scene coordinates. */
    QPointF mapFromScene(qreal x, qreal y) const { return mapFromScene(QPointF(x, y)); }

    bool isAncestorOf(const QGraphicsItem *child) const;
    QGraphicsItem *commonAncestorItem(const QGraphicsItem *other) const;

    QVariant data(int key) const;
    void setData(int key, const QVariant &value);

    virtual int type() const;

    void installSceneEventFilter(QGraphicsItem *filterItem);
    void removeSceneEventFilter(QGraphicsItem *filterItem);

protected:
    virtual bool sceneEventFilter(QGraphicsItem *watched, QEvent *event);
    virtual bool sceneEvent(QEvent *event);
    virtual void focusInEvent(QFocusEvent *event);
    virtual void focusOutEvent(QFocusEvent *event);
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void keyReleaseEvent(QKeyEvent *event);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
    virtual void wheelEvent(QGraphicsSceneWheelEvent *event);
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent *event);
    virtual void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
    virtual void dragMoveEvent(QGraphicsSceneDragDropEvent *event);
    virtual void dragLeaveEvent(QGraphicsSceneDragDropEvent *event);
    virtual void dropEvent(QGraphicsSceneDragDropEvent *event);
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value);
    virtual void inputMethodEvent(QInputMethodEvent *event);
    virtual QVariant inputMethodQuery(Qt::InputMethodQuery query) const;
    virtual bool supportsExtension(Extension extension) const;
    virtual void setExtension(Extension extension, const QVariant &variant);
    virtual QVariant extension(const QVariant &variant) const;
    void addToIndex();
    void removeFromIndex();

    void prepareGeometryChange();
    void updateMicroFocus();

private:
    friend class QGraphicsScene;
    friend class QGraphicsView;
    friend class QGraphicsObject;
    friend class QGraphicsItemGroup;
    friend struct QGraphicsItemAccess;
    // Tells a QGraphicsObject that `change` happened, so that it emits its signals.
    virtual void notifyChange(GraphicsItemChange change);
    struct Private;
    std::unique_ptr<Private> d_ptr;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QGraphicsItem::GraphicsItemFlags)

// Returns `item` cast to T if it is of that type, as type() tells; otherwise returns nullptr. T is a pointer to a
// class with a Type enumerator, such as QGraphicsRectItem *.
template <class T> inline T qgraphicsitem_cast(QGraphicsItem *item) {
    using Item = std::remove_cv_t<std::remove_pointer_t<T>>;
    return int(Item::Type) == int(QGraphicsItem::Type) || (item && int(Item::Type) == item->type())
               ? static_cast<T>(item)
               : nullptr;
}

// The same for a constant item.
template <class T> inline T qgraphicsitem_cast(const QGraphicsItem *item) {
    using Item = std::remove_cv_t<std::remove_pointer_t<T>>;
    return int(Item::Type) == int(QGraphicsItem::Type) || (item && int(Item::Type) == item->type())
               ? static_cast<T>(item)
               : nullptr;
}

class QGraphicsObject : public QObject, public QGraphicsItem {
    Q_OBJECT
    Q_PROPERTY(QGraphicsObject *parent READ parentObject WRITE setParentItem NOTIFY parentChanged)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QPointF pos READ pos WRITE setPos)
    Q_PROPERTY(qreal x READ x WRITE setX NOTIFY xChanged)
    Q_PROPERTY(qreal y READ y WRITE setY NOTIFY yChanged)
    Q_PROPERTY(qreal z READ zValue WRITE setZValue NOTIFY zChanged)
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(qreal scale READ scale WRITE setScale NOTIFY scaleChanged)
    Q_PROPERTY(QPointF transformOriginPoint READ transformOriginPoint WRITE setTransformOriginPoint)
    Q_INTERFACES(QGraphicsItem)
public:
    explicit QGraphicsObject(QGraphicsItem *parent = nullptr);
    ~QGraphicsObject() override;

public slots:
    void updateMicroFocus();

signals:
    /*! This signal is emitted when the parent item changes. */
    void parentChanged() { emitSignal(&QGraphicsObject::parentChanged); }
    /*! This signal is emitted when the opacity changes. */
    void opacityChanged() { emitSignal(&QGraphicsObject::opacityChanged); }
    /*! This signal is emitted when the item becomes visible or hidden. */
    void visibleChanged() { emitSignal(&QGraphicsObject::visibleChanged); }
    /*! This signal is emitted when the item becomes enabled or disabled. */
    void enabledChanged() { emitSignal(&QGraphicsObject::enabledChanged); }
    /*! This signal is emitted when the x coordinate of the position changes. */
    void xChanged() { emitSignal(&QGraphicsObject::xChanged); }
    /*! This signal is emitted when the y coordinate of the position changes. */
    void yChanged() { emitSignal(&QGraphicsObject::yChanged); }
    /*! This signal is emitted when the z value changes. */
    void zChanged() { emitSignal(&QGraphicsObject::zChanged); }
    /*! This signal is emitted when the rotation changes. */
    void rotationChanged() { emitSignal(&QGraphicsObject::rotationChanged); }
    /*! This signal is emitted when the scale changes. */
    void scaleChanged() { emitSignal(&QGraphicsObject::scaleChanged); }
    /*! This signal is emitted when the child items change. */
    void childrenChanged() { emitSignal(&QGraphicsObject::childrenChanged); }
    /*! This signal is emitted by subclasses when their width changes; QGraphicsObject never emits it. */
    void widthChanged() { emitSignal(&QGraphicsObject::widthChanged); }
    /*! This signal is emitted by subclasses when their height changes; QGraphicsObject never emits it. */
    void heightChanged() { emitSignal(&QGraphicsObject::heightChanged); }

protected:
    bool event(QEvent *ev) override;

private:
    void notifyChange(GraphicsItemChange change) override;
};

class QAbstractGraphicsShapeItem : public QGraphicsItem {
public:
    explicit QAbstractGraphicsShapeItem(QGraphicsItem *parent = nullptr);
    ~QAbstractGraphicsShapeItem() override;

    QPen pen() const;
    void setPen(const QPen &pen);
    QBrush brush() const;
    void setBrush(const QBrush &brush);

    bool isObscuredBy(const QGraphicsItem *item) const override;
    QPainterPath opaqueArea() const override;

protected:
    // The bounding rectangle of `rect` grown by half the pen width.
    QRectF adjustedForPen(const QRectF &rect) const;

private:
    QPen m_pen;
    QBrush m_brush;
};

class QGraphicsPathItem : public QAbstractGraphicsShapeItem {
public:
    enum { Type = 2 };
    explicit QGraphicsPathItem(QGraphicsItem *parent = nullptr);
    explicit QGraphicsPathItem(const QPainterPath &path, QGraphicsItem *parent = nullptr);
    ~QGraphicsPathItem() override;

    QPainterPath path() const;
    void setPath(const QPainterPath &path);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    bool contains(const QPointF &point) const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    int type() const override;

private:
    QPainterPath m_path;
};

class QGraphicsRectItem : public QAbstractGraphicsShapeItem {
public:
    enum { Type = 3 };
    explicit QGraphicsRectItem(QGraphicsItem *parent = nullptr);
    explicit QGraphicsRectItem(const QRectF &rect, QGraphicsItem *parent = nullptr);
    explicit QGraphicsRectItem(qreal x, qreal y, qreal width, qreal height, QGraphicsItem *parent = nullptr);
    ~QGraphicsRectItem() override;

    QRectF rect() const;
    void setRect(const QRectF &rect);
    /*! \overload Sets the rectangle to the one at (\a ax, \a ay) that is \a w by \a h. */
    void setRect(qreal ax, qreal ay, qreal w, qreal h) { setRect(QRectF(ax, ay, w, h)); }

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    bool contains(const QPointF &point) const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    int type() const override;

private:
    QRectF m_rect;
};

class QGraphicsEllipseItem : public QAbstractGraphicsShapeItem {
public:
    enum { Type = 4 };
    explicit QGraphicsEllipseItem(QGraphicsItem *parent = nullptr);
    explicit QGraphicsEllipseItem(const QRectF &rect, QGraphicsItem *parent = nullptr);
    explicit QGraphicsEllipseItem(qreal x, qreal y, qreal width, qreal height, QGraphicsItem *parent = nullptr);
    ~QGraphicsEllipseItem() override;

    QRectF rect() const;
    void setRect(const QRectF &rect);
    /*! \overload Sets the rectangle to the one at (\a ax, \a ay) that is \a w by \a h. */
    void setRect(qreal ax, qreal ay, qreal w, qreal h) { setRect(QRectF(ax, ay, w, h)); }
    int startAngle() const;
    void setStartAngle(int angle);
    int spanAngle() const;
    void setSpanAngle(int angle);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    bool contains(const QPointF &point) const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    int type() const override;

private:
    QPainterPath outline() const;
    QRectF m_rect;
    int m_startAngle = 0;
    int m_spanAngle = 360 * 16;
};

class QGraphicsPolygonItem : public QAbstractGraphicsShapeItem {
public:
    enum { Type = 5 };
    explicit QGraphicsPolygonItem(QGraphicsItem *parent = nullptr);
    explicit QGraphicsPolygonItem(const QPolygonF &polygon, QGraphicsItem *parent = nullptr);
    ~QGraphicsPolygonItem() override;

    QPolygonF polygon() const;
    void setPolygon(const QPolygonF &polygon);
    Qt::FillRule fillRule() const;
    void setFillRule(Qt::FillRule rule);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    bool contains(const QPointF &point) const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    int type() const override;

private:
    QPolygonF m_polygon;
    Qt::FillRule m_fillRule = Qt::OddEvenFill;
};

class QGraphicsLineItem : public QGraphicsItem {
public:
    enum { Type = 6 };
    explicit QGraphicsLineItem(QGraphicsItem *parent = nullptr);
    explicit QGraphicsLineItem(const QLineF &line, QGraphicsItem *parent = nullptr);
    explicit QGraphicsLineItem(qreal x1, qreal y1, qreal x2, qreal y2, QGraphicsItem *parent = nullptr);
    ~QGraphicsLineItem() override;

    QPen pen() const;
    void setPen(const QPen &pen);
    QLineF line() const;
    void setLine(const QLineF &line);
    /*! \overload Sets the line to the one from (\a x1, \a y1) to (\a x2, \a y2). */
    void setLine(qreal x1, qreal y1, qreal x2, qreal y2) { setLine(QLineF(x1, y1, x2, y2)); }

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    bool contains(const QPointF &point) const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    int type() const override;

private:
    QPen m_pen;
    QLineF m_line;
};

class QGraphicsPixmapItem : public QGraphicsItem {
public:
    enum ShapeMode { MaskShape, BoundingRectShape, HeuristicMaskShape };
    enum { Type = 7 };
    explicit QGraphicsPixmapItem(QGraphicsItem *parent = nullptr);
    explicit QGraphicsPixmapItem(const QPixmap &pixmap, QGraphicsItem *parent = nullptr);
    ~QGraphicsPixmapItem() override;

    QPixmap pixmap() const;
    void setPixmap(const QPixmap &pixmap);
    Qt::TransformationMode transformationMode() const;
    void setTransformationMode(Qt::TransformationMode mode);
    QPointF offset() const;
    void setOffset(const QPointF &offset);
    /*! \overload Sets the offset to (\a x, \a y). */
    void setOffset(qreal x, qreal y) { setOffset(QPointF(x, y)); }
    ShapeMode shapeMode() const;
    void setShapeMode(ShapeMode mode);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    bool contains(const QPointF &point) const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    int type() const override;

private:
    QPixmap m_pixmap;
    QPointF m_offset;
    Qt::TransformationMode m_mode = Qt::FastTransformation;
    ShapeMode m_shapeMode = MaskShape;
};

class QGraphicsSimpleTextItem : public QAbstractGraphicsShapeItem {
public:
    enum { Type = 9 };
    explicit QGraphicsSimpleTextItem(QGraphicsItem *parent = nullptr);
    explicit QGraphicsSimpleTextItem(const QString &text, QGraphicsItem *parent = nullptr);
    ~QGraphicsSimpleTextItem() override;

    void setText(const QString &text);
    QString text() const;
    void setFont(const QFont &font);
    QFont font() const;

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    bool contains(const QPointF &point) const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    int type() const override;

private:
    void relayout();
    QString m_text;
    QFont m_font;
    QRectF m_bounds;
};

class QGraphicsTextItem : public QGraphicsObject {
    Q_OBJECT
    Q_PROPERTY(bool openExternalLinks READ openExternalLinks WRITE setOpenExternalLinks)
public:
    enum { Type = 8 };
    explicit QGraphicsTextItem(QGraphicsItem *parent = nullptr);
    explicit QGraphicsTextItem(const QString &text, QGraphicsItem *parent = nullptr);
    ~QGraphicsTextItem() override;

    QString toHtml() const;
    void setHtml(const QString &html);
    QString toPlainText() const;
    void setPlainText(const QString &text);

    QFont font() const;
    void setFont(const QFont &font);
    void setDefaultTextColor(const QColor &color);
    QColor defaultTextColor() const;

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    bool contains(const QPointF &point) const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    int type() const override;

    void setTextWidth(qreal width);
    qreal textWidth() const;
    void adjustSize();

    void setDocument(QTextDocument *document);
    QTextDocument *document() const;

    void setTextInteractionFlags(Qt::TextInteractionFlags flags);
    Qt::TextInteractionFlags textInteractionFlags() const;
    void setTabChangesFocus(bool b);
    bool tabChangesFocus() const;
    void setOpenExternalLinks(bool open);
    bool openExternalLinks() const;
    void setTextCursor(const QTextCursor &cursor);
    QTextCursor textCursor() const;

signals:
    /*! This signal is emitted when the user clicks the link \a link. Nt never emits it. */
    void linkActivated(const QString &link) { emitSignal(&QGraphicsTextItem::linkActivated, link); }
    /*! This signal is emitted when the mouse moves over the link \a link. Nt never emits it. */
    void linkHovered(const QString &link) { emitSignal(&QGraphicsTextItem::linkHovered, link); }

private:
    void documentChanged();
    QTextDocument *m_document = nullptr;
    bool m_ownsDocument = true;
    QColor m_defaultTextColor;
    Qt::TextInteractionFlags m_interactionFlags = Qt::NoTextInteraction;
    bool m_tabChangesFocus = false;
    bool m_openExternalLinks = false;
};

class QGraphicsItemGroup : public QGraphicsItem {
public:
    enum { Type = 10 };
    explicit QGraphicsItemGroup(QGraphicsItem *parent = nullptr);
    ~QGraphicsItemGroup() override;

    void addToGroup(QGraphicsItem *item);
    void removeFromGroup(QGraphicsItem *item);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    bool isObscuredBy(const QGraphicsItem *item) const override;
    QPainterPath opaqueArea() const override;
    int type() const override;

private:
    QRectF m_bounds;
};
