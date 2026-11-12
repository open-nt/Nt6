// MIT License - see LICENSE
#include <QtCharts/qabstractaxis.h>

#include "qchartengine_p.h"

using QtChartsPrivate::ChartEngine;

/*!
    \class QAbstractAxis
    \brief The QAbstractAxis class is the base class of the axes of a chart.

    An axis maps the values of the series attached to it to one direction of the plot area, and draws a
    line, ticks, labels, grid lines and an optional title. Add an axis to a chart with QChart::addAxis(),
    then attach series to it with QAbstractSeries::attachAxis():

    \code
    auto *axisY = new QValueAxis;
    axisY->setTitleText("Load (%)");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    \endcode

    Colors and fonts that are not set come from the theme of the chart. QChart::setTheme() forgets the ones
    that were set.

    \differences
    The axis is a QObject, not a graphics item. Labels are drawn upright: labelsAngle() is kept but not
    used. There is no \c QCategoryAxis or \c QColorAxis, and no truncation of labels or label editing.
    \enddifferences

    \sa QValueAxis, QLogValueAxis, QDateTimeAxis, QBarCategoryAxis, QChart
*/

/*!
    \enum QAbstractAxis::AxisType

    The type of an axis.

    \value AxisTypeNoAxis No axis.
    \value AxisTypeValue A QValueAxis.
    \value AxisTypeBarCategory A QBarCategoryAxis.
    \value AxisTypeCategory A category axis with ranges; Nt has none.
    \value AxisTypeDateTime A QDateTimeAxis.
    \value AxisTypeLogValue A QLogValueAxis.
    \value AxisTypeColor A color scale axis; Nt has none.
*/

/*!
    \typedef QAbstractAxis::AxisTypes

    A QFlags of AxisType values.
*/

/*!
    Constructs an axis with the given \a parent. The axis does nothing until it is added to a chart.
*/
QAbstractAxis::QAbstractAxis(QObject *parent) : QObject(parent) {}

/*!
    Destroys the axis and removes it from its chart.
*/
QAbstractAxis::~QAbstractAxis() { ChartEngine::axisDestroyed(this); }

/*!
    \fn QAbstractAxis::AxisType QAbstractAxis::type() const

    Returns the type of the axis.
*/

/*!
    Tells the chart that the axis must be drawn again.
*/
void QAbstractAxis::markChanged() { ChartEngine::changed(this); }

/*!
    \fn void QAbstractAxis::setRangeFromVariant(const QVariant &min, const QVariant &max)

    Sets the range of the axis to \a min and \a max, converted to the type of the axis. setMin(), setMax()
    and setRange() call it.
*/

/*!
    Returns \c true if the axis is drawn; otherwise returns \c false.

    \sa setVisible()
*/
bool QAbstractAxis::isVisible() const { return m_visible; }

/*!
    Shows the axis if \a visible is \c true and hides it otherwise. A hidden axis still maps its series.
*/
void QAbstractAxis::setVisible(bool visible) {
    if (m_visible == visible)
        return;
    m_visible = visible;
    markChanged();
    emit visibleChanged(visible);
}

/*!
    Shows the axis.
*/
void QAbstractAxis::show() { setVisible(true); }

/*!
    Hides the axis.
*/
void QAbstractAxis::hide() { setVisible(false); }

/*!
    Returns \c true if the axis line is drawn; otherwise returns \c false.
*/
bool QAbstractAxis::isLineVisible() const { return m_lineVisible; }

/*!
    Shows the axis line if \a visible is \c true and hides it otherwise.
*/
void QAbstractAxis::setLineVisible(bool visible) {
    if (m_lineVisible == visible)
        return;
    m_lineVisible = visible;
    markChanged();
    emit lineVisibleChanged(visible);
}

/*!
    Returns the pen of the axis line and its ticks.
*/
QPen QAbstractAxis::linePen() const {
    return m_linePenSet ? m_linePen : QPen(QBrush(ChartEngine::themeOf(m_chart).axisLine), 1.0);
}

