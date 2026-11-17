// MIT License - see LICENSE
#pragma once

#include <QtCore/qcoreevent.h>
#include <QtCore/qmap.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qpoint.h>

class QWidget;

class QGraphicsSceneEvent : public QEvent {
public:
    explicit QGraphicsSceneEvent(Type type);
    ~QGraphicsSceneEvent() override;

    /*!
        Returns the widget the event happened in: the viewport of a QGraphicsView, or \c nullptr for an event that
        did not come from a view.
    */
    QWidget *widget() const { return m_widget; }
    /*!
        Sets the widget the event happened in to \a widget.
    */
    void setWidget(QWidget *widget) { m_widget = widget; }

private:
    QWidget *m_widget = nullptr;
};

class QGraphicsSceneMouseEvent : public QGraphicsSceneEvent {
public:
    explicit QGraphicsSceneMouseEvent(Type type = None);
    ~QGraphicsSceneMouseEvent() override;

    /*! Returns the position of the mouse in the coordinates of the item that receives the event. */
    QPointF pos() const { return m_pos; }
    /*! Sets the position in item coordinates to \a pos. */
    void setPos(const QPointF &pos) { m_pos = pos; }
    /*! Returns the position of the mouse in scene coordinates. */
    QPointF scenePos() const { return m_scenePos; }
    /*! Sets the position in scene coordinates to \a pos. */
    void setScenePos(const QPointF &pos) { m_scenePos = pos; }
    /*! Returns the position of the mouse on the screen. */
    QPointF screenPos() const { return m_screenPos; }
    /*! Sets the position on the screen to \a pos. */
    void setScreenPos(const QPointF &pos) { m_screenPos = pos; }

    QPointF buttonDownPos(Qt::MouseButton button) const;
    void setButtonDownPos(Qt::MouseButton button, const QPointF &pos);
    QPointF buttonDownScenePos(Qt::MouseButton button) const;
    void setButtonDownScenePos(Qt::MouseButton button, const QPointF &pos);
    QPointF buttonDownScreenPos(Qt::MouseButton button) const;
    void setButtonDownScreenPos(Qt::MouseButton button, const QPointF &pos);

    /*! Returns the position of the previous mouse event in item coordinates. */
    QPointF lastPos() const { return m_lastPos; }
    /*! Sets the previous position in item coordinates to \a pos. */
    void setLastPos(const QPointF &pos) { m_lastPos = pos; }
    /*! Returns the position of the previous mouse event in scene coordinates. */
    QPointF lastScenePos() const { return m_lastScenePos; }
    /*! Sets the previous position in scene coordinates to \a pos. */
    void setLastScenePos(const QPointF &pos) { m_lastScenePos = pos; }
    /*! Returns the position of the previous mouse event on the screen. */
    QPointF lastScreenPos() const { return m_lastScreenPos; }
    /*! Sets the previous position on the screen to \a pos. */
    void setLastScreenPos(const QPointF &pos) { m_lastScreenPos = pos; }

    /*! Returns the buttons that are down. */
    Qt::MouseButtons buttons() const { return m_buttons; }
    /*! Sets the buttons that are down to \a buttons. */
    void setButtons(Qt::MouseButtons buttons) { m_buttons = buttons; }
    /*! Returns the button that caused the event, or Qt::NoButton for a move. */
    Qt::MouseButton button() const { return m_button; }
    /*! Sets the button that caused the event to \a button. */
    void setButton(Qt::MouseButton button) { m_button = button; }
    /*! Returns the keyboard modifiers that were held. */
    Qt::KeyboardModifiers modifiers() const { return m_modifiers; }
    /*! Sets the keyboard modifiers to \a modifiers. */
    void setModifiers(Qt::KeyboardModifiers modifiers) { m_modifiers = modifiers; }

private:
    QPointF m_pos;
    QPointF m_scenePos;
    QPointF m_screenPos;
    QPointF m_lastPos;
    QPointF m_lastScenePos;
    QPointF m_lastScreenPos;
    QMap<int, QPointF> m_downPos;
    QMap<int, QPointF> m_downScenePos;
    QMap<int, QPointF> m_downScreenPos;
    Qt::MouseButtons m_buttons;
    Qt::MouseButton m_button = Qt::NoButton;
    Qt::KeyboardModifiers m_modifiers;
};

class QGraphicsSceneHoverEvent : public QGraphicsSceneEvent {
public:
    explicit QGraphicsSceneHoverEvent(Type type = None);
    ~QGraphicsSceneHoverEvent() override;

