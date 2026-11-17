// MIT License - see LICENSE
//
// The concrete items of Graphics View: shapes, line, pixmap, text and group.
#include <QtWidgets/qgraphicsitem.h>

#include "qgraphicsscene_p.h"

#include <QtGui/qfontmetrics.h>
#include <QtGui/qpainter.h>
#include <QtGui/qtextdocument.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qtextcursor.h>

#include <algorithm>

namespace {

qreal penWidthOf(const QPen &pen) {
    if (pen.style() == Qt::NoPen)
        return 0.0;
    return pen.widthF() > 0.0 ? pen.widthF() : 1.0;
}

// The outline of `path` drawn with `pen`, united with the path, as a shape for hit tests.
QPainterPath shapeWithPen(const QPainterPath &path, const QPen &pen) {
    if (path.isEmpty() || pen.style() == Qt::NoPen)
        return path;
    QPainterPathStroker stroker(pen);
    stroker.setWidth(penWidthOf(pen));
    QPainterPath shape = stroker.createStroke(path);
    shape.addPath(path);
    return shape;
}

// Draws the dashed selection frame that Qt draws around a selected shape item.
void drawSelection(QPainter *painter, const QStyleOptionGraphicsItem *option, const QRectF &rect) {
    if (!option || !(option->state & QStyle::State_Selected))
        return;
    painter->save();
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(Qt::black, 0, Qt::DashLine));
    painter->drawRect(rect);
    painter->restore();
}

} // namespace

// ---- QAbstractGraphicsShapeItem ------------------------------------------------------------------------------

/*!
    \class QAbstractGraphicsShapeItem
    \brief The QAbstractGraphicsShapeItem class is the base of the shape items, with a pen and a brush.

    \differences
    None beyond those of QGraphicsItem.
    \enddifferences
*/

/*!
    Constructs a shape item with the given \a parent, a default pen and no brush.
*/
QAbstractGraphicsShapeItem::QAbstractGraphicsShapeItem(QGraphicsItem *parent) : QGraphicsItem(parent) {}

/*!
    Destroys the item.
*/
QAbstractGraphicsShapeItem::~QAbstractGraphicsShapeItem() = default;

/*!
    Returns the pen the outline is drawn with.
*/
QPen QAbstractGraphicsShapeItem::pen() const { return m_pen; }

/*!
    Sets the pen to \a pen.
*/
void QAbstractGraphicsShapeItem::setPen(const QPen &pen) {
    if (pen == m_pen)
        return;
    prepareGeometryChange();
    m_pen = pen;
    update();
}

/*!
    Returns the brush the inside is filled with.
*/
QBrush QAbstractGraphicsShapeItem::brush() const { return m_brush; }

/*!
    Sets the brush to \a brush.
*/
void QAbstractGraphicsShapeItem::setBrush(const QBrush &brush) {
    if (brush == m_brush)
        return;
    m_brush = brush;
    update();
}

/*!
    \reimp
*/
bool QAbstractGraphicsShapeItem::isObscuredBy(const QGraphicsItem *item) const {
    return QGraphicsItem::isObscuredBy(item);
}

/*!
    \reimp

    Returns the shape if the brush is opaque, and nothing otherwise.
*/
QPainterPath QAbstractGraphicsShapeItem::opaqueArea() const {
    if (m_brush.style() == Qt::SolidPattern && m_brush.color().alpha() == 255)
        return shape();
    return QPainterPath();
}

/*!
    Returns \a rect grown by half the width of the pen, the rectangle the outline paints in.
*/
QRectF QAbstractGraphicsShapeItem::adjustedForPen(const QRectF &rect) const {
    const qreal half = penWidthOf(m_pen) / 2.0;
    return rect.normalized().adjusted(-half, -half, half, half);
}

// ---- QGraphicsPathItem ---------------------------------------------------------------------------------------

/*!
    \class QGraphicsPathItem
    \brief The QGraphicsPathItem class is a QPainterPath in a scene.

    \differences
    None beyond those of QGraphicsItem.
    \enddifferences
*/

/*!
    Constructs an empty path item with the given \a parent.
*/
QGraphicsPathItem::QGraphicsPathItem(QGraphicsItem *parent) : QAbstractGraphicsShapeItem(parent) {}

