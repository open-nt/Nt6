// MIT License - see LICENSE
#include <QtWidgets/qscroller.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qeasingcurve.h>
#include <QtCore/qhash.h>
#include <QtCore/qtimer.h>
#include <QtGui/qevent.h>
#include <QtWidgets/qabstractscrollarea.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qwidget.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace {

constexpr qreal kInchesPerMeter = 1.0 / 0.0254;
constexpr qreal kDecelerationScale = 10.0; // m/s² of deceleration for a DecelerationFactor of 1

std::unique_ptr<QScrollerProperties> &defaultProperties() {
    static std::unique_ptr<QScrollerProperties> properties;
    return properties;
}

QVariant builtInMetric(QScrollerProperties::ScrollMetric metric) {
    switch (metric) {
    case QScrollerProperties::MousePressEventDelay:
        return 0.25;
    case QScrollerProperties::DragStartDistance:
        return 0.005;
    case QScrollerProperties::DragVelocitySmoothingFactor:
        return 0.8;
    case QScrollerProperties::AxisLockThreshold:
        return 0.0;
    case QScrollerProperties::ScrollingCurve:
        return QVariant::fromValue(QEasingCurve(QEasingCurve::OutQuad));
    case QScrollerProperties::DecelerationFactor:
        return 0.125;
    case QScrollerProperties::MinimumVelocity:
        return 0.05;
    case QScrollerProperties::MaximumVelocity:
        return 0.5;
    case QScrollerProperties::MaximumClickThroughVelocity:
        return 0.066;
    case QScrollerProperties::AcceleratingFlickMaximumTime:
        return 1.25;
    case QScrollerProperties::AcceleratingFlickSpeedupFactor:
        return 3.0;
    case QScrollerProperties::SnapPositionRatio:
        return 0.5;
    case QScrollerProperties::SnapTime:
        return 0.3;
    case QScrollerProperties::OvershootDragResistanceFactor:
        return 0.5;
    case QScrollerProperties::OvershootDragDistanceFactor:
        return 1.0;
    case QScrollerProperties::OvershootScrollDistanceFactor:
        return 0.5;
    case QScrollerProperties::OvershootScrollTime:
        return 0.7;
    case QScrollerProperties::HorizontalOvershootPolicy:
    case QScrollerProperties::VerticalOvershootPolicy:
        return QVariant::fromValue(QScrollerProperties::OvershootWhenScrollable);
    case QScrollerProperties::FrameRate:
        return QVariant::fromValue(QScrollerProperties::Standard);
    case QScrollerProperties::ScrollMetricCount:
        break;
    }
    return QVariant();
}

QHash<const QObject *, QScroller *> &scrollers() {
    static QHash<const QObject *, QScroller *> map;
    return map;
}

// The snap position nearest to `value`, from a list or from a first position and an interval; `value` if there
// are none.
qreal snapValue(qreal value, const QList<qreal> &list, qreal first, qreal interval) {
    if (!list.isEmpty()) {
        return *std::min_element(list.cbegin(), list.cend(),
                                 [value](qreal a, qreal b) { return std::fabs(a - value) < std::fabs(b - value); });
    }
    if (interval > 0)
        return first + std::round((value - first) / interval) * interval;
    return value;
}

} // namespace

/*!
    \class QScrollerProperties
    \inmodule NtWidgets
    \since 6.0
    \brief The QScrollerProperties class holds the settings of a QScroller.

    Distances are in meters and velocities in meters per second, so that scrolling feels the same on screens of any
    density. Change a copy and give it back with QScroller::setScrollerProperties():

    \code
    QScrollerProperties props = scroller->scrollerProperties();
    props.setScrollMetric(QScrollerProperties::DragStartDistance, 0.004);
    scroller->setScrollerProperties(props);
    \endcode

    \differences
    The scroller uses DragStartDistance, DragVelocitySmoothingFactor, DecelerationFactor, MinimumVelocity,
    MaximumVelocity, ScrollingCurve, SnapTime, FrameRate and, for the mouse button gestures, MousePressEventDelay.
    The others are kept but have no effect: there is no overshoot and no accelerating flick.
    \enddifferences

    \sa QScroller
*/

/*!
    \enum QScrollerProperties::OvershootPolicy

    Whether the content may be pulled beyond its end.

    \value OvershootWhenScrollable When there is something to scroll.
    \value OvershootAlwaysOff Never.
    \value OvershootAlwaysOn Always.
*/

