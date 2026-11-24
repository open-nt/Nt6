// MIT License - see LICENSE
#include <QtWidgets/qabstractscrollarea.h>

#include <QtCore/qcoreapplication.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qstyle.h>

#include <algorithm>
#include <functional>

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
        case QEvent::ContextMenu:
        case QEvent::DragEnter:
        case QEvent::DragMove:
        case QEvent::DragLeave:
        case QEvent::Drop:
        case QEvent::ToolTip: {
            const bool handled = m_area->viewportEvent(event);
            const QEvent::Type t = event->type();
            if (t != QEvent::Paint && t != QEvent::Resize && t != QEvent::ToolTip && t != QEvent::ContextMenu &&
                t != QEvent::DragEnter && t != QEvent::DragMove && t != QEvent::DragLeave && t != QEvent::Drop)
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
    setFrameStyle(static_cast<int>(StyledPanel) | Sunken);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_viewport = new QtPrivate::ScrollAreaViewport(this);
    m_hbar = new QScrollBar(Qt::Horizontal, this);
    m_vbar = new QScrollBar(Qt::Vertical, this);
    m_hbar->setRange(0, 0);
    m_vbar->setRange(0, 0);
    m_hbar->hide();
    m_vbar->hide();
    connectBar(m_hbar);
    connectBar(m_vbar);
}

void QAbstractScrollArea::connectBar(QScrollBar *bar) {
    QObject::connect(bar, &QScrollBar::rangeChanged, this, [this](int, int) { layoutChildren(); });
    if (bar->orientation() == Qt::Horizontal) {
        QObject::connect(bar, &QScrollBar::valueChanged, this, [this](int value) {
            const int dx = m_hLast - value;
            m_hLast = value;
            scrollContentsBy(dx, 0);
        });
    } else {
        QObject::connect(bar, &QScrollBar::valueChanged, this, [this](int value) {
            const int dy = m_vLast - value;
            m_vLast = value;
            scrollContentsBy(0, dy);
        });
    }
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
        QRect hRect(inner.left(), inner.bottom() + 1 - hHeight, inner.width() - vWidth, hHeight);
        QRect vRect(inner.right() + 1 - vWidth, inner.top(), vWidth, inner.height() - hHeight);
        // The widgets beside the scroll bars take room from them, at the ends their alignment says.
        for (const BarWidget &b : m_barWidgets) {
            QWidget *w = b.widget.data();
            if (!w)
                continue;
            const bool horizontal = b.alignment & (Qt::AlignLeft | Qt::AlignRight);
            const bool shown = horizontal ? showH : showV;
            w->setVisible(shown);
            if (!shown)
                continue;
            const QSize hint = w->sizeHint().isValid() ? w->sizeHint() : w->size();
            if (b.alignment & Qt::AlignLeft) {
                w->setGeometry(hRect.left(), hRect.top(), hint.width(), hHeight);
                hRect.setLeft(hRect.left() + hint.width());
            } else if (b.alignment & Qt::AlignRight) {
                w->setGeometry(hRect.right() + 1 - hint.width(), hRect.top(), hint.width(), hHeight);
                hRect.setRight(hRect.right() - hint.width());
            } else if (b.alignment & Qt::AlignTop) {
                w->setGeometry(vRect.left(), vRect.top(), vWidth, hint.height());
                vRect.setTop(vRect.top() + hint.height());
            } else if (b.alignment & Qt::AlignBottom) {
                w->setGeometry(vRect.left(), vRect.bottom() + 1 - hint.height(), vWidth, hint.height());
                vRect.setBottom(vRect.bottom() - hint.height());
            }
        }
        m_hbar->setGeometry(hRect);
        m_vbar->setGeometry(vRect);
        if (QWidget *corner = m_corner.data()) {
            corner->setVisible(showH && showV);
            corner->setGeometry(inner.right() + 1 - vWidth, inner.bottom() + 1 - hHeight, vWidth, hHeight);
        }
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

namespace {
// Hands the events of a viewport set with setViewport() to the scroll area, as its own viewport does.
class ViewportFilter : public QObject {
public:
    ViewportFilter(QAbstractScrollArea *area, std::function<bool(QEvent *)> forward)
        : QObject(area), m_area(area), m_forward(std::move(forward)) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched != m_area->viewport())
            return false;
        switch (event->type()) {
        case QEvent::Paint:
        case QEvent::Resize:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        case QEvent::Wheel:
        case QEvent::ContextMenu:
            return m_forward(event);
        default:
            return false;
        }
    }

private:
    QAbstractScrollArea *m_area;
    std::function<bool(QEvent *)> m_forward;
};
} // namespace

/*!
    \enum QAbstractScrollArea::SizeAdjustPolicy
    \since 6.0

    How the size hint of the scroll area follows its contents.

    \value AdjustIgnored The size hint is viewportSizeHint(), or 256 by 192 if it is invalid.
    \value AdjustToContentsOnFirstShow As AdjustIgnored in Nt.
    \value AdjustToContents As AdjustIgnored in Nt: the size hint always follows viewportSizeHint().
*/

