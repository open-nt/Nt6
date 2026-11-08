// MIT License - see LICENSE
#include <QtWidgets/qscrollarea.h>

#include <QtGui/qevent.h>
#include <QtWidgets/qscrollbar.h>

#include "qwidget_p.h"

#include <algorithm>

/*!
    \class QScrollArea
    \brief The QScrollArea class provides a scrolling view of another widget.

    A scroll area shows part of one widget, and scroll bars move the part. Give it the widget with setWidget(); the
    widget keeps its own size, and the scroll bars appear when it is larger than the viewport:

    \code
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    for (int i = 0; i < 40; ++i)
        layout->addWidget(new QLabel(QString("Line %1").arg(i)));

    QScrollArea *area = new QScrollArea(window);
    area->setWidget(page);
    area->setWidgetResizable(true);
    \endcode

    With setWidgetResizable(), the widget is resized to the viewport when the viewport is larger than the widget needs
    to be, so a layout in it fills the area, and it gets no larger than its layout's minimum size requires. The scroll
    area follows the widget when it is resized or its size hint changes.

    A scroll area with a page of widgets: the frame, the page, and the vertical scroll bar.

    \image widgets/qscrollarea.png QScrollArea in the light color scheme

    \differences
    There is no \c setAlignment(), and no height-for-width layouts in the widget. The scroll area takes over the
    widget: it becomes a child of the viewport, and is deleted with the scroll area unless takeWidget() is used.
    Scroll bars move by pixels and a single step is 20 pixels.
    \enddifferences

    \sa QAbstractScrollArea, QScrollBar
*/

/*!
    Constructs a scroll area with no widget and the given \a parent.
*/
QScrollArea::QScrollArea(QWidget *parent) : QAbstractScrollArea(parent) {
    horizontalScrollBar()->setSingleStep(20);
    verticalScrollBar()->setSingleStep(20);
    // The viewport follows the widget it shows: when the widget changes its size, the scroll bars have to follow.
    QtPrivate::WidgetAccess::data(viewport()).childChanged = [this](QWidget *child) {
        if (child == m_widget.data())
            updateScrollBars();
    };
}

/*!
    Destroys the scroll area, and the widget in it.
*/
QScrollArea::~QScrollArea() = default;

/*!
    \fn QWidget *QScrollArea::widget() const

    Returns the widget that the area shows, or \c nullptr if there is none.

    \sa setWidget()
*/

/*!
    Makes \a widget the widget that the area shows, and the area its owner: the widget becomes a child of the
    viewport. A widget that was in the area is deleted. A widget that was never given a size is sized to its size hint.

    \sa widget(), takeWidget()
*/
void QScrollArea::setWidget(QWidget *widget) {
    if (widget == m_widget.data())
        return;
    delete m_widget.data();
    m_widget = widget;
    if (!widget)
        return;
    widget->setParent(viewport());
    if (!QtPrivate::WidgetAccess::data(widget).geometrySet)
        widget->adjustSize();
    horizontalScrollBar()->setValue(0);
    verticalScrollBar()->setValue(0);
    widget->show();
    updateScrollBars();
    updateGeometry();
}

/*!
    Removes the widget from the area and returns it, and the area no longer owns it. Its parent is null.

    \sa setWidget()
*/
QWidget *QScrollArea::takeWidget() {
    QWidget *widget = m_widget.data();
    if (!widget)
        return nullptr;
    m_widget.clear();
    widget->setParent(nullptr);
    updateScrollBars();
    return widget;
}

/*!
    \fn bool QScrollArea::widgetResizable() const

    Returns whether the widget is resized with the area. The default is \c false.

    \sa setWidgetResizable()
*/

/*!
    Sets whether the widget is resized to the viewport to \a resizable.

    \sa widgetResizable()
*/
void QScrollArea::setWidgetResizable(bool resizable) {
    if (m_resizable == resizable)
        return;
    m_resizable = resizable;
    updateScrollBars();
    updateGeometry();
}