/*!
    \enum QScrollerProperties::FrameRates

    How often a scrolling content moves.

    \value Standard 60 times a second.
    \value Fps60 60 times a second.
    \value Fps30 30 times a second.
    \value Fps20 20 times a second.
*/

/*!
    \enum QScrollerProperties::ScrollMetric

    The settings of a scroller.

    \value MousePressEventDelay How long a press is held back to see whether it starts a drag, in seconds.
    \value DragStartDistance How far the press must move to start dragging, in meters.
    \value DragVelocitySmoothingFactor How much a new speed of the drag counts against the speed so far, from 0 to 1.
    \value AxisLockThreshold How much more one direction must be moved than the other to lock scrolling to it.
    \value ScrollingCurve The QEasingCurve of scrollTo().
    \value DecelerationFactor How fast a flicked content slows down.
    \value MinimumVelocity The slowest release that flicks the content, in meters per second.
    \value MaximumVelocity The fastest the content moves, in meters per second.
    \value MaximumClickThroughVelocity The fastest scrolling that a press stops and lets through.
    \value AcceleratingFlickMaximumTime The time within which a new flick speeds up the scrolling one.
    \value AcceleratingFlickSpeedupFactor How much such a flick speeds it up.
    \value SnapPositionRatio How far towards the next snap position the content must be to go to it.
    \value SnapTime How long moving to a snap position takes, in seconds.
    \value OvershootDragResistanceFactor How much harder the content is to drag beyond its end.
    \value OvershootDragDistanceFactor How far the content can be dragged beyond its end.
    \value OvershootScrollDistanceFactor How far a flicked content goes beyond its end.
    \value OvershootScrollTime How long it takes to come back from beyond its end, in seconds.
    \value HorizontalOvershootPolicy The OvershootPolicy across.
    \value VerticalOvershootPolicy The OvershootPolicy down.
    \value FrameRate The FrameRates of moving content.
    \omitvalue ScrollMetricCount
*/

/*!
    Constructs properties with the default values: those of setDefaultScrollerProperties(), or the built-in ones.
*/
QScrollerProperties::QScrollerProperties() {
    if (defaultProperties())
        m_values = defaultProperties()->m_values;
}

/*!
    \fn QScrollerProperties::QScrollerProperties(const QScrollerProperties &other)

    Constructs a copy of \a other.
*/

/*!
    \fn QScrollerProperties &QScrollerProperties::operator=(const QScrollerProperties &other)

    Makes these properties a copy of \a other.
*/

/*!
    \fn QScrollerProperties::~QScrollerProperties()

    Destroys the properties.
*/

/*!
    \fn bool QScrollerProperties::operator==(const QScrollerProperties &other) const

    Returns \c true if these properties are the same as \a other.
*/

/*!
    \fn bool QScrollerProperties::operator!=(const QScrollerProperties &other) const

    Returns \c true if these properties differ from \a other.
*/

/*!
    Makes \a sp the properties that new QScrollerProperties objects, and new scrollers, start with.
*/
void QScrollerProperties::setDefaultScrollerProperties(const QScrollerProperties &sp) {
    defaultProperties() = std::make_unique<QScrollerProperties>(sp);
}

/*!
    Brings the built-in defaults back for new QScrollerProperties objects.
*/
void QScrollerProperties::unsetDefaultScrollerProperties() { defaultProperties().reset(); }

/*!
    Returns the value of \a metric.
*/
QVariant QScrollerProperties::scrollMetric(ScrollMetric metric) const {
    const auto it = m_values.constFind(metric);
    return it != m_values.constEnd() ? *it : builtInMetric(metric);
}

/*!
    Sets \a metric to \a value.
*/
void QScrollerProperties::setScrollMetric(ScrollMetric metric, const QVariant &value) {
    if (metric >= 0 && metric < ScrollMetricCount)
        m_values.insert(metric, value);
}

