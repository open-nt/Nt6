// MIT License - see LICENSE
#include <QtWidgets/qgesture.h>

#include <QtWidgets/qgesturerecognizer.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

/*!
    \class QGesture
    \brief The QGesture class is the base class of gestures: a movement of the pointer or of fingers that has a meaning.

    A recognizer fills a gesture while it recognizes it, and a QGestureEvent carries it to the widget:

    \code
    bool Canvas::event(QEvent *event) {
        if (event->type() == QEvent::Gesture) {
            auto *gestureEvent = static_cast<QGestureEvent *>(event);
            if (QGesture *pan = gestureEvent->gesture(Qt::PanGesture))
                scroll(static_cast<QPanGesture *>(pan)->delta());
        }
        return QWidget::event(event);
    }
    \endcode

    \differences
    Nt does not recognize gestures itself: no widget gets a QEvent::Gesture from the touch or mouse input, and
    \c grabGesture() does not exist. The classes are the data that a QGestureRecognizer of the application can fill, with
    no properties for the meta-object system.
    \enddifferences

    \sa QGestureEvent, QGestureRecognizer
*/

/*!
    \enum QGesture::GestureCancelPolicy

    What happens to the other gestures in the context when this one starts.

    \value CancelNone Nothing.
    \value CancelAllInContext All the other gestures in the context are canceled.
*/

/*!
    Constructs a gesture with the given \a parent.
*/
QGesture::QGesture(QObject *parent) : QObject(parent) {}

/*!
    \internal
*/
QGesture::QGesture(Qt::GestureType type, QObject *parent) : QObject(parent), m_type(type) {}

/*!
    Destroys the gesture.
*/
QGesture::~QGesture() = default;

/*!
    \fn Qt::GestureType QGesture::gestureType() const

    Returns the type of the gesture; Qt::CustomGesture until a recognizer sets it.
*/

/*!
    \fn Qt::GestureState QGesture::state() const

    Returns the state of the gesture.
*/

/*!
    \fn QPointF QGesture::hotSpot() const

    Returns the point that the gesture is about, in screen coordinates.
*/

/*!
    \fn void QGesture::setHotSpot(const QPointF &value)

    Sets the hot spot to \a value.
*/

/*!
    \fn bool QGesture::hasHotSpot() const

    Returns whether a hot spot was set.
*/

/*!
    \fn void QGesture::unsetHotSpot()

    Removes the hot spot.
*/

/*!
    \fn void QGesture::setGestureCancelPolicy(GestureCancelPolicy policy)

    Sets what happens to the other gestures when this one starts to \a policy.
*/

/*!
    \fn QGesture::GestureCancelPolicy QGesture::gestureCancelPolicy() const

    Returns what happens to the other gestures when this one starts.
*/

/*!
    \class QPanGesture
    \brief The QPanGesture class describes a pan gesture.

    \differences
    See QGesture.
    \enddifferences

    \sa QGesture
*/

/*!
    Constructs a pan gesture with the given \a parent.
*/
QPanGesture::QPanGesture(QObject *parent) : QGesture(Qt::PanGesture, parent) {}

/*!
    Destroys the gesture.
*/
QPanGesture::~QPanGesture() = default;

/*!
    \fn QPointF QPanGesture::lastOffset() const

    Returns the lastOffset of the gesture.
*/

/*!
    \fn void QPanGesture::setLastOffset(const QPointF &value)

    Sets the lastOffset to \a value.
*/

/*!
    \fn QPointF QPanGesture::offset() const

    Returns the offset of the gesture.
*/

/*!
    \fn void QPanGesture::setOffset(const QPointF &value)

    Sets the offset to \a value.
*/

/*!
    \fn qreal QPanGesture::acceleration() const

    Returns the acceleration of the gesture.
*/

/*!
    \fn void QPanGesture::setAcceleration(qreal value)

    Sets the acceleration to \a value.
*/

/*!
    \fn QPointF QPanGesture::delta() const

    Returns the movement since the last update: the offset minus the last offset.
*/

/*!
    \class QPinchGesture
    \brief The QPinchGesture class describes a pinch gesture.

    \differences
    See QGesture.
    \enddifferences

    \sa QGesture
*/

/*!
    \enum QPinchGesture::ChangeFlag

    What changed since the last update. ChangeFlags is a typedef for \c{QFlags<ChangeFlag>}.

    \value ScaleFactorChanged The scale factor changed.
    \value RotationAngleChanged The rotation angle changed.
    \value CenterPointChanged The center point changed.
*/