/*!
    Constructs an item for \a path with the given \a parent.
*/
QGraphicsPathItem::QGraphicsPathItem(const QPainterPath &path, QGraphicsItem *parent)
    : QAbstractGraphicsShapeItem(parent), m_path(path) {}

/*!
    Destroys the item.
*/
QGraphicsPathItem::~QGraphicsPathItem() = default;

/*!
    Returns the path.
*/
QPainterPath QGraphicsPathItem::path() const { return m_path; }

/*!
    Sets the path to \a path.
*/
void QGraphicsPathItem::setPath(const QPainterPath &path) {
    prepareGeometryChange();
    m_path = path;
    update();
}

/*!
    \reimp
*/
QRectF QGraphicsPathItem::boundingRect() const { return adjustedForPen(m_path.controlPointRect()); }

/*!
    \reimp
*/
QPainterPath QGraphicsPathItem::shape() const { return shapeWithPen(m_path, pen()); }

/*!
    \reimp
*/
bool QGraphicsPathItem::contains(const QPointF &point) const { return shape().contains(point); }

/*!
    \reimp
*/
void QGraphicsPathItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    (void)widget;
    painter->setPen(pen());
    painter->setBrush(brush());
    painter->drawPath(m_path);
    drawSelection(painter, option, boundingRect());
}

/*!
    \reimp
*/
int QGraphicsPathItem::type() const { return Type; }

// ---- QGraphicsRectItem ---------------------------------------------------------------------------------------

/*!
    \class QGraphicsRectItem
    \brief The QGraphicsRectItem class is a rectangle in a scene.

    \differences
    None beyond those of QGraphicsItem.
    \enddifferences
*/

/*!
    Constructs an empty rectangle item with the given \a parent.
*/
QGraphicsRectItem::QGraphicsRectItem(QGraphicsItem *parent) : QAbstractGraphicsShapeItem(parent) {}

/*!
    Constructs an item for \a rect with the given \a parent.
*/
QGraphicsRectItem::QGraphicsRectItem(const QRectF &rect, QGraphicsItem *parent)
    : QAbstractGraphicsShapeItem(parent), m_rect(rect) {}

/*!
    Constructs an item for the rectangle at (\a x, \a y) that is \a width by \a height, with the given \a parent.
*/
QGraphicsRectItem::QGraphicsRectItem(qreal x, qreal y, qreal width, qreal height, QGraphicsItem *parent)
    : QAbstractGraphicsShapeItem(parent), m_rect(x, y, width, height) {}

/*!
    Destroys the item.
*/
QGraphicsRectItem::~QGraphicsRectItem() = default;

/*!
    Returns the rectangle.
*/
QRectF QGraphicsRectItem::rect() const { return m_rect; }

/*!
    Sets the rectangle to \a rect.
*/
void QGraphicsRectItem::setRect(const QRectF &rect) {
    if (rect == m_rect)
        return;
    prepareGeometryChange();
    m_rect = rect;
    update();
}

/*!
    \reimp
*/
QRectF QGraphicsRectItem::boundingRect() const { return adjustedForPen(m_rect); }

/*!
    \reimp
*/
QPainterPath QGraphicsRectItem::shape() const {
    QPainterPath path;
    path.addRect(m_rect);
    return shapeWithPen(path, pen());
}

/*!
    \reimp
*/
bool QGraphicsRectItem::contains(const QPointF &point) const { return boundingRect().contains(point); }

/*!
    \reimp
*/
void QGraphicsRectItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    (void)widget;
    painter->setPen(pen());
    painter->setBrush(brush());
    painter->drawRect(m_rect);
    drawSelection(painter, option, boundingRect());
}

/*!
    \reimp
*/
int QGraphicsRectItem::type() const { return Type; }

// ---- QGraphicsEllipseItem ------------------------------------------------------------------------------------

/*!
    \class QGraphicsEllipseItem
    \brief The QGraphicsEllipseItem class is an ellipse, or a pie segment of one, in a scene.

    The angles are in 16ths of a degree, counterclockwise from three o'clock, as for QPainter::drawPie().

    \differences
    None beyond those of QGraphicsItem.
    \enddifferences
*/

