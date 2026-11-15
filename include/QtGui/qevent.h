// MIT License - see LICENSE
#pragma once

#include <QtCore/qcoreevent.h>
#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
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