/*!
    Sets the pen of the axis line and its ticks to \a pen.
*/
void QAbstractAxis::setLinePen(const QPen &pen) {
    m_linePen = pen;
    m_linePenSet = true;
    markChanged();
    emit linePenChanged(pen);
}

/*!
    Returns the color of the axis line.
*/
QColor QAbstractAxis::linePenColor() const { return linePen().color(); }

/*!
    Sets the color of the axis line to \a color.
*/
void QAbstractAxis::setLinePenColor(QColor color) {
    QPen pen = linePen();
    pen.setColor(color);
    setLinePen(pen);
    emit colorChanged(color);
}

/*!
    Returns \c true if grid lines are drawn at the ticks; otherwise returns \c false.
*/
bool QAbstractAxis::isGridLineVisible() const { return m_gridVisible; }

/*!
    Shows the grid lines if \a visible is \c true and hides them otherwise.
*/
void QAbstractAxis::setGridLineVisible(bool visible) {
    if (m_gridVisible == visible)
        return;
    m_gridVisible = visible;
    markChanged();
    emit gridVisibleChanged(visible);
}

/*!
    Returns the pen of the grid lines.
*/
QPen QAbstractAxis::gridLinePen() const {
    return m_gridPenSet ? m_gridPen : QPen(QBrush(ChartEngine::themeOf(m_chart).grid), 1.0);
}

/*!
    Sets the pen of the grid lines to \a pen.
*/
void QAbstractAxis::setGridLinePen(const QPen &pen) {
    m_gridPen = pen;
    m_gridPenSet = true;
    markChanged();
    emit gridLinePenChanged(pen);
}

/*!
    Returns the color of the grid lines.
*/
QColor QAbstractAxis::gridLineColor() const { return gridLinePen().color(); }

/*!
    Sets the color of the grid lines to \a color.
*/
void QAbstractAxis::setGridLineColor(const QColor &color) {
    QPen pen = gridLinePen();
    pen.setColor(color);
    setGridLinePen(pen);
    emit gridLineColorChanged(color);
}

/*!
    Returns \c true if minor grid lines are drawn; otherwise returns \c false. Value and logarithmic axes
    have them, between the ticks.
*/
bool QAbstractAxis::isMinorGridLineVisible() const { return m_minorGridVisible; }

/*!
    Shows the minor grid lines if \a visible is \c true and hides them otherwise.

    \sa QValueAxis::setMinorTickCount()
*/
void QAbstractAxis::setMinorGridLineVisible(bool visible) {
    if (m_minorGridVisible == visible)
        return;
    m_minorGridVisible = visible;
    markChanged();
    emit minorGridVisibleChanged(visible);
}

/*!
    Returns the pen of the minor grid lines.
*/
QPen QAbstractAxis::minorGridLinePen() const {
    return m_minorGridPenSet ? m_minorGridPen : QPen(QBrush(ChartEngine::themeOf(m_chart).minorGrid), 1.0);
}

/*!
    Sets the pen of the minor grid lines to \a pen.
*/
void QAbstractAxis::setMinorGridLinePen(const QPen &pen) {
    m_minorGridPen = pen;
    m_minorGridPenSet = true;
    markChanged();
    emit minorGridLinePenChanged(pen);
}

/*!
    Returns the color of the minor grid lines.
*/
QColor QAbstractAxis::minorGridLineColor() const { return minorGridLinePen().color(); }

/*!
    Sets the color of the minor grid lines to \a color.
*/
void QAbstractAxis::setMinorGridLineColor(const QColor &color) {
    QPen pen = minorGridLinePen();
    pen.setColor(color);
    setMinorGridLinePen(pen);
    emit minorGridLineColorChanged(color);
}

/*!
    Returns \c true if the labels are drawn; otherwise returns \c false.
*/
bool QAbstractAxis::labelsVisible() const { return m_labelsVisible; }