/*!
    \class QScroller
    \inmodule NtWidgets
    \since 6.0
    \brief The QScroller class scrolls a widget when the user drags or flicks it.

    grabGesture() makes a scroll area, or its viewport, scroll with the finger or with a mouse button: the content
    follows the drag, and goes on moving and slows down after a flick.

    \code
    QScroller::grabGesture(list->viewport(), QScroller::LeftMouseButtonGesture);
    connect(QScroller::scroller(list->viewport()), &QScroller::stateChanged, this, [](QScroller::State state) {
        qDebug() << state;
    });
    \endcode

    With a mouse button gesture the press is held back for \c MousePressEventDelay, a quarter of a second. If a drag
    starts in that time, the target never gets the press, so that scrolling a list does not select a row. Otherwise
    the target gets it when the time is up, or with the release if that comes first.

    \differences
    The target must be a QAbstractScrollArea or its viewport, whose scroll bars are moved; Nt sends no
    \c QScrollPrepareEvent or \c QScrollEvent, so other widgets and graphics objects cannot be scrolled. There is no
    overshoot. If a drag starts after the target got the press, the scroller takes the moves and the release, and the
    target is not told that the press has ended. The gesture is not a QGesture; grabGesture() returns Qt::CustomGesture.
    \enddifferences

    \sa QScrollerProperties
*/

/*!
    \enum QScroller::State

    \value Inactive Nothing happens.
    \value Pressed The target is pressed, but not dragged yet.
    \value Dragging The content follows the drag.
    \value Scrolling The content moves on its own, after a flick or because of scrollTo().
*/

/*!
    \enum QScroller::ScrollerGestureType

    What makes a scroller scroll.

    \value TouchGesture A finger on a touch screen.
    \value LeftMouseButtonGesture The left mouse button.
    \value RightMouseButtonGesture The right mouse button.
    \value MiddleMouseButtonGesture The middle mouse button.
*/

/*!
    \enum QScroller::Input

    The input that handleInput() gets.

    \value InputPress The press.
    \value InputMove A move while pressed.
    \value InputRelease The release.
*/

QScroller::QScroller(QObject *target) : m_target(target) {
    m_clock.start();
    m_timer = new QTimer(this);
    QObject::connect(m_timer, &QTimer::timeout, this, [this] { tick(); });
    m_pressTimer = new QTimer(this);
    m_pressTimer->setSingleShot(true);
    QObject::connect(m_pressTimer, &QTimer::timeout, this, [this] { deliverHeldPress(); });
    QObject::connect(target, &QObject::destroyed, this, [this, target] {
        scrollers().remove(target);
        m_target = nullptr;
        delete this;
    });
}

QScroller::~QScroller() {
    if (m_target) {
        m_target->removeEventFilter(this);
        scrollers().remove(m_target.data());
    }
}

/*!
    Returns \c true if \a target has a scroller.
*/
bool QScroller::hasScroller(QObject *target) { return scrollers().contains(target); }

/*!
    Returns the scroller of \a target, and makes one if it has none. It is deleted with \a target.
*/
QScroller *QScroller::scroller(QObject *target) {
    if (!target)
        return nullptr;
    QScroller *&s = scrollers()[target];
    if (!s)
        s = new QScroller(target);
    return s;
}

/*!
    \overload

    Returns the scroller of \a target, or \c nullptr if it has none.
*/
const QScroller *QScroller::scroller(const QObject *target) { return scrollers().value(target); }

/*!
    Makes \a target scroll with \a gestureType, and returns the gesture type it was given. A touch gesture makes the
    widget accept touch events.

    \sa ungrabGesture()
*/
Qt::GestureType QScroller::grabGesture(QObject *target, ScrollerGestureType gestureType) {
    QScroller *s = scroller(target);
    if (!s)
        return Qt::GestureType{};
    if (!s->m_grabbed)
        target->installEventFilter(s);
    s->m_grabbed = true;
    s->m_gesture = gestureType;
    if (gestureType == TouchGesture)
        if (auto *widget = qobject_cast<QWidget *>(target))
            widget->setAttribute(Qt::WA_AcceptTouchEvents);
    return Qt::CustomGesture;
}

/*!
    Returns the gesture type that grabGesture() gave \a target, or 0 if it has none.
*/
Qt::GestureType QScroller::grabbedGesture(QObject *target) {
    const QScroller *s = scrollers().value(target);
    return s && s->m_grabbed ? Qt::CustomGesture : Qt::GestureType{};
}

/*!
    Stops \a target from scrolling with its gesture.
*/
void QScroller::ungrabGesture(QObject *target) {
    QScroller *s = scrollers().value(target);
    if (!s || !s->m_grabbed)
        return;
    target->removeEventFilter(s);
    s->m_grabbed = false;
    s->dropHeldPress();
    s->m_swallowRelease = false;
    s->stop();
}

