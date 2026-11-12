// MIT License - see LICENSE
#include <QtGui/qbrush.h>

/*!
    \class QBrush
    \brief The QBrush class defines the fill pattern of shapes drawn by QPainter.

    A brush has a color and a style. The default brush has the style
    Qt::NoBrush, which fills nothing.

    \code
    QPainter painter(&image);
    painter.setBrush(QBrush(Qt::blue));
    painter.drawRect(10, 10, 40, 40);
    \endcode

    \differences
    The styles are Qt::NoBrush, Qt::SolidPattern and the gradient patterns, which a QBrush made from a
    QGradient has. There are no textures or hatch patterns. Gradients fill paths and shapes drawn with
    floating-point coordinates or a transformation; fillRect() with integer coordinates fills with the
    color of the first stop.
    \enddifferences

    \sa QPen, QPainter, QColor
*/

/*!
    \fn QBrush::QBrush()

    Constructs a brush with the style Qt::NoBrush and the color black.
*/

/*!
    \fn QBrush::QBrush(Qt::BrushStyle style)
    \overload

    Constructs a black brush with the given \a style.
*/

/*!
    \fn QBrush::QBrush(const QColor &color, Qt::BrushStyle style)
    \overload

    Constructs a brush with the given \a color and \a style.
*/

/*!
    \fn QBrush::QBrush(Qt::GlobalColor color, Qt::BrushStyle style)
    \overload

    Constructs a brush with the given \a color and \a style.
*/

/*!
    \fn Qt::BrushStyle QBrush::style() const

    Returns the brush style.

    \sa setStyle()
*/

/*!
    \fn void QBrush::setStyle(Qt::BrushStyle style)

    Sets the brush style to \a style.

    \sa style()
*/

/*!
    \fn const QColor &QBrush::color() const

    Returns the brush color.

    \sa setColor()
*/

/*!
    \fn void QBrush::setColor(const QColor &color)

    Sets the brush color to \a color.

    \sa color()
*/

/*!
    \fn void QBrush::setColor(Qt::GlobalColor color)
    \overload

    Sets the brush color to \a color.
*/

/*!
    Constructs a brush that fills with \a gradient.
*/
QBrush::QBrush(const QGradient &gradient) : m_gradient(std::make_shared<QGradient>(gradient)) {
    switch (gradient.type()) {
    case QGradient::LinearGradient:
        m_style = Qt::LinearGradientPattern;
        break;
    case QGradient::RadialGradient:
        m_style = Qt::RadialGradientPattern;
        break;
    case QGradient::ConicalGradient:
        m_style = Qt::ConicalGradientPattern;
        break;
    default:
        m_style = Qt::NoBrush;
        break;
    }
    const QGradientStops stops = gradient.stops();
    if (!stops.isEmpty())
        m_color = stops.first().second;
}

/*!
    \fn const QGradient *QBrush::gradient() const

    Returns the gradient of the brush, or \c nullptr if it has none.
*/

/*!
    \fn QTransform QBrush::transform() const

    Returns the transformation of the gradient of the brush.
*/

/*!
    \fn void QBrush::setTransform(const QTransform &matrix)

    Sets the transformation of the gradient of the brush to \a matrix.
*/

/*!
    Returns \c true if the brush paints every pixel opaquely; otherwise returns \c false.
*/
bool QBrush::isOpaque() const {
    if (m_style == Qt::SolidPattern)
        return m_color.alpha() == 255;
    if (!m_gradient)
        return false;
    for (const QGradientStop &s : m_gradient->stops())
        if (s.second.alpha() != 255)
            return false;
    return true;
}

/*!
    \class QGradient
    \brief The QGradient class is the base of the gradients that brushes fill with.

    A gradient goes through colors at stops from 0.0 to 1.0. setSpread() decides what lies beyond them.
    QLinearGradient, QRadialGradient and QConicalGradient, from left to right:

    \image painter/gradients.png A linear, a radial and a conical gradient

    \snippet gui/painting.cpp gradients

    A shorter example:

    \code
    QLinearGradient sky(0, 0, 0, 200);
    sky.setColorAt(0.0, QColor(30, 60, 140));
    sky.setColorAt(1.0, QColor(200, 220, 255));
    painter.fillRect(QRectF(0, 0, 300, 200), sky);
    \endcode

    \differences
    The coordinate modes are kept, but gradients are always in the logical coordinates of the painter, as
    with LogicalMode. There are no presets and no interpolation modes.
    \enddifferences

    \sa QLinearGradient, QRadialGradient, QConicalGradient, QBrush
*/

/*!
    \enum QGradient::Type

    \value LinearGradient A QLinearGradient.
    \value RadialGradient A QRadialGradient.
    \value ConicalGradient A QConicalGradient.
    \value NoGradient No gradient.
*/

/*!
    \enum QGradient::Spread

    What lies beyond the first and last stops.

    \value PadSpread The colors of the first and last stops.
    \value ReflectSpread The gradient again, mirrored.
    \value RepeatSpread The gradient again.
*/

