// MIT License - see LICENSE
#include <QtWidgets/qabstractscrollarea.h>

#include <QtCore/qcoreapplication.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qstyle.h>

#include <algorithm>

namespace QtPrivate {

// The widget in a scroll area that shows the contents. What happens to it, the area sees: it is drawn by the paintEvent()
// of the area, and the mouse events that it gets are mouse events of the area, in the coordinates of the viewport.
class ScrollAreaViewport : public QWidget {
public:
    explicit ScrollAreaViewport(QAbstractScrollArea *area) : QWidget(area), m_area(area) {}

protected:
    bool event(QEvent *event) override {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
            if (m_area->isEnabled() && (m_area->focusPolicy() & Qt::ClickFocus))
                m_area->setFocus();
            [[fallthrough]];
        case QEvent::Paint:
        case QEvent::Resize:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        case QEvent::Wheel:
        case QEvent::ToolTip: {
            const bool handled = m_area->viewportEvent(event);
            if (event->type() != QEvent::Paint && event->type() != QEvent::Resize && event->type() != QEvent::ToolTip)
                event->accept();
            return handled;
        }
        default:
            return QWidget::event(event);
        }
    }

private:
    QAbstractScrollArea *m_area;
};

} // namespace QtPrivate

/*!
    \class QAbstractScrollArea
    \brief The QAbstractScrollArea class is the base class of widgets that scroll their contents.

    A scroll area has a frame, a viewport and two scroll bars. The viewport is a child widget that shows part of the
    contents: a subclass draws in it from paintEvent() by painting on viewport(), and gets the mouse events of the
    viewport in mousePressEvent() and the other handlers, with positions in the viewport. The scroll bars are shown
    when they have something to scroll (see \l{Qt::ScrollBarPolicy}). A subclass sets their range and page step when
    the viewport or the contents change, usually in resizeEvent(), which it gets for the viewport:

    \code
    void Plot::resizeEvent(QResizeEvent *)
    {
        horizontalScrollBar()->setRange(0, m_width - viewport()->width());
        horizontalScrollBar()->setPageStep(viewport()->width());
    }

    void Plot::scrollContentsBy(int dx, int dy)
    {
        viewport()->update();
    }
    \endcode

    \sa QScrollArea, QScrollBar
*/

/*!
    Constructs a scroll area with the given \a parent. It has a sunken, styled frame and shows its scroll bars as needed.
*/
QAbstractScrollArea::QAbstractScrollArea(QWidget *parent) : QFrame(parent) {
    setFrameStyle(StyledPanel | Sunken);
    m_viewport = new QtPrivate::ScrollAreaViewport(this);
    m_hbar = new QScrollBar(Qt::Horizontal, this);
    m_vbar = new QScrollBar(Qt::Vertical, this);
    m_hbar->setRange(0, 0);
    m_vbar->setRange(0, 0);
    m_hbar->hide();
    m_vbar->hide();
    QObject::connect(m_hbar, &QScrollBar::rangeChanged, [this](int, int) { layoutChildren(); });
    QObject::connect(m_vbar, &QScrollBar::rangeChanged, [this](int, int) { layoutChildren(); });
    QObject::connect(m_hbar, &QScrollBar::valueChanged, [this](int value) {
        const int dx = m_hLast - value;
        m_hLast = value;
        scrollContentsBy(dx, 0);
    });
    QObject::connect(m_vbar, &QScrollBar::valueChanged, [this](int value) {
        const int dy = m_vLast - value;
        m_vLast = value;
        scrollContentsBy(0, dy);
    });
}

/*!
    Destroys the scroll area.
*/
QAbstractScrollArea::~QAbstractScrollArea() = default;

/*!
    \fn QWidget *QAbstractScrollArea::viewport() const

    Returns the widget that shows the contents. It is what a subclass paints on.
*/

/*!
    \fn QScrollBar *QAbstractScrollArea::horizontalScrollBar() const

    Returns the scroll bar at the bottom of the area.

    \sa verticalScrollBar(), horizontalScrollBarPolicy()
*/

/*!
    \fn QScrollBar *QAbstractScrollArea::verticalScrollBar() const

    Returns the scroll bar at the right of the area.

    \sa horizontalScrollBar(), verticalScrollBarPolicy()
*/

/*!
    \fn Qt::ScrollBarPolicy QAbstractScrollArea::horizontalScrollBarPolicy() const

    Returns when the horizontal scroll bar is shown, \c Qt::ScrollBarAsNeeded by default.

    \sa setHorizontalScrollBarPolicy()
*/

