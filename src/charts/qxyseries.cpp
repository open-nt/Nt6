// MIT License - see LICENSE
#include <QtCharts/qlineseries.h>
#include <QtCharts/qscatterseries.h>
#include <QtCharts/qsplineseries.h>
#include <QtCharts/qxyseries.h>

#include "qchartengine_p.h"

using QtChartsPrivate::ChartEngine;

/*!
    \class QXYSeries
    \brief The QXYSeries class is the base class of series of points: lines, splines and scatter plots.

    The series holds a list of points in the coordinates of its axes. append(), insert(), replace() and
    remove() change it one point at a time, and emit pointAdded(), pointReplaced() and pointRemoved();
    replace() with a list swaps all the points at once and emits pointsReplaced(), which is the fast way
    to update a live plot:

    \code
    QList<QPointF> window;
    for (int i = 0; i < samples.size(); ++i)
        window.append(QPointF(i, samples[i]));
    series->replace(window);
    \endcode

    The series can draw its points as markers (setPointsVisible()) and a label next to each point
    (setPointLabelsVisible()).

    \differences
    There are no light markers, selected points, best-fit lines or per-point configuration. Point labels
    are drawn in one font and color.
    \enddifferences

    \sa QLineSeries, QSplineSeries, QScatterSeries
*/

/*!
    Constructs a series without points, with the given \a parent.
*/
QXYSeries::QXYSeries(QObject *parent)
    : QAbstractSeries(parent), m_pointLabelsFormat(QStringLiteral("@xPoint, @yPoint")) {}

/*!
    Destroys the series.
*/
QXYSeries::~QXYSeries() = default;

/*!
    Appends the point (\a x, \a y).
*/
void QXYSeries::append(qreal x, qreal y) { append(QPointF(x, y)); }

/*!
    \overload

    Appends \a point.
*/
void QXYSeries::append(const QPointF &point) {
    m_points.append(point);
    markChanged();
    emit pointAdded(count() - 1);
}

/*!
    \overload

    Appends \a points.
*/
void QXYSeries::append(const QList<QPointF> &points) {
    for (const QPointF &point : points)
        append(point);
}

/*!
    Replaces the first point at (\a oldX, \a oldY) with (\a newX, \a newY).
*/
void QXYSeries::replace(qreal oldX, qreal oldY, qreal newX, qreal newY) {
    replace(QPointF(oldX, oldY), QPointF(newX, newY));
}

/*!
    \overload

    Replaces the first point equal to \a oldPoint with \a newPoint.
*/
void QXYSeries::replace(const QPointF &oldPoint, const QPointF &newPoint) {
    const qsizetype index = m_points.indexOf(oldPoint);
    if (index >= 0)
        replace(static_cast<int>(index), newPoint);
}

/*!
    \overload

    Replaces the point at \a index with (\a newX, \a newY).
*/
void QXYSeries::replace(int index, qreal newX, qreal newY) { replace(index, QPointF(newX, newY)); }

/*!
    \overload

    Replaces the point at \a index with \a newPoint.
*/
void QXYSeries::replace(int index, const QPointF &newPoint) {
    if (index < 0 || index >= count())
        return;
    m_points[index] = newPoint;
    markChanged();
    emit pointReplaced(index);
}

/*!
    \overload

    Replaces all points with \a points, and emits pointsReplaced() once. This is much faster than
    replacing them one by one.
*/
void QXYSeries::replace(const QList<QPointF> &points) {
    m_points = points;
    markChanged();
    emit pointsReplaced();
}

/*!
    Removes the first point at (\a x, \a y).
*/
void QXYSeries::remove(qreal x, qreal y) { remove(QPointF(x, y)); }

/*!
    \overload

    Removes the first point equal to \a point.
*/
void QXYSeries::remove(const QPointF &point) {
    const qsizetype index = m_points.indexOf(point);
    if (index >= 0)
        remove(static_cast<int>(index));
}

/*!
    \overload

    Removes the point at \a index.
*/
void QXYSeries::remove(int index) {
    if (index < 0 || index >= count())
        return;
    m_points.removeAt(index);
    markChanged();
    emit pointRemoved(index);
}

/*!
    Removes \a count points, starting at \a index, and emits pointsRemoved().
*/
void QXYSeries::removePoints(int index, int count) {
    if (index < 0 || count <= 0 || index >= this->count())
        return;
    count = std::min(count, this->count() - index);
    m_points.remove(index, count);
    markChanged();
    emit pointsRemoved(index, count);
}

/*!
    Inserts \a point at \a index. An index out of range appends it.
*/
void QXYSeries::insert(int index, const QPointF &point) {
    if (index < 0 || index > count())
        index = count();
    m_points.insert(index, point);
    markChanged();
    emit pointAdded(index);
}

/*!
    Removes all points.
*/
void QXYSeries::clear() { removePoints(0, count()); }

/*!
    Returns the number of points.
*/
int QXYSeries::count() const { return static_cast<int>(m_points.size()); }

/*!
    Returns the points.
*/
QList<QPointF> QXYSeries::points() const { return m_points; }

