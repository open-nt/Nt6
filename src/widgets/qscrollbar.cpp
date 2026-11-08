// MIT License - see LICENSE
#include <QtWidgets/qscrollbar.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>

/*!
    \class QScrollBar
    \brief The QScrollBar class provides a bar that scrolls the contents of a widget.

    The length of the handle of a scroll bar says how much of the contents is in view: it is pageStep() out of the
    range plus pageStep(). The user drags the handle, clicks next to it to move by a page, or turns the mouse wheel
    over the bar. A QAbstractScrollArea has two scroll bars, and a QScrollBar on its own can be used to scroll
    anything:

    \code
    QScrollBar *bar = new QScrollBar(Qt::Horizontal, window);
    bar->setRange(0, 200);
    bar->setPageStep(50);
    connect(bar, &QScrollBar::valueChanged, [](int value) { qDebug() << value; });
    \endcode

    A vertical scroll bar has its smallest value at the top, a horizontal one at the left.

    Scroll bars with the handle at the start, the middle and the end, a short handle for much to scroll, a disabled
    one, and a vertical one.

    \image widgets/qscrollbar.png QScrollBar in the light color scheme

    \differences
    There are no arrow buttons at the ends, no \c setTracking() (the value always follows the handle), no
    \c invertedAppearance, no \c triggerAction() or \c actionTriggered(), and no \c QAbstractSlider base class. A click
    next to the handle moves by one page and does not repeat while the button is held. The range is \c 0 to \c 99
    as in Qt. A scroll bar does not take the keyboard focus.
    \enddifferences

    \sa QSlider, QAbstractScrollArea
*/

/*!
    Constructs a vertical scroll bar with the given \a parent.
*/
QScrollBar::QScrollBar(QWidget *parent) : QScrollBar(Qt::Vertical, parent) {}

/*!
    Constructs a scroll bar with the given \a orientation and \a parent.
*/
QScrollBar::QScrollBar(Qt::Orientation orientation, QWidget *parent) : QWidget(parent), m_orientation(orientation) {
    setSizePolicy(orientation == Qt::Horizontal ? QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)
                                                : QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding));
}

/*!
    Destroys the scroll bar.
*/
QScrollBar::~QScrollBar() = default;

/*!
    \fn Qt::Orientation QScrollBar::orientation() const

    Returns the direction the handle moves in. A new scroll bar is vertical.

    \sa setOrientation()
*/

/*!
    Sets the direction the handle moves in to \a orientation. The size policy changes with it: a scroll bar expands
    along its direction.

    \sa orientation()
*/
void QScrollBar::setOrientation(Qt::Orientation orientation) {
    if (m_orientation == orientation)
        return;
    m_orientation = orientation;
    setSizePolicy(orientation == Qt::Horizontal ? QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)
                                                : QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding));
    updateGeometry();
    update();
}

/*!
    \fn int QScrollBar::minimum() const

    Returns the smallest value, 0 by default.

    \sa setMinimum(), setRange()
*/

/*!
    Sets the smallest value to \a minimum. The largest value grows to the same if it was smaller.

    \sa minimum()
*/
void QScrollBar::setMinimum(int minimum) { setRange(minimum, std::max(minimum, m_maximum)); }

/*!
    \fn int QScrollBar::maximum() const

    Returns the largest value, 99 by default.

    \sa setMaximum(), setRange()
*/

/*!
    Sets the largest value to \a maximum. The smallest value falls to the same if it was larger.

    \sa maximum()
*/
void QScrollBar::setMaximum(int maximum) { setRange(std::min(m_minimum, maximum), maximum); }

/*!
    Sets the range of the scroll bar to \a minimum to \a maximum. If \a maximum is less than \a minimum, it is
    made equal. The value is brought into the range, and rangeChanged() is emitted if the range changes.

    \sa minimum(), maximum()
*/
void QScrollBar::setRange(int minimum, int maximum) {
    const int oldMin = m_minimum, oldMax = m_maximum;
    m_minimum = minimum;
    m_maximum = std::max(minimum, maximum);
    if (oldMin != m_minimum || oldMax != m_maximum) {
        setValue(m_value);
        m_position = std::clamp(m_position, m_minimum, m_maximum);
        update();
        emit rangeChanged(m_minimum, m_maximum);
    }
}

/*!
    \fn int QScrollBar::value() const

    Returns the value.

    \sa setValue()
*/

