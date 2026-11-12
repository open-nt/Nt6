// MIT License - see LICENSE
#include <QtCharts/qabstractbarseries.h>
#include <QtCharts/qbarseries.h>
#include <QtCharts/qbarset.h>
#include <QtCharts/qhorizontalbarseries.h>
#include <QtCharts/qhorizontalpercentbarseries.h>
#include <QtCharts/qhorizontalstackedbarseries.h>
#include <QtCharts/qpercentbarseries.h>
#include <QtCharts/qstackedbarseries.h>

#include <QtCore/qalgorithms.h>

#include "qchartengine_p.h"

using QtChartsPrivate::ChartEngine;

/*!
    \class QAbstractBarSeries
    \brief The QAbstractBarSeries class is the base class of the bar series.

    \image charts/qbarseries.png A bar series with three sets

    A bar series holds QBarSet objects, and draws value \e i of each set at category \e i. The subclasses
    differ in how they put the sets together: side by side, stacked, or as percentages, with vertical or
    horizontal bars. The series owns its sets.

    \snippet charts/dashboard.cpp bars

    With setLabelsVisible(), each bar has a label with its value, written with labelsFormat().

    \differences
    Labels are drawn upright: labelsAngle() is kept but not used.
    \enddifferences

    \sa QBarSet, QBarCategoryAxis
*/

/*!
    \enum QAbstractBarSeries::LabelsPosition

    Where the value labels are drawn.

    \value LabelsCenter In the middle of the bar.
    \value LabelsInsideEnd Inside the bar, at its end.
    \value LabelsInsideBase Inside the bar, at its base.
    \value LabelsOutsideEnd Beyond the end of the bar.
*/

/*!
    Constructs an empty bar series with the given \a parent.
*/
QAbstractBarSeries::QAbstractBarSeries(QObject *parent)
    : QAbstractSeries(parent), m_labelsFormat(QStringLiteral("@value")) {}

/*!
    Destroys the series and its sets.
*/
QAbstractBarSeries::~QAbstractBarSeries() {
    const QList<QBarSet *> sets = m_sets;
    m_sets.clear();
    for (QBarSet *set : sets) {
        ChartEngine::releaseBarSet(set);
        delete set;
    }
}

/*!
    Sets the width of the bars to \a width, as a share of the room of a category: from 0 to 1. The sets of
    a QBarSeries share it. The default is 0.5.
*/
void QAbstractBarSeries::setBarWidth(qreal width) {
    width = std::clamp(width, 0.0, 1.0);
    if (width == m_barWidth)
        return;
    m_barWidth = width;
    markChanged();
}

/*!
    Returns the width of the bars, as a share of the room of a category.
*/
qreal QAbstractBarSeries::barWidth() const { return m_barWidth; }

bool QAbstractBarSeries::adopt(QBarSet *set) {
    if (!set || m_sets.contains(set) || ChartEngine::barSetSeries(set))
        return false;
    ChartEngine::setBarSetSeries(set, this);
    set->setParent(this);
    return true;
}

/*!
    Appends \a set to the series, which takes ownership of it. Returns \c false if \a set is \c nullptr or
    already in a series.
*/
bool QAbstractBarSeries::append(QBarSet *set) { return insert(count(), set); }

/*!
    Removes \a set from the series and deletes it. Returns \c true on success.
*/
bool QAbstractBarSeries::remove(QBarSet *set) {
    if (!take(set))
        return false;
    delete set;
    return true;
}

/*!
    Removes \a set from the series without deleting it; the caller owns it. Returns \c true on success.
*/
bool QAbstractBarSeries::take(QBarSet *set) {
    if (!set || !m_sets.contains(set))
        return false;
    m_sets.removeAll(set);
    ChartEngine::releaseBarSet(set);
    set->setParent(nullptr);
    markChanged();
    emit barsetsRemoved(QList<QBarSet *>{set});
    emit countChanged();
    return true;
}

/*!
    \overload

    Appends \a sets to the series. Returns \c false, and appends none of them, if one of them is
    \c nullptr, appears twice or is already in a series.
*/
bool QAbstractBarSeries::append(const QList<QBarSet *> &sets) {
    for (qsizetype i = 0; i < sets.size(); ++i) {
        QBarSet *set = sets.at(i);
        if (!set || m_sets.contains(set) || ChartEngine::barSetSeries(set) || sets.indexOf(set) != i)
            return false;
    }
    if (sets.isEmpty())
        return true;
    for (QBarSet *set : sets) {
        adopt(set);
        m_sets.append(set);
    }
    markChanged();
    emit barsetsAdded(sets);
    emit countChanged();
    return true;
}

