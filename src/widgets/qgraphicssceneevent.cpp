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
QPoint QGraphicsSceneMouseEvent::buttonDownScreenPos(Qt::MouseButton button) const {
    return m_downScreenPos.value(static_cast<int>(button)).toPoint();
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
    \class QGraphicsSceneContextMenuEvent
    \since 6.0
    \brief The QGraphicsSceneContextMenuEvent class provides context menu events in the graphics view framework.

    A QGraphicsView turns a request for a context menu over it into this event for its scene, which gives it to the
    items under the mouse, the topmost first, until one accepts it in QGraphicsItem::contextMenuEvent().

    \differences
    None.
    \enddifferences
*/

/*!
    Constructs a context menu event of \a type.
*/
QGraphicsSceneContextMenuEvent::QGraphicsSceneContextMenuEvent(Type type) : QGraphicsSceneEvent(type) {}

/*!
    Destroys the event.
*/
QGraphicsSceneContextMenuEvent::~QGraphicsSceneContextMenuEvent() = default;

/*!
    \class QGraphicsSceneDragDropEvent
    \since 6.0
    \brief The QGraphicsSceneDragDropEvent class provides events for drag and drop in the graphics view framework.

    A QGraphicsView turns the drags over it into these events for its scene. QGraphicsScene::dragEnterEvent() and
    the other handlers give them to the items under the drag that accept drops; a scene that handles drops itself
    reimplements them.

    \differences
    Drags work within one program, as QDrag says.
    \enddifferences
*/

/*!
    Constructs a drag and drop event of \a type.
*/
QGraphicsSceneDragDropEvent::QGraphicsSceneDragDropEvent(Type type) : QGraphicsSceneEvent(type) {}

/*!
    Destroys the event.
*/
QGraphicsSceneDragDropEvent::~QGraphicsSceneDragDropEvent() = default;

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

/*!
    \class QGraphicsSceneHelpEvent
    \inmodule NtWidgets
    \since 6.0
    \brief The QGraphicsSceneHelpEvent class is an event that asks for help, such as a tool tip, over a scene.

    \differences
    The scene does not send it yet; tool tips of items are not shown.
    \enddifferences
*/

/*!
    Constructs a help event of \a type.
*/
QGraphicsSceneHelpEvent::QGraphicsSceneHelpEvent(Type type) : QGraphicsSceneEvent(type) {}

/*!
    Destroys the event.
*/
QGraphicsSceneHelpEvent::~QGraphicsSceneHelpEvent() = default;

/*!
    \class QGraphicsSceneResizeEvent
    \inmodule NtWidgets
    \since 6.0
    \brief The QGraphicsSceneResizeEvent class tells a QGraphicsWidget that it was resized.

    QGraphicsWidget::resizeEvent() gets it.

    \differences
    None.
    \enddifferences
*/

/*!
    Constructs a resize event.
*/
QGraphicsSceneResizeEvent::QGraphicsSceneResizeEvent() : QGraphicsSceneEvent(QEvent::GraphicsSceneResize) {}

/*!
    Destroys the event.
*/
QGraphicsSceneResizeEvent::~QGraphicsSceneResizeEvent() = default;

/*!
    \class QGraphicsSceneMoveEvent
    \inmodule NtWidgets
    \since 6.0
    \brief The QGraphicsSceneMoveEvent class tells a QGraphicsWidget that it moved.

    QGraphicsWidget::moveEvent() gets it.

    \differences
    None.
    \enddifferences
*/

/*!
    Constructs a move event.
*/
QGraphicsSceneMoveEvent::QGraphicsSceneMoveEvent() : QGraphicsSceneEvent(QEvent::GraphicsSceneMove) {}

/*!
    Destroys the event.
*/
QGraphicsSceneMoveEvent::~QGraphicsSceneMoveEvent() = default;
