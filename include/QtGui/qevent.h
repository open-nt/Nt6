// MIT License - see LICENSE
#pragma once

#include <QtCore/qcoreevent.h>
#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtGui/qkeysequence.h>
#include <QtGui/qregion.h>

class QAction;

class QInputEvent : public QEvent {
public:
    QInputEvent(Type type, Qt::KeyboardModifiers modifiers = Qt::NoModifier) : QEvent(type), m_modifiers(modifiers) {}
    ~QInputEvent() override;
    /*!
        \reimp
    */
    QInputEvent *clone() const override { return new QInputEvent(*this); }

    Qt::KeyboardModifiers modifiers() const { return m_modifiers; }
    void setModifiers(Qt::KeyboardModifiers modifiers) { m_modifiers = modifiers; }

private:
    Qt::KeyboardModifiers m_modifiers;
};

class QContextMenuEvent : public QInputEvent {
public:
    /*!
        \enum QContextMenuEvent::Reason

        This enum tells how the user asked for the menu.

        \value Mouse With the right mouse button.
        \value Keyboard With the Menu key, or Shift+F10.
        \value Other Some other way.
    */
    enum Reason { Mouse, Keyboard, Other };

    QContextMenuEvent(Reason reason, const QPoint &pos, const QPoint &globalPos,
                      Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    QContextMenuEvent(Reason reason, const QPoint &pos);
    ~QContextMenuEvent() override;
    /*!
        \reimp
    */
    QContextMenuEvent *clone() const override { return new QContextMenuEvent(*this); }

    /*! Returns where the menu was asked for, in the widget. */
    const QPoint &pos() const { return m_pos; }
    /*! Returns where the menu was asked for, on the screen. */
    const QPoint &globalPos() const { return m_globalPos; }
    /*! Returns the x coordinate of pos(). */
    int x() const { return m_pos.x(); }
    /*! Returns the y coordinate of pos(). */
    int y() const { return m_pos.y(); }
    /*! Returns the x coordinate of globalPos(). */
    int globalX() const { return m_globalPos.x(); }
    /*! Returns the y coordinate of globalPos(). */
    int globalY() const { return m_globalPos.y(); }
    /*! Returns how the user asked for the menu. */
    Reason reason() const { return m_reason; }

private:
    QPoint m_pos;
    QPoint m_globalPos;
    Reason m_reason;
};

class QMimeData;

class QDropEvent : public QEvent {
public:
    QDropEvent(const QPointF &pos, Qt::DropActions actions, const QMimeData *data, Qt::MouseButtons buttons,
               Qt::KeyboardModifiers modifiers, Type type = Drop);
    ~QDropEvent() override;
    /*!
        \reimp
    */
    QDropEvent *clone() const override { return new QDropEvent(*this); }

    /*! Returns where the drag is, in the receiving widget. */
    QPointF position() const { return m_pos; }
    /*! Returns where the drag is, in whole pixels of the receiving widget. */
    QPoint pos() const { return m_pos.toPoint(); }
    /*! Returns the mouse buttons held during the drag. */
    Qt::MouseButtons buttons() const { return m_buttons; }
    /*! Returns the keyboard modifiers held during the drag. */
    Qt::KeyboardModifiers modifiers() const { return m_modifiers; }
    /*! Returns the actions that the source of the drag allows. */
    Qt::DropActions possibleActions() const { return m_actions; }
    /*! Returns the action that the source proposes. */
    Qt::DropAction proposedAction() const { return m_proposed; }
    /*! Sets the proposed action to \a action. */
    void setProposedAction(Qt::DropAction action) { m_proposed = action; }
    /*! Accepts the event with the proposed action. */
    void acceptProposedAction() {
        m_action = m_proposed;
        accept();
    }
    /*! Returns the action the receiver takes; the proposed one until setDropAction() changes it. */
    Qt::DropAction dropAction() const { return m_action; }
    /*! Sets the action the receiver takes to \a action, one of possibleActions(). */
    void setDropAction(Qt::DropAction action) { m_action = action; }
    /*! Returns the object that started the drag, if it is in this program. */
    QObject *source() const { return m_source; }
    /*! Returns the data of the drag. */
    const QMimeData *mimeData() const { return m_data; }