// Sizes the widget if it follows the viewport, and sets the range of the scroll bars from the sizes of the two. A scroll
// bar that comes or goes changes the viewport, so this goes on until the sizes agree.
void QScrollArea::updateScrollBars() {
    if (m_updating) {
        m_updateAgain = true;
        return;
    }
    m_updating = true;
    QScrollBar *hbar = horizontalScrollBar(), *vbar = verticalScrollBar();
    for (int pass = 0; pass < 4; ++pass) {
        m_updateAgain = false;
        QWidget *widget = m_widget.data();
        if (!widget) {
            hbar->setRange(0, 0);
            vbar->setRange(0, 0);
        } else {
            const QSize view = viewport()->size();
            if (m_resizable) {
                const QSize least = widget->minimumSizeHint().expandedTo(widget->minimumSize());
                widget->resize(view.expandedTo(QSize(std::max(0, least.width()), std::max(0, least.height()))));
            }
            const QSize size = widget->size();
            hbar->setPageStep(std::max(1, view.width()));
            vbar->setPageStep(std::max(1, view.height()));
            hbar->setRange(0, std::max(0, size.width() - view.width()));
            vbar->setRange(0, std::max(0, size.height() - view.height()));
            widget->move(-hbar->value(), -vbar->value());
        }
        if (!m_updateAgain)
            break;
    }
    m_updating = false;
}

/*!
    \reimp
*/
void QScrollArea::resizeEvent(QResizeEvent *event) {
    (void)event;
    updateScrollBars();
}

/*!
    \reimp
*/
void QScrollArea::scrollContentsBy(int dx, int dy) {
    (void)dx;
    (void)dy;
    if (QWidget *widget = m_widget.data())
        widget->move(-horizontalScrollBar()->value(), -verticalScrollBar()->value());
}

/*!
    \reimp
*/
QSize QScrollArea::viewportSizeHint() const {
    const int h = fontMetrics().height();
    const QWidget *widget = m_widget.data();
    if (!widget)
        return QSize(12 * h, 12 * h);
    QSize hint = m_resizable && widget->sizeHint().isValid() ? widget->sizeHint() : widget->size();
    return hint.boundedTo(QSize(36 * h, 24 * h));
}

/*!
    Scrolls the widget so that the point (\a x, \a y) in it is in view, with \a xmargin and \a ymargin pixels of the
    widget around it if it fits.

    \sa ensureWidgetVisible()
*/
void QScrollArea::ensureVisible(int x, int y, int xmargin, int ymargin) {
    QScrollBar *hbar = horizontalScrollBar(), *vbar = verticalScrollBar();
    const QSize view = viewport()->size();
    if (x - xmargin < hbar->value())
        hbar->setValue(std::max(0, x - xmargin));
    else if (x + xmargin > hbar->value() + view.width())
        hbar->setValue(x + xmargin - view.width());
    if (y - ymargin < vbar->value())
        vbar->setValue(std::max(0, y - ymargin));
    else if (y + ymargin > vbar->value() + view.height())
        vbar->setValue(y + ymargin - view.height());
}

/*!
    Scrolls the widget so that \a child, which is in it, is in view, with \a xmargin and \a ymargin pixels around
    its center. Does nothing if \a child is not in the widget.

    \sa ensureVisible()
*/
void QScrollArea::ensureWidgetVisible(QWidget *child, int xmargin, int ymargin) {
    QWidget *widget = m_widget.data();
    if (!widget || !child)
        return;
    bool inside = false;
    for (const QWidget *w = child; w; w = w->parentWidget())
        inside = inside || w == widget;
    if (!inside)
        return;
    const QPoint center = child->mapTo(widget, child->rect().center());
    ensureVisible(center.x(), center.y(), xmargin, ymargin);
}

#include "moc_qscrollarea.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
