// MIT License - see LICENSE
#include <QtCharts/qlogvalueaxis.h>
#include <QtCharts/qvalueaxis.h>

#include "qchartengine_p.h"

#include <cmath>

using QtChartsPrivate::ChartEngine;

namespace {

// The "nice" number near `x`: 1, 2 or 5 times a power of ten, rounded up when `up` is set.
double niceNumber(double x, bool up) {
    const double exponent = std::floor(std::log10(x));
    const double fraction = x / std::pow(10.0, exponent);
    double nice = 10.0;
    if (up)
        nice = fraction <= 1.0 ? 1.0 : fraction <= 2.0 ? 2.0 : fraction <= 5.0 ? 5.0 : 10.0;
    else
        nice = fraction < 1.5 ? 1.0 : fraction < 3.0 ? 2.0 : fraction < 7.0 ? 5.0 : 10.0;
    return nice * std::pow(10.0, exponent);
}

} // namespace

/*!
    \class QValueAxis
    \brief The QValueAxis class is an axis of real numbers.

    \image charts/qlineseries.png Value axes on a line chart

    The axis has a range, set with setRange() or taken from the data of the series attached to it, and
    ticks: tickCount() ticks spread evenly over the range (the default), or a tick every tickInterval()
    from tickAnchor() with setTickType(TicksDynamic).

    \code
    auto *axisY = new QValueAxis;
    axisY->setRange(0, 100);
    axisY->setTickCount(6);
    axisY->setLabelFormat("%d %%");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    \endcode

    When a series is attached to an axis whose range was not set, the range becomes the extent of the data
    of its series. It is not changed when the data changes later; call setRange() for data that moves, as
    with a live plot. applyNiceNumbers() rounds the range outwards to round numbers.

    \differences
    Labels are formatted by Nt itself: labelFormat() takes one printf-style conversion and no locale is
    applied.
    \enddifferences

    \sa QLogValueAxis, QAbstractAxis
*/

/*!
    \enum QValueAxis::TickType

    How the ticks are placed.

    \value TicksDynamic A tick at tickAnchor() and every tickInterval() from it.
    \value TicksFixed tickCount() ticks spread evenly from the minimum to the maximum.
*/

/*!
    Constructs a value axis with the given \a parent. Its range is 0 to 0 until it is set or taken from
    the data.
*/
QValueAxis::QValueAxis(QObject *parent) : QAbstractAxis(parent) {}

/*!
    Destroys the axis.
*/
QValueAxis::~QValueAxis() = default;

/*!
    \reimp
    Returns QAbstractAxis::AxisTypeValue.
*/
QAbstractAxis::AxisType QValueAxis::type() const { return AxisTypeValue; }

/*!
    Sets the minimum of the axis to \a min. The maximum grows to \a min if it is smaller.
*/
void QValueAxis::setMin(qreal min) { setRange(min, std::max(min, m_max)); }

/*!
    Returns the minimum of the axis.
*/
qreal QValueAxis::min() const { return m_min; }

/*!
    Sets the maximum of the axis to \a max. The minimum shrinks to \a max if it is larger.
*/
void QValueAxis::setMax(qreal max) { setRange(std::min(m_min, max), max); }

/*!
    Returns the maximum of the axis.
*/
qreal QValueAxis::max() const { return m_max; }

/*!
    Sets the range of the axis to \a min and \a max. A range whose minimum is larger than its maximum, or
    that is not finite, is ignored.
*/
void QValueAxis::setRange(qreal min, qreal max) {
    if (!(min <= max) || !std::isfinite(min) || !std::isfinite(max))
        return;
    ChartEngine::rangeSetByUser(this);
    const bool minChange = min != m_min, maxChange = max != m_max;
    if (!minChange && !maxChange)
        return;
    m_min = min;
    m_max = max;
    markChanged();
    if (minChange)
        emit minChanged(min);
    if (maxChange)
        emit maxChanged(max);
    emit rangeChanged(min, max);
}

/*!
    Sets the number of ticks to \a count, at least 2. Used with TicksFixed.
*/
void QValueAxis::setTickCount(int count) {
    if (count < 2 || count == m_tickCount)
        return;
    m_tickCount = count;
    markChanged();
    emit tickCountChanged(count);
}

/*!
    Returns the number of ticks. The default is 5.
*/
int QValueAxis::tickCount() const { return m_tickCount; }

