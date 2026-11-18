// MIT License - see LICENSE
//
// QGeoRectangle. Longitudes are handled as eastward arcs from the top-left corner, so rectangles that
// cross the 180th meridian (top-left longitude greater than bottom-right) work everywhere.
#include "qgeorectangle.h"
#include "qpositioningmath_p.h"

#include <QtCore/qnumeric.h>

#include <algorithm>
#include <cmath>

using QtPositioningPrivate::eastward;
using QtPositioningPrivate::normalizeLongitude;

/*!
    \class QGeoRectangle
    \inmodule NtPositioning
    \brief The QGeoRectangle class defines a rectangular geographic area.

    The rectangle is given by its top-left (north-west) and bottom-right (south-east) corners. Its
    sides follow lines of latitude and longitude. A rectangle whose top-left longitude is greater
    than its bottom-right longitude crosses the 180th meridian.

    \code
    QGeoRectangle area(QGeoCoordinate(51.20, -1.87), QGeoCoordinate(51.16, -1.80));
    area.contains(QGeoCoordinate(51.1789, -1.8262));   // true
    area.extendRectangle(QGeoCoordinate(51.25, -1.90));
    \endcode

    \differences
    There is no QDataStream operator. The constructor from a list of coordinates grows the
    rectangle one coordinate at a time, so for points spread over more than half the globe the
    result can depend on their order.
    \enddifferences

    \sa QGeoShape, QGeoCircle
*/

/*!
    Constructs an invalid rectangle.
*/
QGeoRectangle::QGeoRectangle() : QGeoShape(RectangleType) {}

/*!
    Constructs a rectangle centred on \a center, \a degreesWidth degrees of longitude wide and
    \a degreesHeight degrees of latitude high. The rectangle is invalid if \a center is.
*/
QGeoRectangle::QGeoRectangle(const QGeoCoordinate &center, double degreesWidth, double degreesHeight)
    : QGeoShape(RectangleType) {
    if (!center.isValid())
        return;
    m_a = QGeoCoordinate(center.latitude(), center.longitude());
    m_b = m_a;
    setWidth(degreesWidth);
    setHeight(degreesHeight);
}

/*!
    Constructs a rectangle with the corners \a topLeft and \a bottomRight.
*/
QGeoRectangle::QGeoRectangle(const QGeoCoordinate &topLeft, const QGeoCoordinate &bottomRight)
    : QGeoShape(RectangleType) {
    m_a = topLeft;
    m_b = bottomRight;
}

/*!
    Constructs the smallest rectangle that contains all \a coordinates, or an invalid rectangle if
    the list holds no valid coordinate.
*/
QGeoRectangle::QGeoRectangle(const QList<QGeoCoordinate> &coordinates) : QGeoShape(RectangleType) {
    for (const QGeoCoordinate &c : coordinates) {
        if (!c.isValid())
            continue;
        if (!isValid()) {
            m_a = QGeoCoordinate(c.latitude(), c.longitude());
            m_b = m_a;
        } else {
            extendRectangle(c);
        }
    }
}

/*!
    Constructs a copy of \a other if it is a rectangle; otherwise constructs an invalid rectangle.
*/
QGeoRectangle::QGeoRectangle(const QGeoShape &other) : QGeoShape(other) {
    if (other.type() != RectangleType)
        *this = QGeoRectangle();
}

/*!
    \fn void QGeoRectangle::setTopLeft(const QGeoCoordinate &topLeft)
    Sets the top-left corner to \a topLeft.
*/

/*!
    \fn QGeoCoordinate QGeoRectangle::topLeft() const
    Returns the top-left (north-west) corner.
*/

/*!
    Sets the top-right corner to \a topRight: the top latitude and the right longitude.
*/
void QGeoRectangle::setTopRight(const QGeoCoordinate &topRight) {
    m_a.setLatitude(topRight.latitude());
    m_b.setLongitude(topRight.longitude());
}

/*!
    Returns the top-right (north-east) corner.
*/
QGeoCoordinate QGeoRectangle::topRight() const {
    return isValid() ? QGeoCoordinate(m_a.latitude(), m_b.longitude()) : QGeoCoordinate();
}

/*!
    Sets the bottom-left corner to \a bottomLeft: the bottom latitude and the left longitude.
*/
void QGeoRectangle::setBottomLeft(const QGeoCoordinate &bottomLeft) {
    m_b.setLatitude(bottomLeft.latitude());
    m_a.setLongitude(bottomLeft.longitude());
}

