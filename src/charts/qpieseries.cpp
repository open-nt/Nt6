// MIT License - see LICENSE
#include <QtCharts/qpieseries.h>
#include <QtCharts/qpieslice.h>

#include <QtCore/qalgorithms.h>

#include "qchartengine_p.h"

#include <cmath>

using QtChartsPrivate::ChartEngine;

/*!
    \class QPieSlice
    \brief The QPieSlice class is one slice of a pie series.

    \image charts/qpieseries.png Slices with outside labels; the Free slice is exploded

    A slice has a value and a label. Its share of the pie, percentage(), is its value divided by the sum of
    the values of the series. A slice can be exploded, moved out of the pie, and its label can be shown
    outside the pie, joined to it by an arm, or inside it.

    \snippet charts/dashboard.cpp pie

    \differences
    The label is drawn upright for every LabelPosition: LabelInsideTangential and LabelInsideNormal are
    drawn as LabelInsideHorizontal. Slices are filled with the color of their brush only.
    \enddifferences

    \sa QPieSeries
*/

/*!
    \enum QPieSlice::LabelPosition

    Where the label of a slice is drawn.

    \value LabelOutside Outside the pie, joined to the slice by an arm.
    \value LabelInsideHorizontal Inside the slice, horizontally.
    \value LabelInsideTangential Inside the slice, along the circle; Nt draws it horizontally.
    \value LabelInsideNormal Inside the slice, along the radius; Nt draws it horizontally.
*/

/*!
    Constructs a slice with no label and a value of 0, with the given \a parent.
*/
QPieSlice::QPieSlice(QObject *parent) : QObject(parent) {}

/*!
    Constructs a slice called \a label with the value \a value, with the given \a parent.
*/
QPieSlice::QPieSlice(const QString &label, qreal value, QObject *parent)
    : QObject(parent), m_label(label), m_value(value) {}

/*!
    Destroys the slice and removes it from its series.
*/
QPieSlice::~QPieSlice() { ChartEngine::sliceDestroyed(this); }

void QPieSlice::markChanged() { ChartEngine::changed(this); }

/*!
    Sets the label of the slice to \a label.
*/
void QPieSlice::setLabel(const QString &label) {
    if (label == m_label)
        return;
    m_label = label;
    markChanged();
    emit labelChanged();
}

/*!
    Returns the label of the slice.
*/
QString QPieSlice::label() const { return m_label; }

/*!
    Sets the value of the slice to \a value. Negative values count as zero.
*/
void QPieSlice::setValue(qreal value) {
    if (value == m_value || !std::isfinite(value))
        return;
    m_value = value;
    if (m_series)
        m_series->updateAngles();
    markChanged();
    emit valueChanged();
}

/*!
    Returns the value of the slice.
*/
qreal QPieSlice::value() const { return m_value; }

/*!
    Shows the label of the slice if \a visible is \c true.
*/
void QPieSlice::setLabelVisible(bool visible) {
    if (visible == m_labelVisible)
        return;
    m_labelVisible = visible;
    markChanged();
    emit labelVisibleChanged();
}

/*!
    Returns \c true if the label is drawn; otherwise returns \c false.
*/
bool QPieSlice::isLabelVisible() const { return m_labelVisible; }

/*!
    Returns where the label is drawn. The default is LabelOutside.
*/
QPieSlice::LabelPosition QPieSlice::labelPosition() { return m_labelPosition; }

/*!
    Sets where the label is drawn to \a position.
*/
void QPieSlice::setLabelPosition(LabelPosition position) {
    if (position == m_labelPosition)
        return;
    m_labelPosition = position;
    markChanged();
}

/*!
    Moves the slice out of the pie if \a exploded is \c true.

    \sa setExplodeDistanceFactor()
*/
void QPieSlice::setExploded(bool exploded) {
    if (exploded == m_exploded)
        return;
    m_exploded = exploded;
    markChanged();
}

