// MIT License - see LICENSE
#include <QtWidgets/qslider.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>

/*!
    \class QSlider
    \brief The QSlider class provides a slider that picks a value from a range.

    The user drags the handle, clicks next to it to move by a page, turns the mouse wheel over the slider, or uses
    the keys when the slider has the focus. Connect to valueChanged():

    \code
    QSlider *volume = new QSlider(Qt::Horizontal, window);
    volume->setRange(0, 100);
    connect(volume, &QSlider::valueChanged, [](int value) { qDebug() << value; });
    \endcode

    A horizontal slider has its smallest value at the left, a vertical one at the bottom.

    \differences
    There are no tick marks (\c setTickPosition(), \c setTickInterval()), no \c invertedAppearance, no
    \c invertedControls, no \c triggerAction() or \c actionTriggered(), and no \c QAbstractSlider base class. The range
    is \c 0 to \c 99 as in Qt.
    \enddifferences

    \sa QProgressBar, QSpinBox
*/

/*!
    Constructs a horizontal slider with the given \a parent.
*/
QSlider::QSlider(QWidget *parent) : QSlider(Qt::Horizontal, parent) {}

/*!
    Constructs a slider with the given \a orientation and \a parent.
*/
QSlider::QSlider(Qt::Orientation orientation, QWidget *parent) : QWidget(parent), m_orientation(orientation) {
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(orientation == Qt::Horizontal ? QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)
                                                : QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding));
}

/*!
    Destroys the slider.
*/
QSlider::~QSlider() = default;

/*!
    \fn Qt::Orientation QSlider::orientation() const

    Returns the direction the handle moves in.

    \sa setOrientation()
*/

/*!
    Sets the direction the handle moves in to \a orientation. The size policy changes with it: a slider expands along
    its direction.

    \sa orientation()
*/
void QSlider::setOrientation(Qt::Orientation orientation) {
    if (m_orientation == orientation)
        return;
    m_orientation = orientation;
    setSizePolicy(orientation == Qt::Horizontal ? QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)
                                                : QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding));
    updateGeometry();
    update();
}

/*!
    \fn int QSlider::minimum() const

    Returns the smallest value, 0 by default.

    \sa setMinimum(), setRange()
*/

/*!
    Sets the smallest value to \a minimum. The largest value grows to the same if it was smaller.

    \sa minimum()
*/
void QSlider::setMinimum(int minimum) { setRange(minimum, std::max(minimum, m_maximum)); }

/*!
    \fn int QSlider::maximum() const

    Returns the largest value, 99 by default.

    \sa setMaximum(), setRange()
*/

/*!
    Sets the largest value to \a maximum. The smallest value falls to the same if it was larger.

    \sa maximum()
*/
void QSlider::setMaximum(int maximum) { setRange(std::min(m_minimum, maximum), maximum); }

/*!
    Sets the range of the slider to \a minimum to \a maximum. If \a maximum is less than \a minimum, it is
    made equal. The value is brought into the range.

    \sa minimum(), maximum()
*/
void QSlider::setRange(int minimum, int maximum) {
    const int oldMin = m_minimum, oldMax = m_maximum;
    m_minimum = minimum;
    m_maximum = std::max(minimum, maximum);
    if (oldMin != m_minimum || oldMax != m_maximum) {
        emit rangeChanged(m_minimum, m_maximum);
        setValue(m_value);
        setPosition(std::clamp(m_position, m_minimum, m_maximum));
        update();
    }
}

/*!
    \fn int QSlider::value() const

    Returns the value.

    \sa setValue()
*/

/*!
    Sets the value to \a value, which is brought into the range. Emits valueChanged() if the value changes.

    \sa value()
*/
void QSlider::setValue(int value) {
    const int clamped = std::clamp(value, m_minimum, m_maximum);
    m_position = clamped;
    if (clamped == m_value) {
        update();
        return;
    }
    m_value = clamped;
    update();
    emit valueChanged(m_value);
}

/*!
    \fn int QSlider::sliderPosition() const

    Returns the value that the handle is at. It differs from value() while the handle is dragged and tracking is off.

    \sa hasTracking()
*/

/*!
    \fn int QSlider::singleStep() const

    Returns how much the arrow keys and the mouse wheel change the value by, 1 by default.

    \sa setSingleStep()
*/

/*!
    Sets the step of the arrow keys to \a step.

    \sa singleStep()
*/
void QSlider::setSingleStep(int step) { m_singleStep = std::max(1, step); }

/*!
    \fn int QSlider::pageStep() const

    Returns how much Page Up, Page Down and a click next to the handle change the value by, 10 by default.

    \sa setPageStep()
*/

/*!
    Sets the step of the page keys to \a step.

    \sa pageStep()
*/
void QSlider::setPageStep(int step) { m_pageStep = std::max(1, step); }

/*!
    \fn bool QSlider::hasTracking() const

    Returns whether the value follows the handle while it is dragged. The default is \c true.

    \sa setTracking()
*/

