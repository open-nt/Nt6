// MIT License - see LICENSE
#include <QtWidgets/qdial.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>
#include <cmath>

/*!
    \class QDial
    \brief The QDial class provides a round knob that picks a value from a range.

    The user turns the knob by dragging its handle round, or turns the mouse wheel over it, or uses the keys when it has
    the focus. Connect to valueChanged():

    \code
    QDial *volume = new QDial(window);
    volume->setRange(0, 100);
    volume->setNotchesVisible(true);
    connect(volume, &QDial::valueChanged, [](int value) { qDebug() << value; });
    \endcode

    A dial that does not wrap turns from the bottom left, over the top, to the bottom right. With setWrapping(), it
    turns the full circle with its smallest value at the bottom, and the keys and the wheel take the value round from the
    largest to the smallest.

    Dials with the handle at the start, the middle and the end, one with notches, one that wraps, and a disabled one.

    \image widgets/qdial.png QDial in the light color scheme

    \differences
    There is no \c notchSize(), \c notchTarget() or \c setNotchTarget(): the notches are at ten places for a dial that does not
    wrap and twelve for one that does. The dial is drawn without antialiasing. There is no \c invertedAppearance and no
    \c QAbstractSlider base class. The range is \c 0 to \c 99 as in Qt.
    \enddifferences

    \sa QSlider, QScrollBar
*/

/*!
    Constructs a dial with the given \a parent. It goes from 0 to 99.
*/
QDial::QDial(QWidget *parent) : QWidget(parent) { setFocusPolicy(Qt::WheelFocus); }

/*!
    Destroys the dial.
*/
QDial::~QDial() = default;

/*!
    \fn int QDial::minimum() const

    Returns the smallest value, 0 by default.

    \sa setMinimum(), setRange()
*/

/*!
    Sets the smallest value to \a minimum. The largest value grows to the same if it was smaller.

    \sa minimum()
*/
void QDial::setMinimum(int minimum) { setRange(minimum, std::max(minimum, m_maximum)); }

/*!
    \fn int QDial::maximum() const

    Returns the largest value, 99 by default.

    \sa setMaximum(), setRange()
*/

/*!
    Sets the largest value to \a maximum. The smallest value falls to the same if it was larger.

    \sa maximum()
*/
void QDial::setMaximum(int maximum) { setRange(std::min(m_minimum, maximum), maximum); }

/*!
    Sets the range of the dial to \a minimum to \a maximum. If \a maximum is less than \a minimum, it is made equal. The
    value is brought into the range.

    \sa minimum(), maximum()
*/
void QDial::setRange(int minimum, int maximum) {
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
    \fn int QDial::value() const

    Returns the value.

    \sa setValue()
*/

/*!
    Sets the value to \a value, which is brought into the range. Emits valueChanged() if the value changes.

    \sa value()
*/
void QDial::setValue(int value) {
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
    \fn int QDial::sliderPosition() const

    Returns the value that the handle is at. It differs from value() while the dial is turned and tracking is off.

    \sa hasTracking()
*/

/*!
    \fn int QDial::singleStep() const

    Returns how much the arrow keys and the mouse wheel change the value by, 1 by default.

    \sa setSingleStep()
*/

/*!
    Sets the step of the arrow keys to \a step.

    \sa singleStep()
*/
void QDial::setSingleStep(int step) { m_singleStep = std::max(1, step); }

/*!
    \fn int QDial::pageStep() const

    Returns how much Page Up and Page Down change the value by, 10 by default.

    \sa setPageStep()
*/

/*!
    Sets the step of the page keys to \a step.

    \sa pageStep()
*/
void QDial::setPageStep(int step) { m_pageStep = std::max(1, step); }

/*!
    \fn bool QDial::hasTracking() const

    Returns whether the value follows the handle while it is turned. The default is \c true.

    \sa setTracking()
*/

/*!
    \fn void QDial::setTracking(bool enable)

    Sets whether the value follows the handle while it is turned to \a enable. Without it, the value changes when the
    dial is released.

    \sa hasTracking()
*/

/*!
    \fn bool QDial::isSliderDown() const

    Returns whether the user holds the dial.
*/

/*!
    \fn bool QDial::wrapping() const

    Returns whether the dial turns the full circle. The default is \c false.

    \sa setWrapping()
*/

/*!
    Sets whether the dial turns the full circle to \a on.

    \sa wrapping()
*/
void QDial::setWrapping(bool on) {
    if (m_wrapping == on)
        return;
    m_wrapping = on;
    update();
}

/*!
    \fn bool QDial::notchesVisible() const

    Returns whether marks are drawn around the dial. The default is \c false.

    \sa setNotchesVisible()
*/

/*!
    Sets whether marks are drawn around the dial to \a visible.

    \sa notchesVisible()
*/
void QDial::setNotchesVisible(bool visible) {
    if (m_notches == visible)
        return;
    m_notches = visible;
    update();
}

void QDial::setPosition(int position) {
    if (m_position == position)
        return;
    m_position = position;
    update();
}

// Moves by `delta`; a dial that wraps goes round the ends.
void QDial::step(int delta) {
    if (m_wrapping) {
        const int count = m_maximum - m_minimum + 1;
        int offset = (m_value - m_minimum + delta) % count;
        if (offset < 0)
            offset += count;
        setValue(m_minimum + offset);
    } else {
        setValue(m_value + delta);
    }
}

/*!
    \reimp
*/
QSize QDial::sizeHint() const { return QSize(100, 100); }

/*!
    \reimp
*/
QSize QDial::minimumSizeHint() const { return QSize(50, 50); }

// The value for the handle at `pos`, a point in the dial: from the angle of the point round the center.
int QDial::valueAt(const QPoint &pos) const {
    const qreal pi = 3.14159265358979323846;
    const qreal dx = pos.x() - rect().center().x(), dy = rect().center().y() - pos.y();
    qreal angle = std::atan2(dy, dx) * 180.0 / pi;
    qreal fraction;
    if (m_wrapping) {
        // The smallest value is at the bottom, and the dial turns clockwise.
        qreal turn = std::fmod(270.0 - angle + 720.0, 360.0);
        fraction = turn / 360.0;
    } else {
        qreal turn = std::fmod(225.0 - angle + 720.0, 360.0);
        if (turn > 270.0)
            turn = turn < 270.0 + 45.0 ? 270.0 : 0.0; // between the ends: the nearer one
        fraction = turn / 270.0;
    }
    return m_minimum + static_cast<int>(std::lround(fraction * (static_cast<qreal>(m_maximum) - m_minimum)));
}

/*!
    \reimp
*/
void QDial::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    QStyleOptionSlider option;
    option.initFrom(this);
    option.minimum = m_minimum;
    option.maximum = m_maximum;
    option.sliderPosition = m_position;
    option.tickMarks = m_notches;
    option.wrapping = m_wrapping;
    if (m_down)
        option.state |= QStyle::State_Sunken;
    style()->drawControl(QStyle::CE_Dial, &option, &painter, this);
}

/*!
    \reimp
*/
void QDial::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    m_down = true;
    emit sliderPressed();
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
void QDial::mouseMoveEvent(QMouseEvent *event) {
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
void QDial::mouseReleaseEvent(QMouseEvent *event) {
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
void QDial::wheelEvent(QWheelEvent *event) {
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
void QDial::keyPressEvent(QKeyEvent *event) {
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
void QDial::changeEvent(QEvent *event) {
    update();
    QWidget::changeEvent(event);
}

#include "moc_qdial.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
