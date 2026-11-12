// MIT License - see LICENSE
#include <QtCharts/qchart.h>

#include "qchartengine_p.h"

using QtChartsPrivate::ChartEngine;

/*!
    \module NtCharts
    \title Nt Charts C++ Classes
    \brief Line, area, scatter, bar and pie charts for dashboards and reports.
    \cmaketarget Nt::Charts

    Nt Charts draws charts in applications made with Nt Widgets: a QChart holds series and axes, and a
    QChartView shows it. See \l{Using Nt Charts}.

    Link with \c Nt::Charts and include \c{<QtCharts>} or the headers of the classes, such as
    \c{<QChartView>}.
*/

/*!
    \class QChart
    \brief The QChart class holds the series, axes, legend and title of a chart, and draws them.

    \image charts/qlineseries.png A chart with a title, a legend, axes and three series

    Add series to a chart, give them axes, and show the chart in a QChartView:

    \snippet charts/dashboard.cpp line

    The chart owns its series and axes. A series without axes is drawn against a scale that fits its data;
    createDefaultAxes() makes a pair of axes that fit all series, and addAxis() with
    QAbstractSeries::attachAxis() gives finer control: several axes on one side, or a second scale on the
    right.

    The look comes from the theme(): background, text, grid and series colors. The themes are pictured in
    \l{Using Nt Charts}. Colors set on the chart, its
    series or its axes win over the theme until setTheme() is called again.

    Zooming changes the ranges of the axes: zoomIn() to a rectangle of the plot area, zoom() by a factor,
    scroll() by pixels, and zoomReset() to go back. QChartView zooms with a rubber band.

    \differences
    The chart is a QObject, not a \c QGraphicsWidget, and is drawn by QChartView or by paint() on any
    painter. There are no animations: animationOptions() is kept but has no effect. There is no drop
    shadow, no polar chart (\c QPolarChart), no localized numbers and no \c QChart::locale(). setGeometry(),
    resize() and paint() are the parts of the \c QGraphicsWidget API that Nt keeps.
    \enddifferences

    \sa QChartView, QAbstractSeries, QAbstractAxis, QLegend, {Using Nt Charts}
*/

/*!
    \enum QChart::ChartTheme

    The themes of a chart. Each sets the colors of the background, text, axes and grid, and a palette for
    the series.

    \value ChartThemeLight Light background, the default.
    \value ChartThemeBlueCerulean Deep blue background.
    \value ChartThemeDark Dark background.
    \value ChartThemeBrownSand Sand background with earthy series.
    \value ChartThemeBlueNcs Light background with shades of blue.
    \value ChartThemeHighContrast White background, black text and strongly contrasting series.
    \value ChartThemeBlueIcy Pale blue background.
    \value ChartThemeQt Light background with green series.
*/

/*!
    \enum QChart::AnimationOption

    Which changes are animated. Nt draws every change at once.

    \value NoAnimation No animations.
    \value GridAxisAnimations Changes of the axes.
    \value SeriesAnimations Changes of the series.
    \value AllAnimations Both.
*/

/*!
    \typedef QChart::AnimationOptions

    A QFlags of AnimationOption values.
*/

/*!
    \enum QChart::ChartType

    The kind of chart.

    \value ChartTypeUndefined Not known.
    \value ChartTypeCartesian A chart with horizontal and vertical axes, as QChart is.
    \value ChartTypePolar A polar chart; Nt has none.
*/

/*!
    Constructs an empty chart with the given \a parent, 400 by 300 pixels.
*/
QChart::QChart(QObject *parent) : QObject(parent), m_legend(ChartEngine::createLegend(this)) {}

/*!
    Destroys the chart, its series and its axes.
*/
QChart::~QChart() { ChartEngine::destroyChart(this); }

/*!
    Adds \a series to the chart, which takes ownership of it. A series can be in one chart only.

    \sa removeSeries(), createDefaultAxes()
*/
void QChart::addSeries(QAbstractSeries *series) { ChartEngine::addSeries(this, series); }

/*!
    Removes \a series from the chart without deleting it; the caller owns it again. Its axes are detached.
*/
void QChart::removeSeries(QAbstractSeries *series) { ChartEngine::removeSeries(this, series); }