/*!
    Returns the scrollers that are not Inactive.
*/
QList<QScroller *> QScroller::activeScrollers() {
    QList<QScroller *> active;
    for (QScroller *s : std::as_const(scrollers()))
        if (s->m_state != Inactive)
            active.append(s);
    return active;
}

/*!
    \fn QObject *QScroller::target() const

    Returns the object that the scroller scrolls.
*/

/*!
    \fn QScroller::State QScroller::state() const

    Returns what the scroller is doing.
*/

/*!
    \fn QScrollerProperties QScroller::scrollerProperties() const

    Returns the settings of the scroller.
*/

void QScroller::setState(State state) {
    if (state == m_state)
        return;
    m_state = state;
    if (state != Scrolling) {
        m_timer->stop();
        m_animating = false;
    }
    emit stateChanged(state);
}

QAbstractScrollArea *QScroller::area() const {
    if (auto *a = qobject_cast<QAbstractScrollArea *>(m_target.data()))
        return a;
    return m_target ? qobject_cast<QAbstractScrollArea *>(m_target->parent()) : nullptr;
}

QPointF QScroller::pixelsPerStep() const {
    QAbstractScrollArea *a = area();
    if (!a)
        return QPointF(1, 1);
    const QSize view = a->viewport()->size();
    const int hPage = a->horizontalScrollBar()->pageStep();
    const int vPage = a->verticalScrollBar()->pageStep();
    return QPointF(hPage > 0 && view.width() > 0 ? qreal(view.width()) / hPage : 1.0,
                   vPage > 0 && view.height() > 0 ? qreal(view.height()) / vPage : 1.0);
}

QPointF QScroller::contentPosition() const {
    QAbstractScrollArea *a = area();
    if (!a)
        return QPointF();
    const QPointF step = pixelsPerStep();
    return QPointF(a->horizontalScrollBar()->value() * step.x(), a->verticalScrollBar()->value() * step.y());
}

QRectF QScroller::contentRange() const {
    QAbstractScrollArea *a = area();
    if (!a)
        return QRectF();
    const QPointF step = pixelsPerStep();
    const QScrollBar *h = a->horizontalScrollBar();
    const QScrollBar *v = a->verticalScrollBar();
    return QRectF(QPointF(h->minimum() * step.x(), v->minimum() * step.y()),
                  QPointF(h->maximum() * step.x(), v->maximum() * step.y()));
}

void QScroller::setContentPosition(const QPointF &position) {
    QAbstractScrollArea *a = area();
    if (!a)
        return;
    const QRectF range = contentRange();
    const QPointF step = pixelsPerStep();
    const qreal x = std::clamp(position.x(), range.left(), range.right());
    const qreal y = std::clamp(position.y(), range.top(), range.bottom());
    a->horizontalScrollBar()->setValue(static_cast<int>(std::lround(x / step.x())));
    a->verticalScrollBar()->setValue(static_cast<int>(std::lround(y / step.y())));
}

qreal QScroller::metric(QScrollerProperties::ScrollMetric metric) const {
    return m_properties.scrollMetric(metric).toDouble();
}

/*!
    Returns the pixels per meter of the screen of the target, across and down.
*/
QPointF QScroller::pixelPerMeter() const {
    if (auto *widget = qobject_cast<QWidget *>(m_target.data()))
        return QPointF(widget->logicalDpiX() * kInchesPerMeter, widget->logicalDpiY() * kInchesPerMeter);
    return QPointF(96 * kInchesPerMeter, 96 * kInchesPerMeter);
}

/*!
    Returns the speed of the content in meters per second, across and down.
*/
QPointF QScroller::velocity() const {
    const QPointF ppm = pixelPerMeter();
    return QPointF(m_velocity.x() / ppm.x(), m_velocity.y() / ppm.y());
}

/*!
    Returns where the content will stop, in pixels: the current position unless it is Scrolling.
*/
QPointF QScroller::finalPosition() const {
    if (m_state != Scrolling)
        return contentPosition();
    if (m_animating)
        return m_animTo;
    // A flick slows down evenly: it goes v² / 2a further.
    const qreal speed = std::hypot(m_velocity.x(), m_velocity.y());
    const qreal deceleration =
        metric(QScrollerProperties::DecelerationFactor) * kDecelerationScale * pixelPerMeter().y();
    if (speed <= 0 || deceleration <= 0)
        return contentPosition();
    const qreal distance = speed * speed / (2 * deceleration);
    const QRectF range = contentRange();
    const QPointF end = m_position + m_velocity / speed * distance;
    return snapped(
        QPointF(std::clamp(end.x(), range.left(), range.right()), std::clamp(end.y(), range.top(), range.bottom())));
}

