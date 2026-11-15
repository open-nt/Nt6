// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qrect.h>
#include <QtGui/qbrush.h>
#include <QtGui/qfont.h>
#include <QtGui/qpalette.h>
#include <QtGui/qpen.h>
#include <QtGui/qtransform.h>
#include <QtWidgets/qgraphicsitem.h>

#include <memory>

class QFocusEvent;
class QGraphicsEllipseItem;
class QGraphicsLineItem;
class QGraphicsPathItem;
class QGraphicsPixmapItem;
class QGraphicsPolygonItem;
class QGraphicsProxyWidget;
class QGraphicsRectItem;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneWheelEvent;
class QGraphicsSimpleTextItem;
class QGraphicsTextItem;
class QGraphicsView;
class QKeyEvent;
class QPainter;

class QGraphicsScene : public QObject {
    Q_OBJECT
    Q_PROPERTY(QBrush backgroundBrush READ backgroundBrush WRITE setBackgroundBrush)
    Q_PROPERTY(QBrush foregroundBrush READ foregroundBrush WRITE setForegroundBrush)
    Q_PROPERTY(QRectF sceneRect READ sceneRect WRITE setSceneRect)
    Q_PROPERTY(QPalette palette READ palette WRITE setPalette)
    Q_PROPERTY(QFont font READ font WRITE setFont)
    Q_PROPERTY(bool stickyFocus READ stickyFocus WRITE setStickyFocus)
public:
    enum ItemIndexMethod { BspTreeIndex, NoIndex = -1 };
    enum SceneLayer { ItemLayer = 0x1, BackgroundLayer = 0x2, ForegroundLayer = 0x4, AllLayers = 0xffff };
    Q_DECLARE_FLAGS(SceneLayers, SceneLayer)

    explicit QGraphicsScene(QObject *parent = nullptr);
    explicit QGraphicsScene(const QRectF &sceneRect, QObject *parent = nullptr);
    QGraphicsScene(qreal x, qreal y, qreal width, qreal height, QObject *parent = nullptr);
    ~QGraphicsScene() override;

    QRectF sceneRect() const;
    /*! Returns the width of sceneRect(). */
    qreal width() const { return sceneRect().width(); }
    /*! Returns the height of sceneRect(). */
    qreal height() const { return sceneRect().height(); }
    void setSceneRect(const QRectF &rect);
    /*! \overload Sets the scene rectangle to the one at (\a x, \a y) that is \a w by \a h. */
    void setSceneRect(qreal x, qreal y, qreal w, qreal h) { setSceneRect(QRectF(x, y, w, h)); }

    void render(QPainter *painter, const QRectF &target = QRectF(), const QRectF &source = QRectF(),
                Qt::AspectRatioMode aspectRatioMode = Qt::KeepAspectRatio);

    ItemIndexMethod itemIndexMethod() const;
    void setItemIndexMethod(ItemIndexMethod method);

    QRectF itemsBoundingRect() const;

    QList<QGraphicsItem *> items(Qt::SortOrder order = Qt::DescendingOrder) const;
    QList<QGraphicsItem *> items(const QPointF &pos, Qt::ItemSelectionMode mode = Qt::IntersectsItemShape,
                                 Qt::SortOrder order = Qt::DescendingOrder,
                                 const QTransform &deviceTransform = QTransform()) const;
    QList<QGraphicsItem *> items(const QRectF &rect, Qt::ItemSelectionMode mode = Qt::IntersectsItemShape,
                                 Qt::SortOrder order = Qt::DescendingOrder,
                                 const QTransform &deviceTransform = QTransform()) const;
    QList<QGraphicsItem *> items(const QPainterPath &path, Qt::ItemSelectionMode mode = Qt::IntersectsItemShape,
                                 Qt::SortOrder order = Qt::DescendingOrder,
                                 const QTransform &deviceTransform = QTransform()) const;
    QList<QGraphicsItem *> collidingItems(const QGraphicsItem *item,
                                          Qt::ItemSelectionMode mode = Qt::IntersectsItemShape) const;
    QGraphicsItem *itemAt(const QPointF &pos, const QTransform &deviceTransform) const;
    /*! \overload Returns the topmost visible item at (\a x, \a y) seen through \a deviceTransform. */
    QGraphicsItem *itemAt(qreal x, qreal y, const QTransform &deviceTransform) const {
        return itemAt(QPointF(x, y), deviceTransform);
    }

    QList<QGraphicsItem *> selectedItems() const;
    QPainterPath selectionArea() const;
    void setSelectionArea(const QPainterPath &path, const QTransform &deviceTransform = QTransform());

    QGraphicsItemGroup *createItemGroup(const QList<QGraphicsItem *> &items);
    void destroyItemGroup(QGraphicsItemGroup *group);