/*!
    Sets the value to \a value, which is brought into the range. Emits valueChanged() if the value changes.

    \sa value()
*/
void QScrollBar::setValue(int value) {
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
    \fn int QScrollBar::sliderPosition() const

    Returns the value that the handle is at.
*/

/*!
    \fn int QScrollBar::singleStep() const

    Returns how much the mouse wheel changes the value by for each third of a notch, 1 by default.

    \sa setSingleStep()
*/

/*!
    Sets the single step to \a step.

    \sa singleStep()
*/
void QScrollBar::setSingleStep(int step) { m_singleStep = std::max(1, step); }

/*!
    \fn int QScrollBar::pageStep() const

    Returns how much a click next to the handle changes the value by, 10 by default. It is also the part of the
    range that is in view, which gives the length of the handle.

    \sa setPageStep()
*/

/*!
    Sets the page step to \a step.

    \sa pageStep()
*/
void QScrollBar::setPageStep(int step) {
    m_pageStep = std::max(1, step);
    update();
}

/*!
    \fn bool QScrollBar::isSliderDown() const

    Returns whether the user holds the handle down.
*/

/*!
    \reimp
*/
QSize QScrollBar::sizeHint() const {
    const int extent = style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this);
    const int length = 2 * extent + style()->pixelMetric(QStyle::PM_ScrollBarSliderMin, nullptr, this);
    return m_orientation == Qt::Horizontal ? QSize(length, extent) : QSize(extent, length);
}

/*!
    \reimp
*/
QSize QScrollBar::minimumSizeHint() const { return sizeHint(); }

QRect QScrollBar::handleRect() const {
    QStyleOptionSlider option;
    option.initFrom(this);
    option.orientation = m_orientation;
    option.minimum = m_minimum;
    option.maximum = m_maximum;
    option.sliderPosition = m_position;
    option.pageStep = m_pageStep;
    return style()->subElementRect(QStyle::SE_ScrollBarSlider, &option, this);
}

// The value for the handle to be dragged to `pos`, which is a point in the scroll bar.
int QScrollBar::valueAt(const QPoint &pos) const {
    const bool horizontal = m_orientation == Qt::Horizontal;
    const QRect handle = handleRect();
    const int length = horizontal ? width() : height();
    const int handleLength = horizontal ? handle.width() : handle.height();
    // The groove is one pixel shorter at the start than the bar is, but the handle always spans whole pixels.
    const int span = std::max(1, length - handleLength);
    const int offset = (horizontal ? pos.x() : pos.y()) - m_grabOffset;
    const qint64 range = static_cast<qint64>(m_maximum) - m_minimum;
    const qint64 clamped = std::clamp<qint64>(offset, 0, span);
    return m_minimum + static_cast<int>((clamped * range + span / 2) / span);
}

/*!
    \reimp
*/
void QScrollBar::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    QStyleOptionSlider option;
    option.initFrom(this);
    option.orientation = m_orientation;
    option.minimum = m_minimum;
    option.maximum = m_maximum;
    option.sliderPosition = m_position;
    option.pageStep = m_pageStep;
    if (m_down)
        option.state |= QStyle::State_Sunken;
    else if (underMouse())
        option.state |= QStyle::State_MouseOver;
    style()->drawControl(QStyle::CE_ScrollBar, &option, &painter, this);
}

/*!
    \reimp
*/
void QScrollBar::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || m_maximum == m_minimum) {
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
        setValue(m_value + (before ? -m_pageStep : m_pageStep));
    }
    event->accept();
}

/*!
    \reimp
*/
void QScrollBar::mouseMoveEvent(QMouseEvent *event) {
    if (!m_down) {
        event->ignore();
        return;
    }
    const int value = valueAt(event->pos());
    m_position = value;
    emit sliderMoved(value);
    setValue(value);
    event->accept();
}

/*!
    \reimp
*/
void QScrollBar::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || !m_down) {
        event->ignore();
        return;
    }
    m_down = false;
    emit sliderReleased();
    update();
    event->accept();
}

/*!
    \reimp
*/
void QScrollBar::wheelEvent(QWheelEvent *event) {
    const QPoint delta = event->angleDelta();
    const int notches = (delta.y() != 0 ? delta.y() : delta.x()) / 120;
    if (notches == 0 || m_maximum == m_minimum) {
        event->ignore();
        return;
    }
    // Turning the wheel away from the user scrolls towards the start.
    setValue(m_value - notches * std::min(3 * m_singleStep, m_pageStep));
    event->accept();
}

/*!
    \reimp
*/
void QScrollBar::changeEvent(QEvent *event) {
    update();
    updateGeometry();
    QWidget::changeEvent(event);
}

#include "moc_qscrollbar.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