/*!
    Constructs an empty ellipse item with the given \a parent.
*/
QGraphicsEllipseItem::QGraphicsEllipseItem(QGraphicsItem *parent) : QAbstractGraphicsShapeItem(parent) {}

/*!
    Constructs an item for the ellipse in \a rect with the given \a parent.
*/
QGraphicsEllipseItem::QGraphicsEllipseItem(const QRectF &rect, QGraphicsItem *parent)
    : QAbstractGraphicsShapeItem(parent), m_rect(rect) {}

/*!
    Constructs an item for the ellipse in the rectangle at (\a x, \a y), \a width by \a height, with the given
    \a parent.
*/
QGraphicsEllipseItem::QGraphicsEllipseItem(qreal x, qreal y, qreal width, qreal height, QGraphicsItem *parent)
    : QAbstractGraphicsShapeItem(parent), m_rect(x, y, width, height) {}

/*!
    Destroys the item.
*/
QGraphicsEllipseItem::~QGraphicsEllipseItem() = default;

/*!
    Returns the rectangle the ellipse fills.
*/
QRectF QGraphicsEllipseItem::rect() const { return m_rect; }

/*!
    Sets the rectangle of the ellipse to \a rect.
*/
void QGraphicsEllipseItem::setRect(const QRectF &rect) {
    if (rect == m_rect)
        return;
    prepareGeometryChange();
    m_rect = rect;
    update();
}

/*!
    Returns the start angle of a segment, in 16ths of a degree.
*/
int QGraphicsEllipseItem::startAngle() const { return m_startAngle; }

/*!
    Sets the start angle to \a angle, in 16ths of a degree.
*/
void QGraphicsEllipseItem::setStartAngle(int angle) {
    if (angle == m_startAngle)
        return;
    prepareGeometryChange();
    m_startAngle = angle;
    update();
}

/*!
    Returns the span of the segment, in 16ths of a degree; 5760 (a full turn) by default.
*/
int QGraphicsEllipseItem::spanAngle() const { return m_spanAngle; }

/*!
    Sets the span of the segment to \a angle, in 16ths of a degree.
*/
void QGraphicsEllipseItem::setSpanAngle(int angle) {
    if (angle == m_spanAngle)
        return;
    prepareGeometryChange();
    m_spanAngle = angle;
    update();
}

QPainterPath QGraphicsEllipseItem::outline() const {
    QPainterPath path;
    if (m_rect.isNull())
        return path;
    if (m_spanAngle == 360 * 16 || m_spanAngle == -360 * 16) {
        path.addEllipse(m_rect);
    } else {
        path.moveTo(m_rect.center());
        path.arcTo(m_rect, m_startAngle / 16.0, m_spanAngle / 16.0);
        path.closeSubpath();
    }
    return path;
}

/*!
    \reimp
*/
QRectF QGraphicsEllipseItem::boundingRect() const { return adjustedForPen(m_rect); }

/*!
    \reimp
*/
QPainterPath QGraphicsEllipseItem::shape() const { return shapeWithPen(outline(), pen()); }

/*!
    \reimp
*/
bool QGraphicsEllipseItem::contains(const QPointF &point) const { return shape().contains(point); }

/*!
    \reimp
*/
void QGraphicsEllipseItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    (void)widget;
    painter->setPen(pen());
    painter->setBrush(brush());
    if (m_spanAngle == 360 * 16 || m_spanAngle == -360 * 16)
        painter->drawEllipse(m_rect);
    else
        painter->drawPie(m_rect, m_startAngle, m_spanAngle);
    drawSelection(painter, option, boundingRect());
}

/*!
    \reimp
*/
int QGraphicsEllipseItem::type() const { return Type; }

// ---- QGraphicsPolygonItem ------------------------------------------------------------------------------------

/*!
    \class QGraphicsPolygonItem
    \brief The QGraphicsPolygonItem class is a polygon in a scene.

    \differences
    None beyond those of QGraphicsItem.
    \enddifferences
*/

/*!
    Constructs an empty polygon item with the given \a parent.
*/
QGraphicsPolygonItem::QGraphicsPolygonItem(QGraphicsItem *parent) : QAbstractGraphicsShapeItem(parent) {}

