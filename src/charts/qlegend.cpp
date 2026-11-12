// MIT License - see LICENSE
#include <QtCharts/qlegend.h>

#include "qchartengine_p.h"

using QtChartsPrivate::ChartEngine;

/*!
    \class QLegend
    \brief The QLegend class is the legend of a chart.

    The legend has an entry for each series that is drawn: one per line, spline, scatter or area series,
    one per set of a bar series and one per slice of a pie series. Each entry has a marker in the color of
    the series and its name. The chart owns its legend; get it with QChart::legend():

    \code
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setMarkerShape(QLegend::MarkerShapeCircle);
    \endcode

    \differences
    The legend is a QObject, not a graphics item: it cannot be detached from the chart or moved, and has no
    \c QLegendMarker objects. MarkerShapeRotatedRectangle, MarkerShapeTriangle, MarkerShapeStar and
    MarkerShapePentagon are drawn as rectangles. Tool tips are not shown.
    \enddifferences

    \sa QChart
*/

/*!
    \enum QLegend::MarkerShape

    The shape of the markers.

    \value MarkerShapeDefault The shape of the legend: rectangles.
    \value MarkerShapeRectangle A square.
    \value MarkerShapeCircle A circle.
    \value MarkerShapeFromSeries A short line for line and spline series, the marker shape for scatter
           series, and a square for the others.
    \value MarkerShapeRotatedRectangle A square standing on a corner; Nt draws a square.
    \value MarkerShapeTriangle A triangle; Nt draws a square.
    \value MarkerShapeStar A star; Nt draws a square.
    \value MarkerShapePentagon A pentagon; Nt draws a square.
*/

QLegend::QLegend(QChart *chart) : QObject(nullptr), m_chart(chart) {}

/*!
    Destroys the legend. The chart does it.
*/
QLegend::~QLegend() = default;

void QLegend::markChanged() { ChartEngine::changed(m_chart); }

/*!
    Sets the brush of the background to \a brush. The background is drawn when isBackgroundVisible() is
    \c true.
*/
void QLegend::setBrush(const QBrush &brush) {
    const QColor before = color();
    m_brush = brush;
    m_brushSet = true;
    markChanged();
    if (brush.color() != before)
        emit colorChanged(brush.color());
}

/*!
    Returns the brush of the background.
*/
QBrush QLegend::brush() const { return m_brushSet ? m_brush : QBrush(ChartEngine::themeOf(m_chart).legendBackground); }

/*!
    Sets the color of the background to \a color.
*/
void QLegend::setColor(QColor color) {
    QBrush b = brush();
    b.setColor(color);
    b.setStyle(Qt::SolidPattern);
    setBrush(b);
}

/*!
    Returns the color of the background.
*/
QColor QLegend::color() { return brush().color(); }

/*!
    Sets the pen of the border to \a pen.
*/
void QLegend::setPen(const QPen &pen) {
    const QColor before = borderColor();
    m_pen = pen;
    m_penSet = true;
    markChanged();
    if (pen.color() != before)
        emit borderColorChanged(pen.color());
}

/*!
    Returns the pen of the border.
*/
QPen QLegend::pen() const { return m_penSet ? m_pen : QPen(QBrush(ChartEngine::themeOf(m_chart).legendBorder), 1.0); }

/*!
    Sets the color of the border to \a color.
*/
void QLegend::setBorderColor(QColor color) {
    QPen p = pen();
    p.setColor(color);
    setPen(p);
}

/*!
    Returns the color of the border.
*/
QColor QLegend::borderColor() { return pen().color(); }

/*!
    Sets the font of the labels to \a font.
*/
void QLegend::setFont(const QFont &font) {
    m_font = font;
    m_fontSet = true;
    markChanged();
    emit fontChanged(font);
}

/*!
    Returns the font of the labels.
*/
QFont QLegend::font() const { return m_font; }