/*!
    Feeds \a input at \a position, in the coordinates of the target, at \a timestamp milliseconds, to the scroller,
    as its gesture does. Returns \c true if the scroller used the input, so that the target should not get it.
*/
bool QScroller::handleInput(Input input, const QPointF &position, qint64 timestamp) {
    const qint64 now = timestamp > 0 ? timestamp : m_clock.elapsed();
    switch (input) {
    case InputPress: {
        const bool wasScrolling = m_state == Scrolling;
        m_timer->stop();
        m_animating = false;
        m_pressPosition = m_lastPosition = position;
        m_lastTime = now;
        m_startContent = contentPosition();
        m_velocity = QPointF();
        setState(Pressed);
        return wasScrolling; // a press that stops a flick goes no further
    }
    case InputMove: {
        if (m_state == Pressed) {
            const QPointF moved = position - m_pressPosition;
            const qreal start = metric(QScrollerProperties::DragStartDistance) * pixelPerMeter().y();
            if (std::hypot(moved.x(), moved.y()) < start)
                return false;
            m_pressPosition = position;
            m_startContent = contentPosition();
            setState(Dragging);
        }
        if (m_state != Dragging)
            return false;
        setContentPosition(m_startContent - (position - m_pressPosition));
        const qint64 dt = now - m_lastTime;
        if (dt > 0) {
            const QPointF instant = -(position - m_lastPosition) * (1000.0 / static_cast<qreal>(dt));
            const qreal f = std::clamp(metric(QScrollerProperties::DragVelocitySmoothingFactor), 0.0, 1.0);
            m_velocity = m_velocity * (1 - f) + instant * f;
            m_lastPosition = position;
            m_lastTime = now;
        }
        return true;
    }
    case InputRelease: {
        if (m_state == Pressed) {
            setState(Inactive);
            return false;
        }
        if (m_state != Dragging)
            return false;
        if (now - m_lastTime > 100) // held still before the release: no flick
            m_velocity = QPointF();
        const QPointF ppm = pixelPerMeter();
        const qreal speed = std::hypot(m_velocity.x(), m_velocity.y());
        const qreal maximum = metric(QScrollerProperties::MaximumVelocity) * ppm.y();
        if (speed > maximum && speed > 0)
            m_velocity *= maximum / speed;
        if (speed < metric(QScrollerProperties::MinimumVelocity) * ppm.y()) {
            m_velocity = QPointF();
            const QPointF snap = snapped(contentPosition());
            if (snap != contentPosition()) {
                scrollTo(snap, static_cast<int>(metric(QScrollerProperties::SnapTime) * 1000));
                return true;
            }
            setState(Inactive);
            return true;
        }
        m_position = contentPosition();
        m_lastTime = m_clock.elapsed();
        setState(Scrolling);
        const int rate = m_properties.scrollMetric(QScrollerProperties::FrameRate).toInt();
        m_timer->start(rate == QScrollerProperties::Fps30 ? 33 : rate == QScrollerProperties::Fps20 ? 50 : 16);
        return true;
    }
    }
    return false;
}

void QScroller::tick() {
    const qint64 now = m_clock.elapsed();
    if (m_animating) {
        const qreal t = m_animTime > 0 ? std::min<qreal>(1.0, static_cast<qreal>(now - m_animStart) / m_animTime) : 1;
        const QEasingCurve curve = m_properties.scrollMetric(QScrollerProperties::ScrollingCurve).value<QEasingCurve>();
        setContentPosition(m_animFrom + (m_animTo - m_animFrom) * curve.valueForProgress(t));
        if (t >= 1)
            setState(Inactive);
        return;
    }
    const qreal dt = static_cast<qreal>(now - m_lastTime) / 1000.0;
    m_lastTime = now;
    const qreal speed = std::hypot(m_velocity.x(), m_velocity.y());
    const qreal deceleration =
        metric(QScrollerProperties::DecelerationFactor) * kDecelerationScale * pixelPerMeter().y();
    const qreal slower = std::max<qreal>(0.0, speed - deceleration * dt);
    if (speed <= 0 || slower <= 0) {
        m_velocity = QPointF();
        const QPointF snap = snapped(contentPosition());
        if (snap != contentPosition())
            scrollTo(snap, static_cast<int>(metric(QScrollerProperties::SnapTime) * 1000));
        else
            setState(Inactive);
        return;
    }
    m_velocity *= slower / speed;
    m_position += m_velocity * dt;
    const QRectF range = contentRange();
    // The content stops at its ends.
    if (m_position.x() <= range.left() || m_position.x() >= range.right())
        m_velocity.setX(0);
    if (m_position.y() <= range.top() || m_position.y() >= range.bottom())
        m_velocity.setY(0);
    m_position = QPointF(std::clamp(m_position.x(), range.left(), range.right()),
                         std::clamp(m_position.y(), range.top(), range.bottom()));
    setContentPosition(m_position);
}