/*!
    Constructs an item for \a polygon with the given \a parent.
*/
QGraphicsPolygonItem::QGraphicsPolygonItem(const QPolygonF &polygon, QGraphicsItem *parent)
    : QAbstractGraphicsShapeItem(parent), m_polygon(polygon) {}

/*!
    Destroys the item.
*/
QGraphicsPolygonItem::~QGraphicsPolygonItem() = default;

/*!
    Returns the polygon.
*/
QPolygonF QGraphicsPolygonItem::polygon() const { return m_polygon; }

/*!
    Sets the polygon to \a polygon.
*/
void QGraphicsPolygonItem::setPolygon(const QPolygonF &polygon) {
    prepareGeometryChange();
    m_polygon = polygon;
    update();
}

/*!
    Returns the fill rule; Qt::OddEvenFill by default.
*/
Qt::FillRule QGraphicsPolygonItem::fillRule() const { return m_fillRule; }

/*!
    Sets the fill rule to \a rule.
*/
void QGraphicsPolygonItem::setFillRule(Qt::FillRule rule) {
    if (rule == m_fillRule)
        return;
    m_fillRule = rule;
    update();
}

/*!
    \reimp
*/
QRectF QGraphicsPolygonItem::boundingRect() const { return adjustedForPen(m_polygon.boundingRect()); }

/*!
    \reimp
*/
QPainterPath QGraphicsPolygonItem::shape() const {
    QPainterPath path;
    path.addPolygon(m_polygon);
    path.closeSubpath();
    path.setFillRule(m_fillRule);
    return shapeWithPen(path, pen());
}

/*!
    \reimp
*/
bool QGraphicsPolygonItem::contains(const QPointF &point) const { return shape().contains(point); }

/*!
    \reimp
*/
void QGraphicsPolygonItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    (void)widget;
    painter->setPen(pen());
    painter->setBrush(brush());
    painter->drawPolygon(m_polygon, m_fillRule);
    drawSelection(painter, option, boundingRect());
}

/*!
    \reimp
*/
int QGraphicsPolygonItem::type() const { return Type; }

// ---- QGraphicsLineItem ---------------------------------------------------------------------------------------

/*!
    \class QGraphicsLineItem
    \brief The QGraphicsLineItem class is a line in a scene.

    \differences
    None beyond those of QGraphicsItem.
    \enddifferences
*/

/*!
    Constructs an empty line item with the given \a parent.
*/
QGraphicsLineItem::QGraphicsLineItem(QGraphicsItem *parent) : QGraphicsItem(parent) {}

/*!
    Constructs an item for \a line with the given \a parent.
*/
QGraphicsLineItem::QGraphicsLineItem(const QLineF &line, QGraphicsItem *parent) : QGraphicsItem(parent), m_line(line) {}

/*!
    Constructs an item for the line from (\a x1, \a y1) to (\a x2, \a y2) with the given \a parent.
*/
QGraphicsLineItem::QGraphicsLineItem(qreal x1, qreal y1, qreal x2, qreal y2, QGraphicsItem *parent)
    : QGraphicsItem(parent), m_line(x1, y1, x2, y2) {}

/*!
    Destroys the item.
*/
QGraphicsLineItem::~QGraphicsLineItem() = default;

/*!
    Returns the pen.
*/
QPen QGraphicsLineItem::pen() const { return m_pen; }

/*!
    Sets the pen to \a pen.
*/
void QGraphicsLineItem::setPen(const QPen &pen) {
    if (pen == m_pen)
        return;
    prepareGeometryChange();
    m_pen = pen;
    update();
}

/*!
    Returns the line.
*/
QLineF QGraphicsLineItem::line() const { return m_line; }

/*!
    Sets the line to \a line.
*/
void QGraphicsLineItem::setLine(const QLineF &line) {
    if (line == m_line)
        return;
    prepareGeometryChange();
    m_line = line;
    update();
}

/*!
    \reimp
*/
QRectF QGraphicsLineItem::boundingRect() const {
    const qreal half = penWidthOf(m_pen) / 2.0;
    return QRectF(m_line.p1(), m_line.p2()).normalized().adjusted(-half, -half, half, half);
}