/*!
    Constructs a pinch gesture with the given \a parent.
*/
QPinchGesture::QPinchGesture(QObject *parent) : QGesture(Qt::PinchGesture, parent) {}

/*!
    Destroys the gesture.
*/
QPinchGesture::~QPinchGesture() = default;

/*!
    \fn QPointF QPinchGesture::startCenterPoint() const

    Returns the startCenterPoint of the gesture.
*/

/*!
    \fn void QPinchGesture::setStartCenterPoint(const QPointF &value)

    Sets the startCenterPoint to \a value.
*/

/*!
    \fn QPointF QPinchGesture::lastCenterPoint() const

    Returns the lastCenterPoint of the gesture.
*/

/*!
    \fn void QPinchGesture::setLastCenterPoint(const QPointF &value)

    Sets the lastCenterPoint to \a value.
*/

/*!
    \fn QPointF QPinchGesture::centerPoint() const

    Returns the centerPoint of the gesture.
*/

/*!
    \fn void QPinchGesture::setCenterPoint(const QPointF &value)

    Sets the centerPoint to \a value.
*/

/*!
    \fn qreal QPinchGesture::totalScaleFactor() const

    Returns the totalScaleFactor of the gesture.
*/

/*!
    \fn void QPinchGesture::setTotalScaleFactor(qreal value)

    Sets the totalScaleFactor to \a value.
*/

/*!
    \fn qreal QPinchGesture::lastScaleFactor() const

    Returns the lastScaleFactor of the gesture.
*/

/*!
    \fn void QPinchGesture::setLastScaleFactor(qreal value)

    Sets the lastScaleFactor to \a value.
*/

/*!
    \fn qreal QPinchGesture::scaleFactor() const

    Returns the scaleFactor of the gesture.
*/

/*!
    \fn void QPinchGesture::setScaleFactor(qreal value)

    Sets the scaleFactor to \a value.
*/

/*!
    \fn qreal QPinchGesture::totalRotationAngle() const

    Returns the totalRotationAngle of the gesture.
*/

/*!
    \fn void QPinchGesture::setTotalRotationAngle(qreal value)

    Sets the totalRotationAngle to \a value.
*/

/*!
    \fn qreal QPinchGesture::lastRotationAngle() const

    Returns the lastRotationAngle of the gesture.
*/

/*!
    \fn void QPinchGesture::setLastRotationAngle(qreal value)

    Sets the lastRotationAngle to \a value.
*/

/*!
    \fn qreal QPinchGesture::rotationAngle() const

    Returns the rotationAngle of the gesture.
*/

/*!
    \fn void QPinchGesture::setRotationAngle(qreal value)

    Sets the rotationAngle to \a value.
*/

/*!
    \fn QPinchGesture::ChangeFlags QPinchGesture::totalChangeFlags() const

    Returns what changed since the gesture started.
*/

/*!
    \fn void QPinchGesture::setTotalChangeFlags(ChangeFlags value)

    Sets what changed since the gesture started to \a value.
*/

/*!
    \fn QPinchGesture::ChangeFlags QPinchGesture::changeFlags() const

    Returns what changed since the last update.
*/

/*!
    \fn void QPinchGesture::setChangeFlags(ChangeFlags value)

    Sets what changed since the last update to \a value.
*/

/*!
    \class QSwipeGesture
    \brief The QSwipeGesture class describes a swipe gesture.

    \differences
    See QGesture.
    \enddifferences

    \sa QGesture
*/

/*!
    \enum QSwipeGesture::SwipeDirection

    The direction of a swipe.

    \value NoDirection No direction.
    \value Left To the left.
    \value Right To the right.
    \value Up Up.
    \value Down Down.
*/

/*!
    Constructs a swipe gesture with the given \a parent.
*/
QSwipeGesture::QSwipeGesture(QObject *parent) : QGesture(Qt::SwipeGesture, parent) {}

/*!
    Destroys the gesture.
*/
QSwipeGesture::~QSwipeGesture() = default;

/*!
    Returns Left or Right for a swipe that is mostly horizontal, from the angle, and NoDirection otherwise.
*/
QSwipeGesture::SwipeDirection QSwipeGesture::horizontalDirection() const {
    const qreal a = std::fmod(m_angle, 360.0);
    if (a < 0 ? (a > -45 || a < -315) : (a < 45 || a > 315))
        return Right;
    if (std::abs(std::abs(a) - 180) < 45)
        return Left;
    return NoDirection;
}