/*!
    Shows the labels if \a visible is \c true and hides them otherwise.
*/
void QAbstractAxis::setLabelsVisible(bool visible) {
    if (m_labelsVisible == visible)
        return;
    m_labelsVisible = visible;
    markChanged();
    emit labelsVisibleChanged(visible);
}

/*!
    Returns the brush of the labels. Its color is the color of the text.
*/
QBrush QAbstractAxis::labelsBrush() const {
    return m_labelsBrushSet ? m_labelsBrush : QBrush(ChartEngine::themeOf(m_chart).labels);
}

/*!
    Sets the brush of the labels to \a brush.
*/
void QAbstractAxis::setLabelsBrush(const QBrush &brush) {
    m_labelsBrush = brush;
    m_labelsBrushSet = true;
    markChanged();
    emit labelsBrushChanged(brush);
}

/*!
    Returns the font of the labels.
*/
QFont QAbstractAxis::labelsFont() const { return m_labelsFont; }

/*!
    Sets the font of the labels to \a font.
*/
void QAbstractAxis::setLabelsFont(const QFont &font) {
    m_labelsFont = font;
    m_labelsFontSet = true;
    markChanged();
    emit labelsFontChanged(font);
}

/*!
    Returns the angle of the labels, in degrees.
*/
int QAbstractAxis::labelsAngle() const { return m_labelsAngle; }

/*!
    Sets the angle of the labels to \a angle degrees. Nt keeps the value and draws the labels upright.
*/
void QAbstractAxis::setLabelsAngle(int angle) {
    if (m_labelsAngle == angle)
        return;
    m_labelsAngle = angle;
    emit labelsAngleChanged(angle);
}

/*!
    Returns the color of the labels.
*/
QColor QAbstractAxis::labelsColor() const { return labelsBrush().color(); }

/*!
    Sets the color of the labels to \a color.
*/
void QAbstractAxis::setLabelsColor(QColor color) {
    QBrush brush = labelsBrush();
    brush.setColor(color);
    setLabelsBrush(brush);
    emit labelsColorChanged(color);
}

/*!
    Returns \c true if the title is drawn when it is not empty; otherwise returns \c false.
*/
bool QAbstractAxis::isTitleVisible() const { return m_titleVisible; }

/*!
    Shows the title if \a visible is \c true and hides it otherwise.
*/
void QAbstractAxis::setTitleVisible(bool visible) {
    if (m_titleVisible == visible)
        return;
    m_titleVisible = visible;
    markChanged();
    emit titleVisibleChanged(visible);
}

/*!
    Returns the brush of the title.
*/
QBrush QAbstractAxis::titleBrush() const {
    return m_titleBrushSet ? m_titleBrush : QBrush(ChartEngine::themeOf(m_chart).title);
}

/*!
    Sets the brush of the title to \a brush.
*/
void QAbstractAxis::setTitleBrush(const QBrush &brush) {
    m_titleBrush = brush;
    m_titleBrushSet = true;
    markChanged();
    emit titleBrushChanged(brush);
}

/*!
    Returns the font of the title.
*/
QFont QAbstractAxis::titleFont() const {
    if (m_titleFontSet)
        return m_titleFont;
    QFont font;
    font.setBold(true);
    return font;
}

/*!
    Sets the font of the title to \a font.
*/
void QAbstractAxis::setTitleFont(const QFont &font) {
    m_titleFont = font;
    m_titleFontSet = true;
    markChanged();
    emit titleFontChanged(font);
}

/*!
    Returns the title of the axis.
*/
QString QAbstractAxis::titleText() const { return m_titleText; }

/*!
    Sets the title of the axis to \a title. A vertical axis draws its title upright, above its labels.
*/
void QAbstractAxis::setTitleText(const QString &title) {
    if (m_titleText == title)
        return;
    m_titleText = title;
    markChanged();
    emit titleTextChanged(title);
}

