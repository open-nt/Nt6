// MIT License - see LICENSE
#include <QtGui/qpagelayout.h>

#include <algorithm>
#include <cmath>

namespace {

// The factor that turns lengths in `from` into lengths in `to`; QPageLayout::Unit has the values of QPageSize::Unit.
double factor(QPageLayout::Unit from, QPageLayout::Unit to) {
    if (from == to)
        return 1.0;
    static const double kPoints[] = {72.0 / 25.4, 1.0, 72.0, 12.0, 0.375 * 72.0 / 25.4, 12.0 * 0.375 * 72.0 / 25.4};
    return kPoints[from] / kPoints[to];
}

QMarginsF scaled(const QMarginsF &m, double f) {
    return QMarginsF(m.left() * f, m.top() * f, m.right() * f, m.bottom() * f);
}

QRect rounded(const QRectF &r) {
    const int left = qRound(r.left()), top = qRound(r.top());
    return QRect(left, top, qRound(r.right()) - left, qRound(r.bottom()) - top);
}

} // namespace

/*!
    \class QPageLayout
    \inmodule NtGui
    \since 6.0
    \brief The QPageLayout class describes the size, orientation and margins of a page.

    A layout is a QPageSize, an orientation and margins in the layout's
    units. fullRect() is the whole page, turned for landscape, and
    paintRect() the part inside the margins, which is what a QPdfWriter
    paints on:

    \code
    const QPageLayout layout(QPageSize(QPageSize::A4), QPageLayout::Landscape,
                             QMarginsF(15, 15, 15, 20), QPageLayout::Millimeter);
    layout.fullRect();          // 0, 0, 297 x 210 mm
    layout.paintRect();         // 15, 15, 267 x 175 mm
    layout.paintRectPixels(300); // the same in pixels at 300 dpi
    \endcode

    Margins cannot be smaller than minimumMargins() or so large that nothing
    is left of the page: setMargins() refuses them, or clamps them with
    OutOfBoundsPolicy::Clamp.

    \differences
    None in the functions it has; there is no QDebug output.
    \enddifferences

    \sa QPageSize, QPagedPaintDevice, QPdfWriter
*/

/*!
    \enum QPageLayout::Unit

    This enum gives the units of the margins and rectangles of a layout.

    \value Millimeter Millimeters.
    \value Point PostScript points, 1/72 of an inch.
    \value Inch Inches.
    \value Pica Picas, 12 points.
    \value Didot Didot points, 0.375 mm.
    \value Cicero Ciceros, 12 Didot points.
*/

/*!
    \enum QPageLayout::Orientation

    This enum gives the orientation of the page.

    \value Portrait Taller than wide.
    \value Landscape Wider than tall: the page size is turned.
*/

/*!
    \enum QPageLayout::Mode

    This enum tells what part of the page is painted on.

    \value StandardMode The paint rectangle is inside the margins.
    \value FullPageMode The paint rectangle is the whole page; the margins only describe where content should go.
*/

/*!
    \enum QPageLayout::OutOfBoundsPolicy
    \since 6.8

    This enum tells what happens to margins outside the allowed range.

    \value Reject The margins are not changed, and the setter returns \c false.
    \value Clamp The margins are brought into the allowed range.
*/

/*!
    \fn QPageLayout::QPageLayout()

    Constructs a layout that is not valid.
*/

/*!
    Constructs a layout of \a pageSize in \a orientation, with \a margins and
    the minimum margins \a minMargins in \a units. Margins outside the
    allowed range are clamped.
*/
QPageLayout::QPageLayout(const QPageSize &pageSize, QPageLayout::Orientation orientation, const QMarginsF &margins,
                         QPageLayout::Unit units, const QMarginsF &minMargins)
    : m_pageSize(pageSize), m_orientation(orientation), m_units(units), m_minMargins(minMargins) {
    m_minMargins = clamped(minMargins);
    m_margins = clamped(margins);
}

/*!
    \fn void QPageLayout::swap(QPageLayout &other)

    Swaps this layout with \a other.
*/

/*!
    Returns \c true if this layout gives the same full and paint rectangles
    in points as \a other, whatever their units.
*/
bool QPageLayout::isEquivalentTo(const QPageLayout &other) const {
    return isValid() && other.isValid() && fullRectPoints() == other.fullRectPoints() &&
           paintRectPoints() == other.paintRectPoints();
}

/*!
    \fn bool QPageLayout::isValid() const

    Returns \c true if the layout has a valid page size.
*/

/*!
    \fn void QPageLayout::setMode(QPageLayout::Mode mode)

    Sets the mode of the layout to \a mode.
*/