/*!
    Returns the bottom-left (south-west) corner.
*/
QGeoCoordinate QGeoRectangle::bottomLeft() const {
    return isValid() ? QGeoCoordinate(m_b.latitude(), m_a.longitude()) : QGeoCoordinate();
}

/*!
    \fn void QGeoRectangle::setBottomRight(const QGeoCoordinate &bottomRight)
    Sets the bottom-right corner to \a bottomRight.
*/

/*!
    \fn QGeoCoordinate QGeoRectangle::bottomRight() const
    Returns the bottom-right (south-east) corner.
*/

/*!
    Moves the rectangle so that its centre is \a center, keeping its width and height. Latitudes
    beyond the poles are clamped.
*/
void QGeoRectangle::setCenter(const QGeoCoordinate &center) {
    if (!isValid() || !center.isValid())
        return;
    const double halfWidth = width() / 2.0;
    const double halfHeight = height() / 2.0;
    m_a = QGeoCoordinate(std::min(90.0, center.latitude() + halfHeight),
                         halfWidth >= 180.0 ? -180.0 : normalizeLongitude(center.longitude() - halfWidth));
    m_b = QGeoCoordinate(std::max(-90.0, center.latitude() - halfHeight),
                         halfWidth >= 180.0 ? 180.0 : normalizeLongitude(center.longitude() + halfWidth));
}

/*!
    Returns the centre of the rectangle, or an invalid coordinate if the rectangle is invalid.
*/
QGeoCoordinate QGeoRectangle::center() const {
    if (!isValid())
        return QGeoCoordinate();
    return QGeoCoordinate((m_a.latitude() + m_b.latitude()) / 2.0, normalizeLongitude(m_a.longitude() + width() / 2.0));
}

/*!
    Sets the width to \a degreesWidth degrees of longitude, keeping the centre. Widths are clamped
    to [0, 360].
*/
void QGeoRectangle::setWidth(double degreesWidth) {
    if (!isValid() || std::isnan(degreesWidth))
        return;
    const QGeoCoordinate c = center();
    const double half = std::clamp(degreesWidth, 0.0, 360.0) / 2.0;
    m_a.setLongitude(half >= 180.0 ? -180.0 : normalizeLongitude(c.longitude() - half));
    m_b.setLongitude(half >= 180.0 ? 180.0 : normalizeLongitude(c.longitude() + half));
}

/*!
    Returns the width in degrees of longitude, or NaN if the rectangle is invalid.
*/
double QGeoRectangle::width() const {
    if (!isValid())
        return qQNaN();
    if (m_a.longitude() == -180.0 && m_b.longitude() == 180.0)
        return 360.0;
    return eastward(m_a.longitude(), m_b.longitude());
}

/*!
    Sets the height to \a degreesHeight degrees of latitude, keeping the centre. The poles clamp
    the result.
*/
void QGeoRectangle::setHeight(double degreesHeight) {
    if (!isValid() || std::isnan(degreesHeight))
        return;
    const double middle = (m_a.latitude() + m_b.latitude()) / 2.0;
    const double half = std::clamp(degreesHeight, 0.0, 180.0) / 2.0;
    m_a.setLatitude(std::min(90.0, middle + half));
    m_b.setLatitude(std::max(-90.0, middle - half));
}

/*!
    Returns the height in degrees of latitude, or NaN if the rectangle is invalid.
*/
double QGeoRectangle::height() const { return isValid() ? m_a.latitude() - m_b.latitude() : qQNaN(); }

/*!
    Returns \c true if \a rectangle lies entirely inside this rectangle; otherwise returns \c false.
*/
bool QGeoRectangle::contains(const QGeoRectangle &rectangle) const {
    if (!isValid() || !rectangle.isValid())
        return false;
    if (rectangle.m_a.latitude() > m_a.latitude() || rectangle.m_b.latitude() < m_b.latitude())
        return false;
    const double w = width();
    if (w >= 360.0)
        return true;
    return eastward(m_a.longitude(), rectangle.m_a.longitude()) + rectangle.width() <= w;
}

/*!
    Returns \c true if \a rectangle and this rectangle overlap or touch; otherwise returns \c false.
*/
bool QGeoRectangle::intersects(const QGeoRectangle &rectangle) const {
    if (!isValid() || !rectangle.isValid())
        return false;
    if (rectangle.m_b.latitude() > m_a.latitude() || rectangle.m_a.latitude() < m_b.latitude())
        return false;
    const double d = eastward(m_a.longitude(), rectangle.m_a.longitude());
    return d <= width() || d + rectangle.width() >= 360.0;
}