/*!
    Removes and deletes all series.
*/
void QChart::removeAllSeries() {
    const QList<QAbstractSeries *> all = m_series;
    for (QAbstractSeries *series : all) {
        removeSeries(series);
        delete series;
    }
}

/*!
    Returns the series of the chart.
*/
QList<QAbstractSeries *> QChart::series() const { return m_series; }

/*!
    Adds \a axis to the chart, on the side of the plot area given by \a alignment: Qt::AlignLeft,
    Qt::AlignRight, Qt::AlignTop or Qt::AlignBottom. The chart takes ownership of it. Axes on the same side
    are drawn side by side.

    \sa QAbstractSeries::attachAxis()
*/
void QChart::addAxis(QAbstractAxis *axis, Qt::Alignment alignment) { ChartEngine::addAxis(this, axis, alignment); }

/*!
    Removes \a axis from the chart without deleting it, and detaches it from its series.
*/
void QChart::removeAxis(QAbstractAxis *axis) { ChartEngine::removeAxis(this, axis); }

/*!
    Returns the axes of the chart with the given \a orientation. If \a series is not \c nullptr, returns
    only the axes attached to it.
*/
QList<QAbstractAxis *> QChart::axes(Qt::Orientations orientation, QAbstractSeries *series) const {
    QList<QAbstractAxis *> result;
    const QList<QAbstractAxis *> pool = series ? series->attachedAxes() : m_axes;
    for (QAbstractAxis *axis : pool)
        if (orientation & axis->orientation())
            result.append(axis);
    return result;
}

/*!
    Removes and deletes the axes of the chart, and makes a horizontal and a vertical axis that fit the data
    of all series, attached to all of them: QBarCategoryAxis for the categories of bar series and
    QValueAxis otherwise. Pie series get no axes. Add the series first.
*/
void QChart::createDefaultAxes() { ChartEngine::createDefaultAxes(this); }

/*!
    Sets the theme of the chart to \a theme. The colors and fonts that were set on the chart, its legend,
    series and axes are forgotten, so that the whole chart follows the theme.
*/
void QChart::setTheme(QChart::ChartTheme theme) {
    m_theme = theme;
    ChartEngine::forgetStyle(this);
    update();
}

/*!
    Returns the theme of the chart. The default is ChartThemeLight.
*/
QChart::ChartTheme QChart::theme() const { return m_theme; }

/*!
    Sets the title, drawn above the plot area, to \a title.
*/
void QChart::setTitle(const QString &title) {
    if (title == m_title)
        return;
    m_title = title;
    update();
}

/*!
    Returns the title.
*/
QString QChart::title() const { return m_title; }

/*!
    Sets the font of the title to \a font.
*/
void QChart::setTitleFont(const QFont &font) {
    m_titleFont = font;
    m_titleFontSet = true;
    update();
}

/*!
    Returns the font of the title: unless one was set, the default font, bold and a quarter larger.
*/
QFont QChart::titleFont() const {
    if (m_titleFontSet)
        return m_titleFont;
    QFont font;
    font.setBold(true);
    if (font.pointSizeF() > 0.0)
        font.setPointSizeF(font.pointSizeF() * 1.25);
    return font;
}

/*!
    Sets the brush of the title to \a brush.
*/
void QChart::setTitleBrush(const QBrush &brush) {
    m_titleBrush = brush;
    m_titleBrushSet = true;
    update();
}

/*!
    Returns the brush of the title.
*/
QBrush QChart::titleBrush() const { return m_titleBrushSet ? m_titleBrush : QBrush(ChartEngine::themeOf(this).title); }

/*!
    Sets the brush of the background of the chart to \a brush.
*/
void QChart::setBackgroundBrush(const QBrush &brush) {
    m_backgroundBrush = brush;
    m_backgroundBrushSet = true;
    update();
}

/*!
    Returns the brush of the background of the chart.
*/
QBrush QChart::backgroundBrush() const {
    return m_backgroundBrushSet ? m_backgroundBrush : QBrush(ChartEngine::themeOf(this).background);
}

/*!
    Sets the pen of the border of the chart to \a pen.
*/
void QChart::setBackgroundPen(const QPen &pen) {
    m_backgroundPen = pen;
    m_backgroundPenSet = true;
    update();
}