/*!
    Returns the point at \a index, which must be valid.
*/
const QPointF &QXYSeries::at(int index) const {
    static const QPointF none;
    return index >= 0 && index < count() ? m_points.at(index) : none;
}

/*!
    Appends \a point and returns a reference to the series.
*/
QXYSeries &QXYSeries::operator<<(const QPointF &point) {
    append(point);
    return *this;
}

/*!
    \overload

    Appends \a points and returns a reference to the series.
*/
QXYSeries &QXYSeries::operator<<(const QList<QPointF> &points) {
    append(points);
    return *this;
}

/*!
    Sets the pen of the line to \a pen. A scatter series draws the borders of its markers with it.
*/
void QXYSeries::setPen(const QPen &pen) {
    const QColor before = color();
    m_pen = pen;
    m_penSet = true;
    markChanged();
    emit penChanged(pen);
    if (color() != before)
        emit colorChanged(color());
}

/*!
    Returns the pen of the line. Unless one was set, it is 2 pixels wide in the color the theme gives the
    series.
*/
QPen QXYSeries::pen() const {
    if (m_penSet)
        return m_pen;
    const QColor c = ChartEngine::seriesColor(this);
    if (type() == SeriesTypeScatter)
        return QPen(QBrush(c.darker(130)), 1.0);
    return QPen(QBrush(c), 2.0);
}

/*!
    Sets the brush of the markers to \a brush.
*/
void QXYSeries::setBrush(const QBrush &brush) {
    m_brush = brush;
    m_brushSet = true;
    markChanged();
}

/*!
    Returns the brush of the markers. Unless one was set, it has the color the theme gives the series.
*/
QBrush QXYSeries::brush() const { return m_brushSet ? m_brush : QBrush(ChartEngine::seriesColor(this)); }

/*!
    Sets the color of the line to \a color.
*/
void QXYSeries::setColor(const QColor &color) {
    QPen p = pen();
    p.setColor(color);
    setPen(p);
}

/*!
    Returns the color of the line.
*/
QColor QXYSeries::color() const { return pen().color(); }

/*!
    Draws a marker at each point if \a visible is \c true.

    \sa setMarkerSize()
*/
void QXYSeries::setPointsVisible(bool visible) {
    if (visible == m_pointsVisible)
        return;
    m_pointsVisible = visible;
    markChanged();
}

/*!
    Returns \c true if a marker is drawn at each point; otherwise returns \c false. A scatter series always
    draws its markers.
*/
bool QXYSeries::pointsVisible() const { return m_pointsVisible; }

/*!
    Sets the format of the point labels to \a format. \c @xPoint and \c @yPoint in it are replaced by the
    coordinates of the point. The default is \c{"@xPoint, @yPoint"}.
*/
void QXYSeries::setPointLabelsFormat(const QString &format) {
    if (format == m_pointLabelsFormat)
        return;
    m_pointLabelsFormat = format;
    markChanged();
    emit pointLabelsFormatChanged(format);
}

/*!
    Returns the format of the point labels.
*/
QString QXYSeries::pointLabelsFormat() const { return m_pointLabelsFormat; }

/*!
    Draws a label above each point if \a visible is \c true.
*/
void QXYSeries::setPointLabelsVisible(bool visible) {
    if (visible == m_pointLabelsVisible)
        return;
    m_pointLabelsVisible = visible;
    markChanged();
    emit pointLabelsVisibilityChanged(visible);
}

/*!
    Returns \c true if the point labels are drawn; otherwise returns \c false.
*/
bool QXYSeries::pointLabelsVisible() const { return m_pointLabelsVisible; }

/*!
    Sets the font of the point labels to \a font.
*/
void QXYSeries::setPointLabelsFont(const QFont &font) {
    m_pointLabelsFont = font;
    m_pointLabelsFontSet = true;
    markChanged();
    emit pointLabelsFontChanged(font);
}

/*!
    Returns the font of the point labels.
*/
QFont QXYSeries::pointLabelsFont() const { return m_pointLabelsFont; }

/*!
    Sets the color of the point labels to \a color.
*/
void QXYSeries::setPointLabelsColor(const QColor &color) {
    m_pointLabelsColor = color;
    m_pointLabelsColorSet = true;
    markChanged();
    emit pointLabelsColorChanged(color);
}

/*!
    Returns the color of the point labels.
*/
QColor QXYSeries::pointLabelsColor() const {
    return m_pointLabelsColorSet ? m_pointLabelsColor : ChartEngine::themeOf(chart()).labels;
}

/*!
    Hides the labels of points outside the plot area if \a enabled is \c true, the default.
*/
void QXYSeries::setPointLabelsClipping(bool enabled) {
    if (enabled == m_pointLabelsClipping)
        return;
    m_pointLabelsClipping = enabled;
    markChanged();
    emit pointLabelsClippingChanged(enabled);
}

/*!
    Returns \c true if the labels of points outside the plot area are hidden; otherwise returns \c false.
*/
bool QXYSeries::pointLabelsClipping() const { return m_pointLabelsClipping; }

