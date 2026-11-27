// MIT License - see LICENSE
#include <QtWidgets/qlayout.h>

#include <QtCore/qpointer.h>

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
    QLayout::SizeConstraint constraint = QLayout::SetDefaultConstraint;  // horizontal
    QLayout::SizeConstraint vConstraint = QLayout::SetDefaultConstraint; // vertical
    bool enabled = true;
    QPointer<QWidget> menuBar; // setMenuBar(): at the top of the widget, above the items
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
    There is no \c setAlignment() for widgets or \c layoutItems, and widgetEvent() does nothing. A window with a
    layout whose height depends on its width is not given a height for width by the window system: the layout only
    makes it higher when it is too low.
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
    \enum QLayout::SizeConstraint
    \since 6.0

    This enum tells how a layout constrains the size of the widget it manages.

    \value SetDefaultConstraint A window grows to the minimum size of the layout; other widgets are not constrained.
    \value SetNoConstraint The widget is not constrained.
    \value SetMinimumSize The minimum size of the widget is the minimum size of the layout.
    \value SetFixedSize The widget has the size hint of the layout, and cannot be resized.
    \value SetMaximumSize The maximum size of the widget is the maximum size of the layout.
    \value SetMinMaxSize Both the minimum and the maximum size of the widget follow the layout.
*/

/*!
    \since 6.0

    Returns how the layout constrains the size of its widget. The default is SetDefaultConstraint.
*/
QLayout::SizeConstraint QLayout::sizeConstraint() const { return m_d->constraint; }

/*!
    \since 6.0

    Returns the menu bar of the layout, or \c nullptr.
*/
QWidget *QLayout::menuBar() const { return m_d->menuBar.data(); }

/*!
    \since 6.0

    Puts \a widget, usually a QMenuBar, at the top of the widget of the layout, above its items and across its
    whole width. The widget becomes a child of the widget of the layout.
*/
void QLayout::setMenuBar(QWidget *widget) {
    m_d->menuBar = widget;
    if (widget && parentWidget() && widget->parentWidget() != parentWidget())
        widget->setParent(parentWidget());
    invalidate();
}