QPointF QScroller::snapped(const QPointF &position) const {
    return QPointF(snapValue(position.x(), m_snapX, m_snapFirstX, m_snapIntervalX),
                   snapValue(position.y(), m_snapY, m_snapFirstY, m_snapIntervalY));
}

/*!
    Stops the scrolling at once, where the content is.
*/
void QScroller::stop() {
    m_velocity = QPointF();
    setState(Inactive);
}

/*!
    Sets the positions across, in pixels, that a flicked content stops at to \a positions.
*/
void QScroller::setSnapPositionsX(const QList<qreal> &positions) {
    m_snapX = positions;
    m_snapIntervalX = 0;
}

/*!
    \overload

    Makes a flicked content stop at \a first and every \a interval pixels across from it.
*/
void QScroller::setSnapPositionsX(qreal first, qreal interval) {
    m_snapX.clear();
    m_snapFirstX = first;
    m_snapIntervalX = interval;
}

/*!
    Sets the positions down, in pixels, that a flicked content stops at to \a positions.
*/
void QScroller::setSnapPositionsY(const QList<qreal> &positions) {
    m_snapY = positions;
    m_snapIntervalY = 0;
}

/*!
    \overload

    Makes a flicked content stop at \a first and every \a interval pixels down from it.
*/
void QScroller::setSnapPositionsY(qreal first, qreal interval) {
    m_snapY.clear();
    m_snapFirstY = first;
    m_snapIntervalY = interval;
}

/*!
    Sets the settings of the scroller to \a prop.
*/
void QScroller::setScrollerProperties(const QScrollerProperties &prop) {
    if (prop == m_properties)
        return;
    m_properties = prop;
    emit scrollerPropertiesChanged(prop);
}

/*!
    Scrolls the content to \a pos, in pixels, at once.
*/
void QScroller::scrollTo(const QPointF &pos) { scrollTo(pos, 0); }

/*!
    \overload

    Scrolls the content to \a pos, in pixels, over \a scrollTime milliseconds along the ScrollingCurve.
*/
void QScroller::scrollTo(const QPointF &pos, int scrollTime) {
    if (scrollTime <= 0) {
        setState(Inactive);
        setContentPosition(pos);
        return;
    }
    m_animFrom = contentPosition();
    const QRectF range = contentRange();
    m_animTo =
        QPointF(std::clamp(pos.x(), range.left(), range.right()), std::clamp(pos.y(), range.top(), range.bottom()));
    m_animStart = m_clock.elapsed();
    m_animTime = scrollTime;
    setState(Scrolling);
    m_animating = true;
    m_timer->start(16);
}

/*!
    Scrolls the content so that \a rect, in pixels of the content, is visible with \a xmargin and \a ymargin around
    it, if the viewport is large enough.
*/
void QScroller::ensureVisible(const QRectF &rect, qreal xmargin, qreal ymargin) {
    ensureVisible(rect, xmargin, ymargin, 1000);
}

/*!
    \overload

    Does the same as ensureVisible(), over \a scrollTime milliseconds, for \a rect with \a xmargin and \a ymargin.
*/
void QScroller::ensureVisible(const QRectF &rect, qreal xmargin, qreal ymargin, int scrollTime) {
    QAbstractScrollArea *a = area();
    if (!a)
        return;
    const QSizeF view = a->viewport()->size();
    QPointF pos = contentPosition();
    if (rect.left() - xmargin < pos.x())
        pos.setX(rect.left() - xmargin);
    else if (rect.right() + xmargin > pos.x() + view.width())
        pos.setX(rect.right() + xmargin - view.width());
    if (rect.top() - ymargin < pos.y())
        pos.setY(rect.top() - ymargin);
    else if (rect.bottom() + ymargin > pos.y() + view.height())
        pos.setY(rect.bottom() + ymargin - view.height());
    scrollTo(pos, scrollTime);
}

