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
    There are no arrow buttons at the ends, no context menu and no \c invertedAppearance. A click next to the handle
    moves by one page and does not repeat while the button is held. A scroll bar does not take the keyboard focus.
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
QScrollBar::QScrollBar(Qt::Orientation orientation, QWidget *parent) : QAbstractSlider(parent) {
    setFocusPolicy(Qt::NoFocus);
    setOrientation(orientation);
}

/*!
    Destroys the scroll bar.
*/
QScrollBar::~QScrollBar() = default;

/*!
    \reimp
*/
QSize QScrollBar::sizeHint() const {
    const int extent = style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this);
    const int length = 2 * extent + style()->pixelMetric(QStyle::PM_ScrollBarSliderMin, nullptr, this);
    return orientation() == Qt::Horizontal ? QSize(length, extent) : QSize(extent, length);
}

/*!
    \reimp
*/
QSize QScrollBar::minimumSizeHint() const { return sizeHint(); }

QRect QScrollBar::handleRect() const {
    QStyleOptionSlider option;
    option.initFrom(this);
    option.orientation = orientation();
    option.minimum = minimum();
    option.maximum = maximum();
    option.sliderPosition = sliderPosition();
    option.pageStep = pageStep();
    return style()->subElementRect(QStyle::SE_ScrollBarSlider, &option, this);
}

// The value for the handle to be dragged to `pos`, which is a point in the scroll bar.
int QScrollBar::valueAt(const QPoint &pos) const {
    const bool horizontal = orientation() == Qt::Horizontal;
    const QRect handle = handleRect();
    const int length = horizontal ? width() : height();
    const int handleLength = horizontal ? handle.width() : handle.height();
    // The groove is one pixel shorter at the start than the bar is, but the handle always spans whole pixels.
    const int span = std::max(1, length - handleLength);
    const int offset = (horizontal ? pos.x() : pos.y()) - m_grabOffset;
    const qint64 range = static_cast<qint64>(maximum()) - minimum();
    const qint64 clamped = std::clamp<qint64>(offset, 0, span);
    return minimum() + static_cast<int>((clamped * range + span / 2) / span);
}

/*!
    \reimp
*/
void QScrollBar::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    QStyleOptionSlider option;
    option.initFrom(this);
    option.orientation = orientation();
    option.minimum = minimum();
    option.maximum = maximum();
    option.sliderPosition = sliderPosition();
    option.pageStep = pageStep();
    if (isSliderDown())
        option.state |= QStyle::State_Sunken;
    else if (underMouse())
        option.state |= QStyle::State_MouseOver;
    style()->drawControl(QStyle::CE_ScrollBar, &option, &painter, this);
}

/*!
    \reimp
*/
void QScrollBar::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || maximum() == minimum()) {
        event->ignore();
        return;
    }
    const QRect handle = handleRect();
    const bool horizontal = orientation() == Qt::Horizontal;
    if (handle.contains(event->pos())) {
        m_grabOffset = horizontal ? event->pos().x() - handle.left() : event->pos().y() - handle.top();
        setSliderDown(true);
    } else {
        const bool before = horizontal ? event->pos().x() < handle.left() : event->pos().y() < handle.top();
        triggerAction(before ? SliderPageStepSub : SliderPageStepAdd);
    }
    event->accept();
}

/*!
    \reimp
*/
void QScrollBar::mouseMoveEvent(QMouseEvent *event) {
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
void QScrollBar::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || !isSliderDown()) {
        event->ignore();
        return;
    }
    setSliderDown(false);
    event->accept();
}

/*!
    \reimp
*/
void QScrollBar::wheelEvent(QWheelEvent *event) {
    const QPoint delta = event->angleDelta();
    const int notches = (delta.y() != 0 ? delta.y() : delta.x()) / 120;
    if (notches == 0 || maximum() == minimum()) {
        event->ignore();
        return;
    }
    // Turning the wheel away from the user scrolls towards the start.
    setValue(value() - notches * std::min(3 * singleStep(), pageStep()));
    event->accept();
}

#include "moc_qscrollbar.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