/*!
    Returns Up or Down for a swipe that is mostly vertical, from the angle, and NoDirection otherwise.
*/
QSwipeGesture::SwipeDirection QSwipeGesture::verticalDirection() const {
    const qreal a = std::fmod(m_angle < 0 ? m_angle + 360 : m_angle, 360.0);
    if (std::abs(a - 90) < 45)
        return Up;
    if (std::abs(a - 270) < 45)
        return Down;
    return NoDirection;
}

/*!
    \fn qreal QSwipeGesture::swipeAngle() const

    Returns the angle of the swipe in degrees, counterclockwise from the right.
*/

/*!
    \fn void QSwipeGesture::setSwipeAngle(qreal value)

    Sets the angle of the swipe to \a value degrees.
*/

/*!
    \class QTapGesture
    \brief The QTapGesture class describes a tap gesture.

    \differences
    See QGesture.
    \enddifferences

    \sa QGesture
*/

/*!
    Constructs a tap gesture with the given \a parent.
*/
QTapGesture::QTapGesture(QObject *parent) : QGesture(Qt::TapGesture, parent) {}

/*!
    Destroys the gesture.
*/
QTapGesture::~QTapGesture() = default;

/*!
    \fn QPointF QTapGesture::position() const

    Returns the position of the gesture.
*/

/*!
    \fn void QTapGesture::setPosition(const QPointF &value)

    Sets the position to \a value.
*/

/*!
    \class QTapAndHoldGesture
    \brief The QTapAndHoldGesture class describes a tap and hold gesture.

    \differences
    See QGesture.
    \enddifferences

    \sa QGesture
*/

/*!
    Constructs a tap and hold gesture with the given \a parent.
*/
QTapAndHoldGesture::QTapAndHoldGesture(QObject *parent) : QGesture(Qt::TapAndHoldGesture, parent) {}

/*!
    Destroys the gesture.
*/
QTapAndHoldGesture::~QTapAndHoldGesture() = default;

/*!
    \fn QPointF QTapAndHoldGesture::position() const

    Returns the position of the gesture.
*/

/*!
    \fn void QTapAndHoldGesture::setPosition(const QPointF &value)

    Sets the position to \a value.
*/

/*!
    \fn void QTapAndHoldGesture::setTimeout(int msecs)

    Sets the time in milliseconds that a tap has to be held to \a msecs.
*/

/*!
    \fn int QTapAndHoldGesture::timeout()

    Returns the time in milliseconds that a tap has to be held, 700 by default.
*/

/*!
    \class QGestureEvent
    \brief The QGestureEvent class carries the gestures that were recognized to a widget.

    Accepting the event, or one gesture of it, tells the sender that the widget uses the gesture.

    \differences
    There is no \c mapToGraphicsScene() and no target widgets for single gestures.
    \enddifferences

    \sa QGesture, QGestureRecognizer
*/

/*!
    Constructs an event for the \a gestures.
*/
QGestureEvent::QGestureEvent(const QList<QGesture *> &gestures) : QEvent(Gesture), m_gestures(gestures) {
    for (QGesture *g : gestures)
        m_accepted.insert(g->gestureType(), false);
    setAccepted(false);
}

/*!
    Destroys the event.
*/
QGestureEvent::~QGestureEvent() = default;

/*!
    \fn QList<QGesture *> QGestureEvent::gestures() const

    Returns all the gestures of the event.
*/

/*!
    Returns the gesture of the \a type, or \c nullptr if there is none.
*/
QGesture *QGestureEvent::gesture(Qt::GestureType type) const {
    for (QGesture *g : m_gestures)
        if (g->gestureType() == type)
            return g;
    return nullptr;
}

/*!
    Returns the gestures that are not finished or canceled.
*/
QList<QGesture *> QGestureEvent::activeGestures() const {
    QList<QGesture *> result;
    for (QGesture *g : m_gestures)
        if (g->state() != Qt::GestureCanceled)
            result.append(g);
    return result;
}

/*!
    Returns the gestures that were canceled.
*/
QList<QGesture *> QGestureEvent::canceledGestures() const {
    QList<QGesture *> result;
    for (QGesture *g : m_gestures)
        if (g->state() == Qt::GestureCanceled)
            result.append(g);
    return result;
}

