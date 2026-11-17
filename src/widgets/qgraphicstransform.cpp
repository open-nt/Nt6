// MIT License - see LICENSE
#include <QtWidgets/qgraphicstransform.h>

#include <QtWidgets/qgraphicsitem.h>

#include "qgraphicsscene_p.h"

/*!
    \class QGraphicsTransform
    \inmodule NtWidgets
    \since 6.0
    \brief The QGraphicsTransform class is the base of transformations of graphics items that can be animated.

    QGraphicsItem::setTransformations() gives an item a list of transformations, applied in their order after the
    transform() of the item and before its rotation and scale. A transformation is a QObject with properties, so a
    QPropertyAnimation can animate it:

    \code
    auto *spin = new QGraphicsRotation(item->toGraphicsObject());
    spin->setOrigin(QVector3D(item->boundingRect().center()));
    spin->setAxis(Qt::YAxis);
    item->setTransformations({spin});
    spin->setAngle(30); // turned about the vertical axis, in perspective
    \endcode

    A subclass reimplements applyTo() and calls update() when it changes.

    \differences
    None.
    \enddifferences

    \sa QGraphicsRotation, QGraphicsScale
*/

/*!
    Constructs a transformation with the given \a parent.
*/
QGraphicsTransform::QGraphicsTransform(QObject *parent) : QObject(parent) {}

/*!
    Destroys the transformation and takes it off the items that it transforms.
*/
QGraphicsTransform::~QGraphicsTransform() {
    for (QGraphicsItem *item : std::as_const(m_items))
        QGraphicsItemAccess::forgetTransformation(item, this);
}

/*!
    \fn void QGraphicsTransform::applyTo(QMatrix4x4 *matrix) const

    Applies the transformation to \a matrix.
*/

/*!
    Tells the items that the transformation changed, so that they are drawn again.
*/
void QGraphicsTransform::update() {
    for (QGraphicsItem *item : std::as_const(m_items))
        QGraphicsItemAccess::transformationChanged(item);
}

/*!
    \class QGraphicsScale
    \inmodule NtWidgets
    \since 6.0
    \brief The QGraphicsScale class scales a graphics item about an origin, along each axis.

    \differences
    None.
    \enddifferences

    \sa QGraphicsTransform
*/

/*!
    Constructs a scale of 1 with the given \a parent.
*/
QGraphicsScale::QGraphicsScale(QObject *parent) : QGraphicsTransform(parent) {}

/*!
    Destroys the scale.
*/
QGraphicsScale::~QGraphicsScale() = default;

/*!
    \fn QVector3D QGraphicsScale::origin() const

    Returns the point that the scale is about, in item coordinates.
*/

/*!
    Sets the point that the scale is about to \a point.
*/
void QGraphicsScale::setOrigin(const QVector3D &point) {
    if (point == m_origin)
        return;
    m_origin = point;
    update();
    emit originChanged();
}

/*!
    \fn qreal QGraphicsScale::xScale() const

    Returns the scale across.
*/

/*!
    Sets the scale across to \a scale.
*/
void QGraphicsScale::setXScale(qreal scale) {
    if (qFuzzyCompare(scale, m_x))
        return;
    m_x = scale;
    update();
    emit xScaleChanged();
    emit scaleChanged();
}

/*!
    \fn qreal QGraphicsScale::yScale() const

    Returns the scale down.
*/

/*!
    Sets the scale down to \a scale.
*/
void QGraphicsScale::setYScale(qreal scale) {
    if (qFuzzyCompare(scale, m_y))
        return;
    m_y = scale;
    update();
    emit yScaleChanged();
    emit scaleChanged();
}

/*!
    \fn qreal QGraphicsScale::zScale() const

    Returns the scale in depth.
*/

/*!
    Sets the scale in depth to \a scale.
*/
void QGraphicsScale::setZScale(qreal scale) {
    if (qFuzzyCompare(scale, m_z))
        return;
    m_z = scale;
    update();
    emit zScaleChanged();
    emit scaleChanged();
}

/*!
    \reimp
*/
void QGraphicsScale::applyTo(QMatrix4x4 *matrix) const {
    if (m_x == 1 && m_y == 1 && m_z == 1)
        return;
    matrix->translate(m_origin);
    matrix->scale(static_cast<float>(m_x), static_cast<float>(m_y), static_cast<float>(m_z));
    matrix->translate(-m_origin);
}

/*!
    \class QGraphicsRotation
    \inmodule NtWidgets
    \since 6.0
    \brief The QGraphicsRotation class turns a graphics item about an axis through an origin.

    The axis is the z axis by default, which turns the item in the plane of the scene; about the x or the y axis it
    turns in perspective.

    \differences
    None.
    \enddifferences

    \sa QGraphicsTransform
*/

/*!
    Constructs a rotation of 0 degrees about the z axis with the given \a parent.
*/
QGraphicsRotation::QGraphicsRotation(QObject *parent) : QGraphicsTransform(parent) {}

/*!
    Destroys the rotation.
*/
QGraphicsRotation::~QGraphicsRotation() = default;

/*!
    \fn QVector3D QGraphicsRotation::origin() const

    Returns the point that the axis goes through, in item coordinates.
*/

/*!
    Sets the point that the axis goes through to \a point.
*/
void QGraphicsRotation::setOrigin(const QVector3D &point) {
    if (point == m_origin)
        return;
    m_origin = point;
    update();
    emit originChanged();
}

/*!
    \fn qreal QGraphicsRotation::angle() const

    Returns the angle in degrees, clockwise.
*/

/*!
    Sets the angle to \a angle degrees.
*/
void QGraphicsRotation::setAngle(qreal angle) {
    if (qFuzzyCompare(angle, m_angle))
        return;
    m_angle = angle;
    update();
    emit angleChanged();
}

/*!
    \fn QVector3D QGraphicsRotation::axis() const

    Returns the direction of the axis.
*/

/*!
    Sets the direction of the axis to \a axis.
*/
void QGraphicsRotation::setAxis(const QVector3D &axis) {
    if (axis == m_axis)
        return;
    m_axis = axis;
    update();
    emit axisChanged();
}

/*!
    \overload

    Sets the axis to \a axis: Qt::XAxis, Qt::YAxis or Qt::ZAxis.
*/
void QGraphicsRotation::setAxis(Qt::Axis axis) {
    switch (axis) {
    case Qt::XAxis:
        setAxis(QVector3D(1, 0, 0));
        break;
    case Qt::YAxis:
        setAxis(QVector3D(0, 1, 0));
        break;
    case Qt::ZAxis:
        setAxis(QVector3D(0, 0, 1));
        break;
    }
}

/*!
    \reimp
*/
void QGraphicsRotation::applyTo(QMatrix4x4 *matrix) const {
    if (m_angle == 0 || m_axis.isNull())
        return;
    matrix->translate(m_origin);
    matrix->rotate(static_cast<float>(m_angle), m_axis);
    matrix->translate(-m_origin);
}

#include "moc_qgraphicstransform.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
