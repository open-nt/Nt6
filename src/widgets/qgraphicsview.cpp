// MIT License - see LICENSE
#include <QtWidgets/qgraphicsview.h>

#include "qgraphicsscene_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpainter.h>
#include <QtGui/qscreen.h>
#include <QtWidgets/qgraphicssceneevent.h>
#include <QtWidgets/qscrollbar.h>

#include <algorithm>
#include <cmath>

void QGraphicsView::Private::sceneDirty(const QRectF &rect) {
    QWidget *vp = q->viewport();
    if (!vp || updateMode == NoViewportUpdate)
        return;
    if (updateMode == FullViewportUpdate) {
        vp->update();
        return;
    }
    const QRect r = q->viewportTransform().mapRect(rect).toAlignedRect().adjusted(-2, -2, 2, 2);
    vp->update(r.intersected(vp->rect()));
}

/*!
    \class QGraphicsView
    \brief The QGraphicsView class shows a QGraphicsScene in a scrollable widget.

    The view draws the part of the scene that its transformation and scroll bars put in the viewport, and turns the
    mouse, wheel and key events of the viewport into scene events. fitInView() scales the view so that a rectangle
    of the scene fills it; centerOn() scrolls to a point.

    \code
    QGraphicsView view(&scene);
    view.setRenderHint(QPainter::Antialiasing);
    view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.fitInView(scene.sceneRect(), Qt::KeepAspectRatio);
    \endcode

    \differences
    The view draws in software, into the viewport's backing store; there is no OpenGL viewport through
    \c setViewport(). The drag modes, cache mode, optimization flags and anchors are kept but have no effect: no
    rubber band or hand dragging, no background cache, and transformations keep the scroll position.
    MinimalViewportUpdate, SmartViewportUpdate and BoundingRectViewportUpdate all redraw the changed rectangles.
    There are no context menu, drag and drop, input method or tool tip events.
    \enddifferences

    \sa QGraphicsScene, QGraphicsItem
*/

/*!
    \enum QGraphicsView::ViewportAnchor

    The point a transformation or resize keeps in place. Nt keeps the value only.

    \value NoAnchor No anchor.
    \value AnchorViewCenter The middle of the view.
    \value AnchorUnderMouse The point under the mouse.
*/

/*!
    \enum QGraphicsView::CacheModeFlag

    What the view caches. Nt caches nothing.

    \value CacheNone Nothing.
    \value CacheBackground The background.
*/

/*!
    \enum QGraphicsView::DragMode

    What dragging the background does. Nt keeps the value only.

    \value NoDrag Nothing.
    \value ScrollHandDrag Scrolls the view.
    \value RubberBandDrag Selects the items in a rubber band.
*/

/*!
    \enum QGraphicsView::ViewportUpdateMode

    How much of the viewport is redrawn when the scene changes.

    \value FullViewportUpdate All of it.
    \value MinimalViewportUpdate The changed rectangles.
    \value SmartViewportUpdate The changed rectangles.
    \value NoViewportUpdate Nothing; the application calls update.
    \value BoundingRectViewportUpdate The changed rectangles.
*/

/*!
    \enum QGraphicsView::OptimizationFlag

    Hints for drawing. Nt keeps them only.

    \value DontSavePainterState The painter state is not saved around items.
    \value DontAdjustForAntialiasing Exposed areas are not grown for antialiasing.
    \value IndirectPainting Items are painted through drawItems().
*/

/*!
    Constructs a view without a scene, with the given \a parent.
*/
QGraphicsView::QGraphicsView(QWidget *parent) : QAbstractScrollArea(parent), d(std::make_unique<Private>()) {
    d->q = this;
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setMouseTracking(true);
    QObject::connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [this] { viewport()->update(); });
    QObject::connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { viewport()->update(); });
}

