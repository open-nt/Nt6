// MIT License - see LICENSE
#include <QtCharts/qbarset.h>

#include "qchartengine_p.h"

using QtChartsPrivate::ChartEngine;

/*!
    \class QBarSet
    \brief The QBarSet class is one set of values of a bar series, drawn as bars of one color.

    Value \e i of the set is drawn at category \e i of the series. A bar series with several sets draws
    them side by side (QBarSeries), stacked (QStackedBarSeries) or as shares of the total
    (QPercentBarSeries), and the legend shows one entry per set, with its label().

    \snippet charts/dashboard.cpp bars

    \differences
    The set is not drawn with a gradient or a texture: only the color of its brush is used.
    \enddifferences

    \sa QAbstractBarSeries, QBarCategoryAxis
*/

/*!
    Constructs an empty set called \a label, with the given \a parent.
*/
QBarSet::QBarSet(const QString &label, QObject *parent) : QObject(parent), m_label(label) {}

/*!
    Destroys the set and removes it from its series.
*/
QBarSet::~QBarSet() { ChartEngine::barSetDestroyed(this); }

void QBarSet::markChanged() { ChartEngine::changed(this); }

/*!
    Sets the label of the set to \a label. The legend shows it.
*/
void QBarSet::setLabel(const QString &label) {
    if (label == m_label)
        return;
    m_label = label;
    markChanged();
    emit labelChanged();
}

/*!
    Returns the label of the set.
*/
QString QBarSet::label() const { return m_label; }

/*!
    Appends \a value.
*/
void QBarSet::append(const qreal value) { append(QList<qreal>{value}); }

/*!
    \overload

    Appends \a values.
*/
void QBarSet::append(const QList<qreal> &values) {
    if (values.isEmpty())
        return;
    const int index = count();
    m_values.append(values);
    markChanged();
    emit valuesAdded(index, static_cast<int>(values.size()));
}

/*!
    Appends \a value and returns a reference to the set.
*/
QBarSet &QBarSet::operator<<(const qreal &value) {
    append(value);
    return *this;
}

/*!
    Inserts \a value at \a index. An index out of range appends it.
*/
void QBarSet::insert(const int index, const qreal value) {
    const int at = index < 0 || index > count() ? count() : index;
    m_values.insert(at, value);
    markChanged();
    emit valuesAdded(at, 1);
}

/*!
    Removes \a count values, starting at \a index.
*/
void QBarSet::remove(const int index, const int count) {
    if (index < 0 || index >= this->count() || count <= 0)
        return;
    const int n = std::min(count, this->count() - index);
    m_values.remove(index, n);
    markChanged();
    emit valuesRemoved(index, n);
}

/*!
    Replaces the value at \a index with \a value.
*/
void QBarSet::replace(const int index, const qreal value) {
    if (index < 0 || index >= count())
        return;
    m_values[index] = value;
    markChanged();
    emit valueChanged(index);
}

/*!
    Returns the value at \a index, or 0 if there is none.
*/
qreal QBarSet::at(const int index) const { return index >= 0 && index < count() ? m_values.at(index) : 0.0; }

/*!
    Returns the value at \a index, or 0 if there is none.
*/
qreal QBarSet::operator[](const int index) const { return at(index); }

/*!
    Returns the number of values.
*/
int QBarSet::count() const { return static_cast<int>(m_values.size()); }

/*!
    Returns the sum of the values.
*/
qreal QBarSet::sum() const {
    qreal total = 0.0;
    for (const qreal v : m_values)
        total += v;
    return total;
}

/*!
    Sets the pen of the borders of the bars to \a pen.
*/
void QBarSet::setPen(const QPen &pen) {
    const QColor before = borderColor();
    m_pen = pen;
    m_penSet = true;
    markChanged();
    emit penChanged();
    if (borderColor() != before)
        emit borderColorChanged(borderColor());
}

/*!
    Returns the pen of the borders of the bars.
*/
QPen QBarSet::pen() const { return m_penSet ? m_pen : QPen(QBrush(ChartEngine::barSetColor(this).darker(115)), 1.0); }

/*!
    Sets the brush of the bars to \a brush.
*/
void QBarSet::setBrush(const QBrush &brush) {
    const QColor before = color();
    m_brush = brush;
    m_brushSet = true;
    markChanged();
    emit brushChanged();
    if (color() != before)
        emit colorChanged(color());
}

/*!
    Returns the brush of the bars. Unless one was set, it has the color the theme gives the set.
*/
QBrush QBarSet::brush() const { return m_brushSet ? m_brush : QBrush(ChartEngine::barSetColor(this)); }

/*!
    Sets the brush of the value labels to \a brush.
*/
void QBarSet::setLabelBrush(const QBrush &brush) {
    const QColor before = labelColor();
    m_labelBrush = brush;
    m_labelBrushSet = true;
    markChanged();
    emit labelBrushChanged();
    if (labelColor() != before)
        emit labelColorChanged(labelColor());
}

/*!
    Returns the brush of the value labels.
*/
QBrush QBarSet::labelBrush() const {
    if (m_labelBrushSet)
        return m_labelBrush;
    QChart *chart = m_series ? m_series->chart() : nullptr;
    return QBrush(ChartEngine::themeOf(chart).labels);
}

/*!
    Sets the font of the value labels to \a font.
*/
void QBarSet::setLabelFont(const QFont &font) {
    m_labelFont = font;
    m_labelFontSet = true;
    markChanged();
    emit labelFontChanged();
}

/*!
    Returns the font of the value labels.
*/
QFont QBarSet::labelFont() const { return m_labelFont; }

/*!
    Returns the fill color of the bars.
*/
QColor QBarSet::color() { return brush().color(); }

/*!
    Sets the fill color of the bars to \a color.
*/
void QBarSet::setColor(QColor color) {
    QBrush b = brush();
    b.setColor(color);
    b.setStyle(Qt::SolidPattern);
    setBrush(b);
}

/*!
    Returns the color of the borders of the bars.
*/
QColor QBarSet::borderColor() { return pen().color(); }

/*!
    Sets the color of the borders of the bars to \a color.
*/
void QBarSet::setBorderColor(QColor color) {
    QPen p = pen();
    p.setColor(color);
    setPen(p);
}

/*!
    Returns the color of the value labels.
*/
QColor QBarSet::labelColor() { return labelBrush().color(); }

/*!
    Sets the color of the value labels to \a color.
*/
void QBarSet::setLabelColor(QColor color) {
    QBrush b = labelBrush();
    b.setColor(color);
    setLabelBrush(b);
}

#include "moc_qbarset.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