/*!
    Moves the rectangle \a degreesLatitude degrees north and \a degreesLongitude degrees east. A
    move in latitude that would take an edge beyond a pole stops at the pole.
*/
void QGeoRectangle::translate(double degreesLatitude, double degreesLongitude) {
    if (!isValid())
        return;
    double dLat = degreesLatitude;
    dLat = std::min(dLat, 90.0 - m_a.latitude());
    dLat = std::max(dLat, -90.0 - m_b.latitude());
    const bool full = width() >= 360.0;
    m_a = QGeoCoordinate(m_a.latitude() + dLat, full ? -180.0 : normalizeLongitude(m_a.longitude() + degreesLongitude));
    m_b = QGeoCoordinate(m_b.latitude() + dLat, full ? 180.0 : normalizeLongitude(m_b.longitude() + degreesLongitude));
}

/*!
    Returns a copy of the rectangle moved \a degreesLatitude degrees north and \a degreesLongitude
    degrees east.

    \sa translate()
*/
QGeoRectangle QGeoRectangle::translated(double degreesLatitude, double degreesLongitude) const {
    QGeoRectangle result(*this);
    result.translate(degreesLatitude, degreesLongitude);
    return result;
}

/*!
    Grows the rectangle by the least amount that makes it contain \a coordinate. Nothing changes
    if either is invalid.
*/
void QGeoRectangle::extendRectangle(const QGeoCoordinate &coordinate) {
    if (!isValid() || !coordinate.isValid() || contains(coordinate))
        return;
    m_a.setLatitude(std::max(m_a.latitude(), coordinate.latitude()));
    m_b.setLatitude(std::min(m_b.latitude(), coordinate.latitude()));
    if (QGeoShape::contains(QGeoCoordinate(m_b.latitude(), coordinate.longitude())))
        return;
    const double east = eastward(m_b.longitude(), coordinate.longitude());
    const double west = eastward(coordinate.longitude(), m_a.longitude());
    if (east <= west)
        m_b.setLongitude(coordinate.longitude());
    else
        m_a.setLongitude(coordinate.longitude());
}

/*!
    Returns the smallest rectangle that contains this rectangle and \a rectangle. If one of them is
    invalid, the other is returned.
*/
QGeoRectangle QGeoRectangle::united(const QGeoRectangle &rectangle) const {
    if (!rectangle.isValid())
        return *this;
    if (!isValid())
        return rectangle;
    const double top = std::max(m_a.latitude(), rectangle.m_a.latitude());
    const double bottom = std::min(m_b.latitude(), rectangle.m_b.latitude());
    // Of the arcs that start at either west edge and cover both, take the shorter.
    const double fromThis = std::max(width(), eastward(m_a.longitude(), rectangle.m_a.longitude()) + rectangle.width());
    const double fromOther =
        std::max(rectangle.width(), eastward(rectangle.m_a.longitude(), m_a.longitude()) + width());
    const double start = fromThis <= fromOther ? m_a.longitude() : rectangle.m_a.longitude();
    const double span = std::min(fromThis, fromOther);
    if (span >= 360.0)
        return QGeoRectangle(QGeoCoordinate(top, -180.0), QGeoCoordinate(bottom, 180.0));
    return QGeoRectangle(QGeoCoordinate(top, start), QGeoCoordinate(bottom, normalizeLongitude(start + span)));
}

/*!
    \fn QGeoRectangle QGeoRectangle::operator|(const QGeoRectangle &rectangle) const
    Returns the union of this rectangle and \a rectangle.

    \sa united()
*/

/*!
    Makes this rectangle the union of itself and \a rectangle, and returns a reference to it.

    \sa united()
*/
QGeoRectangle &QGeoRectangle::operator|=(const QGeoRectangle &rectangle) {
    *this = united(rectangle);
    return *this;
}

/*!
    Returns the rectangle as text: \c{QGeoRectangle({top, left}, {bottom, right})} in degrees.
*/
QString QGeoRectangle::toString() const {
    return QStringLiteral("QGeoRectangle({%1, %2}, {%3, %4})")
        .arg(m_a.latitude())
        .arg(m_a.longitude())
        .arg(m_b.latitude())
        .arg(m_b.longitude());
}

#include "moc_qgeorectangle.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