/*!
    Constructs a view of \a scene with the given \a parent.
*/
QGraphicsView::QGraphicsView(QGraphicsScene *scene, QWidget *parent) : QGraphicsView(parent) { setScene(scene); }

/*!
    Destroys the view. The scene is not deleted.
*/
QGraphicsView::~QGraphicsView() {
    if (d->scene)
        QGraphicsSceneAccess::d(d->scene)->views.removeAll(this);
}

/*!
    \reimp

    Returns the size of the scene rectangle as the view shows it, at most two thirds of the screen.
*/
QSize QGraphicsView::sizeHint() const {
    if (!d->scene)
        return QAbstractScrollArea::sizeHint();
    QSizeF size = d->matrix.mapRect(sceneRect()).size();
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QSize limit = screen->geometry().size() * 2 / 3;
        size = size.boundedTo(QSizeF(limit));
    }
    return size.toSize() + QSize(2 * frameWidth(), 2 * frameWidth());
}

/*!
    Returns the render hints the painter gets; QPainter::TextAntialiasing by default.
*/
QPainter::RenderHints QGraphicsView::renderHints() const { return d->renderHints; }

/*!
    Turns \a hint on if \a enabled, or off.
*/
void QGraphicsView::setRenderHint(QPainter::RenderHint hint, bool enabled) {
    QPainter::RenderHints hints = d->renderHints;
    if (enabled)
        hints |= hint;
    else
        hints &= ~QPainter::RenderHints(hint);
    setRenderHints(hints);
}

/*!
    Sets the render hints to \a hints.
*/
void QGraphicsView::setRenderHints(QPainter::RenderHints hints) {
    if (hints == d->renderHints)
        return;
    d->renderHints = hints;
    viewport()->update();
}

/*!
    Returns where the scene sits in the viewport when it is smaller; centered by default.
*/
Qt::Alignment QGraphicsView::alignment() const { return d->alignment; }

/*!
    Sets where a scene smaller than the viewport sits to \a alignment.
*/
void QGraphicsView::setAlignment(Qt::Alignment alignment) {
    if (alignment == d->alignment)
        return;
    d->alignment = alignment;
    viewport()->update();
}

/*!
    Returns the transformation anchor.
*/
QGraphicsView::ViewportAnchor QGraphicsView::transformationAnchor() const { return d->transformationAnchor; }

/*!
    Sets the transformation anchor to \a anchor. Nt keeps the value.
*/
void QGraphicsView::setTransformationAnchor(ViewportAnchor anchor) { d->transformationAnchor = anchor; }

/*!
    Returns the resize anchor.
*/
QGraphicsView::ViewportAnchor QGraphicsView::resizeAnchor() const { return d->resizeAnchor; }

/*!
    Sets the resize anchor to \a anchor. Nt keeps the value.
*/
void QGraphicsView::setResizeAnchor(ViewportAnchor anchor) { d->resizeAnchor = anchor; }

/*!
    Returns the viewport update mode.
*/
QGraphicsView::ViewportUpdateMode QGraphicsView::viewportUpdateMode() const { return d->updateMode; }

/*!
    Sets the viewport update mode to \a mode.
*/
void QGraphicsView::setViewportUpdateMode(ViewportUpdateMode mode) { d->updateMode = mode; }

/*!
    Returns the optimization flags.
*/
QGraphicsView::OptimizationFlags QGraphicsView::optimizationFlags() const { return d->optimizationFlags; }

/*!
    Sets \a flag if \a enabled, or clears it. Nt keeps the flags.
*/
void QGraphicsView::setOptimizationFlag(OptimizationFlag flag, bool enabled) {
    if (enabled)
        d->optimizationFlags |= flag;
    else
        d->optimizationFlags &= ~OptimizationFlags(flag);
}

/*!
    Sets the optimization flags to \a flags.
*/
void QGraphicsView::setOptimizationFlags(OptimizationFlags flags) { d->optimizationFlags = flags; }

