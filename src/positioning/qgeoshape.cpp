// MIT License - see LICENSE
//
// QGeoShape keeps the data of every shape it supports (two corners or a centre and a radius), so
// that QGeoRectangle and QGeoCircle add no members and a QGeoShape copy keeps everything.
#include "qgeoshape.h"
#include "qgeocircle.h"
#include "qgeorectangle.h"
#include "qpositioningmath_p.h"

#include <cmath>

/*!
    \class QGeoShape
    \inmodule NtPositioning
    \brief The QGeoShape class defines a geographic area.

    QGeoShape is the value type behind QGeoRectangle and QGeoCircle. A shape of either kind can be
    stored as a QGeoShape and converted back with the QGeoRectangle or QGeoCircle constructor that
    takes a QGeoShape; type() tells which it is.

    \differences
    Only rectangles and circles exist; there is no QGeoPath or QGeoPolygon, so PathType and
    PolygonType are never returned. There is no QDataStream operator.
    \enddifferences

    \sa QGeoRectangle, QGeoCircle
*/

/*!
    \enum QGeoShape::ShapeType

    \value UnknownType A default-constructed shape, which is invalid.
    \value RectangleType A QGeoRectangle.
    \value PathType A path; not supported by Nt.
    \value CircleType A QGeoCircle.
    \value PolygonType A polygon; not supported by Nt.
*/

/*!
    Constructs an invalid shape of type UnknownType.
*/
QGeoShape::QGeoShape() = default;

/*!
    \fn QGeoShape::QGeoShape(ShapeType type)
    \internal
*/

/*!
    \fn QGeoShape::ShapeType QGeoShape::type() const
    Returns the type of the shape.
*/

/*!
    Returns \c true if the shape is a valid rectangle or circle; otherwise returns \c false.
*/
bool QGeoShape::isValid() const {
    switch (m_type) {
    case RectangleType:
        return m_a.isValid() && m_b.isValid() && m_a.latitude() >= m_b.latitude();
    case CircleType:
        return m_a.isValid() && m_radius >= 0.0;
    default:
        return false;
    }
}

/*!
    Returns \c true if the shape is invalid or encloses no area; otherwise returns \c false.
*/
bool QGeoShape::isEmpty() const {
    if (!isValid())
        return true;
    if (m_type == CircleType)
        return m_radius == 0.0;
    const QGeoRectangle rectangle(*this);
    return rectangle.width() == 0.0 || rectangle.height() == 0.0;
}

/*!
    Returns \c true if \a coordinate lies inside the shape or on its edge; otherwise returns
    \c false.
*/
bool QGeoShape::contains(const QGeoCoordinate &coordinate) const {
    if (!isValid() || !coordinate.isValid())
        return false;
    if (m_type == CircleType)
        return m_a.distanceTo(coordinate) <= m_radius;
    if (coordinate.latitude() > m_a.latitude() || coordinate.latitude() < m_b.latitude())
        return false;
    const double width = QGeoRectangle(*this).width();
    return width >= 360.0 || QtPositioningPrivate::eastward(m_a.longitude(), coordinate.longitude()) <= width;
}

/*!
    Returns the smallest rectangle that contains the shape, or an invalid rectangle if the shape is
    invalid.
*/
QGeoRectangle QGeoShape::boundingGeoRectangle() const {
    if (!isValid())
        return QGeoRectangle();
    if (m_type == RectangleType)
        return QGeoRectangle(*this);
    using namespace QtPositioningPrivate;
    const double angular = m_radius / kEarthMeanRadius;
    const double dLat = degrees(angular);
    const double top = m_a.latitude() + dLat;
    const double bottom = m_a.latitude() - dLat;
    if (top >= 90.0 || bottom <= -90.0 || angular >= kPi / 2)
        return QGeoRectangle(QGeoCoordinate(std::fmin(top, 90.0), -180.0),
                             QGeoCoordinate(std::fmax(bottom, -90.0), 180.0));
    const double ratio = std::sin(angular) / std::cos(radians(m_a.latitude()));
    if (ratio >= 1.0)
        return QGeoRectangle(QGeoCoordinate(top, -180.0), QGeoCoordinate(bottom, 180.0));
    const double dLon = degrees(std::asin(ratio));
    return QGeoRectangle(QGeoCoordinate(top, normalizeLongitude(m_a.longitude() - dLon)),
                         QGeoCoordinate(bottom, normalizeLongitude(m_a.longitude() + dLon)));
}

/*!
    Returns the centre of the shape, or an invalid coordinate if the shape is invalid.
*/
QGeoCoordinate QGeoShape::center() const {
    if (!isValid())
        return QGeoCoordinate();
    if (m_type == CircleType)
        return m_a;
    return QGeoRectangle(*this).center();
}

/*!
    Returns the shape as text, such as \c{QGeoRectangle({52, -2}, {51, -1})} or
    \c{QGeoCircle({51.5, -0.1}, 500)}.
*/
QString QGeoShape::toString() const {
    switch (m_type) {
    case RectangleType:
        return QGeoRectangle(*this).toString();
    case CircleType:
        return QGeoCircle(*this).toString();
    default:
        return QStringLiteral("QGeoShape(Unknown)");
    }
}

bool QGeoShape::equals(const QGeoShape &other) const {
    if (m_type != other.m_type)
        return false;
    switch (m_type) {
    case RectangleType:
        return m_a == other.m_a && m_b == other.m_b;
    case CircleType:
        return m_a == other.m_a && ((std::isnan(m_radius) && std::isnan(other.m_radius)) || m_radius == other.m_radius);
    default:
        return true;
    }
}

#include "moc_qgeoshape.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
