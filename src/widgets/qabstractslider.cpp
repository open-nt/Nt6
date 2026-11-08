// MIT License - see LICENSE
#include <QtWidgets/qabstractslider.h>

#include <QtGui/qevent.h>

#include <algorithm>

/*!
    \class QAbstractSlider
    \brief The QAbstractSlider class is the base of the widgets that pick an integer value from a range.

    QAbstractSlider holds what QSlider, QScrollBar and QDial have in common: a range from minimum() to maximum(), a
    value(), a sliderPosition() that the user drags, and the steps that the keys and the mouse wheel move by. A
    subclass draws the slider and turns the mouse into setSliderDown() and setSliderPosition() calls.

    While the user drags the slider, sliderPosition() follows the mouse and sliderMoved() is emitted. With tracking
    on, the default, the value follows the position at once; with it off, the value changes when the slider is
    released:

    \code
    QSlider *slider = new QSlider(Qt::Horizontal, window);
    slider->setTracking(false);
    connect(slider, &QAbstractSlider::valueChanged, [](int value) { apply(value); });
    \endcode

    triggerAction() moves the slider the way the keys do, and emits actionTriggered() before the value changes.

    \differences
    There is no \c invertedAppearance and no \c setRepeatAction(). The keys and the wheel move by the steps of the
    slider; the wheel does not take \c QApplication::wheelScrollLines() into account and moves by three single steps,
    at most one page, a notch.
    \enddifferences

    \sa QSlider, QScrollBar, QDial
*/

/*!
    \enum QAbstractSlider::SliderAction

    The ways triggerAction() moves the slider.

    \value SliderNoAction Nothing.
    \value SliderSingleStepAdd Up by singleStep().
    \value SliderSingleStepSub Down by singleStep().
    \value SliderPageStepAdd Up by pageStep().
    \value SliderPageStepSub Down by pageStep().
    \value SliderToMinimum To minimum().
    \value SliderToMaximum To maximum().
    \value SliderMove To sliderPosition(): the value takes the position of the slider.
*/

/*!
    \enum QAbstractSlider::SliderChange

    What changed when sliderChange() is called.

    \value SliderRangeChange The minimum or the maximum.
    \value SliderOrientationChange The orientation.
    \value SliderStepsChange The single or the page step.
    \value SliderValueChange The value.
*/

/*!
    Constructs a slider with the given \a parent. The range is 0 to 99, and the slider is horizontal.
*/
QAbstractSlider::QAbstractSlider(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::WheelFocus);
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
}

/*!
    Destroys the slider.
*/
QAbstractSlider::~QAbstractSlider() = default;

/*!
    \fn Qt::Orientation QAbstractSlider::orientation() const

    Returns the direction the slider moves in, Qt::Horizontal by default.

    \sa setOrientation()
*/

/*!
    Sets the direction the slider moves in to \a orientation. The size policy is transposed with it: a slider
    expands along its direction.

    \sa orientation()
*/
void QAbstractSlider::setOrientation(Qt::Orientation orientation) {
    if (m_orientation == orientation)
        return;
    m_orientation = orientation;
    const QSizePolicy policy = sizePolicy();
    setSizePolicy(QSizePolicy(policy.verticalPolicy(), policy.horizontalPolicy()));
    sliderChange(SliderOrientationChange);
    updateGeometry();
}

/*!
    \fn int QAbstractSlider::minimum() const

    Returns the smallest value, 0 by default.

    \sa setMinimum(), setRange()
*/

/*!
    Sets the smallest value to \a minimum. The largest value grows to the same if it was smaller.

    \sa minimum()
*/
void QAbstractSlider::setMinimum(int minimum) { setRange(minimum, std::max(minimum, m_maximum)); }

/*!
    \fn int QAbstractSlider::maximum() const

    Returns the largest value, 99 by default.

    \sa setMaximum(), setRange()
*/

/*!
    Sets the largest value to \a maximum. The smallest value falls to the same if it was larger.

    \sa maximum()
*/
void QAbstractSlider::setMaximum(int maximum) { setRange(std::min(m_minimum, maximum), maximum); }

