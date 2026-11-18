// MIT License - see LICENSE
#pragma once

#include <QtCore/qmetatype.h>
#include <QtCore/qobjectdefs.h>
#include <QtCore/qstring.h>
#include <QtPositioning/qgeocoordinate.h>

class QGeoRectangle;

class QGeoShape {
    Q_GADGET
    Q_PROPERTY(ShapeType type READ type)
    Q_PROPERTY(bool isValid READ isValid)
    Q_PROPERTY(bool isEmpty READ isEmpty)
    Q_PROPERTY(QGeoCoordinate center READ center)

public:
    enum ShapeType {
        UnknownType = 0x0000,
        RectangleType = 0x0001,
        PathType = 0x0002,
        CircleType = 0x0004,
        PolygonType = 0x0008
    };
    Q_ENUM(ShapeType)

    QGeoShape();

    ShapeType type() const { return m_type; }
    bool isValid() const;
    bool isEmpty() const;
    bool contains(const QGeoCoordinate &coordinate) const;
    QGeoRectangle boundingGeoRectangle() const;
    QGeoCoordinate center() const;
    QString toString() const;

    friend bool operator==(const QGeoShape &lhs, const QGeoShape &rhs) { return lhs.equals(rhs); }
    friend bool operator!=(const QGeoShape &lhs, const QGeoShape &rhs) { return !lhs.equals(rhs); }

protected:
    explicit QGeoShape(ShapeType type) : m_type(type) {}

private:
    friend class QGeoRectangle;
    friend class QGeoCircle;
    bool equals(const QGeoShape &other) const;

    // A rectangle uses m_a as its top left and m_b as its bottom right corner; a circle uses m_a as
    // its centre and m_radius.
    ShapeType m_type = UnknownType;
    QGeoCoordinate m_a;
    QGeoCoordinate m_b;
    double m_radius = -1.0;
};

Q_DECLARE_METATYPE(QGeoShape)
