// MIT License - see LICENSE
#pragma once

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtCore/qpointer.h>
#include <QtCore/qrect.h>
#include <QtWidgets/qscrollerproperties.h>

#include <memory>

class QAbstractScrollArea;
class QMaterialStyle;
class QMouseEvent;
class QTimer;

class QScroller : public QObject {
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QScrollerProperties scrollerProperties READ scrollerProperties WRITE setScrollerProperties NOTIFY
                   scrollerPropertiesChanged)
public:
    enum State { Inactive, Pressed, Dragging, Scrolling };
    Q_ENUM(State)
    enum ScrollerGestureType {
        TouchGesture,
        LeftMouseButtonGesture,
        RightMouseButtonGesture,
        MiddleMouseButtonGesture
    };
    Q_ENUM(ScrollerGestureType)
    enum Input { InputPress = 1, InputMove, InputRelease };
    Q_ENUM(Input)

    static bool hasScroller(QObject *target);
    static QScroller *scroller(QObject *target);
    static const QScroller *scroller(const QObject *target);
    static Qt::GestureType grabGesture(QObject *target, ScrollerGestureType gestureType = TouchGesture);
    static Qt::GestureType grabbedGesture(QObject *target);
    static void ungrabGesture(QObject *target);
    static QList<QScroller *> activeScrollers();

    QObject *target() const { return m_target; }
    State state() const { return m_state; }
    bool handleInput(Input input, const QPointF &position, qint64 timestamp = 0);
    void stop();
    QPointF velocity() const;
    QPointF finalPosition() const;
    QPointF pixelPerMeter() const;
    QScrollerProperties scrollerProperties() const { return m_properties; }

    void setSnapPositionsX(const QList<qreal> &positions);
    void setSnapPositionsX(qreal first, qreal interval);
    void setSnapPositionsY(const QList<qreal> &positions);
    void setSnapPositionsY(qreal first, qreal interval);

public slots:
    void setScrollerProperties(const QScrollerProperties &prop);
    void scrollTo(const QPointF &pos);
    void scrollTo(const QPointF &pos, int scrollTime);
    void ensureVisible(const QRectF &rect, qreal xmargin, qreal ymargin);
    void ensureVisible(const QRectF &rect, qreal xmargin, qreal ymargin, int scrollTime);
    void resendPrepareEvent();

signals:
    /*!
        This signal is emitted when the state of the scroller changes to \a newstate.
    */
    void stateChanged(QScroller::State newstate) { emitSignal(&QScroller::stateChanged, newstate); }
    /*!
        This signal is emitted when the properties of the scroller change to \a newProperties.
    */
    void scrollerPropertiesChanged(const QScrollerProperties &newProperties) {
        emitSignal(&QScroller::scrollerPropertiesChanged, newProperties);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit QScroller(QObject *target);
    ~QScroller() override;
    Q_DISABLE_COPY(QScroller)

    // True if a press of `button` on `target` is held back, and will reach the target later or not at all. The
    // Material style, which sees events before the scroller does, then leaves the press alone.
    friend class QMaterialStyle;
    static bool holdsBackPress(const QObject *target, Qt::MouseButton button);
    Qt::MouseButton gestureButton() const;
    int pressDelay() const; // in milliseconds
    // Sends an event that was held back to the target, past the scroller.
    void deliver(QMouseEvent &event);
    void deliverHeldPress();
    void dropHeldPress();
    bool mouseInput(QMouseEvent *event);
    // The touch gesture takes every touch to see where it goes. One that stays put is a click, which the widget under
    // the finger gets as the mouse, through the window.
    void pressTap();
    void releaseTap(const QPointF &scene, const QPointF &global);
    void cancelTap();
    void sendTapMouse(bool press, const QPointF &scene, const QPointF &global);
    void setState(State state);
    QAbstractScrollArea *area() const;
    QPointF contentPosition() const; // in pixels
    QRectF contentRange() const;     // the positions that can be scrolled to, in pixels
    QPointF pixelsPerStep() const;   // pixels per step of the scroll bars
    void setContentPosition(const QPointF &position);
    qreal metric(QScrollerProperties::ScrollMetric metric) const;
    void tick();
    QPointF snapped(const QPointF &position) const;

    QPointer<QObject> m_target;
    QScrollerProperties m_properties;
    State m_state = Inactive;
    ScrollerGestureType m_gesture = TouchGesture;
    bool m_grabbed = false;
    QTimer *m_timer = nullptr;
    QTimer *m_pressTimer = nullptr;
    std::unique_ptr<QMouseEvent> m_heldPress;
    bool m_delivering = false;
    bool m_swallowRelease = false; // of a press that did no more than stop a flick
    enum class Tap { None, Held, Pressed }; // of the finger of the touch gesture: not yet a click, or its press sent
    Tap m_tap = Tap::None;
    QPointF m_tapScene; // where the finger went down, in the window and on the screen
    QPointF m_tapGlobal;
    QElapsedTimer m_clock;
    QPointF m_pressPosition; // where the press was, in the coordinates of handleInput()
    QPointF m_lastPosition;
    qint64 m_lastTime = 0;
    QPointF m_startContent; // the content position at the press
    QPointF m_velocity;     // pixels per second, in the direction the content moves
    QPointF m_position;     // the content position while it is flicked, in pixels
    QPointF m_animFrom;     // scrollTo(): from where, to where, over how long
    QPointF m_animTo;
    qint64 m_animStart = 0;
    int m_animTime = 0;
    bool m_animating = false;
    QList<qreal> m_snapX;
    QList<qreal> m_snapY;
    qreal m_snapFirstX = 0, m_snapIntervalX = 0, m_snapFirstY = 0, m_snapIntervalY = 0;
};