/*!
    Returns the drag mode.
*/
QGraphicsView::DragMode QGraphicsView::dragMode() const { return d->dragMode; }

/*!
    Sets the drag mode to \a mode. Nt keeps the value.
*/
void QGraphicsView::setDragMode(DragMode mode) { d->dragMode = mode; }

/*!
    Returns the cache mode.
*/
QGraphicsView::CacheMode QGraphicsView::cacheMode() const { return d->cacheMode; }

/*!
    Sets the cache mode to \a mode. Nt keeps the value.
*/
void QGraphicsView::setCacheMode(CacheMode mode) { d->cacheMode = mode; }

/*!
    Redraws the viewport; there is no cache to reset.
*/
void QGraphicsView::resetCachedContent() { viewport()->update(); }

/*!
    Returns \c true if the view passes input to the scene; the default.
*/
bool QGraphicsView::isInteractive() const { return d->interactive; }

/*!
    Sets whether the view passes input to the scene to \a allowed.
*/
void QGraphicsView::setInteractive(bool allowed) { d->interactive = allowed; }

/*!
    Returns the scene, or \c nullptr.
*/
QGraphicsScene *QGraphicsView::scene() const { return d->scene; }

/*!
    Shows \a scene, or nothing if it is \c nullptr. The view does not take ownership.
*/
void QGraphicsView::setScene(QGraphicsScene *scene) {
    if (scene == d->scene)
        return;
    if (d->scene)
        QGraphicsSceneAccess::d(d->scene)->views.removeAll(this);
    d->scene = scene;
    if (scene) {
        QGraphicsSceneAccess::d(scene)->views.append(this);
        if (hasFocus())
            scene->setFocus();
    }
    updateScrollBars();
    viewport()->update();
}

/*!
    Returns the rectangle of the scene the scroll bars cover: what setSceneRect() set, or the scene's rectangle.
*/
QRectF QGraphicsView::sceneRect() const {
    if (d->sceneRectSet)
        return d->sceneRect;
    return d->scene ? d->scene->sceneRect() : QRectF();
}

/*!
    Sets the rectangle of the scene the view scrolls over to \a rect; a null rectangle follows the scene again.
*/
void QGraphicsView::setSceneRect(const QRectF &rect) {
    d->sceneRect = rect;
    d->sceneRectSet = !rect.isNull();
    updateScrollBars();
    viewport()->update();
}

/*!
    Returns the transformation of the view, without the scrolling.
*/
QTransform QGraphicsView::transform() const { return d->matrix; }

/*!
    Returns the transformation from scene to viewport coordinates: transform() and the scrolling or alignment.
*/
QTransform QGraphicsView::viewportTransform() const {
    const QRectF mapped = d->matrix.mapRect(sceneRect());
    const QSize vp = viewport()->size();
    qreal dx = 0.0;
    qreal dy = 0.0;
    if (mapped.width() > vp.width()) {
        dx = -horizontalScrollBar()->value();
    } else if (d->alignment & Qt::AlignLeft) {
        dx = -mapped.left();
    } else if (d->alignment & Qt::AlignRight) {
        dx = vp.width() - mapped.right();
    } else {
        dx = (vp.width() - mapped.width()) / 2.0 - mapped.left();
    }
    if (mapped.height() > vp.height()) {
        dy = -verticalScrollBar()->value();
    } else if (d->alignment & Qt::AlignTop) {
        dy = -mapped.top();
    } else if (d->alignment & Qt::AlignBottom) {
        dy = vp.height() - mapped.bottom();
    } else {
        dy = (vp.height() - mapped.height()) / 2.0 - mapped.top();
    }
    return d->matrix * QTransform::fromTranslate(dx, dy);
}

/*!
    Returns \c true if transform() is not the identity.
*/
bool QGraphicsView::isTransformed() const { return !d->matrix.isIdentity(); }