/*!
    \fn void QSlider::setTracking(bool enable)

    Sets whether the value follows the handle while it is dragged to \a enable. Without it, the value changes when the
    handle is released.

    \sa hasTracking()
*/

/*!
    \fn bool QSlider::isSliderDown() const

    Returns whether the user holds the handle down.
*/

void QSlider::setPosition(int position) {
    if (m_position == position)
        return;
    m_position = position;
    update();
}

void QSlider::step(int delta) { setValue(m_value + delta); }

/*!
    \reimp
*/
QSize QSlider::sizeHint() const {
    const int thickness = style()->pixelMetric(QStyle::PM_SliderThickness, nullptr, this);
    return m_orientation == Qt::Horizontal ? QSize(84, thickness) : QSize(thickness, 84);
}

/*!
    \reimp
*/
QSize QSlider::minimumSizeHint() const {
    const int thickness = style()->pixelMetric(QStyle::PM_SliderThickness, nullptr, this);
    const int length = 2 * style()->pixelMetric(QStyle::PM_SliderLength, nullptr, this);
    return m_orientation == Qt::Horizontal ? QSize(length, thickness) : QSize(thickness, length);
}

QRect QSlider::handleRect() const {
    QStyleOptionSlider option;
    option.initFrom(this);
    option.orientation = m_orientation;
    option.minimum = m_minimum;
    option.maximum = m_maximum;
    option.sliderPosition = m_position;
    return style()->subElementRect(QStyle::SE_SliderHandle, &option, this);
}

// The value for the handle to be dragged to `pos`, which is a point in the slider.
int QSlider::valueAt(const QPoint &pos) const {
    QStyleOptionSlider option;
    option.initFrom(this);
    option.orientation = m_orientation;
    const QRect groove = style()->subElementRect(QStyle::SE_SliderGroove, &option, this);
    const int length = style()->pixelMetric(QStyle::PM_SliderLength, nullptr, this);
    const bool horizontal = m_orientation == Qt::Horizontal;
    const int span = std::max(1, (horizontal ? groove.width() : groove.height()) - length);
    const int offset =
        horizontal ? pos.x() - m_grabOffset - groove.left() : groove.bottom() + 1 - length - (pos.y() - m_grabOffset);
    const qint64 range = static_cast<qint64>(m_maximum) - m_minimum;
    const qint64 clamped = std::clamp<qint64>(offset, 0, span);
    return m_minimum + static_cast<int>((clamped * range + span / 2) / span);
}

/*!
    \reimp
*/
void QSlider::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    QStyleOptionSlider option;
    option.initFrom(this);
    option.orientation = m_orientation;
    option.minimum = m_minimum;
    option.maximum = m_maximum;
    option.sliderPosition = m_position;
    if (m_down)
        option.state |= QStyle::State_Sunken;
    style()->drawControl(QStyle::CE_Slider, &option, &painter, this);
}

/*!
    \reimp
*/
void QSlider::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    const QRect handle = handleRect();
    const bool horizontal = m_orientation == Qt::Horizontal;
    if (handle.contains(event->pos())) {
        m_grabOffset = horizontal ? event->pos().x() - handle.left() : event->pos().y() - handle.top();
        m_down = true;
        update();
        emit sliderPressed();
    } else {
        const bool before = horizontal ? event->pos().x() < handle.left() : event->pos().y() < handle.top();
        // Before the handle is lower for a horizontal slider and higher for a vertical one.
        step(before == horizontal ? -m_pageStep : m_pageStep);
    }
    event->accept();
}

/*!
    \reimp
*/
void QSlider::mouseMoveEvent(QMouseEvent *event) {
    if (!m_down) {
        event->ignore();
        return;
    }
    const int value = valueAt(event->pos());
    setPosition(value);
    emit sliderMoved(value);
    if (m_tracking)
        setValue(value);
    event->accept();
}

/*!
    \reimp
*/
void QSlider::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || !m_down) {
        event->ignore();
        return;
    }
    m_down = false;
    emit sliderReleased();
    if (!m_tracking && m_position != m_value)
        setValue(m_position);
    update();
    event->accept();
}

/*!
    \reimp
*/
void QSlider::wheelEvent(QWheelEvent *event) {
    const int notches = event->angleDelta().y() / 120;
    if (notches == 0) {
        event->ignore();
        return;
    }
    step(notches * std::min(3 * m_singleStep, m_pageStep));
    event->accept();
}

/*!
    \reimp
*/
void QSlider::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Down:
        step(-m_singleStep);
        break;
    case Qt::Key_Right:
    case Qt::Key_Up:
        step(m_singleStep);
        break;
    case Qt::Key_PageUp:
        step(m_pageStep);
        break;
    case Qt::Key_PageDown:
        step(-m_pageStep);
        break;
    case Qt::Key_Home:
        setValue(m_minimum);
        break;
    case Qt::Key_End:
        setValue(m_maximum);
        break;
    default:
        event->ignore();
        return;
    }
    event->accept();
}

/*!
    \reimp
*/
void QSlider::changeEvent(QEvent *event) {
    update();
    updateGeometry();
    QWidget::changeEvent(event);
}

#include "moc_qslider.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