/*!
    \reimp
*/
QPainterPath QGraphicsLineItem::shape() const {
    QPainterPath path;
    if (m_line.isNull())
        return path;
    path.moveTo(m_line.p1());
    path.lineTo(m_line.p2());
    QPen pen = m_pen;
    if (pen.style() == Qt::NoPen)
        pen = QPen(Qt::black, 1.0);
    return shapeWithPen(path, pen);
}

/*!
    \reimp
*/
bool QGraphicsLineItem::contains(const QPointF &point) const { return shape().contains(point); }

/*!
    \reimp
*/
void QGraphicsLineItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    (void)widget;
    painter->setPen(m_pen);
    painter->drawLine(m_line);
    drawSelection(painter, option, boundingRect());
}

/*!
    \reimp
*/
int QGraphicsLineItem::type() const { return Type; }

// ---- QGraphicsPixmapItem -------------------------------------------------------------------------------------

/*!
    \class QGraphicsPixmapItem
    \brief The QGraphicsPixmapItem class is a pixmap in a scene.

    \differences
    The shape is always the bounding rectangle: MaskShape and HeuristicMaskShape do not follow transparent pixels.
    \enddifferences
*/

/*!
    \enum QGraphicsPixmapItem::ShapeMode

    How the shape of the item is computed.

    \value MaskShape From the pixmap's mask; the bounding rectangle in Nt.
    \value BoundingRectShape The bounding rectangle.
    \value HeuristicMaskShape From a heuristic mask; the bounding rectangle in Nt.
*/

/*!
    Constructs an empty pixmap item with the given \a parent.
*/
QGraphicsPixmapItem::QGraphicsPixmapItem(QGraphicsItem *parent) : QGraphicsItem(parent) {}

/*!
    Constructs an item for \a pixmap with the given \a parent.
*/
QGraphicsPixmapItem::QGraphicsPixmapItem(const QPixmap &pixmap, QGraphicsItem *parent)
    : QGraphicsItem(parent), m_pixmap(pixmap) {}

/*!
    Destroys the item.
*/
QGraphicsPixmapItem::~QGraphicsPixmapItem() = default;

/*!
    Returns the pixmap.
*/
QPixmap QGraphicsPixmapItem::pixmap() const { return m_pixmap; }

/*!
    Sets the pixmap to \a pixmap.
*/
void QGraphicsPixmapItem::setPixmap(const QPixmap &pixmap) {
    prepareGeometryChange();
    m_pixmap = pixmap;
    update();
}

/*!
    Returns how the pixmap is resampled when the item is transformed; Qt::FastTransformation by default.
*/
Qt::TransformationMode QGraphicsPixmapItem::transformationMode() const { return m_mode; }

/*!
    Sets how the pixmap is resampled to \a mode.
*/
void QGraphicsPixmapItem::setTransformationMode(Qt::TransformationMode mode) {
    if (mode == m_mode)
        return;
    m_mode = mode;
    update();
}

/*!
    Returns where the top-left corner of the pixmap is, in item coordinates.
*/
QPointF QGraphicsPixmapItem::offset() const { return m_offset; }

/*!
    Moves the top-left corner of the pixmap to \a offset, in item coordinates.
*/
void QGraphicsPixmapItem::setOffset(const QPointF &offset) {
    if (offset == m_offset)
        return;
    prepareGeometryChange();
    m_offset = offset;
    update();
}

/*!
    Returns the shape mode.
*/
QGraphicsPixmapItem::ShapeMode QGraphicsPixmapItem::shapeMode() const { return m_shapeMode; }

/*!
    Sets the shape mode to \a mode.
*/
void QGraphicsPixmapItem::setShapeMode(ShapeMode mode) { m_shapeMode = mode; }

/*!
    \reimp
*/
QRectF QGraphicsPixmapItem::boundingRect() const {
    if (m_pixmap.isNull())
        return QRectF();
    return QRectF(m_offset, QSizeF(m_pixmap.size()));
}

/*!
    \reimp
*/
QPainterPath QGraphicsPixmapItem::shape() const {
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

/*!
    \reimp
*/
bool QGraphicsPixmapItem::contains(const QPointF &point) const { return boundingRect().contains(point); }

/*!
    \reimp
*/
void QGraphicsPixmapItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    (void)widget;
    painter->setRenderHint(QPainter::SmoothPixmapTransform, m_mode == Qt::SmoothTransformation);
    painter->drawPixmap(m_offset, m_pixmap);
    drawSelection(painter, option, boundingRect());
}

