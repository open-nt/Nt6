// MIT License - see LICENSE
#include <QtWidgets/qlayoutitem.h>

#include "qwidgetutil_p.h"

#include <QtWidgets/qlayout.h>
#include <QtWidgets/qwidget.h>

#include <algorithm>

/*!
    \class QLayoutItem
    \brief The QLayoutItem class provides an abstract item that a QLayout manipulates.

    A layout arranges items: widgets (QWidgetItem), blank space (QSpacerItem) and other layouts.
    A layout asks each item for its size hint, minimum and maximum size, and tells it where it is placed
    with setGeometry().

    \differences
    There is no height-for-width support: \c hasHeightForWidth(), \c heightForWidth() and
    \c minimumHeightForWidth() do not exist. There is no \c controlTypes() or \c spacerItem() on QWidgetItem other than the one here.
    \enddifferences

    \sa QLayout, QWidgetItem, QSpacerItem
*/

/*!
    \fn QLayoutItem::QLayoutItem(Qt::Alignment alignment)

    Constructs an item with the given \a alignment.
*/

/*!
    Destroys the item.
*/
QLayoutItem::~QLayoutItem() = default;

/*!
    \fn QSize QLayoutItem::sizeHint() const

    Returns the size that the item would like to have.
*/

/*!
    \fn QSize QLayoutItem::minimumSize() const

    Returns the smallest size that the item can have.
*/

/*!
    \fn QSize QLayoutItem::maximumSize() const

    Returns the largest size that the item can have.
*/

/*!
    \fn Qt::Orientations QLayoutItem::expandingDirections() const

    Returns the directions in which the item wants to get more space than its size hint.
*/

/*!
    \fn void QLayoutItem::setGeometry(const QRect &rect)

    Places the item in \a rect, which is in the coordinates of the widget that the layout manages.
*/

/*!
    \fn QRect QLayoutItem::geometry() const

    Returns the rectangle that the item was placed in.
*/

/*!
    \fn bool QLayoutItem::isEmpty() const

    Returns \c true if the item takes no space, as a hidden widget does.
*/

/*!
    \fn void QLayoutItem::invalidate()

    Tells the item that its size hint may have changed. The default does nothing.
*/

/*!
    \fn QWidget *QLayoutItem::widget() const

    Returns the widget of the item, or \c nullptr if it is not a widget item.
*/

/*!
    \fn QLayout *QLayoutItem::layout()

    Returns the layout of the item, or \c nullptr if it is not a layout.
*/

/*!
    \fn QSpacerItem *QLayoutItem::spacerItem()

    Returns the item as a QSpacerItem, or \c nullptr if it is not a spacer.
*/

/*!
    \fn Qt::Alignment QLayoutItem::alignment() const

    Returns how the item is aligned in the space it gets. The default, no alignment, lets it fill the space.

    \sa setAlignment()
*/

/*!
    \fn void QLayoutItem::setAlignment(Qt::Alignment alignment)

    Sets the alignment of the item to \a alignment.

    \sa alignment()
*/

/*!
    \class QSpacerItem
    \brief The QSpacerItem class provides blank space in a layout.

    \sa QLayoutItem, QBoxLayout::addSpacing(), QBoxLayout::addStretch()
*/

/*!
    Constructs a spacer item with the size hint \a w by \a h and the size policies \a hPolicy and
    \a vPolicy.
*/
QSpacerItem::QSpacerItem(int w, int h, QSizePolicy::Policy hPolicy, QSizePolicy::Policy vPolicy)
    : m_size(w, h), m_policy(hPolicy, vPolicy) {}

/*!
    \reimp
*/
QSize QSpacerItem::minimumSize() const {
    return QSize((m_policy.horizontalPolicy() & QSizePolicy::ShrinkFlag) ? 0 : m_size.width(),
                 (m_policy.verticalPolicy() & QSizePolicy::ShrinkFlag) ? 0 : m_size.height());
}

/*!
    \reimp
*/
QSize QSpacerItem::maximumSize() const {
    return QSize((m_policy.horizontalPolicy() & QSizePolicy::GrowFlag) ? QLAYOUTSIZE_MAX : m_size.width(),
                 (m_policy.verticalPolicy() & QSizePolicy::GrowFlag) ? QLAYOUTSIZE_MAX : m_size.height());
}

/*!
    \fn QSize QSpacerItem::sizeHint() const

    \reimp
*/

/*!
    \fn Qt::Orientations QSpacerItem::expandingDirections() const

    \reimp
*/

/*!
    \fn void QSpacerItem::setGeometry(const QRect &rect)

    \reimp
*/

/*!
    \fn QRect QSpacerItem::geometry() const

    \reimp
*/

/*!
    \fn bool QSpacerItem::isEmpty() const

    Returns \c true. A spacer takes space, but does not count as content.
*/