/*!
    Sets when the horizontal scroll bar is shown to \a policy.

    \sa horizontalScrollBarPolicy()
*/
void QAbstractScrollArea::setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy policy) {
    if (m_hPolicy == policy)
        return;
    m_hPolicy = policy;
    layoutChildren();
    updateGeometry();
}

/*!
    \fn Qt::ScrollBarPolicy QAbstractScrollArea::verticalScrollBarPolicy() const

    Returns when the vertical scroll bar is shown, \c Qt::ScrollBarAsNeeded by default.

    \sa setVerticalScrollBarPolicy()
*/

/*!
    Sets when the vertical scroll bar is shown to \a policy.

    \sa verticalScrollBarPolicy()
*/
void QAbstractScrollArea::setVerticalScrollBarPolicy(Qt::ScrollBarPolicy policy) {
    if (m_vPolicy == policy)
        return;
    m_vPolicy = policy;
    layoutChildren();
    updateGeometry();
}

/*!
    \fn QMargins QAbstractScrollArea::viewportMargins() const

    Returns the space between the frame and the viewport that the viewport does not use.

    \sa setViewportMargins()
*/

/*!
    Sets the space inside the frame that the viewport leaves free, to \a left, \a top, \a right and \a bottom pixels.
    A subclass puts widgets in it, as a table puts its headers.

    \sa viewportMargins()
*/
void QAbstractScrollArea::setViewportMargins(int left, int top, int right, int bottom) {
    const QMargins margins(left, top, right, bottom);
    if (m_margins == margins)
        return;
    m_margins = margins;
    layoutChildren();
    updateGeometry();
}

/*!
    \fn void QAbstractScrollArea::setViewportMargins(const QMargins &margins)
    \overload

    Sets the space that the viewport leaves free to \a margins.
*/

// Puts the viewport and the scroll bars in the area. Which scroll bars are shown follows from their ranges, and the
// size of the viewport from which are, so this runs again when a subclass changes a range in response to a new size.
void QAbstractScrollArea::layoutChildren() {
    if (m_layouting) {
        m_layoutAgain = true;
        return;
    }
    m_layouting = true;
    const int extent = style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this);
    const QRect inner = contentsRect();
    for (int pass = 0; pass < 4; ++pass) {
        m_layoutAgain = false;
        auto wanted = [](Qt::ScrollBarPolicy policy, const QScrollBar *bar) {
            return policy == Qt::ScrollBarAlwaysOn || (policy == Qt::ScrollBarAsNeeded && bar->maximum() > bar->minimum());
        };
        const bool showH = wanted(m_hPolicy, m_hbar), showV = wanted(m_vPolicy, m_vbar);
        const int vWidth = showV ? extent : 0, hHeight = showH ? extent : 0;
        m_hbar->setVisible(showH);
        m_vbar->setVisible(showV);
        m_hbar->setGeometry(QRect(inner.left(), inner.bottom() + 1 - hHeight, inner.width() - vWidth, hHeight));
        m_vbar->setGeometry(QRect(inner.right() + 1 - vWidth, inner.top(), vWidth, inner.height() - hHeight));
        m_viewport->setGeometry(QRect(inner.left() + m_margins.left(), inner.top() + m_margins.top(),
                                      std::max(0, inner.width() - vWidth - m_margins.left() - m_margins.right()),
                                      std::max(0, inner.height() - hHeight - m_margins.top() - m_margins.bottom())));
        if (!m_layoutAgain)
            break;
    }
    m_layouting = false;
    update();
}

/*!
    \reimp
*/
QSize QAbstractScrollArea::sizeHint() const {
    QSize hint = viewportSizeHint();
    if (!hint.isValid())
        hint = QSize(256, 192);
    const int extent = style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this);
    const int frame = 2 * frameWidth();
    return QSize(hint.width() + frame + m_margins.left() + m_margins.right() +
                     (m_vPolicy == Qt::ScrollBarAlwaysOn ? extent : 0),
                 hint.height() + frame + m_margins.top() + m_margins.bottom() +
                     (m_hPolicy == Qt::ScrollBarAlwaysOn ? extent : 0));
}

/*!
    \reimp
*/
QSize QAbstractScrollArea::minimumSizeHint() const {
    const int extent = style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this);
    const int frame = 2 * frameWidth();
    return QSize(frame + m_margins.left() + m_margins.right() + 3 * extent,
                 frame + m_margins.top() + m_margins.bottom() + 3 * extent);
}