/*!
    Sets the range of the slider to \a minimum to \a maximum. If \a maximum is less than \a minimum, it is made
    equal. Emits rangeChanged() if the range changes, and the value is then brought into the range.

    \sa minimum(), maximum()
*/
void QAbstractSlider::setRange(int minimum, int maximum) {
    const int oldMin = m_minimum, oldMax = m_maximum;
    m_minimum = minimum;
    m_maximum = std::max(minimum, maximum);
    if (oldMin == m_minimum && oldMax == m_maximum)
        return;
    sliderChange(SliderRangeChange);
    emit rangeChanged(m_minimum, m_maximum);
    setValue(m_value);
}

/*!
    \fn int QAbstractSlider::singleStep() const

    Returns how much the arrow keys change the value by, 1 by default.

    \sa setSingleStep(), pageStep()
*/

/*!
    Sets the step of the arrow keys to \a step, at least 1.

    \sa singleStep()
*/
void QAbstractSlider::setSingleStep(int step) {
    step = std::max(1, step);
    if (step == m_singleStep)
        return;
    m_singleStep = step;
    sliderChange(SliderStepsChange);
}

/*!
    \fn int QAbstractSlider::pageStep() const

    Returns how much Page Up and Page Down change the value by, 10 by default.

    \sa setPageStep(), singleStep()
*/

/*!
    Sets the step of the page keys to \a step, at least 1.

    \sa pageStep()
*/
void QAbstractSlider::setPageStep(int step) {
    step = std::max(1, step);
    if (step == m_pageStep)
        return;
    m_pageStep = step;
    sliderChange(SliderStepsChange);
}

/*!
    \fn bool QAbstractSlider::hasTracking() const

    Returns whether the value follows the slider while it is dragged. The default is \c true.

    \sa setTracking()
*/

/*!
    \fn void QAbstractSlider::setTracking(bool enable)

    Sets whether the value follows the slider while it is dragged to \a enable. Without tracking, the value changes
    when the slider is released.

    \sa hasTracking()
*/

/*!
    \fn bool QAbstractSlider::isSliderDown() const

    Returns whether the slider is held down, by the user or by setSliderDown().

    \sa setSliderDown()
*/

/*!
    Sets whether the slider is held down to \a down, emitting sliderPressed() or sliderReleased() if that changes.
    When the slider is let up, the value takes its position. Subclasses call it when the user presses and releases
    the slider.

    \sa isSliderDown()
*/
void QAbstractSlider::setSliderDown(bool down) {
    if (m_down == down)
        return;
    m_down = down;
    update();
    if (down) {
        emit sliderPressed();
    } else {
        emit sliderReleased();
        if (m_position != m_value)
            triggerAction(SliderMove);
    }
}

/*!
    \fn int QAbstractSlider::sliderPosition() const

    Returns the position of the slider. It differs from value() while the slider is dragged with tracking off.

    \sa setSliderPosition(), value()
*/

/*!
    Moves the slider to \a position, brought into the range. Emits sliderMoved() if the slider is down, and changes
    the value too if tracking is on.

    \sa sliderPosition()
*/
void QAbstractSlider::setSliderPosition(int position) {
    position = bound(position);
    if (position == m_position)
        return;
    m_position = position;
    update();
    if (m_down)
        emit sliderMoved(position);
    if (m_tracking && !m_blockTracking)
        triggerAction(SliderMove);
}

/*!
    \fn bool QAbstractSlider::invertedControls() const

    Returns whether the keys and the mouse wheel move the slider the other way. The default is \c false.

    \sa setInvertedControls()
*/

/*!
    \fn void QAbstractSlider::setInvertedControls(bool invert)

    Sets whether the keys and the mouse wheel move the slider the other way to \a invert.

    \sa invertedControls()
*/

/*!
    \fn int QAbstractSlider::value() const

    Returns the value.

    \sa setValue(), sliderPosition()
*/

