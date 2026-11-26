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

    Sliders at different values, one with the keyboard focus, one disabled, and a vertical one.

    \image widgets/qslider.png QSlider in the light color scheme

    The range, the value and the steps come from QAbstractSlider.

    \differences
    There are no tick marks (\c setTickPosition(), \c setTickInterval()) and no \c invertedAppearance. A click next
    to the handle moves by one page and does not repeat while the button is held.
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
QSlider::QSlider(Qt::Orientation orientation, QWidget *parent) : QAbstractSlider(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setOrientation(orientation);
}

/*!
    Destroys the slider.
*/
QSlider::~QSlider() = default;

/*!
    \reimp
*/
QSize QSlider::sizeHint() const {
    const int thickness = style()->pixelMetric(QStyle::PM_SliderThickness, nullptr, this) + tickSpace();
    return orientation() == Qt::Horizontal ? QSize(84, thickness) : QSize(thickness, 84);
}

/*!
    \reimp
*/
QSize QSlider::minimumSizeHint() const {
    const int thickness = style()->pixelMetric(QStyle::PM_SliderThickness, nullptr, this) + tickSpace();
    const int length = 2 * style()->pixelMetric(QStyle::PM_SliderLength, nullptr, this);
    return orientation() == Qt::Horizontal ? QSize(length, thickness) : QSize(thickness, length);
}

QRect QSlider::handleRect() const {
    QStyleOptionSlider option;
    initStyleOption(&option);
    return style()->subElementRect(QStyle::SE_SliderHandle, &option, this);
}

// The value for the handle to be dragged to `pos`, which is a point in the slider.
int QSlider::valueAt(const QPoint &pos) const {
    QStyleOptionSlider option;
    option.initFrom(this);
    option.orientation = orientation();
    const QRect groove = style()->subElementRect(QStyle::SE_SliderGroove, &option, this);
    const int length = style()->pixelMetric(QStyle::PM_SliderLength, nullptr, this);
    const bool horizontal = orientation() == Qt::Horizontal;
    const int span = std::max(1, (horizontal ? groove.width() : groove.height()) - length);
    int offset =
        horizontal ? pos.x() - m_grabOffset - groove.left() : groove.bottom() + 1 - length - (pos.y() - m_grabOffset);
    if (invertedAppearance())
        offset = span - offset;
    const qint64 range = static_cast<qint64>(maximum()) - minimum();
    const qint64 clamped = std::clamp<qint64>(offset, 0, span);
    return minimum() + static_cast<int>((clamped * range + span / 2) / span);
}

/*!
    \reimp
*/
void QSlider::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    QStyleOptionSlider option;
    initStyleOption(&option);
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
    const bool horizontal = orientation() == Qt::Horizontal;
    if (handle.contains(event->pos())) {
        m_grabOffset = horizontal ? event->pos().x() - handle.left() : event->pos().y() - handle.top();
        setSliderDown(true);
    } else if (style()->styleHint(QStyle::SH_Slider_AbsoluteSetButtons, nullptr, this) & Qt::LeftButton) {
        // A style for touch: the handle goes to where the press is, and follows it from there.
        m_grabOffset = (horizontal ? handle.width() : handle.height()) / 2;
        setSliderDown(true);
        setSliderPosition(valueAt(event->pos()));
    } else {
        const bool before = horizontal ? event->pos().x() < handle.left() : event->pos().y() < handle.top();
        // Before the handle is lower for a horizontal slider and higher for a vertical one.
        triggerAction(before == horizontal ? SliderPageStepSub : SliderPageStepAdd);
    }
    event->accept();
}

/*!
    \reimp
*/
void QSlider::mouseMoveEvent(QMouseEvent *event) {
    if (!isSliderDown()) {
        event->ignore();
        return;
    }
    setSliderPosition(valueAt(event->pos()));
    event->accept();
}

/*!
    \reimp
*/
void QSlider::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || !isSliderDown()) {
        event->ignore();
        return;
    }
    setSliderDown(false);
    event->accept();
}

/*!
    \enum QSlider::TickPosition
    \since 6.0

    Where the tick marks of the slider are.

    \value NoTicks No tick marks.
    \value TicksAbove Above a horizontal slider.
    \value TicksLeft At the left of a vertical slider.
    \value TicksBelow Below a horizontal slider.
    \value TicksRight At the right of a vertical slider.
    \value TicksBothSides On both sides.
*/

int QSlider::tickSpace() const {
    int sides = 0;
    if (m_ticks & TicksAbove)
        ++sides;
    if (m_ticks & TicksBelow)
        ++sides;
    return sides * 5;
}

/*!
    \since 6.0

    Fills \a option with the state of the slider, for the style to draw it.
*/
void QSlider::initStyleOption(QStyleOptionSlider *option) const {
    if (!option)
        return;
    option->initFrom(this);
    option->orientation = orientation();
    option->minimum = minimum();
    option->maximum = maximum();
    option->sliderPosition = sliderPosition();
    option->sliderValue = value();
    option->singleStep = singleStep();
    option->pageStep = pageStep();
    option->upsideDown = invertedAppearance();
    option->tickPosition = m_ticks;
    option->tickInterval = m_tickInterval;
    if (orientation() == Qt::Horizontal)
        option->state |= QStyle::State_Horizontal;
    if (isSliderDown())
        option->state |= QStyle::State_Sunken;
}

/*!
    \fn QSlider::TickPosition QSlider::tickPosition() const
    \since 6.0

    Returns where the tick marks are, NoTicks by default.
*/

/*!
    \since 6.0

    Puts tick marks at \a position.
*/
void QSlider::setTickPosition(TickPosition position) {
    m_ticks = position;
    updateGeometry();
    update();
}

/*!
    \fn int QSlider::tickInterval() const
    \since 6.0

    Returns the value between tick marks; 0, the default, uses the page step.
*/

/*!
    \since 6.0

    Puts a tick mark every \a ti values; 0 uses the page step.
*/
void QSlider::setTickInterval(int ti) {
    m_tickInterval = std::max(0, ti);
    update();
}

#include "moc_qslider.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