/*!
    Returns \c true if the slice is moved out of the pie; otherwise returns \c false.
*/
bool QPieSlice::isExploded() const { return m_exploded; }

/*!
    Sets the pen of the border of the slice to \a pen.
*/
void QPieSlice::setPen(const QPen &pen) {
    const QPen before = this->pen();
    m_pen = pen;
    m_penSet = true;
    markChanged();
    emit penChanged();
    if (pen.color() != before.color())
        emit borderColorChanged();
    if (pen.widthF() != before.widthF())
        emit borderWidthChanged();
}

/*!
    Returns the pen of the border of the slice. Unless one was set, it has the background color of the
    chart and is 2 pixels wide, which separates the slices.
*/
QPen QPieSlice::pen() const {
    if (m_penSet)
        return m_pen;
    return QPen(QBrush(ChartEngine::backgroundColor(m_series ? m_series->chart() : nullptr)), 2.0);
}

/*!
    Returns the color of the border.
*/
QColor QPieSlice::borderColor() { return pen().color(); }

/*!
    Sets the color of the border to \a color.
*/
void QPieSlice::setBorderColor(QColor color) {
    QPen p = pen();
    p.setColor(color);
    setPen(p);
}

/*!
    Returns the width of the border in pixels.
*/
int QPieSlice::borderWidth() { return pen().width(); }

/*!
    Sets the width of the border to \a width pixels.
*/
void QPieSlice::setBorderWidth(int width) {
    QPen p = pen();
    p.setWidth(width);
    setPen(p);
}

/*!
    Sets the brush of the slice to \a brush.
*/
void QPieSlice::setBrush(const QBrush &brush) {
    const QColor before = brush.color();
    const QColor old = this->brush().color();
    m_brush = brush;
    m_brushSet = true;
    markChanged();
    emit brushChanged();
    if (before != old)
        emit colorChanged();
}

/*!
    Returns the brush of the slice. Unless one was set, it has the color the theme gives the slice.
*/
QBrush QPieSlice::brush() const { return m_brushSet ? m_brush : QBrush(ChartEngine::sliceColor(this)); }

/*!
    Returns the fill color of the slice.
*/
QColor QPieSlice::color() { return brush().color(); }

/*!
    Sets the fill color of the slice to \a color.
*/
void QPieSlice::setColor(QColor color) {
    QBrush b = brush();
    b.setColor(color);
    b.setStyle(Qt::SolidPattern);
    setBrush(b);
}

/*!
    Sets the brush of the label to \a brush.
*/
void QPieSlice::setLabelBrush(const QBrush &brush) {
    const QColor old = labelBrush().color();
    m_labelBrush = brush;
    m_labelBrushSet = true;
    markChanged();
    emit labelBrushChanged();
    if (brush.color() != old)
        emit labelColorChanged();
}

/*!
    Returns the brush of the label.
*/
QBrush QPieSlice::labelBrush() const {
    if (m_labelBrushSet)
        return m_labelBrush;
    return QBrush(ChartEngine::themeOf(m_series ? m_series->chart() : nullptr).labels);
}

/*!
    Returns the color of the label.
*/
QColor QPieSlice::labelColor() { return labelBrush().color(); }

/*!
    Sets the color of the label to \a color.
*/
void QPieSlice::setLabelColor(QColor color) {
    QBrush b = labelBrush();
    b.setColor(color);
    setLabelBrush(b);
}

/*!
    Sets the font of the label to \a font.
*/
void QPieSlice::setLabelFont(const QFont &font) {
    m_labelFont = font;
    m_labelFontSet = true;
    markChanged();
    emit labelFontChanged();
}

/*!
    Returns the font of the label.
*/
QFont QPieSlice::labelFont() const { return m_labelFont; }

/*!
    Sets the length of the arm of an outside label to \a factor times the radius of the pie. The default
    is 0.15.
*/
void QPieSlice::setLabelArmLengthFactor(qreal factor) {
    if (!(factor >= 0.0) || factor == m_armLength)
        return;
    m_armLength = factor;
    markChanged();
}