    /*!
        \internal
        Sets the source of the drag to \a source.
    */
    void setSource(QObject *source) { m_source = source; }

private:
    QPointF m_pos;
    Qt::DropActions m_actions;
    Qt::DropAction m_proposed = Qt::IgnoreAction;
    Qt::DropAction m_action = Qt::IgnoreAction;
    const QMimeData *m_data;
    Qt::MouseButtons m_buttons;
    Qt::KeyboardModifiers m_modifiers;
    QObject *m_source = nullptr;
};

class QDragMoveEvent : public QDropEvent {
public:
    QDragMoveEvent(const QPoint &pos, Qt::DropActions actions, const QMimeData *data, Qt::MouseButtons buttons,
                   Qt::KeyboardModifiers modifiers, Type type = DragMove);
    ~QDragMoveEvent() override;
    /*!
        \reimp
    */
    QDragMoveEvent *clone() const override { return new QDragMoveEvent(*this); }

    /*! Returns the rectangle in which moving the drag gives the same answer; the point of the drag in Nt. */
    QRect answerRect() const { return m_rect; }
    using QDropEvent::accept;
    using QDropEvent::ignore;
    /*! Accepts the drag while it is in \a rectangle. */
    void accept(const QRect &rectangle) {
        m_rect = rectangle;
        accept();
    }
    /*! Refuses the drag while it is in \a rectangle. */
    void ignore(const QRect &rectangle) {
        m_rect = rectangle;
        ignore();
    }

private:
    QRect m_rect;
};

class QDragEnterEvent : public QDragMoveEvent {
public:
    QDragEnterEvent(const QPoint &pos, Qt::DropActions actions, const QMimeData *data, Qt::MouseButtons buttons,
                    Qt::KeyboardModifiers modifiers);
    ~QDragEnterEvent() override;
    /*!
        \reimp
    */
    QDragEnterEvent *clone() const override { return new QDragEnterEvent(*this); }
};

class QDragLeaveEvent : public QEvent {
public:
    QDragLeaveEvent();
    ~QDragLeaveEvent() override;
    /*!
        \reimp
    */
    QDragLeaveEvent *clone() const override { return new QDragLeaveEvent(*this); }
};

class QMouseEvent : public QInputEvent {
public:
    QMouseEvent(Type type, const QPointF &localPos, const QPointF &globalPos, Qt::MouseButton button,
                Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
    ~QMouseEvent() override;
    /*!
        \reimp
    */
    QMouseEvent *clone() const override { return new QMouseEvent(*this); }

    Qt::MouseButton button() const { return m_button; }
    Qt::MouseButtons buttons() const { return m_buttons; }
    QPointF position() const { return m_local; }
    QPointF globalPosition() const { return m_global; }
    QPoint pos() const { return m_local.toPoint(); }
    QPoint globalPos() const { return m_global.toPoint(); }
    int x() const { return pos().x(); }
    int y() const { return pos().y(); }

private:
    QPointF m_local;
    QPointF m_global;
    Qt::MouseButton m_button;
    Qt::MouseButtons m_buttons;
};

class QWheelEvent : public QInputEvent {
public:
    QWheelEvent(const QPointF &pos, const QPointF &globalPos, const QPoint &pixelDelta, const QPoint &angleDelta,
                Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
    ~QWheelEvent() override;
    /*!
        \reimp
    */
    QWheelEvent *clone() const override { return new QWheelEvent(*this); }

    QPoint pixelDelta() const { return m_pixelDelta; }
    QPoint angleDelta() const { return m_angleDelta; }
    Qt::MouseButtons buttons() const { return m_buttons; }
    QPointF position() const { return m_local; }
    QPointF globalPosition() const { return m_global; }

private:
    QPointF m_local;
    QPointF m_global;
    QPoint m_pixelDelta;
    QPoint m_angleDelta;
    Qt::MouseButtons m_buttons;
};

class QKeyEvent : public QInputEvent {
public:
    QKeyEvent(Type type, int key, Qt::KeyboardModifiers modifiers, const QString &text = QString(),
              bool autorep = false, ushort count = 1);
    QKeyEvent(Type type, int key, Qt::KeyboardModifiers modifiers, quint32 nativeScanCode, quint32 nativeVirtualKey,
              quint32 nativeModifiers, const QString &text = QString(), bool autorep = false, ushort count = 1);
    ~QKeyEvent() override;
    /*!
        \reimp
    */
    QKeyEvent *clone() const override { return new QKeyEvent(*this); }