/*!
    Inserts \a set at \a index; an index out of range appends it. Returns \c false if \a set is
    \c nullptr or already in a series.
*/
bool QAbstractBarSeries::insert(int index, QBarSet *set) {
    if (!adopt(set))
        return false;
    if (index < 0 || index > count())
        index = count();
    m_sets.insert(index, set);
    markChanged();
    emit barsetsAdded(QList<QBarSet *>{set});
    emit countChanged();
    return true;
}

/*!
    Returns the number of sets.
*/
int QAbstractBarSeries::count() const { return static_cast<int>(m_sets.size()); }

/*!
    Returns the sets.
*/
QList<QBarSet *> QAbstractBarSeries::barSets() const { return m_sets; }

/*!
    Removes and deletes all sets.
*/
void QAbstractBarSeries::clear() {
    if (m_sets.isEmpty())
        return;
    const QList<QBarSet *> sets = m_sets;
    m_sets.clear();
    for (QBarSet *set : sets)
        ChartEngine::releaseBarSet(set);
    markChanged();
    emit barsetsRemoved(sets);
    emit countChanged();
    qDeleteAll(sets);
}

/*!
    Draws a label with its value on each bar if \a visible is \c true.
*/
void QAbstractBarSeries::setLabelsVisible(bool visible) {
    if (visible == m_labelsVisible)
        return;
    m_labelsVisible = visible;
    markChanged();
    emit labelsVisibleChanged();
}

/*!
    Returns \c true if the value labels are drawn; otherwise returns \c false.
*/
bool QAbstractBarSeries::isLabelsVisible() const { return m_labelsVisible; }

/*!
    Sets the format of the value labels to \a format. \c @value in it is replaced by the value of the bar,
    written with labelsPrecision() significant digits. The default is \c{"@value"}.
*/
void QAbstractBarSeries::setLabelsFormat(const QString &format) {
    if (format == m_labelsFormat)
        return;
    m_labelsFormat = format;
    markChanged();
    emit labelsFormatChanged(format);
}

/*!
    Returns the format of the value labels.
*/
QString QAbstractBarSeries::labelsFormat() const { return m_labelsFormat; }

/*!
    Sets the angle of the value labels to \a angle degrees. Nt keeps the value and draws the labels
    upright.
*/
void QAbstractBarSeries::setLabelsAngle(qreal angle) {
    if (angle == m_labelsAngle)
        return;
    m_labelsAngle = angle;
    emit labelsAngleChanged(angle);
}

/*!
    Returns the angle of the value labels, in degrees.
*/
qreal QAbstractBarSeries::labelsAngle() const { return m_labelsAngle; }

/*!
    Sets where the value labels are drawn to \a position.
*/
void QAbstractBarSeries::setLabelsPosition(QAbstractBarSeries::LabelsPosition position) {
    if (position == m_labelsPosition)
        return;
    m_labelsPosition = position;
    markChanged();
    emit labelsPositionChanged(position);
}

/*!
    Returns where the value labels are drawn. The default is LabelsCenter.
*/
QAbstractBarSeries::LabelsPosition QAbstractBarSeries::labelsPosition() const { return m_labelsPosition; }

/*!
    Sets the number of significant digits of the value labels to \a precision. The default is 6.
*/
void QAbstractBarSeries::setLabelsPrecision(int precision) {
    if (precision < 1 || precision == m_labelsPrecision)
        return;
    m_labelsPrecision = precision;
    markChanged();
    emit labelsPrecisionChanged(precision);
}

/*!
    Returns the number of significant digits of the value labels.
*/
int QAbstractBarSeries::labelsPrecision() const { return m_labelsPrecision; }

/*!
    \class QBarSeries
    \brief The QBarSeries class draws its sets as vertical bars side by side.

    \image charts/qbarseries.png A bar series with three sets

    The bars of a category are grouped: one bar per set, in the order of the sets, sharing barWidth().

    \differences
    See QAbstractBarSeries.
    \enddifferences

    \sa QAbstractBarSeries, QBarSet
*/

/*!
    Constructs an empty series with the given \a parent.
*/
QBarSeries::QBarSeries(QObject *parent) : QAbstractBarSeries(parent) {}

/*!
    Destroys the series and its sets.
*/
QBarSeries::~QBarSeries() = default;

/*!
    \reimp
    Returns QAbstractSeries::SeriesTypeBar.
*/
QAbstractSeries::SeriesType QBarSeries::type() const { return SeriesTypeBar; }

/*!
    \class QStackedBarSeries
    \brief The QStackedBarSeries class stacks its sets in vertical bars.

    \image charts/qstackedbarseries.png A stacked bar series with three sets

    The values of a category are stacked on top of each other: positive values upwards from zero, negative ones downwards.

    \differences
    See QAbstractBarSeries.
    \enddifferences

    \sa QAbstractBarSeries, QBarSet
*/

