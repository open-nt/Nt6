// MIT License - see LICENSE
#include <QtWidgets/qlayout.h>

#include <QtCore/qdebug.h>

#include "qwidget_p.h"
#include "qwidgetutil_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qstyle.h>

using QtPrivate::WidgetAccess;

struct QLayout::Data {
    int spacing = -1;
    bool marginsSet = false;
    QMargins margins;
    bool pending = false;
};

/*!
    \class QLayout
    \brief The QLayout class is the base class of geometry managers.

    A layout places the child widgets of a widget, and resizes them when the widget is resized or when the size
    hint of a child changes. Create a layout on a widget, or give it to the widget with
    QWidget::setLayout(), then add widgets:

    \code
    QHBoxLayout *layout = new QHBoxLayout(window);
    layout->addWidget(new QLabel("Name:"));
    layout->addWidget(new QPushButton("OK"));
    \endcode

    Adding a widget makes it a child of the widget that the layout manages. Use QBoxLayout and its subclasses
    QHBoxLayout and QVBoxLayout, or derive your own layout from QLayout.

    The layout is done again from the event loop when invalidate() is called, which the layout does by itself when a child
    widget calls QWidget::updateGeometry().

    \differences
    There is no \c setSizeConstraint(), \c setMenuBar(), height-for-width, \c setAlignment() for widgets,
    \c addChildLayout(), \c widgetEvent() or \c layoutItems. QGridLayout, QFormLayout and QStackedLayout are
    not implemented.
    \enddifferences

    \sa QBoxLayout, QWidget
*/

/*!
    Constructs a layout for \a parent, which has no layout yet. If \a parent is \c nullptr, add the layout to a
    widget later with QWidget::setLayout() or to another layout.
*/
QLayout::QLayout(QWidget *parent) : QObject(parent), m_d(std::make_unique<Data>()) {
    if (parent) {
        QWidget::Data &wd = WidgetAccess::data(parent);
        if (wd.layout)
            qWarning("QLayout: attempting to add a layout to a widget that already has one");
        else
            wd.layout = this;
    }
}

/*!
    Destroys the layout. The widgets in it are not destroyed.
*/
QLayout::~QLayout() {
    if (QWidget *w = parentWidget()) {
        QWidget::Data &wd = WidgetAccess::data(w);
        if (wd.layout == this)
            wd.layout = nullptr;
    }
}

/*!
    Returns the spacing between the items, in pixels. Unless it was set, it is that of the parent layout or the
    one that the style prescribes.

    \sa setSpacing()
*/
int QLayout::spacing() const {
    if (m_d->spacing >= 0)
        return m_d->spacing;
    if (auto *parentLayout = dynamic_cast<QLayout *>(parent()))
        return parentLayout->spacing();
    QWidget *w = parentWidget();
    QStyle *style = w ? w->style() : QApplication::style();
    return style->pixelMetric(QStyle::PM_LayoutHorizontalSpacing, nullptr, w);
}

/*!
    Sets the spacing between the items to \a spacing pixels.

    \sa spacing()
*/
void QLayout::setSpacing(int spacing) {
    m_d->spacing = spacing;
    invalidate();
}

/*!
    Sets the space between the edge of the widget and the items to \a left, \a top, \a right and \a bottom pixels.

    \sa contentsMargins()
*/
void QLayout::setContentsMargins(int left, int top, int right, int bottom) {
    m_d->margins = QMargins(left, top, right, bottom);
    m_d->marginsSet = true;
    invalidate();
}

/*!
    \overload

    Sets the margins to \a margins.
*/
void QLayout::setContentsMargins(const QMargins &margins) {
    setContentsMargins(margins.left(), margins.top(), margins.right(), margins.bottom());
}

/*!
    Returns the margins. Unless they were set, they are zero for a layout in another layout and what the style prescribes
    for the layout of a widget.

    \sa setContentsMargins()
*/
QMargins QLayout::contentsMargins() const {
    if (m_d->marginsSet)
        return m_d->margins;
    if (dynamic_cast<QLayout *>(parent()))
        return QMargins();
    QWidget *w = parentWidget();
    QStyle *style = w ? w->style() : QApplication::style();
    return QMargins(style->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, w),
                    style->pixelMetric(QStyle::PM_LayoutTopMargin, nullptr, w),
                    style->pixelMetric(QStyle::PM_LayoutRightMargin, nullptr, w),
                    style->pixelMetric(QStyle::PM_LayoutBottomMargin, nullptr, w));
}

/*!
    Returns the rectangle that the layout has for its items: its geometry minus the margins.
*/
QRect QLayout::contentsRect() const { return m_rect.marginsRemoved(contentsMargins()); }

/*!
    Adds \a widget to the layout, making it a child of the widget that the layout manages. A box layout has its own
    addWidget() with more arguments.
*/
void QLayout::addWidget(QWidget *widget) {
    if (!widget)
        return;
    addChildWidget(widget);
    addItem(new QWidgetItem(widget));
}

/*!
    \fn void QLayout::addItem(QLayoutItem *item)

    Adds \a item to the layout, which takes ownership of it.
*/

