// MIT License - see LICENSE
#pragma once

#include <QtCore/qcoreevent.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>

class QWidget;

class QGesture : public QObject {
    Q_OBJECT
public:
    enum GestureCancelPolicy { CancelNone = 0, CancelAllInContext };

    explicit QGesture(QObject *parent = nullptr);
    ~QGesture() override;

    Qt::GestureType gestureType() const { return m_type; }
    Qt::GestureState state() const { return m_state; }
    QPointF hotSpot() const { return m_hotSpot; }
    void setHotSpot(const QPointF &value) {
        m_hotSpot = value;
        m_hasHotSpot = true;
    }
    bool hasHotSpot() const { return m_hasHotSpot; }
    void unsetHotSpot() { m_hasHotSpot = false; }
    void setGestureCancelPolicy(GestureCancelPolicy policy) { m_policy = policy; }
    GestureCancelPolicy gestureCancelPolicy() const { return m_policy; }

protected:
    QGesture(Qt::GestureType type, QObject *parent);

private:
    friend class QGestureRecognizer;
    Qt::GestureType m_type = Qt::CustomGesture;
    Qt::GestureState m_state = Qt::NoGesture;
    QPointF m_hotSpot;
    bool m_hasHotSpot = false;
    GestureCancelPolicy m_policy = CancelNone;
};

class QPanGesture : public QGesture {
    Q_OBJECT
public:
    explicit QPanGesture(QObject *parent = nullptr);
    ~QPanGesture() override;
    QPointF lastOffset() const { return m_lastOffset; }
    void setLastOffset(const QPointF &value) { m_lastOffset = value; }
    QPointF offset() const { return m_offset; }
    void setOffset(const QPointF &value) { m_offset = value; }
    qreal acceleration() const { return m_acceleration; }
    void setAcceleration(qreal value) { m_acceleration = value; }
    QPointF delta() const { return m_offset - m_lastOffset; }

private:
    QPointF m_lastOffset{};
    QPointF m_offset{};
    qreal m_acceleration{};
};

class QPinchGesture : public QGesture {
    Q_OBJECT
public:
    enum ChangeFlag { ScaleFactorChanged = 0x1, RotationAngleChanged = 0x2, CenterPointChanged = 0x4 };
    Q_ENUM(ChangeFlag)
    Q_DECLARE_FLAGS(ChangeFlags, ChangeFlag)

    explicit QPinchGesture(QObject *parent = nullptr);
    ~QPinchGesture() override;
    QPointF startCenterPoint() const { return m_startCenterPoint; }
    void setStartCenterPoint(const QPointF &value) { m_startCenterPoint = value; }
    QPointF lastCenterPoint() const { return m_lastCenterPoint; }
    void setLastCenterPoint(const QPointF &value) { m_lastCenterPoint = value; }
    QPointF centerPoint() const { return m_centerPoint; }
    void setCenterPoint(const QPointF &value) { m_centerPoint = value; }
    qreal totalScaleFactor() const { return m_totalScaleFactor; }
    void setTotalScaleFactor(qreal value) { m_totalScaleFactor = value; }
    qreal lastScaleFactor() const { return m_lastScaleFactor; }
    void setLastScaleFactor(qreal value) { m_lastScaleFactor = value; }
    qreal scaleFactor() const { return m_scaleFactor; }
    void setScaleFactor(qreal value) { m_scaleFactor = value; }
    qreal totalRotationAngle() const { return m_totalRotationAngle; }
    void setTotalRotationAngle(qreal value) { m_totalRotationAngle = value; }
    qreal lastRotationAngle() const { return m_lastRotationAngle; }
    void setLastRotationAngle(qreal value) { m_lastRotationAngle = value; }
    qreal rotationAngle() const { return m_rotationAngle; }
    void setRotationAngle(qreal value) { m_rotationAngle = value; }
    ChangeFlags totalChangeFlags() const { return m_total; }
    void setTotalChangeFlags(ChangeFlags value) { m_total = value; }
    ChangeFlags changeFlags() const { return m_flags; }
    void setChangeFlags(ChangeFlags value) { m_flags = value; }

private:
    QPointF m_startCenterPoint{};
    QPointF m_lastCenterPoint{};
    QPointF m_centerPoint{};
    qreal m_totalScaleFactor{};
    qreal m_lastScaleFactor{};
    qreal m_scaleFactor{};
    qreal m_totalRotationAngle{};
    qreal m_lastRotationAngle{};
    qreal m_rotationAngle{};
    ChangeFlags m_total;
    ChangeFlags m_flags;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QPinchGesture::ChangeFlags)

class QSwipeGesture : public QGesture {
    Q_OBJECT
public:
    enum SwipeDirection { NoDirection, Left, Right, Up, Down };
    Q_ENUM(SwipeDirection)

    explicit QSwipeGesture(QObject *parent = nullptr);
    ~QSwipeGesture() override;
    SwipeDirection horizontalDirection() const;
    SwipeDirection verticalDirection() const;
    qreal swipeAngle() const { return m_angle; }
    void setSwipeAngle(qreal value) { m_angle = value; }

private:
    qreal m_angle = 0;
};

class QTapGesture : public QGesture {
    Q_OBJECT
public:
    explicit QTapGesture(QObject *parent = nullptr);
    ~QTapGesture() override;
    QPointF position() const { return m_position; }
    void setPosition(const QPointF &value) { m_position = value; }

private:
    QPointF m_position{};
};

class QTapAndHoldGesture : public QGesture {
    Q_OBJECT
public:
    explicit QTapAndHoldGesture(QObject *parent = nullptr);
    ~QTapAndHoldGesture() override;
    QPointF position() const { return m_position; }
    void setPosition(const QPointF &value) { m_position = value; }
    static void setTimeout(int msecs) { s_timeout = msecs; }
    static int timeout() { return s_timeout; }

private:
    QPointF m_position{};
    static inline int s_timeout = 700;
};

class QGestureEvent : public QEvent {
public:
    explicit QGestureEvent(const QList<QGesture *> &gestures);
    ~QGestureEvent() override;

    QList<QGesture *> gestures() const { return m_gestures; }
    QGesture *gesture(Qt::GestureType type) const;
    QList<QGesture *> activeGestures() const;
    QList<QGesture *> canceledGestures() const;

    using QEvent::accept;
    using QEvent::ignore;
    using QEvent::isAccepted;
    using QEvent::setAccepted;

    void setAccepted(QGesture *gesture, bool value);
    void accept(QGesture *gesture) { setAccepted(gesture, true); }
    void ignore(QGesture *gesture) { setAccepted(gesture, false); }
    bool isAccepted(QGesture *gesture) const;

    void setAccepted(Qt::GestureType type, bool value);
    void accept(Qt::GestureType type) { setAccepted(type, true); }
    void ignore(Qt::GestureType type) { setAccepted(type, false); }
    bool isAccepted(Qt::GestureType type) const;

    void setWidget(QWidget *widget) { m_widget = widget; }
    QWidget *widget() const { return m_widget; }

private:
    QList<QGesture *> m_gestures;
    QWidget *m_widget = nullptr;
    QMap<Qt::GestureType, bool> m_accepted;
};