/*!
    Accepts the \a gesture if \a value is \c true, and ignores it otherwise.
*/
void QGestureEvent::setAccepted(QGesture *gesture, bool value) {
    if (gesture)
        setAccepted(gesture->gestureType(), value);
}

/*!
    \fn void QGestureEvent::accept(QGesture *gesture)

    Accepts the \a gesture.
*/

/*!
    \fn void QGestureEvent::ignore(QGesture *gesture)

    Ignores the \a gesture.
*/

/*!
    Returns whether the \a gesture was accepted.
*/
bool QGestureEvent::isAccepted(QGesture *gesture) const { return gesture && isAccepted(gesture->gestureType()); }

/*!
    Accepts the gestures of the \a type if \a value is \c true, and ignores them otherwise.
*/
void QGestureEvent::setAccepted(Qt::GestureType type, bool value) {
    m_accepted.insert(type, value);
    QEvent::setAccepted(value);
}

/*!
    \fn void QGestureEvent::accept(Qt::GestureType type)

    Accepts the gestures of the \a type.
*/

/*!
    \fn void QGestureEvent::ignore(Qt::GestureType type)

    Ignores the gestures of the \a type.
*/

/*!
    Returns whether the gestures of the \a type were accepted.
*/
bool QGestureEvent::isAccepted(Qt::GestureType type) const { return m_accepted.value(type, false); }

/*!
    \fn void QGestureEvent::setWidget(QWidget *widget)

    Sets the widget that gets the event to \a widget.
*/

/*!
    \fn QWidget *QGestureEvent::widget() const

    Returns the widget that gets the event.
*/

/*!
    \class QGestureRecognizer
    \brief The QGestureRecognizer class is the base class of the objects that recognize gestures from events.

    Subclass it, create the gesture in create() and follow the events in recognize(). \l registerRecognizer() gives it
    a gesture type.

    \differences
    The recognizers are kept in a list, but no widget asks them: Nt does not feed events to the registered recognizers.
    \enddifferences

    \sa QGesture
*/

/*!
    \enum QGestureRecognizer::ResultFlag

    What recognize() says about the event. Result is a typedef for \c{QFlags<ResultFlag>}.

    \value Ignore The event is not part of the gesture.
    \value MayBeGesture The event may be the start of the gesture.
    \value TriggerGesture The gesture starts.
    \value FinishGesture The gesture is finished.
    \value CancelGesture The gesture is canceled.
    \value ResultState_Mask The bits of the state.
    \value ConsumeEventHint The event is not passed on.
    \value ResultHint_Mask The bits of the hints.
*/

namespace {
std::vector<std::unique_ptr<QGestureRecognizer>> &recognizers() {
    static std::vector<std::unique_ptr<QGestureRecognizer>> list;
    return list;
}
} // namespace

/*!
    Constructs a recognizer.
*/
QGestureRecognizer::QGestureRecognizer() = default;

/*!
    Destroys the recognizer.
*/
QGestureRecognizer::~QGestureRecognizer() = default;

/*!
    Creates the gesture for the \a target. The default makes a QGesture.
*/
QGesture *QGestureRecognizer::create(QObject *target) {
    (void)target;
    return new QGesture;
}

/*!
    \fn QGestureRecognizer::Result QGestureRecognizer::recognize(QGesture *state, QObject *watched, QEvent *event)

    Handles the \a event for the \a watched object, updates the gesture \a state, and returns what the event means.
*/

/*!
    Sets the \a state of a gesture back to the start. The default does nothing.
*/
void QGestureRecognizer::reset(QGesture *state) { (void)state; }

/*!
    Registers the \a recognizer, which the application then owns no more, and returns the type of its gestures: a new
    value from Qt::CustomGesture up.
*/
Qt::GestureType QGestureRecognizer::registerRecognizer(QGestureRecognizer *recognizer) {
    recognizers().emplace_back(recognizer);
    return static_cast<Qt::GestureType>(static_cast<unsigned>(Qt::CustomGesture) + recognizers().size());
}

/*!
    Unregisters and deletes the recognizer of the gesture \a type.
*/
void QGestureRecognizer::unregisterRecognizer(Qt::GestureType type) {
    const unsigned index = static_cast<unsigned>(type) - static_cast<unsigned>(Qt::CustomGesture);
    if (index >= 1 && index <= recognizers().size())
        recognizers()[index - 1].reset();
}

#include "moc_qgesture.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