/*!
    Does nothing: Nt sends no QScrollPrepareEvent, and reads the scroll range from the scroll bars each time.
*/
void QScroller::resendPrepareEvent() {}

/*!
    \reimp
*/
bool QScroller::eventFilter(QObject *watched, QEvent *event) {
    if (watched != m_target)
        return false;
    const QEvent::Type type = event->type();
    if (m_gesture == TouchGesture) {
        if (type != QEvent::TouchBegin && type != QEvent::TouchUpdate && type != QEvent::TouchEnd)
            return false;
        const auto *touch = static_cast<QTouchEvent *>(event);
        if (touch->points().isEmpty())
            return false;
        const QPointF pos = touch->points().first().position();
        const Input input = type == QEvent::TouchBegin ? InputPress
                            : type == QEvent::TouchEnd ? InputRelease
                                                       : InputMove;
        const bool used = handleInput(input, pos);
        if (type == QEvent::TouchBegin) {
            event->accept(); // to be given the updates
            return true;
        }
        return used;
    }
    if (type != QEvent::MouseButtonPress && type != QEvent::MouseButtonDblClick && type != QEvent::MouseMove &&
        type != QEvent::MouseButtonRelease)
        return false;
    return !m_delivering && mouseInput(static_cast<QMouseEvent *>(event));
}

bool QScroller::mouseInput(QMouseEvent *event) {
    const Qt::MouseButton button = gestureButton();
    if (event->type() == QEvent::MouseMove) {
        if (!(event->buttons() & button))
            return false;
        const bool used = handleInput(InputMove, event->position());
        if (m_state == Dragging)
            dropHeldPress();                   // a drag: the target never gets the press
        return used || m_heldPress != nullptr; // no moves before the press
    }
    if (event->button() != button)
        return false;
    if (event->type() == QEvent::MouseButtonRelease) {
        const bool swallow = std::exchange(m_swallowRelease, false);
        if (!m_heldPress)
            return handleInput(InputRelease, event->position()) || swallow;
        // A click that was over before the delay was: the target gets the press, and then the release.
        const std::unique_ptr<QMouseEvent> release(event->clone());
        const QPointF position = event->position();
        const QPointer<QScroller> self(this);
        deliverHeldPress();
        if (!self)
            return true;
        handleInput(InputRelease, position);
        deliver(*release);
        return true;
    }
    dropHeldPress();
    m_swallowRelease = handleInput(InputPress, event->position()); // true if it stopped a flick
    if (m_swallowRelease)
        return true;
    if (pressDelay() <= 0)
        return false;
    // Held back until it shows whether it starts a drag.
    m_heldPress.reset(event->clone());
    m_pressTimer->start(pressDelay());
    return true;
}

Qt::MouseButton QScroller::gestureButton() const {
    return m_gesture == LeftMouseButtonGesture    ? Qt::LeftButton
           : m_gesture == RightMouseButtonGesture ? Qt::RightButton
                                                  : Qt::MiddleButton;
}

int QScroller::pressDelay() const {
    return static_cast<int>(std::lround(metric(QScrollerProperties::MousePressEventDelay) * 1000));
}

bool QScroller::holdsBackPress(const QObject *target, Qt::MouseButton button) {
    const QScroller *s = scrollers().value(target);
    return s && s->m_grabbed && !s->m_delivering && s->m_gesture != TouchGesture && s->gestureButton() == button &&
           s->pressDelay() > 0;
}

void QScroller::deliver(QMouseEvent &event) {
    if (!m_target)
        return;
    const QPointer<QScroller> self(this);
    m_delivering = true;
    event.accept();
    QCoreApplication::sendEvent(m_target, &event);
    if (self) // what the target does with it may delete the target, and the scroller with it
        m_delivering = false;
}

void QScroller::deliverHeldPress() {
    m_pressTimer->stop();
    if (const std::unique_ptr<QMouseEvent> press = std::move(m_heldPress))
        deliver(*press);
}

void QScroller::dropHeldPress() {
    m_pressTimer->stop();
    m_heldPress.reset();
}

#include "moc_qscroller.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