/*!
    Sets the transformation of the view to \a matrix, or combines it with the current one if \a combine.
*/
void QGraphicsView::setTransform(const QTransform &matrix, bool combine) {
    d->matrix = combine ? matrix * d->matrix : matrix;
    updateScrollBars();
    viewport()->update();
}

/*!
    Resets the transformation to the identity.
*/
void QGraphicsView::resetTransform() { setTransform(QTransform()); }

/*!
    Rotates the view by \a angle degrees clockwise.
*/
void QGraphicsView::rotate(qreal angle) {
    QTransform r;
    r.rotate(angle);
    setTransform(r, true);
}

/*!
    Scales the view by \a sx horizontally and \a sy vertically.
*/
void QGraphicsView::scale(qreal sx, qreal sy) { setTransform(QTransform::fromScale(sx, sy), true); }

/*!
    Shears the view by \a sh horizontally and \a sv vertically.
*/
void QGraphicsView::shear(qreal sh, qreal sv) {
    QTransform s;
    s.shear(sh, sv);
    setTransform(s, true);
}

/*!
    Translates the view by \a dx and \a dy.
*/
void QGraphicsView::translate(qreal dx, qreal dy) { setTransform(QTransform::fromTranslate(dx, dy), true); }

/*!
    Scrolls so that \a pos (scene coordinates) is in the middle of the viewport, as far as the scroll bars allow.
*/
void QGraphicsView::centerOn(const QPointF &pos) {
    const QPointF mapped = d->matrix.map(pos);
    const QSize vp = viewport()->size();
    horizontalScrollBar()->setValue(static_cast<int>(std::lround(mapped.x() - vp.width() / 2.0)));
    verticalScrollBar()->setValue(static_cast<int>(std::lround(mapped.y() - vp.height() / 2.0)));
}

/*!
    \overload

    Scrolls so that the middle of \a item is in the middle of the viewport.
*/
void QGraphicsView::centerOn(const QGraphicsItem *item) {
    if (item)
        centerOn(item->sceneBoundingRect().center());
}

/*!
    Scrolls so that \a rect (scene coordinates) is in the viewport with \a xmargin and \a ymargin pixels around it,
    as far as the scroll bars allow.
*/
void QGraphicsView::ensureVisible(const QRectF &rect, int xmargin, int ymargin) {
    const QRectF view = viewportTransform().mapRect(rect);
    const QSize vp = viewport()->size();
    QScrollBar *h = horizontalScrollBar();
    QScrollBar *v = verticalScrollBar();
    if (view.left() - xmargin < 0)
        h->setValue(h->value() + static_cast<int>(std::floor(view.left() - xmargin)));
    else if (view.right() + xmargin > vp.width())
        h->setValue(h->value() + static_cast<int>(std::ceil(view.right() + xmargin - vp.width())));
    if (view.top() - ymargin < 0)
        v->setValue(v->value() + static_cast<int>(std::floor(view.top() - ymargin)));
    else if (view.bottom() + ymargin > vp.height())
        v->setValue(v->value() + static_cast<int>(std::ceil(view.bottom() + ymargin - vp.height())));
}

/*!
    \overload

    Scrolls so that \a item is visible with \a xmargin and \a ymargin pixels around it.
*/
void QGraphicsView::ensureVisible(const QGraphicsItem *item, int xmargin, int ymargin) {
    if (item)
        ensureVisible(item->sceneBoundingRect(), xmargin, ymargin);
}