    int key() const { return m_key; }
    QString text() const { return m_text; }
    bool isAutoRepeat() const { return m_autorep; }
    int count() const { return m_count; }
    bool matches(QKeySequence::StandardKey key) const;
    /*!
        \since 6.0
        Returns the key and the modifiers of the event as one combination.
    */
    QKeyCombination keyCombination() const { return QKeyCombination(modifiers(), static_cast<Qt::Key>(m_key)); }
    /*!
        Returns the scan code of the key on the platform: the X11 key code on \c xcb, 0 if the platform gave none.
    */
    quint32 nativeScanCode() const { return m_nativeScanCode; }
    /*!
        Returns the virtual key of the platform: the X11 keysym on \c xcb, 0 if the platform gave none.
    */
    quint32 nativeVirtualKey() const { return m_nativeVirtualKey; }
    /*!
        Returns the modifier state of the platform: the X11 key state mask on \c xcb, 0 if the platform gave none.
    */
    quint32 nativeModifiers() const { return m_nativeModifiers; }

private:
    int m_key;
    QString m_text;
    bool m_autorep;
    ushort m_count;
    quint32 m_nativeScanCode = 0;
    quint32 m_nativeVirtualKey = 0;
    quint32 m_nativeModifiers = 0;
};

class QActionEvent : public QEvent {
public:
    QActionEvent(int type, QAction *action, QAction *before = nullptr);
    ~QActionEvent() override;
    /*!
        \reimp
    */
    QActionEvent *clone() const override { return new QActionEvent(*this); }

    QAction *action() const { return m_action; }
    QAction *before() const { return m_before; }

private:
    QAction *m_action;
    QAction *m_before;
};

class QHelpEvent : public QEvent {
public:
    QHelpEvent(Type type, const QPoint &pos, const QPoint &globalPos) : QEvent(type), m_pos(pos), m_global(globalPos) {}
    ~QHelpEvent() override;
    /*!
        \reimp
    */
    QHelpEvent *clone() const override { return new QHelpEvent(*this); }

    int x() const { return m_pos.x(); }
    int y() const { return m_pos.y(); }
    int globalX() const { return m_global.x(); }
    int globalY() const { return m_global.y(); }
    QPoint pos() const { return m_pos; }
    QPoint globalPos() const { return m_global; }

private:
    QPoint m_pos;
    QPoint m_global;
};

class QFocusEvent : public QEvent {
public:
    explicit QFocusEvent(Type type) : QEvent(type) {}
    ~QFocusEvent() override;
    /*!
        \reimp
    */
    QFocusEvent *clone() const override { return new QFocusEvent(*this); }

    bool gotFocus() const { return type() == FocusIn; }
    bool lostFocus() const { return type() == FocusOut; }
};

class QEnterEvent : public QEvent {
public:
    QEnterEvent(const QPointF &localPos, const QPointF &globalPos)
        : QEvent(Enter), m_local(localPos), m_global(globalPos) {}
    ~QEnterEvent() override;
    /*!
        \reimp
    */
    QEnterEvent *clone() const override { return new QEnterEvent(*this); }

    QPointF position() const { return m_local; }
    QPointF globalPosition() const { return m_global; }
    QPoint pos() const { return m_local.toPoint(); }
    QPoint globalPos() const { return m_global.toPoint(); }

private:
    QPointF m_local;
    QPointF m_global;
};

class QPaintEvent : public QEvent {
public:
    explicit QPaintEvent(const QRegion &paintRegion) : QEvent(Paint), m_region(paintRegion) {}
    explicit QPaintEvent(const QRect &paintRect) : QEvent(Paint), m_region(paintRect) {}
    ~QPaintEvent() override;
    /*!
        \reimp
    */
    QPaintEvent *clone() const override { return new QPaintEvent(*this); }

    const QRegion &region() const { return m_region; }
    QRect rect() const { return m_region.boundingRect(); }

private:
    QRegion m_region;
};

class QExposeEvent : public QEvent {
public:
    explicit QExposeEvent(const QRegion &exposeRegion) : QEvent(Expose), m_region(exposeRegion) {}
    ~QExposeEvent() override;
    /*!
        \reimp
    */
    QExposeEvent *clone() const override { return new QExposeEvent(*this); }

    const QRegion &region() const { return m_region; }

private:
    QRegion m_region;
};

class QResizeEvent : public QEvent {
public:
    QResizeEvent(const QSize &size, const QSize &oldSize) : QEvent(Resize), m_size(size), m_oldSize(oldSize) {}
    ~QResizeEvent() override;
    /*!
        \reimp
    */
    QResizeEvent *clone() const override { return new QResizeEvent(*this); }

    const QSize &size() const { return m_size; }
    const QSize &oldSize() const { return m_oldSize; }

private:
    QSize m_size;
    QSize m_oldSize;
};

class QMoveEvent : public QEvent {
public:
    QMoveEvent(const QPoint &pos, const QPoint &oldPos) : QEvent(Move), m_pos(pos), m_oldPos(oldPos) {}
    ~QMoveEvent() override;
    /*!
        \reimp
    */
    QMoveEvent *clone() const override { return new QMoveEvent(*this); }