/*!
    Sets the brush of the labels to \a brush.
*/
void QLegend::setLabelBrush(const QBrush &brush) {
    const QColor before = labelColor();
    m_labelBrush = brush;
    m_labelBrushSet = true;
    markChanged();
    if (brush.color() != before)
        emit labelColorChanged(brush.color());
}

/*!
    Returns the brush of the labels.
*/
QBrush QLegend::labelBrush() const {
    return m_labelBrushSet ? m_labelBrush : QBrush(ChartEngine::themeOf(m_chart).labels);
}

/*!
    Sets the color of the labels to \a color.
*/
void QLegend::setLabelColor(QColor color) {
    QBrush b = labelBrush();
    b.setColor(color);
    setLabelBrush(b);
}

/*!
    Returns the color of the labels.
*/
QColor QLegend::labelColor() const { return labelBrush().color(); }

/*!
    Puts the legend on the side of the chart given by \a alignment: Qt::AlignTop (the default),
    Qt::AlignBottom, Qt::AlignLeft or Qt::AlignRight.
*/
void QLegend::setAlignment(Qt::Alignment alignment) {
    if (alignment == m_alignment)
        return;
    if (!(alignment & (Qt::AlignTop | Qt::AlignBottom | Qt::AlignLeft | Qt::AlignRight)))
        return;
    m_alignment = alignment;
    markChanged();
}

/*!
    Returns the side of the chart the legend is on.
*/
Qt::Alignment QLegend::alignment() const { return m_alignment; }

/*!
    Shows the legend if \a visible is \c true and hides it otherwise.
*/
void QLegend::setVisible(bool visible) {
    if (visible == m_visible)
        return;
    m_visible = visible;
    markChanged();
}

/*!
    Returns \c true if the legend is drawn; otherwise returns \c false.
*/
bool QLegend::isVisible() const { return m_visible; }

/*!
    Shows the legend.
*/
void QLegend::show() { setVisible(true); }

/*!
    Hides the legend.
*/
void QLegend::hide() { setVisible(false); }

/*!
    Draws the background and border of the legend if \a visible is \c true.
*/
void QLegend::setBackgroundVisible(bool visible) {
    if (visible == m_backgroundVisible)
        return;
    m_backgroundVisible = visible;
    markChanged();
    emit backgroundVisibleChanged(visible);
}

/*!
    Returns \c true if the background is drawn; otherwise returns \c false.
*/
bool QLegend::isBackgroundVisible() const { return m_backgroundVisible; }

/*!
    Lists the entries in reverse order if \a reverseMarkers is \c true.
*/
void QLegend::setReverseMarkers(bool reverseMarkers) {
    if (reverseMarkers == m_reverseMarkers)
        return;
    m_reverseMarkers = reverseMarkers;
    markChanged();
    emit reverseMarkersChanged(reverseMarkers);
}

/*!
    Returns \c true if the entries are listed in reverse order; otherwise returns \c false.
*/
bool QLegend::reverseMarkers() { return m_reverseMarkers; }

/*!
    Sets whether the legend shows tool tips with labels that do not fit to \a show. Nt keeps the value and
    shows none.
*/
void QLegend::setShowToolTips(bool show) {
    if (show == m_showToolTips)
        return;
    m_showToolTips = show;
    emit showToolTipsChanged(show);
}

/*!
    Returns \c true if tool tips were asked for; otherwise returns \c false.
*/
bool QLegend::showToolTips() const { return m_showToolTips; }

/*!
    Returns the shape of the markers.
*/
QLegend::MarkerShape QLegend::markerShape() const { return m_markerShape; }

/*!
    Sets the shape of the markers to \a shape.
*/
void QLegend::setMarkerShape(MarkerShape shape) {
    if (shape == m_markerShape)
        return;
    m_markerShape = shape;
    markChanged();
    emit markerShapeChanged(shape);
}

#include "moc_qlegend.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