/*!
    \fn QPageLayout::Mode QPageLayout::mode() const

    Returns the mode of the layout. The default is StandardMode.
*/

/*!
    Sets the page size to \a pageSize and the minimum margins to
    \a minMargins, clamping the margins to the new page.
*/
void QPageLayout::setPageSize(const QPageSize &pageSize, const QMarginsF &minMargins) {
    if (!pageSize.isValid())
        return;
    m_pageSize = pageSize;
    m_minMargins = clamped(minMargins);
    m_margins = clamped(m_margins);
}

/*!
    \fn QPageSize QPageLayout::pageSize() const

    Returns the page size of the layout.
*/

/*!
    Sets the orientation to \a orientation, clamping the margins to the turned
    page.
*/
void QPageLayout::setOrientation(QPageLayout::Orientation orientation) {
    m_orientation = orientation;
    m_minMargins = clamped(m_minMargins);
    m_margins = clamped(m_margins);
}

/*!
    \fn QPageLayout::Orientation QPageLayout::orientation() const

    Returns the orientation. The default is Portrait.
*/

/*!
    Sets the units of the layout to \a units, converting the margins to them.
*/
void QPageLayout::setUnits(QPageLayout::Unit units) {
    if (units == m_units)
        return;
    const double f = factor(m_units, units);
    const auto round2 = [](qreal v) { return std::round(v * 100.0) / 100.0; };
    const auto convert = [&](const QMarginsF &m) {
        return QMarginsF(round2(m.left() * f), round2(m.top() * f), round2(m.right() * f), round2(m.bottom() * f));
    };
    m_margins = convert(m_margins);
    m_minMargins = convert(m_minMargins);
    m_units = units;
}

/*!
    \fn QPageLayout::Unit QPageLayout::units() const

    Returns the units of the margins. The default is Point.
*/

/*!
    Sets the margins to \a margins, in the units of the layout, and returns
    \c true if they are within the minimum and maximum margins. Otherwise
    \a outOfBoundsPolicy decides: Reject keeps the old margins and returns
    \c false, Clamp brings them into range and returns \c true.
*/
bool QPageLayout::setMargins(const QMarginsF &margins, QPageLayout::OutOfBoundsPolicy outOfBoundsPolicy) {
    return accept(margins, outOfBoundsPolicy);
}

/*!
    Sets the left margin to \a leftMargin, following \a outOfBoundsPolicy as
    setMargins() does.
*/
bool QPageLayout::setLeftMargin(qreal leftMargin, QPageLayout::OutOfBoundsPolicy outOfBoundsPolicy) {
    QMarginsF m = m_margins;
    m.setLeft(leftMargin);
    return accept(m, outOfBoundsPolicy);
}

/*!
    Sets the right margin to \a rightMargin, following \a outOfBoundsPolicy
    as setMargins() does.
*/
bool QPageLayout::setRightMargin(qreal rightMargin, QPageLayout::OutOfBoundsPolicy outOfBoundsPolicy) {
    QMarginsF m = m_margins;
    m.setRight(rightMargin);
    return accept(m, outOfBoundsPolicy);
}

/*!
    Sets the top margin to \a topMargin, following \a outOfBoundsPolicy as
    setMargins() does.
*/
bool QPageLayout::setTopMargin(qreal topMargin, QPageLayout::OutOfBoundsPolicy outOfBoundsPolicy) {
    QMarginsF m = m_margins;
    m.setTop(topMargin);
    return accept(m, outOfBoundsPolicy);
}

/*!
    Sets the bottom margin to \a bottomMargin, following
    \a outOfBoundsPolicy as setMargins() does.
*/
bool QPageLayout::setBottomMargin(qreal bottomMargin, QPageLayout::OutOfBoundsPolicy outOfBoundsPolicy) {
    QMarginsF m = m_margins;
    m.setBottom(bottomMargin);
    return accept(m, outOfBoundsPolicy);
}

/*!
    \fn QMarginsF QPageLayout::margins() const

    Returns the margins in the units of the layout.
*/

/*!
    Returns the margins in \a units.
*/
QMarginsF QPageLayout::margins(QPageLayout::Unit units) const { return scaled(m_margins, factor(m_units, units)); }

/*!
    Returns the margins in whole points.
*/
QMargins QPageLayout::marginsPoints() const {
    const QMarginsF m = margins(Point);
    return QMargins(qRound(m.left()), qRound(m.top()), qRound(m.right()), qRound(m.bottom()));
}