    const QPoint &pos() const { return m_pos; }
    const QPoint &oldPos() const { return m_oldPos; }

private:
    QPoint m_pos;
    QPoint m_oldPos;
};

class QShowEvent : public QEvent {
public:
    QShowEvent() : QEvent(Show) {}
    ~QShowEvent() override;
    /*!
        \reimp
    */
    QShowEvent *clone() const override { return new QShowEvent(*this); }
};

class QHideEvent : public QEvent {
public:
    QHideEvent() : QEvent(Hide) {}
    ~QHideEvent() override;
    /*!
        \reimp
    */
    QHideEvent *clone() const override { return new QHideEvent(*this); }
};

class QCloseEvent : public QEvent {
public:
    QCloseEvent() : QEvent(Close) {}
    ~QCloseEvent() override;
    /*!
        \reimp
    */
    QCloseEvent *clone() const override { return new QCloseEvent(*this); }
};

class QEventPoint {
public:
    enum State : quint8 {
        Unknown = Qt::TouchPointUnknownState,
        Stationary = Qt::TouchPointStationary,
        Pressed = Qt::TouchPointPressed,
        Updated = Qt::TouchPointMoved,
        Released = Qt::TouchPointReleased
    };

    /*! Constructs a point with identifier -1 in the Unknown state at the origin. */
    QEventPoint() = default;
    QEventPoint(int pointId, State state, const QPointF &scenePosition, const QPointF &globalPosition);

    /*! Returns the identifier of the touch point, the same for the whole sequence. */
    int id() const { return m_id; }
    /*! Returns what the point did. */
    State state() const { return m_state; }
    /*! Returns the position in the coordinates of the receiver. */
    QPointF position() const { return m_position; }
    /*! Sets the position in the coordinates of the receiver to \a position. */
    void setPosition(const QPointF &position) { m_position = position; }
    /*! Returns the position in scene or window coordinates. */
    QPointF scenePosition() const { return m_scenePosition; }
    /*! Returns the position on the screen. */
    QPointF globalPosition() const { return m_globalPosition; }
    /*! Returns where the point went down, in the coordinates of the receiver. */
    QPointF pressPosition() const { return m_pressPosition; }
    /*! Sets where the point went down to \a position. */
    void setPressPosition(const QPointF &position) { m_pressPosition = position; }
    /*! Returns the pressure from 0.0 to 1.0; 1.0 if the device does not tell. */
    qreal pressure() const { return m_pressure; }
    /*! Sets the pressure to \a pressure. */
    void setPressure(qreal pressure) { m_pressure = pressure; }
    /*! Returns \c true if the point is valid. */
    bool isAccepted() const { return m_accepted; }
    /*! Sets whether the receiver took the point to \a accepted. */
    void setAccepted(bool accepted = true) { m_accepted = accepted; }

private:
    int m_id = -1;
    State m_state = Unknown;
    QPointF m_position;
    QPointF m_scenePosition;
    QPointF m_globalPosition;
    QPointF m_pressPosition;
    qreal m_pressure = 1.0;
    bool m_accepted = false;
};

class QTouchEvent : public QInputEvent {
public:
    explicit QTouchEvent(Type eventType, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
                         const QList<QEventPoint> &touchPoints = {});
    ~QTouchEvent() override;
    /*!
        \reimp
    */
    QTouchEvent *clone() const override { return new QTouchEvent(*this); }

    /*! Returns the touch points. */
    const QList<QEventPoint> &points() const { return m_points; }
    /*! Returns the number of touch points. */
    qsizetype pointCount() const { return m_points.size(); }
    /*! Returns the touch point at \a i. */
    QEventPoint &point(qsizetype i) { return m_points[i]; }
    Qt::TouchPointStates touchPointStates() const;

private:
    QList<QEventPoint> m_points;
};

class QShortcutEvent : public QEvent {
public:
    QShortcutEvent(const QKeySequence &key, int id, bool ambiguous = false);
    ~QShortcutEvent() override;

    /*! Returns the key sequence that was pressed. */
    const QKeySequence &key() const { return m_key; }
    /*! Returns the id that QWidget::grabShortcut() returned for the shortcut. */
    int shortcutId() const { return m_id; }
    /*! Returns \c true if more than one widget has the shortcut; never in Nt. */
    bool isAmbiguous() const { return m_ambiguous; }

private:
    QKeySequence m_key;
    int m_id;
    bool m_ambiguous;
};