/*!
    \reimp
*/
int QGraphicsPixmapItem::type() const { return Type; }

// ---- QGraphicsSimpleTextItem ---------------------------------------------------------------------------------

/*!
    \class QGraphicsSimpleTextItem
    \brief The QGraphicsSimpleTextItem class is plain text in a scene.

    The text is filled with the brush (black by default) and outlined with the pen (none by default). A new line in
    the text starts a new line.

    \differences
    None beyond those of QGraphicsItem.
    \enddifferences
*/

/*!
    Constructs an empty text item with the given \a parent.
*/
QGraphicsSimpleTextItem::QGraphicsSimpleTextItem(QGraphicsItem *parent) : QAbstractGraphicsShapeItem(parent) {
    setBrush(QBrush(Qt::black));
    setPen(QPen(Qt::NoPen));
    relayout();
}

/*!
    Constructs an item showing \a text, with the given \a parent.
*/
QGraphicsSimpleTextItem::QGraphicsSimpleTextItem(const QString &text, QGraphicsItem *parent)
    : QGraphicsSimpleTextItem(parent) {
    setText(text);
}

/*!
    Destroys the item.
*/
QGraphicsSimpleTextItem::~QGraphicsSimpleTextItem() = default;

/*!
    Sets the text to \a text.
*/
void QGraphicsSimpleTextItem::setText(const QString &text) {
    if (text == m_text)
        return;
    prepareGeometryChange();
    m_text = text;
    relayout();
    update();
}

/*!
    Returns the text.
*/
QString QGraphicsSimpleTextItem::text() const { return m_text; }

/*!
    Sets the font to \a font.
*/
void QGraphicsSimpleTextItem::setFont(const QFont &font) {
    prepareGeometryChange();
    m_font = font;
    relayout();
    update();
}

/*!
    Returns the font.
*/
QFont QGraphicsSimpleTextItem::font() const { return m_font; }

void QGraphicsSimpleTextItem::relayout() {
    const QFontMetricsF fm(m_font);
    const QStringList lines = m_text.split(QLatin1Char('\n'));
    qreal width = 0.0;
    for (const QString &line : lines)
        width = std::max(width, fm.horizontalAdvance(line));
    const qreal height = m_text.isEmpty() ? 0.0 : fm.height() + fm.lineSpacing() * static_cast<qreal>(lines.size() - 1);
    m_bounds = QRectF(0, 0, width, height);
}

/*!
    \reimp
*/
QRectF QGraphicsSimpleTextItem::boundingRect() const { return adjustedForPen(m_bounds); }

/*!
    \reimp
*/
QPainterPath QGraphicsSimpleTextItem::shape() const {
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

/*!
    \reimp
*/
bool QGraphicsSimpleTextItem::contains(const QPointF &point) const { return boundingRect().contains(point); }

/*!
    \reimp
*/
void QGraphicsSimpleTextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    (void)widget;
    const QFontMetricsF fm(m_font);
    painter->setFont(m_font);
    const QStringList lines = m_text.split(QLatin1Char('\n'));
    qreal baseline = fm.ascent();
    for (const QString &line : lines) {
        if (brush().style() != Qt::NoBrush) {
            painter->setPen(brush().color());
            painter->drawText(QPointF(0, baseline), line);
        }
        if (pen().style() != Qt::NoPen) {
            QPainterPath outline;
            outline.addText(QPointF(0, baseline), m_font, line);
            painter->strokePath(outline, pen());
        }
        baseline += fm.lineSpacing();
    }
    drawSelection(painter, option, boundingRect());
}

/*!
    \reimp
*/
int QGraphicsSimpleTextItem::type() const { return Type; }

// ---- QGraphicsTextItem ---------------------------------------------------------------------------------------

/*!
    \class QGraphicsTextItem
    \brief The QGraphicsTextItem class is rich text in a scene, laid out by a QTextDocument.

    \differences
    The text cannot be edited, selected or followed as links in the scene: the interaction flags, tabChangesFocus()
    and openExternalLinks() are only kept, textCursor() returns a null cursor and setTextCursor() does nothing.
    \enddifferences
*/