/*!
    Returns the margins in pixels at \a resolution dots per inch.
*/
QMargins QPageLayout::marginsPixels(int resolution) const {
    const QMarginsF m = scaled(margins(Point), resolution / 72.0);
    return QMargins(qRound(m.left()), qRound(m.top()), qRound(m.right()), qRound(m.bottom()));
}

/*!
    Sets the minimum margins to \a minMargins, in the units of the layout,
    and clamps the margins to them.
*/
void QPageLayout::setMinimumMargins(const QMarginsF &minMargins) {
    m_minMargins = clamped(minMargins);
    m_margins = clamped(m_margins);
}

/*!
    \fn QMarginsF QPageLayout::minimumMargins() const

    Returns the minimum margins, in the units of the layout.
*/

/*!
    Returns the largest margins the page allows: on each side, the page
    less the minimum margin of the opposite side.
*/
QMarginsF QPageLayout::maximumMargins() const {
    const QSizeF s = orientedSize(m_units);
    return QMarginsF(s.width() - m_minMargins.right(), s.height() - m_minMargins.bottom(),
                     s.width() - m_minMargins.left(), s.height() - m_minMargins.top());
}

/*!
    Returns the whole page, turned for the orientation, in the units of the
    layout.
*/
QRectF QPageLayout::fullRect() const { return fullRect(m_units); }

/*!
    Returns the whole page, turned for the orientation, in \a units.
*/
QRectF QPageLayout::fullRect(QPageLayout::Unit units) const {
    return isValid() ? QRectF(QPointF(0, 0), orientedSize(units)) : QRectF();
}

/*!
    Returns the whole page in whole points.
*/
QRect QPageLayout::fullRectPoints() const {
    if (!isValid())
        return QRect();
    const QSize s = m_pageSize.sizePoints();
    return QRect(QPoint(0, 0), m_orientation == Landscape ? s.transposed() : s);
}

/*!
    Returns the whole page in pixels at \a resolution dots per inch.
*/
QRect QPageLayout::fullRectPixels(int resolution) const {
    if (!isValid())
        return QRect();
    const QSize s = m_pageSize.sizePixels(resolution);
    return QRect(QPoint(0, 0), m_orientation == Landscape ? s.transposed() : s);
}

/*!
    Returns the part of the page that is painted on, in the units of the
    layout: inside the margins, or the whole page in FullPageMode.
*/
QRectF QPageLayout::paintRect() const { return paintRect(m_units); }

/*!
    Returns the part of the page that is painted on, in \a units.
*/
QRectF QPageLayout::paintRect(QPageLayout::Unit units) const {
    const QRectF full = fullRect(units);
    if (!isValid() || m_mode == FullPageMode)
        return full;
    return full.marginsRemoved(margins(units));
}

/*!
    Returns the part of the page that is painted on, in whole points.
*/
QRect QPageLayout::paintRectPoints() const {
    if (!isValid() || m_mode == FullPageMode)
        return fullRectPoints();
    return rounded(paintRect(Point));
}

/*!
    Returns the part of the page that is painted on, in pixels at
    \a resolution dots per inch.
*/
QRect QPageLayout::paintRectPixels(int resolution) const {
    if (!isValid() || m_mode == FullPageMode)
        return fullRectPixels(resolution);
    const QRectF points = paintRect(Point);
    const double f = resolution / 72.0;
    return rounded(QRectF(points.x() * f, points.y() * f, points.width() * f, points.height() * f));
}

QSizeF QPageLayout::orientedSize(QPageLayout::Unit units) const {
    const QSizeF s = m_pageSize.size(static_cast<QPageSize::Unit>(units));
    return m_orientation == Landscape ? s.transposed() : s;
}

QMarginsF QPageLayout::clamped(const QMarginsF &margins) const {
    if (!isValid())
        return margins;
    const QMarginsF lo = m_minMargins;
    const QMarginsF hi = maximumMargins();
    const auto bound = [](qreal low, qreal v, qreal high) { return std::max(low, std::min(v, std::max(low, high))); };
    return QMarginsF(bound(lo.left(), margins.left(), hi.left()), bound(lo.top(), margins.top(), hi.top()),
                     bound(lo.right(), margins.right(), hi.right()), bound(lo.bottom(), margins.bottom(), hi.bottom()));
}

bool QPageLayout::accept(const QMarginsF &margins, QPageLayout::OutOfBoundsPolicy policy) {
    const QMarginsF fixed = clamped(margins);
    if (fixed != margins && policy == OutOfBoundsPolicy::Reject)
        return false;
    m_margins = fixed;
    return true;
}