/*!
    Returns \c true if every other interval between ticks is shaded; otherwise returns \c false.
*/
bool QAbstractAxis::shadesVisible() const { return m_shadesVisible; }

/*!
    Shades every other interval between ticks if \a visible is \c true.
*/
void QAbstractAxis::setShadesVisible(bool visible) {
    if (m_shadesVisible == visible)
        return;
    m_shadesVisible = visible;
    markChanged();
    emit shadesVisibleChanged(visible);
}

/*!
    Returns the pen of the borders of the shades.
*/
QPen QAbstractAxis::shadesPen() const { return m_shadesPenSet ? m_shadesPen : QPen(Qt::NoPen); }

/*!
    Sets the pen of the borders of the shades to \a pen.
*/
void QAbstractAxis::setShadesPen(const QPen &pen) {
    m_shadesPen = pen;
    m_shadesPenSet = true;
    markChanged();
    emit shadesPenChanged(pen);
}

/*!
    Returns the brush of the shades.
*/
QBrush QAbstractAxis::shadesBrush() const {
    return m_shadesBrushSet ? m_shadesBrush : QBrush(ChartEngine::themeOf(m_chart).shades);
}

/*!
    Sets the brush of the shades to \a brush.
*/
void QAbstractAxis::setShadesBrush(const QBrush &brush) {
    m_shadesBrush = brush;
    m_shadesBrushSet = true;
    markChanged();
    emit shadesBrushChanged(brush);
}

/*!
    Returns the fill color of the shades.
*/
QColor QAbstractAxis::shadesColor() const { return shadesBrush().color(); }

/*!
    Sets the fill color of the shades to \a color.
*/
void QAbstractAxis::setShadesColor(QColor color) {
    QBrush brush = shadesBrush();
    brush.setColor(color);
    brush.setStyle(Qt::SolidPattern);
    setShadesBrush(brush);
    emit shadesColorChanged(color);
}

/*!
    Returns the border color of the shades.
*/
QColor QAbstractAxis::shadesBorderColor() const { return shadesPen().color(); }

/*!
    Sets the border color of the shades to \a color.
*/
void QAbstractAxis::setShadesBorderColor(QColor color) {
    QPen pen = shadesPen();
    pen.setColor(color);
    if (pen.style() == Qt::NoPen)
        pen.setStyle(Qt::SolidLine);
    setShadesPen(pen);
    emit shadesBorderColorChanged(color);
}

/*!
    Returns the orientation of the axis, which follows from the alignment it was added with:
    Qt::Horizontal at the top or bottom, Qt::Vertical at the left or right.
*/
Qt::Orientation QAbstractAxis::orientation() const { return m_orientation; }

/*!
    Returns the side of the plot area the axis is drawn on, or no alignment if it is not in a chart.
*/
Qt::Alignment QAbstractAxis::alignment() const { return m_alignment; }

/*!
    Sets the minimum of the axis to \a min, converted to the type of the axis.
*/
void QAbstractAxis::setMin(const QVariant &min) { setRangeFromVariant(min, QVariant()); }

/*!
    Sets the maximum of the axis to \a max, converted to the type of the axis.
*/
void QAbstractAxis::setMax(const QVariant &max) { setRangeFromVariant(QVariant(), max); }

/*!
    Sets the range of the axis to \a min and \a max, converted to the type of the axis.
*/
void QAbstractAxis::setRange(const QVariant &min, const QVariant &max) { setRangeFromVariant(min, max); }

/*!
    Returns \c true if the axis runs from its maximum to its minimum; otherwise returns \c false.
*/
bool QAbstractAxis::isReverse() const { return m_reverse; }

/*!
    Makes the axis run from its maximum to its minimum if \a reverse is \c true: right to left, or top to
    bottom.
*/
void QAbstractAxis::setReverse(bool reverse) {
    if (m_reverse == reverse)
        return;
    m_reverse = reverse;
    markChanged();
    emit reverseChanged(reverse);
}

#include "moc_qabstractaxis.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