/*!
    Constructs an empty text item with the given \a parent.
*/
QGraphicsTextItem::QGraphicsTextItem(QGraphicsItem *parent)
    : QGraphicsObject(parent), m_document(new QTextDocument(this)) {
    QObject::connect(m_document, &QTextDocument::contentsChanged, this, [this] { documentChanged(); });
}

/*!
    Constructs an item showing the plain \a text, with the given \a parent.
*/
QGraphicsTextItem::QGraphicsTextItem(const QString &text, QGraphicsItem *parent) : QGraphicsTextItem(parent) {
    setPlainText(text);
}

/*!
    Destroys the item.
*/
QGraphicsTextItem::~QGraphicsTextItem() = default;

void QGraphicsTextItem::documentChanged() {
    prepareGeometryChange();
    update();
}

/*!
    Returns the text as HTML.
*/
QString QGraphicsTextItem::toHtml() const { return m_document->toHtml(); }

/*!
    Sets the text to the rich text \a html.
*/
void QGraphicsTextItem::setHtml(const QString &html) {
    prepareGeometryChange();
    m_document->setHtml(html);
    update();
}

/*!
    Returns the text without formatting.
*/
QString QGraphicsTextItem::toPlainText() const { return m_document->toPlainText(); }

/*!
    Sets the text to the plain \a text.
*/
void QGraphicsTextItem::setPlainText(const QString &text) {
    prepareGeometryChange();
    m_document->setPlainText(text);
    update();
}

/*!
    Returns the default font of the document.
*/
QFont QGraphicsTextItem::font() const { return m_document->defaultFont(); }

/*!
    Sets the default font of the document to \a font.
*/
void QGraphicsTextItem::setFont(const QFont &font) {
    prepareGeometryChange();
    m_document->setDefaultFont(font);
    update();
}

/*!
    Sets the color of text without a color of its own to \a color.
*/
void QGraphicsTextItem::setDefaultTextColor(const QColor &color) {
    m_defaultTextColor = color;
    update();
}

/*!
    Returns the default text color; an invalid color means black.
*/
QColor QGraphicsTextItem::defaultTextColor() const { return m_defaultTextColor; }

/*!
    \reimp
*/
QRectF QGraphicsTextItem::boundingRect() const { return QRectF(QPointF(0, 0), m_document->size()); }

/*!
    \reimp
*/
QPainterPath QGraphicsTextItem::shape() const {
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

/*!
    \reimp
*/
bool QGraphicsTextItem::contains(const QPointF &point) const { return boundingRect().contains(point); }

/*!
    \reimp
*/
void QGraphicsTextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    (void)widget;
    painter->setPen(m_defaultTextColor.isValid() ? m_defaultTextColor : QColor(Qt::black));
    m_document->drawContents(painter, boundingRect());
    drawSelection(painter, option, boundingRect());
}

/*!
    \reimp
*/
int QGraphicsTextItem::type() const { return Type; }

/*!
    Sets the width the text wraps at to \a width; -1 does not wrap.
*/
void QGraphicsTextItem::setTextWidth(qreal width) {
    prepareGeometryChange();
    m_document->setTextWidth(width);
    update();
}

/*!
    Returns the width the text wraps at, or -1.
*/
qreal QGraphicsTextItem::textWidth() const { return m_document->textWidth(); }

/*!
    Sets the text width to the width the text needs on its widest line.
*/
void QGraphicsTextItem::adjustSize() { setTextWidth(m_document->idealWidth()); }

/*!
    Shows \a document instead of the item's own. The item does not take ownership of \a document.
*/
void QGraphicsTextItem::setDocument(QTextDocument *document) {
    if (!document || document == m_document)
        return;
    prepareGeometryChange();
    QObject::disconnect(m_document, nullptr, this, nullptr);
    if (m_ownsDocument)
        delete m_document;
    m_document = document;
    m_ownsDocument = false;
    QObject::connect(m_document, &QTextDocument::contentsChanged, this, [this] { documentChanged(); });
    update();
}

/*!
    Returns the document.
*/
QTextDocument *QGraphicsTextItem::document() const { return m_document; }

