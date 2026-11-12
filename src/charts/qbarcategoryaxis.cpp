// MIT License - see LICENSE
#include <QtCharts/qbarcategoryaxis.h>

#include "qchartengine_p.h"

#include <cmath>

using QtChartsPrivate::ChartEngine;

/*!
    \class QBarCategoryAxis
    \brief The QBarCategoryAxis class is an axis of named categories, as bar charts have.

    \image charts/qbarseries.png A category axis under a bar series

    Each category takes the same room, and its label is drawn in the middle of it. A bar series puts the
    values of its sets at the categories in order: the first value of each set at the first category.

    \code
    auto *axisX = new QBarCategoryAxis;
    axisX->append({"Jan", "Feb", "Mar", "Apr"});
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);
    \endcode

    An axis without categories gets the numbers 1, 2, 3 and so on when a bar series is attached to it.
    setRange() shows only some of the categories.

    \differences
    None in behavior; the axis is a QObject, as all Nt axes are.
    \enddifferences

    \sa QAbstractBarSeries, QValueAxis
*/

/*!
    Constructs an axis without categories, with the given \a parent.
*/
QBarCategoryAxis::QBarCategoryAxis(QObject *parent) : QAbstractAxis(parent) {}

/*!
    Destroys the axis.
*/
QBarCategoryAxis::~QBarCategoryAxis() = default;

/*!
    \reimp
    Returns QAbstractAxis::AxisTypeBarCategory.
*/
QAbstractAxis::AxisType QBarCategoryAxis::type() const { return AxisTypeBarCategory; }

void QBarCategoryAxis::categoriesUpdated(int oldCount) {
    const int n = count();
    const qreal oldMin = m_min, oldMax = m_max;
    // A range set by the user is kept as far as the categories allow; otherwise all are shown.
    if (!ChartEngine::hasUserRange(this) || n == 0) {
        m_min = -0.5;
        m_max = n - 0.5;
    } else {
        m_min = std::min(m_min, n - 0.5);
        m_max = std::min(m_max, n - 0.5);
    }
    markChanged();
    emit categoriesChanged();
    if (n != oldCount)
        emit countChanged();
    if (m_min != oldMin)
        emit minChanged(min());
    if (m_max != oldMax)
        emit maxChanged(max());
    if (m_min != oldMin || m_max != oldMax)
        emit rangeChanged(min(), max());
}

void QBarCategoryAxis::setIndexRange(qreal min, qreal max) {
    if (min == m_min && max == m_max)
        return;
    const bool minChange = min != m_min, maxChange = max != m_max;
    m_min = min;
    m_max = max;
    markChanged();
    if (minChange)
        emit minChanged(this->min());
    if (maxChange)
        emit maxChanged(this->max());
    emit rangeChanged(this->min(), this->max());
}

/*!
    Appends \a categories. Categories that are empty or already on the axis are skipped.
*/
void QBarCategoryAxis::append(const QStringList &categories) {
    const int old = count();
    bool added = false;
    for (const QString &category : categories) {
        if (category.isEmpty() || m_categories.contains(category))
            continue;
        m_categories.append(category);
        added = true;
    }
    if (added)
        categoriesUpdated(old);
}

/*!
    \overload

    Appends \a category, unless it is empty or already on the axis.
*/
void QBarCategoryAxis::append(const QString &category) { append(QStringList{category}); }

/*!
    Removes \a category.
*/
void QBarCategoryAxis::remove(const QString &category) {
    const int old = count();
    if (m_categories.removeAll(category) > 0)
        categoriesUpdated(old);
}

/*!
    Inserts \a category at \a index, unless it is empty or already on the axis. An index out of range
    appends it.
*/
void QBarCategoryAxis::insert(int index, const QString &category) {
    if (category.isEmpty() || m_categories.contains(category))
        return;
    const int old = count();
    if (index < 0 || index > old)
        index = old;
    m_categories.insert(index, category);
    categoriesUpdated(old);
}

/*!
    Replaces \a oldCategory with \a newCategory, unless \a newCategory is empty or already on the axis.
*/
void QBarCategoryAxis::replace(const QString &oldCategory, const QString &newCategory) {
    const qsizetype at = m_categories.indexOf(oldCategory);
    if (at < 0 || newCategory.isEmpty() || m_categories.contains(newCategory))
        return;
    m_categories[at] = newCategory;
    categoriesUpdated(count());
}

/*!
    Removes all categories.
*/
void QBarCategoryAxis::clear() {
    if (m_categories.isEmpty())
        return;
    const int old = count();
    m_categories.clear();
    categoriesUpdated(old);
}

/*!
    Replaces the categories with \a categories.
*/
void QBarCategoryAxis::setCategories(const QStringList &categories) {
    const int old = count();
    m_categories.clear();
    for (const QString &category : categories)
        if (!category.isEmpty() && !m_categories.contains(category))
            m_categories.append(category);
    categoriesUpdated(old);
}

/*!
    Returns the categories.
*/
QStringList QBarCategoryAxis::categories() { return m_categories; }

/*!
    Returns the number of categories.
*/
int QBarCategoryAxis::count() const { return static_cast<int>(m_categories.size()); }

/*!
    Returns the category at \a index, or an empty string if there is none.
*/
QString QBarCategoryAxis::at(int index) const {
    return index >= 0 && index < count() ? m_categories.at(index) : QString();
}

/*!
    Makes \a minCategory the first category shown.
*/
void QBarCategoryAxis::setMin(const QString &minCategory) {
    const qsizetype i = m_categories.indexOf(minCategory);
    if (i < 0)
        return;
    ChartEngine::rangeSetByUser(this);
    setIndexRange(static_cast<qreal>(i) - 0.5, std::max(m_max, static_cast<qreal>(i) + 0.5));
}

/*!
    Returns the first category shown.
*/
QString QBarCategoryAxis::min() const { return at(static_cast<int>(std::lround(std::ceil(m_min)))); }

/*!
    Makes \a maxCategory the last category shown.
*/
void QBarCategoryAxis::setMax(const QString &maxCategory) {
    const qsizetype i = m_categories.indexOf(maxCategory);
    if (i < 0)
        return;
    ChartEngine::rangeSetByUser(this);
    setIndexRange(std::min(m_min, static_cast<qreal>(i) - 0.5), static_cast<qreal>(i) + 0.5);
}

/*!
    Returns the last category shown.
*/
QString QBarCategoryAxis::max() const { return at(static_cast<int>(std::lround(std::floor(m_max)))); }

/*!
    Shows the categories from \a minCategory to \a maxCategory. Nothing changes if either is not on the
    axis or \a maxCategory comes before \a minCategory.
*/
void QBarCategoryAxis::setRange(const QString &minCategory, const QString &maxCategory) {
    const qsizetype lo = m_categories.indexOf(minCategory), hi = m_categories.indexOf(maxCategory);
    if (lo < 0 || hi < lo)
        return;
    ChartEngine::rangeSetByUser(this);
    setIndexRange(static_cast<qreal>(lo) - 0.5, static_cast<qreal>(hi) + 0.5);
}

/*!
    \reimp
    Shows the categories named by \a min and \a max, converted to strings. An invalid variant leaves that
    end as it is.
*/
void QBarCategoryAxis::setRangeFromVariant(const QVariant &min, const QVariant &max) {
    if (min.isValid() && !max.isValid())
        setMin(min.toString());
    else if (max.isValid() && !min.isValid())
        setMax(max.toString());
    else
        setRange(min.toString(), max.toString());
}

#include "moc_qbarcategoryaxis.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