/*!
    \fn QSpacerItem *QSpacerItem::spacerItem()

    \reimp
*/

/*!
    \fn QSizePolicy QSpacerItem::sizePolicy() const

    Returns the size policy of the spacer.
*/

/*!
    \class QWidgetItem
    \brief The QWidgetItem class is a layout item that represents a widget.

    Layouts create one for each widget that is added. The size hint and limits come from the widget:
    its sizeHint(), minimumSizeHint(), minimumSize(), maximumSize() and sizePolicy().

    \sa QLayoutItem, QWidget
*/

/*!
    \fn QWidgetItem::QWidgetItem(QWidget *widget)

    Constructs an item for \a widget.
*/

namespace {

QSize validSize(const QSize &s) { return QSize(std::max(0, s.width()), std::max(0, s.height())); }

} // namespace

/*!
    \reimp
*/
QSize QWidgetItem::sizeHint() const {
    QSize s = validSize(m_widget->sizeHint());
    s = QtPrivate::expandedTo(s, validSize(m_widget->minimumSizeHint()));
    s = QtPrivate::boundedTo(s, m_widget->maximumSize());
    s = QtPrivate::expandedTo(s, m_widget->minimumSize());
    const QSizePolicy p = m_widget->sizePolicy();
    if (p.horizontalPolicy() == QSizePolicy::Ignored)
        s.setWidth(0);
    if (p.verticalPolicy() == QSizePolicy::Ignored)
        s.setHeight(0);
    return s;
}

/*!
    \reimp
*/
QSize QWidgetItem::minimumSize() const {
    const QSizePolicy p = m_widget->sizePolicy();
    const QSize hint = validSize(m_widget->sizeHint());
    const QSize minimumHint = validSize(m_widget->minimumSizeHint());
    const QSize set = m_widget->minimumSize();
    QSize s(0, 0);
    if (set.width() > 0)
        s.setWidth(set.width());
    else if (p.horizontalPolicy() != QSizePolicy::Ignored)
        s.setWidth((p.horizontalPolicy() & QSizePolicy::ShrinkFlag) ? minimumHint.width()
                                                                    : std::max(hint.width(), minimumHint.width()));
    if (set.height() > 0)
        s.setHeight(set.height());
    else if (p.verticalPolicy() != QSizePolicy::Ignored)
        s.setHeight((p.verticalPolicy() & QSizePolicy::ShrinkFlag) ? minimumHint.height()
                                                                   : std::max(hint.height(), minimumHint.height()));
    return QtPrivate::boundedTo(s, m_widget->maximumSize());
}

/*!
    \reimp
*/
QSize QWidgetItem::maximumSize() const {
    // Each direction on its own: a maximum that was set, or else what the policy allows.
    const QSize limit = m_widget->maximumSize();
    const QSize lowest = m_widget->minimumSize();
    const QSizePolicy p = m_widget->sizePolicy();
    const QSize hint = validSize(m_widget->sizeHint());
    const int width = limit.width() < QLAYOUTSIZE_MAX                  ? std::max(limit.width(), lowest.width())
                      : (p.horizontalPolicy() & QSizePolicy::GrowFlag) ? QLAYOUTSIZE_MAX
                                                                       : hint.width();
    const int height = limit.height() < QLAYOUTSIZE_MAX               ? std::max(limit.height(), lowest.height())
                       : (p.verticalPolicy() & QSizePolicy::GrowFlag) ? QLAYOUTSIZE_MAX
                                                                      : hint.height();
    return QSize(width, height);
}

/*!
    \reimp
*/
Qt::Orientations QWidgetItem::expandingDirections() const {
    const QSizePolicy policy = m_widget->sizePolicy();
    Qt::Orientations directions = policy.expandingDirections();
    // As in Qt, a widget with a layout that wants room also expands where its policy lets it grow.
    if (const QLayout *layout = m_widget->layout()) {
        const Qt::Orientations inside = layout->expandingDirections();
        if ((policy.horizontalPolicy() & QSizePolicy::GrowFlag) && (inside & Qt::Horizontal))
            directions |= Qt::Horizontal;
        if ((policy.verticalPolicy() & QSizePolicy::GrowFlag) && (inside & Qt::Vertical))
            directions |= Qt::Vertical;
    }
    return directions;
}

/*!
    \reimp
*/
void QWidgetItem::setGeometry(const QRect &rect) { m_widget->setGeometry(rect); }

/*!
    \reimp
*/
QRect QWidgetItem::geometry() const { return m_widget->geometry(); }

/*!
    \reimp
*/
bool QWidgetItem::isEmpty() const { return m_widget->isHidden(); }

/*!
    \fn QWidget *QWidgetItem::widget() const

    \reimp
*/