/*!
    Scales the view so that \a rect (scene coordinates) fills the viewport, less a margin of two pixels, as
    \a aspectRatioMode says, and centers it.
*/
void QGraphicsView::fitInView(const QRectF &rect, Qt::AspectRatioMode aspectRatioMode) {
    if (rect.isEmpty() || viewport()->rect().isEmpty())
        return;
    // Start from a transformation without scaling, so that the result does not depend on the current zoom.
    const QRectF unity = d->matrix.mapRect(QRectF(0, 0, 1, 1));
    if (unity.width() > 0.0 && unity.height() > 0.0)
        d->matrix = QTransform::fromScale(1.0 / unity.width(), 1.0 / unity.height()) * d->matrix;
    const QRectF viewRect = QRectF(viewport()->rect()).adjusted(2, 2, -2, -2);
    const QRectF mapped = d->matrix.mapRect(rect);
    qreal xratio = viewRect.width() / mapped.width();
    qreal yratio = viewRect.height() / mapped.height();
    if (aspectRatioMode == Qt::KeepAspectRatio)
        xratio = yratio = std::min(xratio, yratio);
    else if (aspectRatioMode == Qt::KeepAspectRatioByExpanding)
        xratio = yratio = std::max(xratio, yratio);
    d->matrix = QTransform::fromScale(xratio, yratio) * d->matrix;
    updateScrollBars();
    centerOn(rect.center());
    viewport()->update();
}

/*!
    \overload

    Scales the view so that \a item fills the viewport as \a aspectRatioMode says.
*/
void QGraphicsView::fitInView(const QGraphicsItem *item, Qt::AspectRatioMode aspectRatioMode) {
    if (item)
        fitInView(item->sceneBoundingRect(), aspectRatioMode);
}

/*!
    Draws \a source of the viewport (all of it if null) into \a target of the painter's device (all of it if null)
    with \a painter, fitting it as \a aspectRatioMode says.
*/
void QGraphicsView::render(QPainter *painter, const QRectF &target, const QRect &source,
                           Qt::AspectRatioMode aspectRatioMode) {
    if (!painter || !d->scene)
        return;
    const QRect src = source.isNull() ? viewport()->rect() : source;
    const QPolygonF sceneArea = mapToScene(src);
    d->scene->render(painter, target, sceneArea.boundingRect(), aspectRatioMode);
}

/*!
    Returns the items of the scene, the topmost first.
*/
QList<QGraphicsItem *> QGraphicsView::items() const { return d->scene ? d->scene->items() : QList<QGraphicsItem *>(); }

/*!
    Returns the items at \a pos in viewport coordinates, the topmost first.
*/
QList<QGraphicsItem *> QGraphicsView::items(const QPoint &pos) const {
    if (!d->scene)
        return {};
    return d->scene->items(viewportTransform().inverted().map(QPointF(pos)));
}

/*!
    \overload

    Returns the items that \a rect (viewport coordinates) touches or contains, as \a mode says.
*/
QList<QGraphicsItem *> QGraphicsView::items(const QRect &rect, Qt::ItemSelectionMode mode) const {
    if (!d->scene)
        return {};
    return d->scene->items(mapToScene(rect).boundingRect(), mode);
}

/*!
    Returns the topmost item at \a pos in viewport coordinates, or \c nullptr.
*/
QGraphicsItem *QGraphicsView::itemAt(const QPoint &pos) const {
    const QList<QGraphicsItem *> found = items(pos);
    return found.isEmpty() ? nullptr : found.first();
}

/*!
    Maps \a point from viewport to scene coordinates.
*/
QPointF QGraphicsView::mapToScene(const QPoint &point) const {
    return viewportTransform().inverted().map(QPointF(point));
}

/*!
    \overload

    Maps \a rect from viewport to scene coordinates.
*/
QPolygonF QGraphicsView::mapToScene(const QRect &rect) const {
    return viewportTransform().inverted().map(QPolygonF(QRectF(rect)));
}

/*!
    \overload

    Maps \a polygon from viewport to scene coordinates.
*/
QPolygonF QGraphicsView::mapToScene(const QPolygon &polygon) const {
    QPolygonF f;
    for (const QPoint &p : polygon)
        f.append(QPointF(p));
    return viewportTransform().inverted().map(f);
}