/*!
    Sets the number of minor ticks between two ticks to \a count. They have minor grid lines.

    \sa QAbstractAxis::setMinorGridLineVisible()
*/
void QValueAxis::setMinorTickCount(int count) {
    if (count < 0 || count == m_minorTickCount)
        return;
    m_minorTickCount = count;
    markChanged();
    emit minorTickCountChanged(count);
}

/*!
    Returns the number of minor ticks between two ticks. The default is 0.
*/
int QValueAxis::minorTickCount() const { return m_minorTickCount; }

/*!
    Sets the value that has a tick when the tick type is TicksDynamic to \a anchor.
*/
void QValueAxis::setTickAnchor(qreal anchor) {
    if (anchor == m_tickAnchor)
        return;
    m_tickAnchor = anchor;
    markChanged();
    emit tickAnchorChanged(anchor);
}

/*!
    Returns the value that has a tick when the tick type is TicksDynamic.
*/
qreal QValueAxis::tickAnchor() const { return m_tickAnchor; }

/*!
    Sets the distance between ticks when the tick type is TicksDynamic to \a interval.
*/
void QValueAxis::setTickInterval(qreal interval) {
    if (interval == m_tickInterval)
        return;
    m_tickInterval = interval;
    markChanged();
    emit tickIntervalChanged(interval);
}

/*!
    Returns the distance between ticks when the tick type is TicksDynamic.
*/
qreal QValueAxis::tickInterval() const { return m_tickInterval; }

/*!
    Sets how the ticks are placed to \a type.
*/
void QValueAxis::setTickType(QValueAxis::TickType type) {
    if (type == m_tickType)
        return;
    m_tickType = type;
    markChanged();
    emit tickTypeChanged(type);
}

/*!
    Returns how the ticks are placed. The default is TicksFixed.
*/
QValueAxis::TickType QValueAxis::tickType() const { return m_tickType; }

/*!
    Sets the format of the labels to \a format: text with one printf-style conversion, such as
    \c{"%.1f"}, \c{"%d ms"} or \c{"%5.2e"}. With an empty format, the default, each label has as many
    decimals as the ticks need.
*/
void QValueAxis::setLabelFormat(const QString &format) {
    if (format == m_labelFormat)
        return;
    m_labelFormat = format;
    markChanged();
    emit labelFormatChanged(format);
}

/*!
    Returns the format of the labels.
*/
QString QValueAxis::labelFormat() const { return m_labelFormat; }

/*!
    Widens the range to round numbers, with ticks at round numbers in between. The number of ticks may
    change.
*/
void QValueAxis::applyNiceNumbers() {
    if (!(m_max > m_min))
        return;
    const double range = niceNumber(m_max - m_min, false);
    const double step = niceNumber(range / std::max(1, m_tickCount - 1), true);
    const double min = std::floor(m_min / step) * step;
    const double max = std::ceil(m_max / step) * step;
    const int count = static_cast<int>(std::lround((max - min) / step)) + 1;
    setRange(min, max);
    setTickCount(count);
}

/*!
    \reimp
    Sets the range to \a min and \a max, converted to real numbers. An invalid variant leaves that end as
    it is.
*/
void QValueAxis::setRangeFromVariant(const QVariant &min, const QVariant &max) {
    bool okMin = false, okMax = false;
    const qreal lo = min.isValid() ? min.toReal(&okMin) : m_min;
    const qreal hi = max.isValid() ? max.toReal(&okMax) : m_max;
    if ((min.isValid() && !okMin) || (max.isValid() && !okMax))
        return;
    if (min.isValid() && !max.isValid())
        setMin(lo);
    else if (max.isValid() && !min.isValid())
        setMax(hi);
    else
        setRange(lo, hi);
}

/*!
    \class QLogValueAxis
    \brief The QLogValueAxis class is an axis with a logarithmic scale.

    \image charts/qlogvalueaxis.png A logarithmic axis with minor grid lines

    The axis maps positive values: equal ratios take equal room. It has a tick at each power of base()
    within its range, and minorTickCount() minor ticks between them.

    \code
    auto *axisY = new QLogValueAxis;
    axisY->setBase(10);
    axisY->setLabelFormat("%g");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    \endcode

    Values that are zero or negative are not drawn.

    \differences
    Labels are formatted by Nt itself, as for QValueAxis.
    \enddifferences

    \sa QValueAxis
*/

/*!
    Constructs a logarithmic axis with the given \a parent. Its range is 1 to 1 until it is set or taken
    from the data.
*/
QLogValueAxis::QLogValueAxis(QObject *parent) : QAbstractAxis(parent) {}