/*!
    Returns the length of the arm of an outside label, as a factor of the radius of the pie.
*/
qreal QPieSlice::labelArmLengthFactor() const { return m_armLength; }

/*!
    Sets how far an exploded slice moves out to \a factor times the radius of the pie. The default is
    0.15.
*/
void QPieSlice::setExplodeDistanceFactor(qreal factor) {
    if (!(factor >= 0.0) || factor == m_explodeDistance)
        return;
    m_explodeDistance = factor;
    markChanged();
}

/*!
    Returns how far an exploded slice moves out, as a factor of the radius of the pie.
*/
qreal QPieSlice::explodeDistanceFactor() const { return m_explodeDistance; }

/*!
    Returns the share of the slice in its series, from 0.0 to 1.0.
*/
qreal QPieSlice::percentage() const { return m_percentage; }

/*!
    Returns the angle at which the slice starts, in degrees clockwise from 12 o'clock.
*/
qreal QPieSlice::startAngle() const { return m_startAngle; }

/*!
    Returns the angle the slice spans, in degrees.
*/
qreal QPieSlice::angleSpan() const { return m_angleSpan; }

/*!
    Returns the series the slice is in, or \c nullptr if it is in none.
*/
QPieSeries *QPieSlice::series() const { return m_series; }

/*!
    \class QPieSeries
    \brief The QPieSeries class draws its slices as a pie or a donut.

    \image charts/qpieseries.png A pie series with labels and an exploded slice

    Each QPieSlice takes a share of the circle in proportion to its value. setHoleSize() makes the pie a
    donut, and setPieStartAngle() and setPieEndAngle() draw part of a circle, such as a half-circle
    gauge. The series owns its slices.

    \snippet charts/dashboard.cpp pie

    \image charts/donut.png A donut: a pie series with a hole

    A pie series has no axes. The pie is centered at horizontalPosition() and verticalPosition() of the
    plot area, and its diameter is pieSize() times the smaller side of the plot area. Outside labels
    must fit in the plot area, so the pie is made smaller when they need the room.

    \differences
    There is no \c QPieModelMapper. See QPieSlice for the labels.
    \enddifferences

    \sa QPieSlice
*/

/*!
    Constructs an empty pie series with the given \a parent.
*/
QPieSeries::QPieSeries(QObject *parent) : QAbstractSeries(parent) {}

/*!
    Destroys the series and its slices.
*/
QPieSeries::~QPieSeries() {
    const QList<QPieSlice *> slices = m_slices;
    m_slices.clear();
    for (QPieSlice *slice : slices) {
        slice->m_series = nullptr;
        delete slice;
    }
}

/*!
    \reimp
    Returns QAbstractSeries::SeriesTypePie.
*/
QAbstractSeries::SeriesType QPieSeries::type() const { return SeriesTypePie; }

void QPieSeries::updateAngles() {
    qreal sum = 0.0;
    for (const QPieSlice *slice : std::as_const(m_slices))
        sum += std::max(0.0, slice->m_value);
    const bool sumChange = sum != m_sum;
    m_sum = sum;
    const qreal total = m_endAngle - m_startAngle;
    qreal angle = m_startAngle;
    for (QPieSlice *slice : std::as_const(m_slices)) {
        const qreal share = sum > 0.0 ? std::max(0.0, slice->m_value) / sum : 0.0;
        const qreal span = share * total;
        const bool p = share != slice->m_percentage, s = angle != slice->m_startAngle, a = span != slice->m_angleSpan;
        slice->m_percentage = share;
        slice->m_startAngle = angle;
        slice->m_angleSpan = span;
        angle += span;
        if (p)
            emit slice->percentageChanged();
        if (s)
            emit slice->startAngleChanged();
        if (a)
            emit slice->angleSpanChanged();
    }
    if (sumChange)
        emit sumChanged();
}