/*!
    Sets how the user may interact with the text to \a flags. Nt keeps the value.
*/
void QGraphicsTextItem::setTextInteractionFlags(Qt::TextInteractionFlags flags) { m_interactionFlags = flags; }

/*!
    Returns the interaction flags; Qt::NoTextInteraction by default.
*/
Qt::TextInteractionFlags QGraphicsTextItem::textInteractionFlags() const { return m_interactionFlags; }

/*!
    Sets whether Tab moves the focus instead of typing a tab to \a b. Nt keeps the value.
*/
void QGraphicsTextItem::setTabChangesFocus(bool b) { m_tabChangesFocus = b; }

/*!
    Returns the value set with setTabChangesFocus().
*/
bool QGraphicsTextItem::tabChangesFocus() const { return m_tabChangesFocus; }

/*!
    Sets whether clicked links open in a browser to \a open. Nt keeps the value.
*/
void QGraphicsTextItem::setOpenExternalLinks(bool open) { m_openExternalLinks = open; }

/*!
    Returns the value set with setOpenExternalLinks().
*/
bool QGraphicsTextItem::openExternalLinks() const { return m_openExternalLinks; }

/*!
    Does nothing with \a cursor: the text cannot be edited in Nt.
*/
void QGraphicsTextItem::setTextCursor(const QTextCursor &cursor) { (void)cursor; }

/*!
    Returns a null cursor.
*/
QTextCursor QGraphicsTextItem::textCursor() const { return QTextCursor(); }

// ---- QGraphicsItemGroup --------------------------------------------------------------------------------------

/*!
    \class QGraphicsItemGroup
    \brief The QGraphicsItemGroup class moves, transforms and shows a set of items as one.

    \differences
    None. As in Qt, the group takes the mouse events of its items; setHandlesChildEvents(false) gives them back.
    \enddifferences
*/

/*!
    Constructs an empty group with the given \a parent.
*/
QGraphicsItemGroup::QGraphicsItemGroup(QGraphicsItem *parent) : QGraphicsItem(parent) {
    setFlag(ItemHasNoContents);
    setHandlesChildEvents(true);
}

/*!
    Destroys the group and its items.
*/
QGraphicsItemGroup::~QGraphicsItemGroup() = default;

/*!
    Adds \a item to the group, keeping where it is in the scene.
*/
void QGraphicsItemGroup::addToGroup(QGraphicsItem *item) {
    if (!item || item == this || item->group() == this)
        return;
    if (item->group())
        item->group()->removeFromGroup(item);
    const QTransform toScene = item->sceneTransform();
    prepareGeometryChange();
    item->setParentItem(this);
    // Keep the item where it was: its new local transformation is its old scene one seen from the group.
    const QTransform local = toScene * sceneTransform().inverted();
    item->setPos(local.map(QPointF(0, 0)));
    item->setTransform(local * QTransform::fromTranslate(-item->pos().x(), -item->pos().y()));
    item->setRotation(0);
    item->setScale(1);
    QGraphicsItemAccess::d(item)->group = this;
    m_bounds = childrenBoundingRect();
}

/*!
    Takes \a item out of the group, keeping where it is in the scene; it gets the group's parent.
*/
void QGraphicsItemGroup::removeFromGroup(QGraphicsItem *item) {
    if (!item || item->group() != this)
        return;
    const QPointF scenePos = item->scenePos();
    prepareGeometryChange();
    QGraphicsItemAccess::d(item)->group = nullptr;
    item->setParentItem(parentItem());
    item->setPos(parentItem() ? parentItem()->mapFromScene(scenePos) : scenePos);
    m_bounds = childrenBoundingRect();
}

/*!
    \reimp
*/
QRectF QGraphicsItemGroup::boundingRect() const { return m_bounds; }

/*!
    \reimp

    Paints nothing.
*/
void QGraphicsItemGroup::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    (void)painter;
    (void)option;
    (void)widget;
}

/*!
    \reimp
*/
bool QGraphicsItemGroup::isObscuredBy(const QGraphicsItem *item) const { return QGraphicsItem::isObscuredBy(item); }

/*!
    \reimp
*/
QPainterPath QGraphicsItemGroup::opaqueArea() const { return QPainterPath(); }

/*!
    \reimp
*/
int QGraphicsItemGroup::type() const { return Type; }