/*!
    Returns the pen of the border of the chart. Unless one was set, there is no border.
*/
QPen QChart::backgroundPen() const { return m_backgroundPenSet ? m_backgroundPen : QPen(Qt::NoPen); }

/*!
    Draws the background of the chart if \a visible is \c true. Without it, what is under the chart shows
    through.
*/
void QChart::setBackgroundVisible(bool visible) {
    if (visible == m_backgroundVisible)
        return;
    m_backgroundVisible = visible;
    update();
}

/*!
    Returns \c true if the background is drawn; otherwise returns \c false.
*/
bool QChart::isBackgroundVisible() const { return m_backgroundVisible; }

/*!
    Sets whether the background casts a shadow to \a enabled. Nt keeps the value and draws no shadow.
*/
void QChart::setDropShadowEnabled(bool enabled) { m_dropShadow = enabled; }

/*!
    Returns \c true if a drop shadow was asked for; otherwise returns \c false.
*/
bool QChart::isDropShadowEnabled() const { return m_dropShadow; }

/*!
    Rounds the corners of the background with circles of the given \a diameter.
*/
void QChart::setBackgroundRoundness(qreal diameter) {
    if (!(diameter >= 0.0) || diameter == m_roundness)
        return;
    m_roundness = diameter;
    update();
}

/*!
    Returns the diameter of the rounded corners of the background. The default is 0.
*/
qreal QChart::backgroundRoundness() const { return m_roundness; }

/*!
    Sets the brush of the background of the plot area to \a brush.

    \sa setPlotAreaBackgroundVisible()
*/
void QChart::setPlotAreaBackgroundBrush(const QBrush &brush) {
    m_plotBrush = brush;
    m_plotBrushSet = true;
    update();
}

/*!
    Returns the brush of the background of the plot area.
*/
QBrush QChart::plotAreaBackgroundBrush() const {
    return m_plotBrushSet ? m_plotBrush : QBrush(ChartEngine::themeOf(this).background);
}

/*!
    Sets the pen of the border of the plot area to \a pen.
*/
void QChart::setPlotAreaBackgroundPen(const QPen &pen) {
    m_plotPen = pen;
    m_plotPenSet = true;
    update();
}

/*!
    Returns the pen of the border of the plot area.
*/
QPen QChart::plotAreaBackgroundPen() const { return m_plotPenSet ? m_plotPen : QPen(Qt::NoPen); }

/*!
    Draws the background of the plot area if \a visible is \c true. It is off by default.
*/
void QChart::setPlotAreaBackgroundVisible(bool visible) {
    if (visible == m_plotBackgroundVisible)
        return;
    m_plotBackgroundVisible = visible;
    update();
}

/*!
    Returns \c true if the background of the plot area is drawn; otherwise returns \c false.
*/
bool QChart::isPlotAreaBackgroundVisible() const { return m_plotBackgroundVisible; }

/*!
    Sets the animation options to \a options. Nt keeps them and animates nothing.
*/
void QChart::setAnimationOptions(AnimationOptions options) { m_animationOptions = options; }

/*!
    Returns the animation options.
*/
QChart::AnimationOptions QChart::animationOptions() const { return m_animationOptions; }

/*!
    Sets the duration of animations to \a msecs milliseconds. Nt keeps the value.
*/
void QChart::setAnimationDuration(int msecs) { m_animationDuration = msecs; }

/*!
    Returns the duration of animations in milliseconds.
*/
int QChart::animationDuration() const { return m_animationDuration; }

/*!
    Returns the legend of the chart. The chart owns it.
*/
QLegend *QChart::legend() const { return m_legend; }

/*!
    Sets the room between the edges of the chart and its contents to \a margins.
*/
void QChart::setMargins(const QMargins &margins) {
    m_margins = margins;
    update();
}

/*!
    Returns the room between the edges of the chart and its contents. The default is 20 pixels on each
    side.
*/
QMargins QChart::margins() const { return m_margins; }

/*!
    Returns the rectangle in which the series are drawn, in the coordinates of the chart.
*/
QRectF QChart::plotArea() const { return ChartEngine::layout(const_cast<QChart *>(this)).plot; }