/*!
    \overload

    Maps \a path from viewport to scene coordinates.
*/
QPainterPath QGraphicsView::mapToScene(const QPainterPath &path) const {
    return viewportTransform().inverted().map(path);
}

/*!
    Maps \a point from scene to viewport coordinates.
*/
QPoint QGraphicsView::mapFromScene(const QPointF &point) const { return viewportTransform().map(point).toPoint(); }

/*!
    \overload

    Maps \a rect from scene to viewport coordinates.
*/
QPolygon QGraphicsView::mapFromScene(const QRectF &rect) const { return mapFromScene(QPolygonF(rect)); }

/*!
    \overload

    Maps \a polygon from scene to viewport coordinates.
*/
QPolygon QGraphicsView::mapFromScene(const QPolygonF &polygon) const {
    QPolygon out;
    for (const QPointF &p : viewportTransform().map(polygon))
        out.append(p.toPoint());
    return out;
}

/*!
    \overload

    Maps \a path from scene to viewport coordinates.
*/
QPainterPath QGraphicsView::mapFromScene(const QPainterPath &path) const { return viewportTransform().map(path); }

/*!
    Returns the background brush of the view; none by default, which shows the scene's background.
*/
QBrush QGraphicsView::backgroundBrush() const { return d->backgroundBrush; }

/*!
    Sets the background brush of the view to \a brush.
*/
void QGraphicsView::setBackgroundBrush(const QBrush &brush) {
    d->backgroundBrush = brush;
    viewport()->update();
}

/*!
    Returns the foreground brush of the view; none by default.
*/
QBrush QGraphicsView::foregroundBrush() const { return d->foregroundBrush; }

/*!
    Sets the foreground brush of the view to \a brush.
*/
void QGraphicsView::setForegroundBrush(const QBrush &brush) {
    d->foregroundBrush = brush;
    viewport()->update();
}

/*!
    Redraws \a rects (scene coordinates).
*/
void QGraphicsView::updateScene(const QList<QRectF> &rects) {
    for (const QRectF &r : rects)
        d->sceneDirty(r);
}

/*!
    Redraws \a rect (all of the viewport if null) of \a layers.
*/
void QGraphicsView::invalidateScene(const QRectF &rect, QGraphicsScene::SceneLayers layers) {
    (void)layers;
    if (rect.isNull())
        viewport()->update();
    else
        d->sceneDirty(rect);
}

/*!
    Brings the scroll bars up to date with the scene rectangle \a rect.
*/
void QGraphicsView::updateSceneRect(const QRectF &rect) {
    (void)rect;
    updateScrollBars();
    viewport()->update();
}

void QGraphicsView::updateScrollBars() {
    const QRectF mapped = d->matrix.mapRect(sceneRect());
    const QSize vp = viewport()->size();
    QScrollBar *h = horizontalScrollBar();
    QScrollBar *v = verticalScrollBar();
    if (mapped.width() > vp.width()) {
        h->setRange(static_cast<int>(std::floor(mapped.left())),
                    static_cast<int>(std::ceil(mapped.right() - vp.width())));
        h->setPageStep(vp.width());
        h->setSingleStep(std::max(1, vp.width() / 20));
    } else {
        h->setRange(0, 0);
    }
    if (mapped.height() > vp.height()) {
        v->setRange(static_cast<int>(std::floor(mapped.top())),
                    static_cast<int>(std::ceil(mapped.bottom() - vp.height())));
        v->setPageStep(vp.height());
        v->setSingleStep(std::max(1, vp.height() / 20));
    } else {
        v->setRange(0, 0);
    }
}

/*!
    \reimp
*/
bool QGraphicsView::event(QEvent *event) { return QAbstractScrollArea::event(event); }

/*!
    \reimp

    Also ends the hover of the scene when the mouse leaves the viewport.
*/
bool QGraphicsView::viewportEvent(QEvent *event) {
    if (event->type() == QEvent::Leave && d->scene) {
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(d->scene, &leave);
    }
    return QAbstractScrollArea::viewportEvent(event);
}