/*!
    Destroys the axis.
*/
QLogValueAxis::~QLogValueAxis() = default;

/*!
    \reimp
    Returns QAbstractAxis::AxisTypeLogValue.
*/
QAbstractAxis::AxisType QLogValueAxis::type() const { return AxisTypeLogValue; }

/*!
    Sets the minimum of the axis to \a min, which must be positive.
*/
void QLogValueAxis::setMin(qreal min) { setRange(min, std::max(min, m_max)); }

/*!
    Returns the minimum of the axis.
*/
qreal QLogValueAxis::min() const { return m_min; }

/*!
    Sets the maximum of the axis to \a max, which must be positive.
*/
void QLogValueAxis::setMax(qreal max) { setRange(std::min(m_min, max), max); }

/*!
    Returns the maximum of the axis.
*/
qreal QLogValueAxis::max() const { return m_max; }

/*!
    Sets the range of the axis to \a min and \a max. A range that is not positive, or whose minimum is
    larger than its maximum, is ignored.
*/
void QLogValueAxis::setRange(qreal min, qreal max) {
    if (!(min > 0.0) || !(min <= max) || !std::isfinite(max))
        return;
    ChartEngine::rangeSetByUser(this);
    const bool minChange = min != m_min, maxChange = max != m_max;
    if (!minChange && !maxChange)
        return;
    m_min = min;
    m_max = max;
    updateTickCount();
    markChanged();
    if (minChange)
        emit minChanged(min);
    if (maxChange)
        emit maxChanged(max);
    emit rangeChanged(min, max);
}

/*!
    Sets the format of the labels to \a format, as QValueAxis::setLabelFormat() does. The default format
    is empty, which writes each label as a number with up to six significant digits.
*/
void QLogValueAxis::setLabelFormat(const QString &format) {
    if (format == m_labelFormat)
        return;
    m_labelFormat = format;
    markChanged();
    emit labelFormatChanged(format);
}

/*!
    Returns the format of the labels.
*/
QString QLogValueAxis::labelFormat() const { return m_labelFormat; }

/*!
    Sets the base of the logarithm to \a base, which must be positive and not 1. The default is 10.
*/
void QLogValueAxis::setBase(qreal base) {
    if (!(base > 0.0) || base == 1.0 || base == m_base)
        return;
    m_base = base;
    updateTickCount();
    markChanged();
    emit baseChanged(base);
}

/*!
    Returns the base of the logarithm.
*/
qreal QLogValueAxis::base() const { return m_base; }

/*!
    Returns the number of ticks: the powers of base() within the range.
*/
int QLogValueAxis::tickCount() const { return m_tickCount; }

/*!
    Sets the number of minor ticks between two ticks to \a minorTickCount.
*/
void QLogValueAxis::setMinorTickCount(int minorTickCount) {
    if (minorTickCount < 0 || minorTickCount == m_minorTickCount)
        return;
    m_minorTickCount = minorTickCount;
    markChanged();
    emit minorTickCountChanged(minorTickCount);
}

/*!
    Returns the number of minor ticks between two ticks. The default is 0.
*/
int QLogValueAxis::minorTickCount() const { return m_minorTickCount; }

/*!
    \reimp
    Sets the range to \a min and \a max, converted to real numbers. An invalid variant leaves that end as
    it is.
*/
void QLogValueAxis::setRangeFromVariant(const QVariant &min, const QVariant &max) {
    bool okMin = false, okMax = false;
    const qreal lo = min.isValid() ? min.toReal(&okMin) : m_min;
    const qreal hi = max.isValid() ? max.toReal(&okMax) : m_max;
    if ((min.isValid() && !okMin) || (max.isValid() && !okMax))
        return;
    setRange(min.isValid() ? lo : std::min(m_min, hi), max.isValid() ? hi : std::max(m_max, lo));
}

void QLogValueAxis::updateTickCount() {
    const double lo = std::log(m_min) / std::log(m_base);
    const double hi = std::log(m_max) / std::log(m_base);
    const double first = std::ceil(std::min(lo, hi) - 1e-9), last = std::floor(std::max(lo, hi) + 1e-9);
    const int count = last >= first ? static_cast<int>(last - first) + 1 : 0;
    if (count == m_tickCount)
        return;
    m_tickCount = count;
    emit tickCountChanged(count);
}

#include "moc_qlogvalueaxis.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qvalueaxis.cpp"    // NOLINT(bugprone-suspicious-include): generated by ntmoc