/*!
    Sets the value to \a value, brought into the range, and moves the slider there. Emits valueChanged() if the value
    changes.

    \sa value()
*/
void QAbstractSlider::setValue(int value) {
    value = bound(value);
    if (value == m_value && value == m_position)
        return;
    m_value = value;
    if (m_position != value) {
        m_position = value;
        if (m_down)
            emit sliderMoved(value);
    }
    sliderChange(SliderValueChange);
    emit valueChanged(value);
}

/*!
    Moves the slider as \a action says, emits actionTriggered(), and sets the value to the new position.

    \sa actionTriggered()
*/
void QAbstractSlider::triggerAction(SliderAction action) {
    m_blockTracking = true;
    switch (action) {
    case SliderSingleStepAdd:
        setSliderPosition(stepped(m_singleStep));
        break;
    case SliderSingleStepSub:
        setSliderPosition(stepped(-static_cast<qint64>(m_singleStep)));
        break;
    case SliderPageStepAdd:
        setSliderPosition(stepped(m_pageStep));
        break;
    case SliderPageStepSub:
        setSliderPosition(stepped(-static_cast<qint64>(m_pageStep)));
        break;
    case SliderToMinimum:
        setSliderPosition(m_minimum);
        break;
    case SliderToMaximum:
        setSliderPosition(m_maximum);
        break;
    case SliderMove:
    case SliderNoAction:
        break;
    }
    emit actionTriggered(action);
    m_blockTracking = false;
    setValue(m_position);
}

/*!
    Is called when the range, the orientation, the steps or the value change; \a change tells which. The default
    implementation repaints the slider.
*/
void QAbstractSlider::sliderChange(SliderChange change) {
    (void)change;
    update();
}

int QAbstractSlider::bound(int value) const { return std::clamp(value, m_minimum, m_maximum); }

// The position `delta` away from the current one: round the ends for a dial that wraps, otherwise in the range.
int QAbstractSlider::stepped(qint64 delta) const {
    const qint64 target = static_cast<qint64>(m_position) + delta;
    if (m_wrapSteps) {
        const qint64 count = static_cast<qint64>(m_maximum) - m_minimum + 1;
        qint64 offset = (target - m_minimum) % count;
        if (offset < 0)
            offset += count;
        return static_cast<int>(m_minimum + offset);
    }
    return static_cast<int>(std::clamp<qint64>(target, m_minimum, m_maximum));
}

/*!
    \reimp

    The arrow keys move by singleStep(), Page Up and Page Down by pageStep(), and Home and End go to the ends.
*/
void QAbstractSlider::keyPressEvent(QKeyEvent *event) {
    SliderAction action = SliderNoAction;
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Down:
        action = m_invertedControls ? SliderSingleStepAdd : SliderSingleStepSub;
        break;
    case Qt::Key_Right:
    case Qt::Key_Up:
        action = m_invertedControls ? SliderSingleStepSub : SliderSingleStepAdd;
        break;
    case Qt::Key_PageUp:
        action = m_invertedControls ? SliderPageStepSub : SliderPageStepAdd;
        break;
    case Qt::Key_PageDown:
        action = m_invertedControls ? SliderPageStepAdd : SliderPageStepSub;
        break;
    case Qt::Key_Home:
        action = SliderToMinimum;
        break;
    case Qt::Key_End:
        action = SliderToMaximum;
        break;
    default:
        event->ignore();
        return;
    }
    triggerAction(action);
    event->accept();
}

/*!
    \reimp

    Turning the wheel away from the user raises the value by three single steps a notch, at most one page.
*/
void QAbstractSlider::wheelEvent(QWheelEvent *event) {
    const QPoint delta = event->angleDelta();
    int notches = (delta.y() != 0 ? delta.y() : delta.x()) / 120;
    if (notches == 0) {
        event->ignore();
        return;
    }
    if (m_invertedControls)
        notches = -notches;
    const qint64 step = std::min(3 * static_cast<qint64>(m_singleStep), static_cast<qint64>(m_pageStep));
    setValue(stepped(notches * step));
    event->accept();
}

/*!
    \reimp
*/
void QAbstractSlider::changeEvent(QEvent *event) {
    update();
    updateGeometry();
    QWidget::changeEvent(event);
}

#include "moc_qabstractslider.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