    /*! Returns the position of the mouse in item coordinates. */
    QPointF pos() const { return m_pos; }
    /*! Sets the position in item coordinates to \a pos. */
    void setPos(const QPointF &pos) { m_pos = pos; }
    /*! Returns the position of the mouse in scene coordinates. */
    QPointF scenePos() const { return m_scenePos; }
    /*! Sets the position in scene coordinates to \a pos. */
    void setScenePos(const QPointF &pos) { m_scenePos = pos; }
    /*! Returns the position of the mouse on the screen. */
    QPointF screenPos() const { return m_screenPos; }
    /*! Sets the position on the screen to \a pos. */
    void setScreenPos(const QPointF &pos) { m_screenPos = pos; }
    /*! Returns the previous position in item coordinates. */
    QPointF lastPos() const { return m_lastPos; }
    /*! Sets the previous position in item coordinates to \a pos. */
    void setLastPos(const QPointF &pos) { m_lastPos = pos; }
    /*! Returns the previous position in scene coordinates. */
    QPointF lastScenePos() const { return m_lastScenePos; }
    /*! Sets the previous position in scene coordinates to \a pos. */
    void setLastScenePos(const QPointF &pos) { m_lastScenePos = pos; }
    /*! Returns the previous position on the screen. */
    QPointF lastScreenPos() const { return m_lastScreenPos; }
    /*! Sets the previous position on the screen to \a pos. */
    void setLastScreenPos(const QPointF &pos) { m_lastScreenPos = pos; }
    /*! Returns the keyboard modifiers that were held. */
    Qt::KeyboardModifiers modifiers() const { return m_modifiers; }
    /*! Sets the keyboard modifiers to \a modifiers. */
    void setModifiers(Qt::KeyboardModifiers modifiers) { m_modifiers = modifiers; }

private:
    QPointF m_pos;
    QPointF m_scenePos;
    QPointF m_screenPos;
    QPointF m_lastPos;
    QPointF m_lastScenePos;
    QPointF m_lastScreenPos;
    Qt::KeyboardModifiers m_modifiers;
};

class QGraphicsSceneContextMenuEvent : public QGraphicsSceneEvent {
public:
    /*!
        \enum QGraphicsSceneContextMenuEvent::Reason

        This enum tells how the user asked for the menu.

        \value Mouse With the right mouse button.
        \value Keyboard With the Menu key.
        \value Other Some other way.
    */
    enum Reason { Mouse, Keyboard, Other };

    explicit QGraphicsSceneContextMenuEvent(Type type = None);
    ~QGraphicsSceneContextMenuEvent() override;

    /*! Returns where the menu was asked for, in item coordinates. */
    QPointF pos() const { return m_pos; }
    /*! Sets the position in item coordinates to \a pos. */
    void setPos(const QPointF &pos) { m_pos = pos; }
    /*! Returns where the menu was asked for, in scene coordinates. */
    QPointF scenePos() const { return m_scenePos; }
    /*! Sets the position in scene coordinates to \a pos. */
    void setScenePos(const QPointF &pos) { m_scenePos = pos; }
    /*! Returns where the menu was asked for, on the screen. */
    QPoint screenPos() const { return m_screenPos; }
    /*! Sets the position on the screen to \a pos. */
    void setScreenPos(const QPoint &pos) { m_screenPos = pos; }
    /*! Returns the keyboard modifiers that were held. */
    Qt::KeyboardModifiers modifiers() const { return m_modifiers; }
    /*! Sets the keyboard modifiers to \a modifiers. */
    void setModifiers(Qt::KeyboardModifiers modifiers) { m_modifiers = modifiers; }
    /*! Returns how the user asked for the menu. */
    Reason reason() const { return m_reason; }
    /*! Sets how the user asked for the menu to \a reason. */
    void setReason(Reason reason) { m_reason = reason; }

private:
    QPointF m_pos;
    QPointF m_scenePos;
    QPoint m_screenPos;
    Qt::KeyboardModifiers m_modifiers;
    Reason m_reason = Mouse;
};

class QMimeData;

class QGraphicsSceneDragDropEvent : public QGraphicsSceneEvent {
public:
    explicit QGraphicsSceneDragDropEvent(Type type = None);
    ~QGraphicsSceneDragDropEvent() override;

