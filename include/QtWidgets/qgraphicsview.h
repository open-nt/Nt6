// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qrect.h>
#include <QtGui/qbrush.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpolygon.h>
#include <QtGui/qtransform.h>
#include <QtWidgets/qabstractscrollarea.h>
#include <QtWidgets/qgraphicsscene.h>

#include <memory>

class QGraphicsItem;
class QGraphicsScene;

class QGraphicsView : public QAbstractScrollArea {
    Q_OBJECT
    Q_PROPERTY(QBrush backgroundBrush READ backgroundBrush WRITE setBackgroundBrush)
    Q_PROPERTY(QBrush foregroundBrush READ foregroundBrush WRITE setForegroundBrush)
    Q_PROPERTY(bool interactive READ isInteractive WRITE setInteractive)
    Q_PROPERTY(QRectF sceneRect READ sceneRect WRITE setSceneRect)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)
    Q_PROPERTY(QGraphicsView::DragMode dragMode READ dragMode WRITE setDragMode)
    Q_PROPERTY(
        QGraphicsView::ViewportAnchor transformationAnchor READ transformationAnchor WRITE setTransformationAnchor)
    Q_PROPERTY(QGraphicsView::ViewportAnchor resizeAnchor READ resizeAnchor WRITE setResizeAnchor)
    Q_PROPERTY(QGraphicsView::ViewportUpdateMode viewportUpdateMode READ viewportUpdateMode WRITE setViewportUpdateMode)