/*!
    Returns the size of the viewport that the area is made for, which sizeHint() adds the frame and the margins to. A
    subclass returns the size of its contents here; the default is an invalid size, which stands for 256 by 192.

    \sa sizeHint()
*/
QSize QAbstractScrollArea::viewportSizeHint() const { return QSize(); }

/*!
    Called when a scroll bar moves, with the number of pixels that the contents moved by: \a dx to the right and
    \a dy down, so that they are negative when the bar scrolls forward. A subclass moves what it shows. The default
    repaints the viewport.
*/
void QAbstractScrollArea::scrollContentsBy(int dx, int dy) {
    (void)dx;
    (void)dy;
    m_viewport->update();
}

/*!
    \reimp
*/
bool QAbstractScrollArea::event(QEvent *event) {
    switch (event->type()) {
    case QEvent::Paint: {
        // Only the frame is the area's own: what is in the viewport is drawn by paintEvent().
        QPainter painter(this);
        drawFrame(&painter);
        return true;
    }
    case QEvent::Resize:
        layoutChildren();
        return true;
    case QEvent::StyleChange:
        layoutChildren();
        return QFrame::event(event);
    default:
        return QFrame::event(event);
    }
}

/*!
    Handles \a event of the viewport: a paint, resize, mouse, wheel or tool tip event. The default sends them to the
    handlers of the area, which is why a subclass receives them as if the area were the viewport. A subclass can
    override it to see them first. Returns \c true if the event was handled.
*/
bool QAbstractScrollArea::viewportEvent(QEvent *event) {
    switch (event->type()) {
    case QEvent::Paint:
        paintEvent(static_cast<QPaintEvent *>(event));
        return true;
    case QEvent::Resize:
        resizeEvent(static_cast<QResizeEvent *>(event));
        return true;
    case QEvent::MouseButtonPress:
        mousePressEvent(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::MouseButtonRelease:
        mouseReleaseEvent(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::MouseButtonDblClick:
        mouseDoubleClickEvent(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::MouseMove:
        mouseMoveEvent(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::Wheel:
        wheelEvent(static_cast<QWheelEvent *>(event));
        return true;
    default:
        return QFrame::event(event);
    }
}

/*!
    \reimp

    The default draws nothing: a subclass draws the contents on viewport().
*/
void QAbstractScrollArea::paintEvent(QPaintEvent *event) { (void)event; }

/*!
    \reimp

    Scrolls the vertical scroll bar by the wheel, and the horizontal one by a sideways wheel.
*/
void QAbstractScrollArea::wheelEvent(QWheelEvent *event) {
    const QPoint delta = event->angleDelta();
    bool scrolled = false;
    auto turn = [&scrolled](QScrollBar *bar, int notches) {
        if (notches == 0 || bar->maximum() == bar->minimum())
            return;
        const int before = bar->value();
        bar->setValue(before - notches * std::min(3 * bar->singleStep(), bar->pageStep()));
        scrolled = scrolled || bar->value() != before;
    };
    turn(m_vbar, delta.y() / 120);
    turn(m_hbar, delta.x() / 120);
    event->setAccepted(scrolled);
}

/*!
    \reimp

    Scrolls with the arrow keys, Page Up, Page Down, Home and End.
*/
void QAbstractScrollArea::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_Up:
        m_vbar->setValue(m_vbar->value() - m_vbar->singleStep());
        break;
    case Qt::Key_Down:
        m_vbar->setValue(m_vbar->value() + m_vbar->singleStep());
        break;
    case Qt::Key_Left:
        m_hbar->setValue(m_hbar->value() - m_hbar->singleStep());
        break;
    case Qt::Key_Right:
        m_hbar->setValue(m_hbar->value() + m_hbar->singleStep());
        break;
    case Qt::Key_PageUp:
        m_vbar->setValue(m_vbar->value() - m_vbar->pageStep());
        break;
    case Qt::Key_PageDown:
        m_vbar->setValue(m_vbar->value() + m_vbar->pageStep());
        break;
    case Qt::Key_Home:
        m_vbar->setValue(m_vbar->minimum());
        break;
    case Qt::Key_End:
        m_vbar->setValue(m_vbar->maximum());
        break;
    default:
        event->ignore();
        return;
    }
    event->accept();
}

#include "moc_qabstractscrollarea.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