/*!
    \enum QGradient::CoordinateMode

    \value LogicalMode The points of the gradient are in the logical coordinates of the painter.
    \value StretchToDeviceMode Relative to the paint device; drawn as LogicalMode in Nt.
    \value ObjectBoundingMode Relative to the shape filled; drawn as LogicalMode in Nt.
    \value ObjectMode Relative to the shape filled; drawn as LogicalMode in Nt.
*/

/*!
    \fn QGradient::QGradient()

    Constructs an empty gradient of no type.
*/

/*!
    \fn QGradient::Type QGradient::type() const

    Returns the type of the gradient.
*/

/*!
    \fn void QGradient::setSpread(QGradient::Spread spread)

    Sets what lies beyond the stops to \a spread.
*/

/*!
    \fn QGradient::Spread QGradient::spread() const

    Returns what lies beyond the stops; PadSpread by default.
*/

/*!
    \fn QGradient::CoordinateMode QGradient::coordinateMode() const

    Returns the coordinate mode; LogicalMode by default.
*/

/*!
    \fn void QGradient::setCoordinateMode(QGradient::CoordinateMode mode)

    Sets the coordinate mode to \a mode.
*/

/*!
    Puts \a color at \a position, from 0.0 to 1.0, replacing a stop at the same position.
*/
void QGradient::setColorAt(qreal position, const QColor &color) {
    if (!(position >= 0.0 && position <= 1.0))
        return;
    qsizetype i = 0;
    while (i < m_stops.size() && m_stops.at(i).first < position)
        ++i;
    if (i < m_stops.size() && m_stops.at(i).first == position)
        m_stops[i].second = color;
    else
        m_stops.insert(i, QGradientStop(position, color));
}

/*!
    Replaces the stops with \a stops; positions outside 0.0 to 1.0 are skipped.
*/
void QGradient::setStops(const QGradientStops &stops) {
    m_stops.clear();
    for (const QGradientStop &s : stops)
        setColorAt(s.first, s.second);
}

/*!
    Returns the stops, in order. A gradient without stops goes from black to white.
*/
QGradientStops QGradient::stops() const {
    if (m_stops.isEmpty())
        return {QGradientStop(0.0, QColor(Qt::black)), QGradientStop(1.0, QColor(Qt::white))};
    return m_stops;
}

/*!
    Returns \c true if \a other has the same type, stops, spread, mode and points; otherwise returns
    \c false.
*/
bool QGradient::operator==(const QGradient &other) const {
    if (m_type != other.m_type || m_spread != other.m_spread || m_mode != other.m_mode || m_stops != other.m_stops)
        return false;
    for (int i = 0; i < 6; ++i)
        if (m_data[i] != other.m_data[i])
            return false;
    return true;
}

/*!
    \fn bool QGradient::operator!=(const QGradient &other) const

    Returns \c true if \a other differs from this gradient.
*/

/*!
    \class QLinearGradient
    \brief The QLinearGradient class changes color along a line.

    The colors are constant along lines perpendicular to the one from start() to finalStop().

    \differences
    See QGradient.
    \enddifferences

    \sa QGradient
*/

/*!
    Constructs a gradient from (0, 0) to (1, 1).
*/
QLinearGradient::QLinearGradient() : QLinearGradient(0, 0, 1, 1) {}

/*!
    Constructs a gradient from \a start to \a finalStop.
*/
QLinearGradient::QLinearGradient(const QPointF &start, const QPointF &finalStop)
    : QLinearGradient(start.x(), start.y(), finalStop.x(), finalStop.y()) {}

/*!
    Constructs a gradient from (\a x1, \a y1) to (\a x2, \a y2).
*/
QLinearGradient::QLinearGradient(qreal x1, qreal y1, qreal x2, qreal y2) {
    m_type = LinearGradient;
    m_data[0] = x1;
    m_data[1] = y1;
    m_data[2] = x2;
    m_data[3] = y2;
}

/*!
    \fn QPointF QLinearGradient::start() const
    Returns where the gradient starts.
*/

/*!
    Sets where the gradient starts to \a start.
*/
void QLinearGradient::setStart(const QPointF &start) {
    m_data[0] = start.x();
    m_data[1] = start.y();
}

/*!
    \fn void QLinearGradient::setStart(qreal x, qreal y)
    \overload
    Sets where the gradient starts to (\a x, \a y).
*/
/*!
    \fn QPointF QLinearGradient::finalStop() const
    Returns where the gradient ends.
*/

/*!
    Sets where the gradient ends to \a stop.
*/
void QLinearGradient::setFinalStop(const QPointF &stop) {
    m_data[2] = stop.x();
    m_data[3] = stop.y();
}

/*!
    \fn void QLinearGradient::setFinalStop(qreal x, qreal y)
    \overload
    Sets where the gradient ends to (\a x, \a y).
*/