/*!
    \since 6.0

    Sets how the layout constrains the size of its widget to \a constraint. Only the top-level layout of a
    widget constrains it.
*/
void QLayout::setSizeConstraint(SizeConstraint constraint) {
    m_d->constraint = constraint;
    m_d->vConstraint = constraint;
    invalidate();
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
    if (!w || !top->m_d->enabled)
        return false;
    // The constraints across and down: each sets the minimum and the maximum of the widget in its direction.
    const auto constrain = [&](SizeConstraint constraint, bool horizontal) {
        const auto pick = [horizontal](const QSize &s) { return horizontal ? s.width() : s.height(); };
        QSize minimum = w->minimumSize(), maximum = w->maximumSize();
        auto setMin = [&](int v) { horizontal ? minimum.setWidth(v) : minimum.setHeight(v); };
        auto setMax = [&](int v) { horizontal ? maximum.setWidth(v) : maximum.setHeight(v); };
        switch (constraint) {
        case SetMinimumSize:
            setMin(pick(top->totalMinimumSize()));
            break;
        case SetFixedSize:
            setMin(pick(top->totalSizeHint()));
            setMax(pick(top->totalSizeHint()));
            break;
        case SetMaximumSize:
            setMax(pick(top->totalMaximumSize()));
            break;
        case SetMinMaxSize:
            setMin(pick(top->totalMinimumSize()));
            setMax(pick(top->totalMaximumSize()));
            break;
        case SetDefaultConstraint:
        case SetNoConstraint:
            return;
        }
        w->setMinimumSize(minimum);
        w->setMaximumSize(maximum);
    };
    constrain(top->m_d->constraint, true);
    constrain(top->m_d->vConstraint, false);
    QRect area = w->QWidget::contentsRect();
    if (QWidget *bar = top->m_d->menuBar.data(); bar && !bar->isHidden()) {
        const int height = bar->sizeHint().height();
        bar->setGeometry(QRect(area.left(), area.top(), area.width(), height));
        area.setTop(area.top() + height);
    }
    top->setGeometry(area);
    if (w->isWindow() && top->m_d->constraint == SetDefaultConstraint) {
        QSize minimum = top->minimumSize();
        const QSize current = w->size();
        if (top->hasHeightForWidth()) {
            // The least height depends on the width, which the contents margins and a menu bar are not part of.
            const int width = area.width() + std::max(0, minimum.width() - current.width());
            const int least = top->minimumHeightForWidth(width);
            minimum.setHeight(std::max(minimum.height(), least + current.height() - area.height()));
        }
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
        // Where a height depends on a width, what changed inside may change how high the widget has to be.
        if (!w->isWindow() && hasHeightForWidth())
            w->updateGeometry();
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
        if (!itemAt(i)->isEmpty() || itemAt(i)->spacerItem()) // a stretch expands, though it is empty
            o |= itemAt(i)->expandingDirections();
    return o;
}

/*!
    \since 6.0

    Returns the constraint on the width of the widget.
*/
QLayout::SizeConstraint QLayout::horizontalSizeConstraint() const { return m_d->constraint; }

/*!
    \since 6.0

    Sets the constraint on the width of the widget to \a constraint.
*/
void QLayout::setHorizontalSizeConstraint(SizeConstraint constraint) {
    m_d->constraint = constraint;
    invalidate();
}

/*!
    \since 6.0

    Returns the constraint on the height of the widget.
*/
QLayout::SizeConstraint QLayout::verticalSizeConstraint() const { return m_d->vConstraint; }

/*!
    \since 6.0

    Sets the constraint on the height of the widget to \a constraint.
*/
void QLayout::setVerticalSizeConstraint(SizeConstraint constraint) {
    m_d->vConstraint = constraint;
    invalidate();
}

/*!
    \since 6.0

    Sets the constraints on the width and the height of the widget to \a horizontal and \a vertical.
*/
void QLayout::setSizeConstraints(SizeConstraint horizontal, SizeConstraint vertical) {
    m_d->constraint = horizontal;
    m_d->vConstraint = vertical;
    invalidate();
}

/*!
    \since 6.0

    Returns \c true if the layout places its items. The default is \c true.
*/
bool QLayout::isEnabled() const { return m_d->enabled; }

/*!
    \since 6.0

    Makes the layout place its items if \a enable is \c true; a disabled layout leaves them where they are.
*/
void QLayout::setEnabled(bool enable) { m_d->enabled = enable; }

/*!
    \since 6.0

    Puts the contents margins in \a left, \a top, \a right and \a bottom, any of which may be \c nullptr.
*/
void QLayout::getContentsMargins(int *left, int *top, int *right, int *bottom) const {
    const QMargins m = contentsMargins();
    if (left)
        *left = m.left();
    if (top)
        *top = m.top();
    if (right)
        *right = m.right();
    if (bottom)
        *bottom = m.bottom();
}

/*!
    \since 6.0

    Goes back to the contents margins of the style.
*/
void QLayout::unsetContentsMargins() {
    m_d->marginsSet = false;
    invalidate();
}

QLayoutItem *QLayout::replaceAt(int index, QLayoutItem *item) {
    (void)index;
    (void)item;
    return nullptr;
}

/*!
    \since 6.0

    Puts \a to in the place of \a from in the layout, or in a layout in it with Qt::FindChildrenRecursively in
    \a options, and returns the item of \a from, which the caller owns; \a from is left as it is. Returns
    \c nullptr if \a from is not found.
*/
QLayoutItem *QLayout::replaceWidget(QWidget *from, QWidget *to, Qt::FindChildOptions options) {
    if (!from || !to)
        return nullptr;
    for (int i = 0; i < count(); ++i) {
        QLayoutItem *item = itemAt(i);
        if (!item)
            continue;
        if (item->widget() == from) {
            auto *replacement = new QWidgetItem(to);
            replacement->setAlignment(item->alignment());
            QLayoutItem *old = replaceAt(i, replacement);
            if (!old) {
                delete replacement;
                return nullptr;
            }
            addChildWidget(to);
            invalidate();
            return old;
        }
        if ((options & Qt::FindChildrenRecursively) && item->layout())
            if (QLayoutItem *found = item->layout()->replaceWidget(from, to, options))
                return found;
    }
    return nullptr;
}

QSize QLayout::addTotalMargins(const QSize &size) const {
    QSize s = size;
    if (QWidget *w = parentWidget()) {
        const QMargins m = w->contentsMargins();
        s += QSize(m.left() + m.right(), m.top() + m.bottom());
    }
    if (QWidget *bar = m_d->menuBar.data(); bar && !bar->isHidden())
        s.setHeight(s.height() + bar->sizeHint().height());
    return s;
}

/*!
    \since 6.0

    Returns the size hint of the layout with the contents margins of its widget and its menu bar.
*/
QSize QLayout::totalSizeHint() const { return addTotalMargins(sizeHint()); }

/*!
    \since 6.0

    Returns the minimum size of the layout with the contents margins of its widget and its menu bar.
*/
QSize QLayout::totalMinimumSize() const { return addTotalMargins(minimumSize()); }

/*!
    \since 6.0

    Returns the maximum size of the layout with the contents margins of its widget and its menu bar.
*/
QSize QLayout::totalMaximumSize() const {
    const QSize s = addTotalMargins(maximumSize());
    return QSize(std::min(s.width(), QLAYOUTSIZE_MAX), std::min(s.height(), QLAYOUTSIZE_MAX));
}

/*!
    \since 6.0

    Returns the preferred height at the width \a w, with the margins, or -1 if the layout has no height for width.
*/
int QLayout::totalHeightForWidth(int w) const {
    const int h = heightForWidth(w);
    return h < 0 ? -1 : addTotalMargins(QSize(0, h)).height();
}

/*!
    \since 6.0

    Returns the smallest height at the width \a w, with the margins, or -1 if the layout has no height for width.
*/
int QLayout::totalMinimumHeightForWidth(int w) const {
    const int h = minimumHeightForWidth(w);
    return h < 0 ? -1 : addTotalMargins(QSize(0, h)).height();
}

/*!
    \reimp

    Returns the kinds of control of the items of the layout.
*/
QSizePolicy::ControlTypes QLayout::controlTypes() const {
    QSizePolicy::ControlTypes types;
    for (int i = 0; i < count(); ++i)
        if (const QLayoutItem *item = itemAt(i))
            types |= item->controlTypes();
    return types ? types : QSizePolicy::ControlTypes(QSizePolicy::DefaultType);
}

/*!
    \since 6.0

    Returns \a size brought between the minimum and the maximum size of \a widget, and no smaller than its minimum
    size hint where it has no minimum size.
*/
QSize QLayout::closestAcceptableSize(const QWidget *widget, const QSize &size) {
    if (!widget)
        return size;
    QSize minimum = widget->minimumSize();
    const QSize hint = widget->minimumSizeHint();
    if (minimum.width() <= 0)
        minimum.setWidth(std::max(0, hint.width()));
    if (minimum.height() <= 0)
        minimum.setHeight(std::max(0, hint.height()));
    const QSize maximum = widget->maximumSize();
    return QSize(std::clamp(size.width(), minimum.width(), std::max(minimum.width(), maximum.width())),
                 std::clamp(size.height(), minimum.height(), std::max(minimum.height(), maximum.height())));
}

/*!
    \since 6.0

    Makes \a l a child layout of this one, and the widgets in it children of the widget of this layout. A subclass
    calls it when it adds a layout.
*/
void QLayout::addChildLayout(QLayout *l) {
    if (!l)
        return;
    l->setParent(this);
    reparentChildWidgets();
    invalidate();
}

/*!
    \since 6.0

    Adopts \a layout as addChildLayout() does, and returns \c true, unless it has a parent already.
*/
bool QLayout::adoptLayout(QLayout *layout) {
    if (!layout || layout->parent())
        return false;
    addChildLayout(layout);
    return true;
}

/*!
    \since 6.0

    Returns the part of \a r that the layout takes when it has an alignment: its size hint, placed in \a r as the
    alignment says. Without an alignment it is \a r.
*/
QRect QLayout::alignmentRect(const QRect &r) const {
    const Qt::Alignment a = alignment();
    if (!a)
        return r;
    const QSize hint = sizeHint().boundedTo(r.size());
    QSize size = r.size();
    if (a & Qt::AlignHorizontal_Mask)
        size.setWidth(hint.width());
    if (a & Qt::AlignVertical_Mask)
        size.setHeight(hint.height());
    int x = r.left();
    int y = r.top();
    if (a & Qt::AlignRight)
        x = r.right() + 1 - size.width();
    else if (a & Qt::AlignHCenter)
        x = r.left() + (r.width() - size.width()) / 2;
    if (a & Qt::AlignBottom)
        y = r.bottom() + 1 - size.height();
    else if (a & Qt::AlignVCenter)
        y = r.top() + (r.height() - size.height()) / 2;
    return QRect(QPoint(x, y), size);
}

/*!
    \since 6.0

    Handles \a event of the widget of the layout. Nt widgets tell their layouts of resizes themselves, so this does
    nothing.
*/
void QLayout::widgetEvent(QEvent *event) { (void)event; }

#include "moc_qlayout.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