/*!
    Sets the diameter of the markers to \a size pixels. The default is 15.
*/
void QXYSeries::setMarkerSize(qreal size) {
    if (!(size >= 0.0) || size == m_markerSize)
        return;
    m_markerSize = size;
    markChanged();
    emit markerSizeChanged(size);
}

/*!
    Returns the diameter of the markers in pixels.
*/
qreal QXYSeries::markerSize() const { return m_markerSize; }

/*!
    \class QLineSeries
    \brief The QLineSeries class draws its points joined by straight lines.

    \image charts/qlineseries.png Three line series with value axes

    \snippet charts/dashboard.cpp line

    The points are joined in the order they are in, which need not be the order of their x values.

    \differences
    See QXYSeries.
    \enddifferences

    \sa QSplineSeries, QXYSeries
*/

/*!
    Constructs an empty line series with the given \a parent.
*/
QLineSeries::QLineSeries(QObject *parent) : QXYSeries(parent) {}

/*!
    Destroys the series.
*/
QLineSeries::~QLineSeries() = default;

/*!
    \reimp
    Returns QAbstractSeries::SeriesTypeLine.
*/
QAbstractSeries::SeriesType QLineSeries::type() const { return SeriesTypeLine; }

/*!
    \class QSplineSeries
    \brief The QSplineSeries class draws a smooth curve through its points.

    \image charts/qsplineseries.png A spline series with its points shown

    The curve passes through every point, and is made of cubic Bézier segments whose tangents follow the
    neighboring points.

    \differences
    The control points are computed by Nt, so a curve may differ slightly from Qt's.
    \enddifferences

    \sa QLineSeries
*/

/*!
    Constructs an empty spline series with the given \a parent.
*/
QSplineSeries::QSplineSeries(QObject *parent) : QLineSeries(parent) {}

/*!
    Destroys the series.
*/
QSplineSeries::~QSplineSeries() = default;

/*!
    \reimp
    Returns QAbstractSeries::SeriesTypeSpline.
*/
QAbstractSeries::SeriesType QSplineSeries::type() const { return SeriesTypeSpline; }

/*!
    \class QScatterSeries
    \brief The QScatterSeries class draws a marker at each of its points.

    \image charts/qscatterseries.png Two scatter series with circle and diamond markers

    \snippet charts/dashboard.cpp scatter

    The markers are filled with brush() and outlined with pen(); markerSize() is their diameter.

    \differences
    Markers are drawn as shapes only; there are no image markers.
    \enddifferences

    \sa QXYSeries
*/

/*!
    \enum QScatterSeries::MarkerShape

    The shape of the markers.

    \value MarkerShapeCircle A circle.
    \value MarkerShapeRectangle A square.
    \value MarkerShapeRotatedRectangle A square standing on a corner.
    \value MarkerShapeTriangle A triangle pointing up.
    \value MarkerShapeStar A five-pointed star.
    \value MarkerShapePentagon A pentagon.
*/

/*!
    Constructs an empty scatter series with the given \a parent.
*/
QScatterSeries::QScatterSeries(QObject *parent) : QXYSeries(parent) {}

/*!
    Destroys the series.
*/
QScatterSeries::~QScatterSeries() = default;

/*!
    \reimp
    Returns QAbstractSeries::SeriesTypeScatter.
*/
QAbstractSeries::SeriesType QScatterSeries::type() const { return SeriesTypeScatter; }

/*!
    \reimp
    Sets the pen of the borders of the markers to \a pen.
*/
void QScatterSeries::setPen(const QPen &pen) {
    const QColor before = borderColor();
    QXYSeries::setPen(pen);
    if (borderColor() != before)
        emit borderColorChanged(borderColor());
}

/*!
    \reimp
    Sets the brush of the markers to \a brush.
*/
void QScatterSeries::setBrush(const QBrush &brush) {
    const QColor before = color();
    QXYSeries::setBrush(brush);
    if (color() != before)
        emit colorChanged(color());
}

/*!
    \reimp
    Sets the fill color of the markers to \a color.
*/
void QScatterSeries::setColor(const QColor &color) {
    QBrush b = brush();
    b.setColor(color);
    b.setStyle(Qt::SolidPattern);
    setBrush(b);
}

/*!
    \reimp
    Returns the fill color of the markers.
*/
QColor QScatterSeries::color() const { return brush().color(); }

/*!
    Sets the color of the borders of the markers to \a color.
*/
void QScatterSeries::setBorderColor(const QColor &color) {
    QPen p = pen();
    p.setColor(color);
    setPen(p);
}

/*!
    Returns the color of the borders of the markers.
*/
QColor QScatterSeries::borderColor() const { return pen().color(); }

/*!
    Returns the shape of the markers.
*/
QScatterSeries::MarkerShape QScatterSeries::markerShape() const { return m_shape; }

/*!
    Sets the shape of the markers to \a shape.
*/
void QScatterSeries::setMarkerShape(MarkerShape shape) {
    if (shape == m_shape)
        return;
    m_shape = shape;
    markChanged();
    emit markerShapeChanged(shape);
}

#include "moc_qlineseries.cpp"    // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qscatterseries.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qsplineseries.cpp"  // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qxyseries.cpp"      // NOLINT(bugprone-suspicious-include): generated by ntmoc