/*!
    Constructs an empty series with the given \a parent.
*/
QStackedBarSeries::QStackedBarSeries(QObject *parent) : QAbstractBarSeries(parent) {}

/*!
    Destroys the series and its sets.
*/
QStackedBarSeries::~QStackedBarSeries() = default;

/*!
    \reimp
    Returns QAbstractSeries::SeriesTypeStackedBar.
*/
QAbstractSeries::SeriesType QStackedBarSeries::type() const { return SeriesTypeStackedBar; }

/*!
    \class QPercentBarSeries
    \brief The QPercentBarSeries class draws its sets as shares of each category.

    \image charts/qpercentbarseries.png A percent bar series with three sets

    Each category is a bar from 0 to 100, divided among the sets in proportion to their values. Negative values count as zero.

    \differences
    See QAbstractBarSeries.
    \enddifferences

    \sa QAbstractBarSeries, QBarSet
*/

/*!
    Constructs an empty series with the given \a parent.
*/
QPercentBarSeries::QPercentBarSeries(QObject *parent) : QAbstractBarSeries(parent) {}

/*!
    Destroys the series and its sets.
*/
QPercentBarSeries::~QPercentBarSeries() = default;

/*!
    \reimp
    Returns QAbstractSeries::SeriesTypePercentBar.
*/
QAbstractSeries::SeriesType QPercentBarSeries::type() const { return SeriesTypePercentBar; }

/*!
    \class QHorizontalBarSeries
    \brief The QHorizontalBarSeries class draws its sets as horizontal bars side by side.

    \image charts/qhorizontalbarseries.png A horizontal bar series with three sets

    The categories run from the bottom up, along the vertical axis; the values along the horizontal one.

    \differences
    See QAbstractBarSeries.
    \enddifferences

    \sa QAbstractBarSeries, QBarSet
*/

/*!
    Constructs an empty series with the given \a parent.
*/
QHorizontalBarSeries::QHorizontalBarSeries(QObject *parent) : QAbstractBarSeries(parent) {}

/*!
    Destroys the series and its sets.
*/
QHorizontalBarSeries::~QHorizontalBarSeries() = default;

/*!
    \reimp
    Returns QAbstractSeries::SeriesTypeHorizontalBar.
*/
QAbstractSeries::SeriesType QHorizontalBarSeries::type() const { return SeriesTypeHorizontalBar; }

/*!
    \class QHorizontalStackedBarSeries
    \brief The QHorizontalStackedBarSeries class stacks its sets in horizontal bars.

    Positive values are stacked to the right of zero, negative ones to the left.

    \differences
    See QAbstractBarSeries.
    \enddifferences

    \sa QAbstractBarSeries, QBarSet
*/

/*!
    Constructs an empty series with the given \a parent.
*/
QHorizontalStackedBarSeries::QHorizontalStackedBarSeries(QObject *parent) : QAbstractBarSeries(parent) {}

/*!
    Destroys the series and its sets.
*/
QHorizontalStackedBarSeries::~QHorizontalStackedBarSeries() = default;

/*!
    \reimp
    Returns QAbstractSeries::SeriesTypeHorizontalStackedBar.
*/
QAbstractSeries::SeriesType QHorizontalStackedBarSeries::type() const { return SeriesTypeHorizontalStackedBar; }

/*!
    \class QHorizontalPercentBarSeries
    \brief The QHorizontalPercentBarSeries class draws its sets as shares of each category, horizontally.

    Each category is a horizontal bar from 0 to 100, divided among the sets in proportion to their values.

    \differences
    See QAbstractBarSeries.
    \enddifferences

    \sa QAbstractBarSeries, QBarSet
*/

/*!
    Constructs an empty series with the given \a parent.
*/
QHorizontalPercentBarSeries::QHorizontalPercentBarSeries(QObject *parent) : QAbstractBarSeries(parent) {}

/*!
    Destroys the series and its sets.
*/
QHorizontalPercentBarSeries::~QHorizontalPercentBarSeries() = default;

/*!
    \reimp
    Returns QAbstractSeries::SeriesTypeHorizontalPercentBar.
*/
QAbstractSeries::SeriesType QHorizontalPercentBarSeries::type() const { return SeriesTypeHorizontalPercentBar; }

#include "moc_qabstractbarseries.cpp"          // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qbarseries.cpp"                  // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qhorizontalbarseries.cpp"        // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qhorizontalpercentbarseries.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qhorizontalstackedbarseries.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qpercentbarseries.cpp"           // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qstackedbarseries.cpp"           // NOLINT(bugprone-suspicious-include): generated by ntmoc