/*!
    \reimp
*/
void QGraphicsView::scrollContentsBy(int dx, int dy) {
    (void)dx;
    (void)dy;
    viewport()->update();
}

/*!
    \reimp

    Draws the background, the items and the foreground of the exposed part of the viewport.
*/
void QGraphicsView::paintEvent(QPaintEvent *event) {
    QPainter painter(viewport());
    painter.setRenderHints(d->renderHints);
    const QRect exposedRect = event->rect();
    if (viewport()->autoFillBackground())
        painter.fillRect(exposedRect, viewport()->palette().brush(QPalette::Base));
    const QTransform vt = viewportTransform();
    const QRectF exposed = vt.inverted().mapRect(QRectF(exposedRect)).adjusted(-1, -1, 1, 1);
    painter.setWorldTransform(vt);
    drawBackground(&painter, exposed);
    if (d->scene)
        QGraphicsSceneAccess::d(d->scene)->drawItems(&painter, vt, exposed, viewport());
    painter.setWorldTransform(vt);
    drawForeground(&painter, exposed);
}

/*!
    \reimp
*/
void QGraphicsView::resizeEvent(QResizeEvent *event) {
    updateScrollBars();
    QAbstractScrollArea::resizeEvent(event);
}

/*!
    \reimp
*/
void QGraphicsView::showEvent(QShowEvent *event) {
    updateScrollBars();
    QAbstractScrollArea::showEvent(event);
}

void QGraphicsView::sendMouse(QMouseEvent *event, QEvent::Type type) {
    if (!d->scene || !d->interactive) {
        event->ignore();
        return;
    }
    const QPointF scenePos = viewportTransform().inverted().map(event->position());
    const QPointF screenPos = event->globalPosition();
    if (type == QEvent::GraphicsSceneMousePress || type == QEvent::GraphicsSceneMouseDoubleClick) {
        d->downScenePos.insert(static_cast<int>(event->button()), scenePos);
        d->downScreenPos.insert(static_cast<int>(event->button()), screenPos);
    }
    if (!d->haveLastPos) {
        d->lastScenePos = scenePos;
        d->lastScreenPos = screenPos;
        d->haveLastPos = true;
    }
    QGraphicsSceneMouseEvent me(type);
    me.setWidget(viewport());
    me.setScenePos(scenePos);
    me.setScreenPos(screenPos);
    me.setLastScenePos(d->lastScenePos);
    me.setLastScreenPos(d->lastScreenPos);
    for (auto it = d->downScenePos.cbegin(); it != d->downScenePos.cend(); ++it) {
        const auto b = static_cast<Qt::MouseButton>(it.key());
        me.setButtonDownScenePos(b, it.value());
        me.setButtonDownScreenPos(b, d->downScreenPos.value(it.key()));
    }
    me.setButtons(event->buttons());
    me.setButton(event->button());
    me.setModifiers(event->modifiers());
    me.setAccepted(false);
    QCoreApplication::sendEvent(d->scene, &me);
    event->setAccepted(me.isAccepted());
    d->lastScenePos = scenePos;
    d->lastScreenPos = screenPos;
    if (d->scene) {
        if (QGraphicsItem *item = QGraphicsSceneAccess::d(d->scene)->cursorItemAt(scenePos))
            viewport()->setCursor(item->cursor());
        else
            viewport()->unsetCursor();
    }
}

/*!
    \reimp

    Passes the press to the scene as a QGraphicsSceneMouseEvent.
*/
void QGraphicsView::mousePressEvent(QMouseEvent *event) { sendMouse(event, QEvent::GraphicsSceneMousePress); }

/*!
    \reimp

    Passes the move to the scene as a QGraphicsSceneMouseEvent.
*/
void QGraphicsView::mouseMoveEvent(QMouseEvent *event) { sendMouse(event, QEvent::GraphicsSceneMouseMove); }