bool QPieSeries::adopt(QPieSlice *slice) {
    if (!slice || slice->m_series || m_slices.contains(slice))
        return false;
    slice->m_series = this;
    slice->setParent(this);
    return true;
}

/*!
    Appends \a slice to the series, which takes ownership of it. Returns \c false if \a slice is
    \c nullptr or already in a series.
*/
bool QPieSeries::append(QPieSlice *slice) { return insert(count(), slice); }

/*!
    \overload

    Appends \a slices. Returns \c false, and appends none of them, if one of them is \c nullptr, appears
    twice or is already in a series.
*/
bool QPieSeries::append(const QList<QPieSlice *> &slices) {
    for (qsizetype i = 0; i < slices.size(); ++i) {
        const QPieSlice *slice = slices.at(i);
        if (!slice || slice->m_series || slices.indexOf(slice) != i)
            return false;
    }
    if (slices.isEmpty())
        return true;
    for (QPieSlice *slice : slices) {
        adopt(slice);
        m_slices.append(slice);
    }
    updateAngles();
    markChanged();
    emit added(slices);
    emit countChanged();
    return true;
}

/*!
    Appends \a slice and returns a reference to the series.
*/
QPieSeries &QPieSeries::operator<<(QPieSlice *slice) {
    append(slice);
    return *this;
}

/*!
    \overload

    Appends a new slice called \a label with the value \a value, and returns it.
*/
QPieSlice *QPieSeries::append(const QString &label, qreal value) {
    auto *slice = new QPieSlice(label, value);
    append(slice);
    return slice;
}

/*!
    Inserts \a slice at \a index; an index out of range appends it. Returns \c false if \a slice is
    \c nullptr or already in a series.
*/
bool QPieSeries::insert(int index, QPieSlice *slice) {
    if (!adopt(slice))
        return false;
    if (index < 0 || index > count())
        index = count();
    m_slices.insert(index, slice);
    updateAngles();
    markChanged();
    emit added(QList<QPieSlice *>{slice});
    emit countChanged();
    return true;
}

/*!
    Removes \a slice from the series and deletes it. Returns \c true on success.
*/
bool QPieSeries::remove(QPieSlice *slice) {
    if (!take(slice))
        return false;
    delete slice;
    return true;
}

/*!
    Removes \a slice from the series without deleting it; the caller owns it. Returns \c true on success.
*/
bool QPieSeries::take(QPieSlice *slice) {
    if (!slice || slice->m_series != this)
        return false;
    m_slices.removeAll(slice);
    slice->m_series = nullptr;
    slice->setParent(nullptr);
    updateAngles();
    markChanged();
    emit removed(QList<QPieSlice *>{slice});
    emit countChanged();
    return true;
}

/*!
    Removes and deletes all slices.
*/
void QPieSeries::clear() {
    if (m_slices.isEmpty())
        return;
    const QList<QPieSlice *> slices = m_slices;
    m_slices.clear();
    for (QPieSlice *slice : slices)
        slice->m_series = nullptr;
    updateAngles();
    markChanged();
    emit removed(slices);
    emit countChanged();
    qDeleteAll(slices);
}

/*!
    Returns the slices.
*/
QList<QPieSlice *> QPieSeries::slices() const { return m_slices; }

/*!
    Returns the number of slices.
*/
int QPieSeries::count() const { return static_cast<int>(m_slices.size()); }

/*!
    Returns \c true if the series has no slices; otherwise returns \c false.
*/
bool QPieSeries::isEmpty() const { return m_slices.isEmpty(); }

/*!
    Returns the sum of the values of the slices; negative values count as zero.
*/
qreal QPieSeries::sum() const { return m_sum; }