    void addItem(QGraphicsItem *item);
    QGraphicsEllipseItem *addEllipse(const QRectF &rect, const QPen &pen = QPen(), const QBrush &brush = QBrush());
    /*! \overload Adds an ellipse in the rectangle at (\a x, \a y), \a w by \a h, with \a pen and \a brush. */
    QGraphicsEllipseItem *addEllipse(qreal x, qreal y, qreal w, qreal h, const QPen &pen = QPen(),
                                     const QBrush &brush = QBrush()) {
        return addEllipse(QRectF(x, y, w, h), pen, brush);
    }
    QGraphicsLineItem *addLine(const QLineF &line, const QPen &pen = QPen());
    /*! \overload Adds the line from (\a x1, \a y1) to (\a x2, \a y2) with \a pen. */
    QGraphicsLineItem *addLine(qreal x1, qreal y1, qreal x2, qreal y2, const QPen &pen = QPen()) {
        return addLine(QLineF(x1, y1, x2, y2), pen);
    }
    QGraphicsPathItem *addPath(const QPainterPath &path, const QPen &pen = QPen(), const QBrush &brush = QBrush());
    QGraphicsPixmapItem *addPixmap(const QPixmap &pixmap);
    QGraphicsPolygonItem *addPolygon(const QPolygonF &polygon, const QPen &pen = QPen(),
                                     const QBrush &brush = QBrush());
    QGraphicsRectItem *addRect(const QRectF &rect, const QPen &pen = QPen(), const QBrush &brush = QBrush());
    /*! \overload Adds the rectangle at (\a x, \a y), \a w by \a h, with \a pen and \a brush. */
    QGraphicsRectItem *addRect(qreal x, qreal y, qreal w, qreal h, const QPen &pen = QPen(),
                               const QBrush &brush = QBrush()) {
        return addRect(QRectF(x, y, w, h), pen, brush);
    }
    QGraphicsTextItem *addText(const QString &text, const QFont &font = QFont());
    QGraphicsProxyWidget *addWidget(QWidget *widget, Qt::WindowFlags wFlags = Qt::WindowFlags());
    QGraphicsSimpleTextItem *addSimpleText(const QString &text, const QFont &font = QFont());
    void removeItem(QGraphicsItem *item);

    QGraphicsItem *focusItem() const;
    void setFocusItem(QGraphicsItem *item, Qt::FocusReason focusReason = Qt::OtherFocusReason);
    bool hasFocus() const;
    void setFocus(Qt::FocusReason focusReason = Qt::OtherFocusReason);
    void clearFocus();
    void setStickyFocus(bool enabled);
    bool stickyFocus() const;

    QGraphicsItem *mouseGrabberItem() const;

    QBrush backgroundBrush() const;
    void setBackgroundBrush(const QBrush &brush);
    QBrush foregroundBrush() const;
    void setForegroundBrush(const QBrush &brush);

    QList<QGraphicsView *> views() const;

    QFont font() const;
    void setFont(const QFont &font);
    QPalette palette() const;
    void setPalette(const QPalette &palette);

    bool isActive() const;
    QGraphicsItem *activePanel() const;

    bool sendEvent(QGraphicsItem *item, QEvent *event);

public slots:
    void update(const QRectF &rect = QRectF());
    void invalidate(const QRectF &rect = QRectF(), QGraphicsScene::SceneLayers layers = AllLayers);
    void advance();
    void clearSelection();
    void clear();

signals:
    /*! This signal is emitted when the scene changes; \a region lists the changed rectangles in scene coordinates. */
    void changed(const QList<QRectF> &region) { emitSignal(&QGraphicsScene::changed, region); }
    /*! This signal is emitted when the scene rectangle changes to \a rect. */
    void sceneRectChanged(const QRectF &rect) { emitSignal(&QGraphicsScene::sceneRectChanged, rect); }
    /*! This signal is emitted when items are selected or deselected. */
    void selectionChanged() { emitSignal(&QGraphicsScene::selectionChanged); }
    /*! This signal is emitted when the focus moves from \a oldFocus to \a newFocus, for \a reason. */
    void focusItemChanged(QGraphicsItem *newFocus, QGraphicsItem *oldFocus, Qt::FocusReason reason) {
        emitSignal(&QGraphicsScene::focusItemChanged, newFocus, oldFocus, reason);
    }

protected:
    bool event(QEvent *event) override;
    virtual void focusInEvent(QFocusEvent *event);
    virtual void focusOutEvent(QFocusEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void keyReleaseEvent(QKeyEvent *event);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
    virtual void wheelEvent(QGraphicsSceneWheelEvent *event);
    virtual void drawBackground(QPainter *painter, const QRectF &rect);
    virtual void drawForeground(QPainter *painter, const QRectF &rect);

private:
    friend class QGraphicsItem;
    friend class QGraphicsView;
    friend struct QGraphicsSceneAccess;
    struct Private;
    std::unique_ptr<Private> d;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QGraphicsScene::SceneLayers)