    /*! Returns where the drag is, in item coordinates. */
    QPointF pos() const { return m_pos; }
    /*! Sets the position in item coordinates to \a pos. */
    void setPos(const QPointF &pos) { m_pos = pos; }
    /*! Returns where the drag is, in scene coordinates. */
    QPointF scenePos() const { return m_scenePos; }
    /*! Sets the position in scene coordinates to \a pos. */
    void setScenePos(const QPointF &pos) { m_scenePos = pos; }
    /*! Returns where the drag is, on the screen. */
    QPointF screenPos() const { return m_screenPos; }
    /*! Sets the position on the screen to \a pos. */
    void setScreenPos(const QPointF &pos) { m_screenPos = pos; }
    /*! Returns the mouse buttons held during the drag. */
    Qt::MouseButtons buttons() const { return m_buttons; }
    /*! Sets the buttons to \a buttons. */
    void setButtons(Qt::MouseButtons buttons) { m_buttons = buttons; }
    /*! Returns the keyboard modifiers held during the drag. */
    Qt::KeyboardModifiers modifiers() const { return m_modifiers; }
    /*! Sets the modifiers to \a modifiers. */
    void setModifiers(Qt::KeyboardModifiers modifiers) { m_modifiers = modifiers; }
    /*! Returns the actions the source of the drag allows. */
    Qt::DropActions possibleActions() const { return m_actions; }
    /*! Sets the possible actions to \a actions. */
    void setPossibleActions(Qt::DropActions actions) { m_actions = actions; }
    /*! Returns the action the source proposes. */
    Qt::DropAction proposedAction() const { return m_proposed; }
    /*! Sets the proposed action to \a action. */
    void setProposedAction(Qt::DropAction action) { m_proposed = action; }
    /*! Accepts the event with the proposed action. */
    void acceptProposedAction() {
        m_action = m_proposed;
        accept();
    }
    /*! Returns the action the receiver takes. */
    Qt::DropAction dropAction() const { return m_action; }
    /*! Sets the action the receiver takes to \a action. */
    void setDropAction(Qt::DropAction action) { m_action = action; }
    /*! Returns the widget that started the drag, if it is in this program. */
    QWidget *source() const { return m_source; }
    /*! Sets the source of the drag to \a source. */
    void setSource(QWidget *source) { m_source = source; }
    /*! Returns the data of the drag. */
    const QMimeData *mimeData() const { return m_data; }
    /*! Sets the data of the drag to \a data. */
    void setMimeData(const QMimeData *data) { m_data = data; }

private:
    QPointF m_pos;
    QPointF m_scenePos;
    QPointF m_screenPos;
    Qt::MouseButtons m_buttons;
    Qt::KeyboardModifiers m_modifiers;
    Qt::DropActions m_actions;
    Qt::DropAction m_proposed = Qt::IgnoreAction;
    Qt::DropAction m_action = Qt::IgnoreAction;
    QWidget *m_source = nullptr;
    const QMimeData *m_data = nullptr;
};

class QGraphicsSceneWheelEvent : public QGraphicsSceneEvent {
public:
    explicit QGraphicsSceneWheelEvent(Type type = None);
    ~QGraphicsSceneWheelEvent() override;

    /*! Returns the position of the mouse in item coordinates. */
    QPointF pos() const { return m_pos; }
    /*! Sets the position in item coordinates to \a pos. */
    void setPos(const QPointF &pos) { m_pos = pos; }
    /*! Returns the position of the mouse in scene coordinates. */
    QPointF scenePos() const { return m_scenePos; }
    /*! Sets the position in scene coordinates to \a pos. */
    void setScenePos(const QPointF &pos) { m_scenePos = pos; }
    /*! Returns the position of the mouse on the screen. */
    QPointF screenPos() const { return m_screenPos; }
    /*! Sets the position on the screen to \a pos. */
    void setScreenPos(const QPointF &pos) { m_screenPos = pos; }
    /*! Returns the buttons that were down. */
    Qt::MouseButtons buttons() const { return m_buttons; }
    /*! Sets the buttons that were down to \a buttons. */
    void setButtons(Qt::MouseButtons buttons) { m_buttons = buttons; }
    /*! Returns the keyboard modifiers that were held. */
    Qt::KeyboardModifiers modifiers() const { return m_modifiers; }
    /*! Sets the keyboard modifiers to \a modifiers. */
    void setModifiers(Qt::KeyboardModifiers modifiers) { m_modifiers = modifiers; }
    /*! Returns how far the wheel turned, in eighths of a degree: 120 for one notch away from the user. */
    int delta() const { return m_delta; }
    /*! Sets how far the wheel turned to \a delta. */
    void setDelta(int delta) { m_delta = delta; }
    /*! Returns the direction of the wheel: Qt::Vertical for an ordinary wheel. */
    Qt::Orientation orientation() const { return m_orientation; }
    /*! Sets the direction of the wheel to \a orientation. */
    void setOrientation(Qt::Orientation orientation) { m_orientation = orientation; }
    /*! Returns the scrolling distance in pixels, for devices that report it. */
    QPoint pixelDelta() const { return m_pixelDelta; }
    /*! Sets the scrolling distance in pixels to \a delta. */
    void setPixelDelta(const QPoint &delta) { m_pixelDelta = delta; }

private:
    QPointF m_pos;
    QPointF m_scenePos;
    QPointF m_screenPos;
    Qt::MouseButtons m_buttons;
    Qt::KeyboardModifiers m_modifiers;
    int m_delta = 0;
    Qt::Orientation m_orientation = Qt::Vertical;
    QPoint m_pixelDelta;
};