/*!
    Sets the diameter of the hole in the middle to \a holeSize, as a share of the smaller side of the plot
    area, from 0.0 (a pie) to pieSize() (nothing left).
*/
void QPieSeries::setHoleSize(qreal holeSize) {
    holeSize = std::clamp(holeSize, 0.0, 1.0);
    if (holeSize == m_holeSize)
        return;
    m_holeSize = holeSize;
    m_size = std::max(m_size, holeSize);
    markChanged();
}

/*!
    Returns the diameter of the hole, as a share of the smaller side of the plot area.
*/
qreal QPieSeries::holeSize() const { return m_holeSize; }

/*!
    Sets the horizontal position of the center to \a relativePosition, from 0.0 (left edge of the plot
    area) to 1.0 (right edge). The default is 0.5.
*/
void QPieSeries::setHorizontalPosition(qreal relativePosition) {
    relativePosition = std::clamp(relativePosition, 0.0, 1.0);
    if (relativePosition == m_horizontal)
        return;
    m_horizontal = relativePosition;
    markChanged();
}

/*!
    Returns the horizontal position of the center, from 0.0 to 1.0.
*/
qreal QPieSeries::horizontalPosition() const { return m_horizontal; }

/*!
    Sets the vertical position of the center to \a relativePosition, from 0.0 (top edge of the plot area)
    to 1.0 (bottom edge). The default is 0.5.
*/
void QPieSeries::setVerticalPosition(qreal relativePosition) {
    relativePosition = std::clamp(relativePosition, 0.0, 1.0);
    if (relativePosition == m_vertical)
        return;
    m_vertical = relativePosition;
    markChanged();
}

/*!
    Returns the vertical position of the center, from 0.0 to 1.0.
*/
qreal QPieSeries::verticalPosition() const { return m_vertical; }

/*!
    Sets the diameter of the pie to \a relativeSize, as a share of the smaller side of the plot area, from
    0.0 to 1.0. The default is 0.7.
*/
void QPieSeries::setPieSize(qreal relativeSize) {
    relativeSize = std::clamp(relativeSize, 0.0, 1.0);
    if (relativeSize == m_size)
        return;
    m_size = relativeSize;
    m_holeSize = std::min(m_holeSize, relativeSize);
    markChanged();
}

/*!
    Returns the diameter of the pie, as a share of the smaller side of the plot area.
*/
qreal QPieSeries::pieSize() const { return m_size; }

/*!
    Sets the angle at which the first slice starts to \a startAngle degrees, clockwise from 12 o'clock.
    The default is 0.
*/
void QPieSeries::setPieStartAngle(qreal startAngle) {
    if (startAngle == m_startAngle || !std::isfinite(startAngle))
        return;
    m_startAngle = startAngle;
    updateAngles();
    markChanged();
}

/*!
    Returns the angle at which the first slice starts.
*/
qreal QPieSeries::pieStartAngle() const { return m_startAngle; }

/*!
    Sets the angle at which the last slice ends to \a endAngle degrees, clockwise from 12 o'clock. The
    default is 360.
*/
void QPieSeries::setPieEndAngle(qreal endAngle) {
    if (endAngle == m_endAngle || !std::isfinite(endAngle))
        return;
    m_endAngle = endAngle;
    updateAngles();
    markChanged();
}

/*!
    Returns the angle at which the last slice ends.
*/
qreal QPieSeries::pieEndAngle() const { return m_endAngle; }

/*!
    Shows the labels of all slices if \a visible is \c true, and hides them otherwise.
*/
void QPieSeries::setLabelsVisible(bool visible) {
    for (QPieSlice *slice : std::as_const(m_slices))
        slice->setLabelVisible(visible);
}

/*!
    Sets where the labels of all slices are drawn to \a position.
*/
void QPieSeries::setLabelsPosition(QPieSlice::LabelPosition position) {
    for (QPieSlice *slice : std::as_const(m_slices))
        slice->setLabelPosition(position);
}

#include "moc_qpieseries.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qpieslice.cpp"  // NOLINT(bugprone-suspicious-include): generated by ntmoc