public:
    enum ViewportAnchor { NoAnchor, AnchorViewCenter, AnchorUnderMouse };
    Q_ENUM(ViewportAnchor)
    enum CacheModeFlag { CacheNone = 0x0, CacheBackground = 0x1 };
    Q_DECLARE_FLAGS(CacheMode, CacheModeFlag)
    enum DragMode { NoDrag, ScrollHandDrag, RubberBandDrag };
    Q_ENUM(DragMode)
    enum ViewportUpdateMode {
        FullViewportUpdate,
        MinimalViewportUpdate,
        SmartViewportUpdate,
        NoViewportUpdate,
        BoundingRectViewportUpdate
    };
    Q_ENUM(ViewportUpdateMode)
    enum OptimizationFlag { DontSavePainterState = 0x1, DontAdjustForAntialiasing = 0x2, IndirectPainting = 0x4 };
    Q_DECLARE_FLAGS(OptimizationFlags, OptimizationFlag)

    explicit QGraphicsView(QWidget *parent = nullptr);
    explicit QGraphicsView(QGraphicsScene *scene, QWidget *parent = nullptr);
    ~QGraphicsView() override;

    QSize sizeHint() const override;

    QPainter::RenderHints renderHints() const;
    void setRenderHint(QPainter::RenderHint hint, bool enabled = true);
    void setRenderHints(QPainter::RenderHints hints);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);
    ViewportAnchor transformationAnchor() const;
    void setTransformationAnchor(ViewportAnchor anchor);
    ViewportAnchor resizeAnchor() const;
    void setResizeAnchor(ViewportAnchor anchor);
    ViewportUpdateMode viewportUpdateMode() const;
    void setViewportUpdateMode(ViewportUpdateMode mode);
    OptimizationFlags optimizationFlags() const;
    void setOptimizationFlag(OptimizationFlag flag, bool enabled = true);
    void setOptimizationFlags(OptimizationFlags flags);
    DragMode dragMode() const;
    void setDragMode(DragMode mode);
    CacheMode cacheMode() const;
    void setCacheMode(CacheMode mode);
    void resetCachedContent();

    bool isInteractive() const;
    void setInteractive(bool allowed);

    QGraphicsScene *scene() const;
    void setScene(QGraphicsScene *scene);

    QRectF sceneRect() const;
    void setSceneRect(const QRectF &rect);
    /*! \overload Sets the scene rectangle to the one at (\a x, \a y) that is \a w by \a h. */
    void setSceneRect(qreal x, qreal y, qreal w, qreal h) { setSceneRect(QRectF(x, y, w, h)); }

    QTransform transform() const;
    QTransform viewportTransform() const;
    bool isTransformed() const;
    void setTransform(const QTransform &matrix, bool combine = false);
    void resetTransform();
    void rotate(qreal angle);
    void scale(qreal sx, qreal sy);
    void shear(qreal sh, qreal sv);
    void translate(qreal dx, qreal dy);

    void centerOn(const QPointF &pos);
    /*! \overload Scrolls so that (\a x, \a y) is in the middle of the viewport. */
    void centerOn(qreal x, qreal y) { centerOn(QPointF(x, y)); }
    void centerOn(const QGraphicsItem *item);
    void ensureVisible(const QRectF &rect, int xmargin = 50, int ymargin = 50);
    void ensureVisible(const QGraphicsItem *item, int xmargin = 50, int ymargin = 50);
    void fitInView(const QRectF &rect, Qt::AspectRatioMode aspectRatioMode = Qt::IgnoreAspectRatio);
    /*! \overload Fits the rectangle at (\a x, \a y), \a w by \a h, into the viewport with \a aspectRatioMode. */
    void fitInView(qreal x, qreal y, qreal w, qreal h, Qt::AspectRatioMode aspectRatioMode = Qt::IgnoreAspectRatio) {
        fitInView(QRectF(x, y, w, h), aspectRatioMode);
    }
    void fitInView(const QGraphicsItem *item, Qt::AspectRatioMode aspectRatioMode = Qt::IgnoreAspectRatio);

    void render(QPainter *painter, const QRectF &target = QRectF(), const QRect &source = QRect(),
                Qt::AspectRatioMode aspectRatioMode = Qt::KeepAspectRatio);

    QList<QGraphicsItem *> items() const;
    QList<QGraphicsItem *> items(const QPoint &pos) const;
    /*! \overload Returns the items at (\a x, \a y) in viewport coordinates. */
    QList<QGraphicsItem *> items(int x, int y) const { return items(QPoint(x, y)); }
    QList<QGraphicsItem *> items(const QRect &rect, Qt::ItemSelectionMode mode = Qt::IntersectsItemShape) const;
    QGraphicsItem *itemAt(const QPoint &pos) const;
    /*! \overload Returns the topmost item at (\a x, \a y) in viewport coordinates. */
    QGraphicsItem *itemAt(int x, int y) const { return itemAt(QPoint(x, y)); }

    QPointF mapToScene(const QPoint &point) const;
    QPolygonF mapToScene(const QRect &rect) const;
    QPolygonF mapToScene(const QPolygon &polygon) const;
    QPainterPath mapToScene(const QPainterPath &path) const;
    QPoint mapFromScene(const QPointF &point) const;
    QPolygon mapFromScene(const QRectF &rect) const;
    QPolygon mapFromScene(const QPolygonF &polygon) const;
    QPainterPath mapFromScene(const QPainterPath &path) const;
    /*! \overload Maps (\a x, \a y) in viewport coordinates to the scene. */
    QPointF mapToScene(int x, int y) const { return mapToScene(QPoint(x, y)); }
    /*! \overload Maps (\a x, \a y) in scene coordinates to the viewport. */
    QPoint mapFromScene(qreal x, qreal y) const { return mapFromScene(QPointF(x, y)); }

    QBrush backgroundBrush() const;
    void setBackgroundBrush(const QBrush &brush);
    QBrush foregroundBrush() const;
    void setForegroundBrush(const QBrush &brush);

public slots:
    void updateScene(const QList<QRectF> &rects);
    void invalidateScene(const QRectF &rect = QRectF(), QGraphicsScene::SceneLayers layers = QGraphicsScene::AllLayers);
    void updateSceneRect(const QRectF &rect);

protected:
    bool event(QEvent *event) override;
    bool viewportEvent(QEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    virtual void drawBackground(QPainter *painter, const QRectF &rect);
    virtual void drawForeground(QPainter *painter, const QRectF &rect);

private:
    friend class QGraphicsScene;
    friend struct QGraphicsViewAccess;
    void sendMouse(QMouseEvent *event, QEvent::Type type);
    void updateScrollBars();
    struct Private;
    std::unique_ptr<Private> d;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QGraphicsView::CacheMode)
Q_DECLARE_OPERATORS_FOR_FLAGS(QGraphicsView::OptimizationFlags)