/*!
    Fixes the plot area to \a rect, in the coordinates of the chart. An empty rectangle lets the chart lay
    it out again.
*/
void QChart::setPlotArea(const QRectF &rect) {
    m_fixedPlotArea = rect.isValid();
    if (m_fixedPlotArea)
        m_plotArea = rect;
    update();
    ChartEngine::layout(this);
}

/*!
    Zooms in by a factor of 2, around the center of the plot area.
*/
void QChart::zoomIn() { zoom(2.0); }

/*!
    \overload

    Zooms in so that \a rect, in the coordinates of the chart, fills the plot area.
*/
void QChart::zoomIn(const QRectF &rect) { ChartEngine::zoomTo(this, rect); }

/*!
    Zooms out by a factor of 2, around the center of the plot area.
*/
void QChart::zoomOut() { zoom(0.5); }

/*!
    Zooms by \a factor around the center of the plot area: in if it is larger than 1, out if it is
    smaller.
*/
void QChart::zoom(qreal factor) {
    if (!(factor > 0.0))
        return;
    const QRectF plot = plotArea();
    QRectF rect(0, 0, plot.width() / factor, plot.height() / factor);
    rect.moveCenter(plot.center());
    ChartEngine::zoomTo(this, rect);
}

/*!
    Gives the axes the ranges they had before the first zoom or scroll.
*/
void QChart::zoomReset() {
    if (m_zoomOrigin.isEmpty())
        return;
    const auto origin = m_zoomOrigin;
    m_zoomOrigin.clear();
    for (auto it = origin.cbegin(); it != origin.cend(); ++it)
        if (m_axes.contains(it.key()))
            ChartEngine::setAxisRange(it.key(), it.value().first, it.value().second, true);
}

/*!
    Returns \c true if the chart was zoomed or scrolled since it was created or zoomReset() was called;
    otherwise returns \c false.
*/
bool QChart::isZoomed() { return !m_zoomOrigin.isEmpty(); }

/*!
    Scrolls the plot area by \a dx pixels to the right and \a dy pixels up: the axes show larger values.
*/
void QChart::scroll(qreal dx, qreal dy) { ChartEngine::zoomTo(this, plotArea().translated(dx, -dy)); }

/*!
    Returns the value of \a series at \a position, in the coordinates of the chart. Without a series, the
    first series of the chart is used.

    \sa mapToPosition()
*/
QPointF QChart::mapToValue(const QPointF &position, QAbstractSeries *series) {
    if (!series)
        series = m_series.value(0);
    if (!series)
        return QPointF();
    return ChartEngine::toValue(series, position, plotArea());
}

/*!
    Returns where \a value of \a series is drawn, in the coordinates of the chart. Without a series, the
    first series of the chart is used.

    \sa mapToValue()
*/
QPointF QChart::mapToPosition(const QPointF &value, QAbstractSeries *series) {
    if (!series)
        series = m_series.value(0);
    if (!series)
        return QPointF();
    return ChartEngine::toPosition(series, value, plotArea());
}

/*!
    Returns ChartTypeCartesian.
*/
QChart::ChartType QChart::chartType() const { return ChartTypeCartesian; }

/*!
    Sets the geometry of the chart to \a rect: where paint() draws it. QChartView makes it fill the view.
*/
void QChart::setGeometry(const QRectF &rect) {
    if (rect == m_geometry)
        return;
    m_geometry = rect;
    update();
    ChartEngine::layout(this);
}

/*!
    Returns the geometry of the chart.
*/
QRectF QChart::geometry() const { return m_geometry; }

/*!
    Sets the size of the chart to \a size, keeping its position.
*/
void QChart::resize(const QSizeF &size) { setGeometry(QRectF(m_geometry.topLeft(), size)); }

/*!
    Returns the size of the chart.
*/
QSizeF QChart::size() const { return m_geometry.size(); }

/*!
    Draws the chart with \a painter, at its geometry(). This draws a chart on an image or a printer without
    a view:

    \snippet charts/dashboard.cpp image
*/
void QChart::paint(QPainter *painter) { ChartEngine::paint(this, painter); }

/*!
    Asks the view that shows the chart to draw it again. Changes to the chart, its series and axes call
    it.
*/
void QChart::update() { ChartEngine::changed(this); }

#include "moc_qchart.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