/*!
    \since 6.0

    Makes \a widget the viewport, the widget that shows the contents, and takes ownership of it; the old viewport is
    deleted. Its paint, resize and mouse events go to the scroll area as those of the viewport did. setupViewport()
    is called with it.
*/
void QAbstractScrollArea::setViewport(QWidget *widget) {
    if (!widget || widget == m_viewport)
        return;
    QWidget *old = m_viewport;
    m_viewport = widget;
    widget->setParent(this);
    if (!m_viewportFilter)
        m_viewportFilter = new ViewportFilter(this, [this](QEvent *event) { return viewportEvent(event); });
    widget->installEventFilter(m_viewportFilter);
    delete old;
    setupViewport(widget);
    layoutChildren();
    widget->show();
}

/*!
    \since 6.0

    Is called by setViewport() with the new \a viewport, to set it up. The default does nothing.
*/
void QAbstractScrollArea::setupViewport(QWidget *viewport) { (void)viewport; }

/*!
    \since 6.0

    Replaces the horizontal scroll bar with \a scrollBar, which takes its range, value and steps; the scroll area
    takes ownership of it and deletes the old one.
*/
void QAbstractScrollArea::setHorizontalScrollBar(QScrollBar *scrollBar) {
    if (!scrollBar || scrollBar == m_hbar)
        return;
    scrollBar->setOrientation(Qt::Horizontal);
    scrollBar->setRange(m_hbar->minimum(), m_hbar->maximum());
    scrollBar->setPageStep(m_hbar->pageStep());
    scrollBar->setSingleStep(m_hbar->singleStep());
    scrollBar->setValue(m_hbar->value());
    scrollBar->setParent(this);
    delete m_hbar;
    m_hbar = scrollBar;
    connectBar(scrollBar);
    layoutChildren();
}

/*!
    \since 6.0

    Replaces the vertical scroll bar with \a scrollBar, which takes its range, value and steps; the scroll area takes
    ownership of it and deletes the old one.
*/
void QAbstractScrollArea::setVerticalScrollBar(QScrollBar *scrollBar) {
    if (!scrollBar || scrollBar == m_vbar)
        return;
    scrollBar->setOrientation(Qt::Vertical);
    scrollBar->setRange(m_vbar->minimum(), m_vbar->maximum());
    scrollBar->setPageStep(m_vbar->pageStep());
    scrollBar->setSingleStep(m_vbar->singleStep());
    scrollBar->setValue(m_vbar->value());
    scrollBar->setParent(this);
    delete m_vbar;
    m_vbar = scrollBar;
    connectBar(scrollBar);
    layoutChildren();
}

/*!
    \fn QWidget *QAbstractScrollArea::cornerWidget() const
    \since 6.0

    Returns the widget in the corner between the scroll bars, or \c nullptr.
*/

/*!
    \since 6.0

    Puts \a widget in the corner between the scroll bars, shown while both are; the scroll area takes ownership of
    it and deletes the widget that was there.
*/
void QAbstractScrollArea::setCornerWidget(QWidget *widget) {
    if (widget == m_corner)
        return;
    delete m_corner.data();
    m_corner = widget;
    if (widget)
        widget->setParent(this);
    layoutChildren();
}

/*!
    \since 6.0

    Shows \a widget beside a scroll bar: at the left or right end of the horizontal one for Qt::AlignLeft or
    Qt::AlignRight, at the top or bottom of the vertical one for Qt::AlignTop or Qt::AlignBottom, as \a alignment
    says. The widget takes room from the bar, and is shown while the bar is. The scroll area takes ownership of it.
*/
void QAbstractScrollArea::addScrollBarWidget(QWidget *widget, Qt::Alignment alignment) {
    if (!widget)
        return;
    widget->setParent(this);
    m_barWidgets.push_back({widget, alignment});
    layoutChildren();
}

/*!
    \since 6.0

    Returns the widgets that addScrollBarWidget() added with \a alignment.
*/
QWidgetList QAbstractScrollArea::scrollBarWidgets(Qt::Alignment alignment) {
    QWidgetList list;
    for (const BarWidget &b : m_barWidgets)
        if (b.widget && (b.alignment & alignment))
            list.append(b.widget.data());
    return list;
}

/*!
    \since 6.0

    Returns the largest size the viewport can have: the area inside the frame and the margins, without the scroll
    bars unless they are always on.
*/
QSize QAbstractScrollArea::maximumViewportSize() const {
    const int extent = style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this);
    const QRect inner = contentsRect();
    const int w =
        inner.width() - m_margins.left() - m_margins.right() - (m_vPolicy == Qt::ScrollBarAlwaysOn ? extent : 0);
    const int h =
        inner.height() - m_margins.top() - m_margins.bottom() - (m_hPolicy == Qt::ScrollBarAlwaysOn ? extent : 0);
    return QSize(std::max(0, w), std::max(0, h));
}

/*!
    \fn QAbstractScrollArea::SizeAdjustPolicy QAbstractScrollArea::sizeAdjustPolicy() const
    \since 6.0

    Returns how the size hint follows the contents, AdjustIgnored by default.
*/

/*!
    \since 6.0

    Sets how the size hint follows the contents to \a policy.
*/
void QAbstractScrollArea::setSizeAdjustPolicy(SizeAdjustPolicy policy) {
    m_sizeAdjust = policy;
    updateGeometry();
}

#include "moc_qabstractscrollarea.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