/*!
    \reimp

    Passes the release to the scene as a QGraphicsSceneMouseEvent.
*/
void QGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    sendMouse(event, QEvent::GraphicsSceneMouseRelease);
    if (event->buttons() == Qt::NoButton) {
        d->downScenePos.clear();
        d->downScreenPos.clear();
    }
}

/*!
    \reimp

    Passes the double click to the scene as a QGraphicsSceneMouseEvent.
*/
void QGraphicsView::mouseDoubleClickEvent(QMouseEvent *event) {
    sendMouse(event, QEvent::GraphicsSceneMouseDoubleClick);
}

/*!
    \reimp

    Passes the wheel event to the scene; the view scrolls if no item accepts it.
*/
void QGraphicsView::wheelEvent(QWheelEvent *event) {
    if (d->scene && d->interactive) {
        QGraphicsSceneWheelEvent we(QEvent::GraphicsSceneWheel);
        we.setWidget(viewport());
        we.setScenePos(viewportTransform().inverted().map(event->position()));
        we.setScreenPos(event->globalPosition());
        we.setButtons(event->buttons());
        we.setModifiers(event->modifiers());
        const QPoint angle = event->angleDelta();
        const bool horizontal = angle.y() == 0 && angle.x() != 0;
        we.setDelta(horizontal ? angle.x() : angle.y());
        we.setOrientation(horizontal ? Qt::Horizontal : Qt::Vertical);
        we.setPixelDelta(event->pixelDelta());
        we.setAccepted(false);
        QCoreApplication::sendEvent(d->scene, &we);
        if (we.isAccepted()) {
            event->accept();
            return;
        }
    }
    QAbstractScrollArea::wheelEvent(event);
}

/*!
    \reimp

    Passes the key press to the scene; the view scrolls with the arrow keys if the scene does not take it.
*/
void QGraphicsView::keyPressEvent(QKeyEvent *event) {
    if (d->scene && d->interactive) {
        event->setAccepted(false);
        QCoreApplication::sendEvent(d->scene, event);
        if (event->isAccepted())
            return;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

/*!
    \reimp

    Passes the key release to the scene.
*/
void QGraphicsView::keyReleaseEvent(QKeyEvent *event) {
    if (d->scene && d->interactive) {
        event->setAccepted(false);
        QCoreApplication::sendEvent(d->scene, event);
        if (event->isAccepted())
            return;
    }
    QAbstractScrollArea::keyReleaseEvent(event);
}

/*!
    \reimp

    Gives the scene the focus.
*/
void QGraphicsView::focusInEvent(QFocusEvent *event) {
    if (d->scene)
        d->scene->setFocus();
    QAbstractScrollArea::focusInEvent(event);
}

/*!
    \reimp

    Takes the focus from the scene.
*/
void QGraphicsView::focusOutEvent(QFocusEvent *event) {
    if (d->scene)
        d->scene->clearFocus();
    QAbstractScrollArea::focusOutEvent(event);
}

/*!
    Draws the background of \a rect (scene coordinates) with \a painter: the view's background brush, or else the
    scene's background.
*/
void QGraphicsView::drawBackground(QPainter *painter, const QRectF &rect) {
    if (d->backgroundBrush.style() != Qt::NoBrush)
        painter->fillRect(rect, d->backgroundBrush);
    else if (d->scene)
        d->scene->drawBackground(painter, rect);
}

/*!
    Draws the foreground of \a rect (scene coordinates) with \a painter: the view's foreground brush, or else the
    scene's foreground.
*/
void QGraphicsView::drawForeground(QPainter *painter, const QRectF &rect) {
    if (d->foregroundBrush.style() != Qt::NoBrush)
        painter->fillRect(rect, d->foregroundBrush);
    else if (d->scene)
        d->scene->drawForeground(painter, rect);
}

#include "moc_qgraphicsview.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
