// MIT License - see LICENSE
#include <QtCharts/qareaseries.h>
#include <QtCharts/qlineseries.h>

#include "qchartengine_p.h"

using QtChartsPrivate::ChartEngine;

/*!
    \class QAreaSeries
    \brief The QAreaSeries class fills the area between two lines, or below one.

    \image charts/qareaseries.png Two area series stacked on each other

    The area is bounded by the points of an upper QLineSeries and of a lower one. Without a lower series it
    reaches down to zero, or to the edge of the plot area if zero is not shown.

    \snippet charts/dashboard.cpp area

    The line series are not added to the chart themselves. The area series watches them: changing their
    points redraws it. A line series without a parent becomes a child of the area series.

    \differences
    There is no best-fit line, and the point labels are drawn for the upper series only.
    \enddifferences

    \sa QLineSeries
*/

/*!
    Constructs an area series without line series, with the given \a parent.
*/
QAreaSeries::QAreaSeries(QObject *parent)
    : QAbstractSeries(parent), m_pointLabelsFormat(QStringLiteral("@xPoint, @yPoint")) {}

/*!
    Constructs an area series between \a upperSeries and \a lowerSeries, which may be \c nullptr.
*/
QAreaSeries::QAreaSeries(QLineSeries *upperSeries, QLineSeries *lowerSeries)
    : QAreaSeries(static_cast<QObject *>(nullptr)) {
    setUpperSeries(upperSeries);
    setLowerSeries(lowerSeries);
}

/*!
    Destroys the series.
*/
QAreaSeries::~QAreaSeries() = default;

/*!
    \reimp
    Returns QAbstractSeries::SeriesTypeArea.
*/
QAbstractSeries::SeriesType QAreaSeries::type() const { return SeriesTypeArea; }

void QAreaSeries::watch(QLineSeries *series) {
    if (!series)
        return;
    if (!series->parent())
        series->setParent(this);
    const auto redraw = [this] { markChanged(); };
    connect(series, &QXYSeries::pointAdded, this, redraw);
    connect(series, &QXYSeries::pointRemoved, this, redraw);
    connect(series, &QXYSeries::pointReplaced, this, redraw);
    connect(series, &QXYSeries::pointsReplaced, this, redraw);
    connect(series, &QXYSeries::pointsRemoved, this, redraw);
    connect(series, &QObject::destroyed, this, [this, series] {
        if (m_upper == series)
            m_upper = nullptr;
        if (m_lower == series)
            m_lower = nullptr;
        markChanged();
    });
}

/*!
    Sets the series that bounds the area from above to \a series.
*/
void QAreaSeries::setUpperSeries(QLineSeries *series) {
    if (series == m_upper)
        return;
    if (m_upper)
        disconnect(m_upper, nullptr, this, nullptr);
    m_upper = series;
    watch(series);
    markChanged();
}

/*!
    Returns the series that bounds the area from above.
*/
QLineSeries *QAreaSeries::upperSeries() const { return m_upper; }

/*!
    Sets the series that bounds the area from below to \a series, or to \c nullptr for zero.
*/
void QAreaSeries::setLowerSeries(QLineSeries *series) {
    if (series == m_lower)
        return;
    if (m_lower)
        disconnect(m_lower, nullptr, this, nullptr);
    m_lower = series;
    watch(series);
    markChanged();
}

/*!
    Returns the series that bounds the area from below, or \c nullptr.
*/
QLineSeries *QAreaSeries::lowerSeries() const { return m_lower; }

/*!
    Sets the pen of the border of the area to \a pen.
*/
void QAreaSeries::setPen(const QPen &pen) {
    const QColor before = borderColor();
    m_pen = pen;
    m_penSet = true;
    markChanged();
    if (borderColor() != before)
        emit borderColorChanged(borderColor());
}

/*!
    Returns the pen of the border of the area.
*/
QPen QAreaSeries::pen() const {
    return m_penSet ? m_pen : QPen(QBrush(ChartEngine::seriesColor(this).darker(120)), 1.5);
}

/*!
    Sets the brush that fills the area to \a brush.
*/
void QAreaSeries::setBrush(const QBrush &brush) {
    const QColor before = color();
    m_brush = brush;
    m_brushSet = true;
    markChanged();
    if (color() != before)
        emit colorChanged(color());
}

/*!
    Returns the brush that fills the area.
*/
QBrush QAreaSeries::brush() const { return m_brushSet ? m_brush : QBrush(ChartEngine::seriesColor(this)); }

/*!
    Sets the fill color to \a color.
*/
void QAreaSeries::setColor(const QColor &color) {
    QBrush b = brush();
    b.setColor(color);
    b.setStyle(Qt::SolidPattern);
    setBrush(b);
}

/*!
    Returns the fill color.
*/
QColor QAreaSeries::color() const { return brush().color(); }

/*!
    Sets the color of the border to \a color.
*/
void QAreaSeries::setBorderColor(const QColor &color) {
    QPen p = pen();
    p.setColor(color);
    setPen(p);
}

/*!
    Returns the color of the border.
*/
QColor QAreaSeries::borderColor() const { return pen().color(); }

/*!
    Draws a marker at each point of the bounding series if \a visible is \c true.
*/
void QAreaSeries::setPointsVisible(bool visible) {
    if (visible == m_pointsVisible)
        return;
    m_pointsVisible = visible;
    markChanged();
}

/*!
    Returns \c true if markers are drawn at the points; otherwise returns \c false.
*/
bool QAreaSeries::pointsVisible() const { return m_pointsVisible; }

/*!
    Sets the format of the point labels to \a format, as QXYSeries::setPointLabelsFormat() does.
*/
void QAreaSeries::setPointLabelsFormat(const QString &format) {
    if (format == m_pointLabelsFormat)
        return;
    m_pointLabelsFormat = format;
    markChanged();
    emit pointLabelsFormatChanged(format);
}

/*!
    Returns the format of the point labels.
*/
QString QAreaSeries::pointLabelsFormat() const { return m_pointLabelsFormat; }

/*!
    Draws a label above each point of the upper series if \a visible is \c true.
*/
void QAreaSeries::setPointLabelsVisible(bool visible) {
    if (visible == m_pointLabelsVisible)
        return;
    m_pointLabelsVisible = visible;
    markChanged();
    emit pointLabelsVisibilityChanged(visible);
}

/*!
    Returns \c true if the point labels are drawn; otherwise returns \c false.
*/
bool QAreaSeries::pointLabelsVisible() const { return m_pointLabelsVisible; }

#include "moc_qareaseries.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