/*!
    \class QRadialGradient
    \brief The QRadialGradient class changes color in circles from a focal point.

    Stop 0 is at the focal point, and stop 1 on the circle of radius() around center().

    \differences
    The focal radius is kept but not drawn. See QGradient.
    \enddifferences

    \sa QGradient
*/

/*!
    Constructs a gradient at (0, 0) with a radius of 1.
*/
QRadialGradient::QRadialGradient() : QRadialGradient(0, 0, 1) {}

/*!
    Constructs a gradient at \a center with \a radius, from \a focalPoint.
*/
QRadialGradient::QRadialGradient(const QPointF &center, qreal radius, const QPointF &focalPoint)
    : QRadialGradient(center.x(), center.y(), radius, focalPoint.x(), focalPoint.y()) {}

/*!
    Constructs a gradient at (\a cx, \a cy) with \a radius, from (\a fx, \a fy).
*/
QRadialGradient::QRadialGradient(qreal cx, qreal cy, qreal radius, qreal fx, qreal fy) {
    m_type = RadialGradient;
    m_data[0] = cx;
    m_data[1] = cy;
    m_data[2] = fx;
    m_data[3] = fy;
    m_data[4] = radius;
}

/*!
    Constructs a gradient at \a center with \a radius, from its center.
*/
QRadialGradient::QRadialGradient(const QPointF &center, qreal radius)
    : QRadialGradient(center.x(), center.y(), radius) {}

/*!
    Constructs a gradient at (\a cx, \a cy) with \a radius, from its center.
*/
QRadialGradient::QRadialGradient(qreal cx, qreal cy, qreal radius) : QRadialGradient(cx, cy, radius, cx, cy) {}

/*!
    \fn QPointF QRadialGradient::center() const
    Returns the center of the outer circle.
*/

/*!
    Sets the center of the outer circle to \a center.
*/
void QRadialGradient::setCenter(const QPointF &center) {
    m_data[0] = center.x();
    m_data[1] = center.y();
}

/*!
    \fn void QRadialGradient::setCenter(qreal x, qreal y)
    \overload
    Sets the center to (\a x, \a y).
*/
/*!
    \fn QPointF QRadialGradient::focalPoint() const
    Returns the focal point, where stop 0 is.
*/

/*!
    Sets the focal point to \a focalPoint.
*/
void QRadialGradient::setFocalPoint(const QPointF &focalPoint) {
    m_data[2] = focalPoint.x();
    m_data[3] = focalPoint.y();
}

/*!
    \fn void QRadialGradient::setFocalPoint(qreal x, qreal y)
    \overload
    Sets the focal point to (\a x, \a y).
*/
/*!
    \fn qreal QRadialGradient::radius() const
    Returns the radius of the outer circle.
*/
/*!
    \fn void QRadialGradient::setRadius(qreal radius)
    Sets the radius of the outer circle to \a radius.
*/
/*!
    \fn qreal QRadialGradient::centerRadius() const
    Returns the radius of the outer circle.
*/
/*!
    \fn void QRadialGradient::setCenterRadius(qreal radius)
    Sets the radius of the outer circle to \a radius.
*/
/*!
    \fn qreal QRadialGradient::focalRadius() const
    Returns the radius of the focal circle, which Nt keeps but does not draw.
*/
/*!
    \fn void QRadialGradient::setFocalRadius(qreal radius)
    Sets the radius of the focal circle to \a radius.
*/

/*!
    \class QConicalGradient
    \brief The QConicalGradient class changes color around a center.

    The colors go counterclockwise around center(), starting at angle() degrees from 3 o'clock.

    \differences
    See QGradient.
    \enddifferences

    \sa QGradient
*/

/*!
    Constructs a gradient at (0, 0) starting at 0 degrees.
*/
QConicalGradient::QConicalGradient() : QConicalGradient(0, 0, 0) {}

/*!
    Constructs a gradient around \a center starting at \a angle degrees.
*/
QConicalGradient::QConicalGradient(const QPointF &center, qreal angle)
    : QConicalGradient(center.x(), center.y(), angle) {}

/*!
    Constructs a gradient around (\a cx, \a cy) starting at \a angle degrees.
*/
QConicalGradient::QConicalGradient(qreal cx, qreal cy, qreal angle) {
    m_type = ConicalGradient;
    m_data[0] = cx;
    m_data[1] = cy;
    m_data[2] = angle;
}

/*!
    \fn QPointF QConicalGradient::center() const
    Returns the center.
*/

/*!
    Sets the center to \a center.
*/
void QConicalGradient::setCenter(const QPointF &center) {
    m_data[0] = center.x();
    m_data[1] = center.y();
}

/*!
    \fn void QConicalGradient::setCenter(qreal x, qreal y)
    \overload
    Sets the center to (\a x, \a y).
*/
/*!
    \fn qreal QConicalGradient::angle() const
    Returns the angle where the gradient starts, in degrees counterclockwise from 3 o'clock.
*/
/*!
    \fn void QConicalGradient::setAngle(qreal angle)
    Sets the angle where the gradient starts to \a angle degrees.
*/