/*!
    Removes \a item from the layout. The item is not deleted.
*/
void QLayout::removeItem(QLayoutItem *item) {
    for (int i = 0; i < count(); ++i) {
        if (itemAt(i) == item) {
            takeAt(i);
            invalidate();
            return;
        }
    }
}

/*!
    Removes \a widget from the layout, also when it is in a layout inside this one. The widget stays a child of its
    parent. Widgets are removed from their layout when they are deleted.
*/
void QLayout::removeWidget(QWidget *widget) {
    for (int i = 0; i < count(); ++i) {
        QLayoutItem *item = itemAt(i);
        if (item->widget() == widget) {
            takeAt(i);
            delete item;
            invalidate();
            return;
        }
        if (QLayout *inner = item->layout())
            inner->removeWidget(widget);
    }
}

/*!
    \fn QLayoutItem *QLayout::itemAt(int index) const

    Returns the item at \a index, or \c nullptr if there is none.
*/

/*!
    \fn QLayoutItem *QLayout::takeAt(int index)

    Removes the item at \a index from the layout and returns it. The caller owns the returned item.
*/

/*!
    \fn int QLayout::count() const

    Returns the number of items in the layout.
*/

/*!
    Returns the index of the item that holds \a widget, or -1 if the widget is not directly in this layout.
*/
int QLayout::indexOf(const QWidget *widget) const {
    for (int i = 0; i < count(); ++i)
        if (itemAt(i)->widget() == widget)
            return i;
    return -1;
}

/*!
    Returns the widget that the layout manages, also for a layout in another layout, or \c nullptr if the layout
    has no widget yet.
*/
QWidget *QLayout::parentWidget() const {
    if (auto *w = dynamic_cast<QWidget *>(parent()))
        return w;
    if (auto *l = dynamic_cast<QLayout *>(parent()))
        return l->parentWidget();
    return nullptr;
}

/*!
    Makes \a widget a child of the widget that the layout manages, and shows it if that widget is visible
    and \a widget was not hidden. Layouts call it for the widgets that are added.
*/
void QLayout::addChildWidget(QWidget *widget) {
    QWidget *pw = parentWidget();
    if (!pw)
        return;
    if (widget->parentWidget() != pw)
        widget->setParent(pw);
    const QWidget::Data &d = WidgetAccess::data(widget);
    if (pw->isVisible() && !d.hiddenExplicitly)
        widget->show();
}

void QLayout::clearPendingRequest() { m_d->pending = false; }

void QLayout::reparentChildWidgets() {
    if (!parentWidget())
        return;
    for (int i = 0; i < count(); ++i) {
        QLayoutItem *item = itemAt(i);
        if (QWidget *w = item->widget())
            addChildWidget(w);
        else if (QLayout *inner = item->layout())
            inner->reparentChildWidgets();
    }
}

/*!
    Does the layout now: places the items in the geometry of the widget that the layout manages. A window grows if it is smaller than the
    minimum size of its layout. Returns \c false if there is no widget.

    \sa invalidate()
*/
bool QLayout::activate() {
    QLayout *top = this;
    while (auto *parentLayout = dynamic_cast<QLayout *>(top->parent()))
        top = parentLayout;
    QWidget *w = top->parentWidget();
    if (!w)
        return false;
    top->setGeometry(w->rect());
    if (w->isWindow()) {
        const QSize minimum = top->minimumSize();
        const QSize current = w->size();
        if (current.width() < minimum.width() || current.height() < minimum.height())
            w->resize(QtPrivate::expandedTo(current, minimum));
    }
    return true;
}

/*!
    Does the same as invalidate().
*/
void QLayout::update() { invalidate(); }

/*!
    Tells the layout that its size hint changed, so that it does its work again from the event loop.
*/
void QLayout::invalidate() {
    if (auto *parentLayout = dynamic_cast<QLayout *>(parent())) {
        parentLayout->invalidate();
        return;
    }
    QWidget *w = parentWidget();
    if (w && !m_d->pending) {
        m_d->pending = true;
        QCoreApplication::postEvent(w, new QEvent(QEvent::LayoutRequest));
    }
}

/*!
    \fn QRect QLayout::geometry() const

    \reimp
*/

/*!
    Stores \a rect as the geometry of the layout. Subclasses place their items in it.
*/
void QLayout::setGeometry(const QRect &rect) { m_rect = rect; }

/*!
    Returns \c true if the layout has no items that take space.
*/
bool QLayout::isEmpty() const {
    for (int i = 0; i < count(); ++i)
        if (!itemAt(i)->isEmpty())
            return false;
    return true;
}

/*!
    \fn QLayout *QLayout::layout()

    \reimp
*/

/*!
    \reimp
*/
QSize QLayout::maximumSize() const { return QSize(QLAYOUTSIZE_MAX, QLAYOUTSIZE_MAX); }

/*!
    \reimp
*/
Qt::Orientations QLayout::expandingDirections() const {
    Qt::Orientations o;
    for (int i = 0; i < count(); ++i)
        if (!itemAt(i)->isEmpty())
            o |= itemAt(i)->expandingDirections();
    return o;
}

#include "moc_qlayout.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
