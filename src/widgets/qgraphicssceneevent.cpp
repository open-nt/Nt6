// MIT License - see LICENSE
#include <QtWidgets/qgraphicssceneevent.h>

/*!
    \class QGraphicsSceneEvent
    \brief The QGraphicsSceneEvent class is the base of the events a QGraphicsScene sends its items.

    \differences
    There is no timestamp.
    \enddifferences
*/

/*!
    Constructs a scene event of \a type.
*/
QGraphicsSceneEvent::QGraphicsSceneEvent(Type type) : QEvent(type) {}

/*!
    Destroys the event.
*/
QGraphicsSceneEvent::~QGraphicsSceneEvent() = default;

/*!
    \class QGraphicsSceneMouseEvent
    \brief The QGraphicsSceneMouseEvent class describes a mouse event in a scene.

    The positions are given in item, scene and screen coordinates, with those of the previous event and of the press
    of each button.

    \differences
    There is no \c source() or \c flags().
    \enddifferences
*/

/*!
    Constructs a mouse event of \a type.
*/
QGraphicsSceneMouseEvent::QGraphicsSceneMouseEvent(Type type) : QGraphicsSceneEvent(type) {}

/*!
    Destroys the event.
*/
QGraphicsSceneMouseEvent::~QGraphicsSceneMouseEvent() = default;

/*!
    Returns where \a button was pressed, in item coordinates.
*/
QPointF QGraphicsSceneMouseEvent::buttonDownPos(Qt::MouseButton button) const {
    return m_downPos.value(static_cast<int>(button));
}

/*!
    Sets where \a button was pressed, in item coordinates, to \a pos.
*/
void QGraphicsSceneMouseEvent::setButtonDownPos(Qt::MouseButton button, const QPointF &pos) {
    m_downPos.insert(static_cast<int>(button), pos);
}

/*!
    Returns where \a button was pressed, in scene coordinates.
*/
QPointF QGraphicsSceneMouseEvent::buttonDownScenePos(Qt::MouseButton button) const {
    return m_downScenePos.value(static_cast<int>(button));
}

/*!
    Sets where \a button was pressed, in scene coordinates, to \a pos.
*/
void QGraphicsSceneMouseEvent::setButtonDownScenePos(Qt::MouseButton button, const QPointF &pos) {
    m_downScenePos.insert(static_cast<int>(button), pos);
}

/*!
    Returns where \a button was pressed, on the screen.
*/
QPointF QGraphicsSceneMouseEvent::buttonDownScreenPos(Qt::MouseButton button) const {
    return m_downScreenPos.value(static_cast<int>(button));
}

/*!
    Sets where \a button was pressed, on the screen, to \a pos.
*/
void QGraphicsSceneMouseEvent::setButtonDownScreenPos(Qt::MouseButton button, const QPointF &pos) {
    m_downScreenPos.insert(static_cast<int>(button), pos);
}

/*!
    \class QGraphicsSceneHoverEvent
    \brief The QGraphicsSceneHoverEvent class describes the mouse entering, moving over or leaving an item.

    \differences
    None.
    \enddifferences
*/

/*!
    Constructs a hover event of \a type.
*/
QGraphicsSceneHoverEvent::QGraphicsSceneHoverEvent(Type type) : QGraphicsSceneEvent(type) {}

/*!
    Destroys the event.
*/
QGraphicsSceneHoverEvent::~QGraphicsSceneHoverEvent() = default;

/*!
    \class QGraphicsSceneWheelEvent
    \brief The QGraphicsSceneWheelEvent class describes a turn of the mouse wheel over an item.

    \differences
    There is no \c phase() or \c isInverted().
    \enddifferences
*/

/*!
    Constructs a wheel event of \a type.
*/
QGraphicsSceneWheelEvent::QGraphicsSceneWheelEvent(Type type) : QGraphicsSceneEvent(type) {}

/*!
    Destroys the event.
*/
QGraphicsSceneWheelEvent::~QGraphicsSceneWheelEvent() = default;
