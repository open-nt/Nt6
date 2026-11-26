// MIT License - see LICENSE
#include <QtWidgets/qtabbar.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qmaterialstyle.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qtooltip.h>

#include <algorithm>

namespace {

constexpr int kCloseSize = 14; // the close button of a tab
constexpr int kDragDistance = 4;

} // namespace

/*!
    \class QTabBar
    \brief The QTabBar class provides a row of tabs.

    A tab bar shows tabs with a text, one of which is current. QTabWidget uses one above its pages; a QTabBar on its
    own suits a view that changes with the tab:

    \code
    QTabBar *bar = new QTabBar(window);
    bar->addTab("Day");
    bar->addTab("Week");
    connect(bar, &QTabBar::currentChanged, [](int index) { showRange(index); });
    \endcode

    The user clicks a tab, turns the mouse wheel over the bar, or uses Left, Right, Home and End while it has the focus;
    disabled tabs are skipped. With setTabsClosable() each tab has a close button that emits tabCloseRequested(), and a
    movable tab bar lets the user drag the tabs into another order.

    setShape() puts the tabs at another side of what they switch: below it, or at its left or right, where the
    tabs run down and their text is turned. Tabs that do not fit are shortened if elideMode() allows it, and cut off
    otherwise.

    A tab bar with the second tab current, and one with close buttons and a disabled tab.

    \image widgets/qtabbar.png QTabBar in the light color scheme

    \differences
    Triangular shapes are drawn rounded, and there are no scroll buttons (usesScrollButtons() is only kept).
    expanding() works only under QMaterialStyle, and drawBase() is only kept. The close button is drawn as an "x". changeCurrentOnDrag() is only
    kept, as Nt does not drag over tab bars.
    \enddifferences

    \sa QTabWidget
*/

/*!
    \enum QTabBar::ButtonPosition
    \since 6.0

    The side of a tab that setTabButton() puts a widget at.

    \value LeftSide Before the text.
    \value RightSide After the text.
*/

/*!
    \enum QTabBar::Shape
    \since 6.0

    The side of the pages that the tabs are at.

    \value RoundedNorth Above them, the default.
    \value RoundedSouth Below them.
    \value RoundedWest At their left, running down, with the text turned to read upwards.
    \value RoundedEast At their right, running down, with the text turned to read downwards.
    \value TriangularNorth As RoundedNorth.
    \value TriangularSouth As RoundedSouth.
    \value TriangularWest As RoundedWest.
    \value TriangularEast As RoundedEast.
*/

/*!
    \enum QTabBar::SelectionBehavior

    Which tab becomes current when the current tab is removed.

    \value SelectLeftTab The tab at its left.
    \value SelectRightTab The tab at its right, the default.
    \value SelectPreviousTab The tab that was current before it.
*/

/*!
    Constructs an empty tab bar with the given \a parent.
*/
QTabBar::QTabBar(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::TabFocus);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

/*!
    Destroys the tab bar.
*/
QTabBar::~QTabBar() = default;

/*!
    Adds a tab with the given \a text at the end and returns its index. The first tab becomes current.

    \sa insertTab()
*/
int QTabBar::addTab(const QString &text) { return insertTab(count(), text); }

/*!
    \since 6.0
    \overload

    Adds a tab with the given \a icon and \a text at the end and returns its index.
*/
int QTabBar::addTab(const QIcon &icon, const QString &text) { return insertTab(count(), icon, text); }

/*!
    \since 6.0
    \overload

    Inserts a tab with the given \a icon and \a text at \a index and returns its index.
*/
int QTabBar::insertTab(int index, const QIcon &icon, const QString &text) {
    const int at = insertTab(index, text);
    setTabIcon(at, icon);
    return at;
}

/*!
    Inserts a tab with the given \a text at \a index, or at the end if \a index is out of range, and returns its
    index.

    \sa removeTab()
*/
int QTabBar::insertTab(int index, const QString &text) {
    const int at = valid(index) ? index : count();
    Tab tab;
    tab.text = text;
    m_tabs.insert(m_tabs.begin() + at, tab);
    for (int &h : m_history)
        if (h >= at)
            ++h;
    if (m_current >= at)
        ++m_current;
    tabInserted(at);
    if (m_current < 0)
        setCurrentIndex(at);
    changed();
    autoHideChanged();
    return at;
}

/*!
    Removes the tab at \a index. If it was current, another tab becomes current as selectionBehaviorOnRemove() says.

    \sa insertTab()
*/
void QTabBar::removeTab(int index) {
    if (!valid(index))
        return;
    const bool wasCurrent = index == m_current;
    delete m_tabs[static_cast<std::size_t>(index)].left.data();
    delete m_tabs[static_cast<std::size_t>(index)].right.data();
    m_tabs.erase(m_tabs.begin() + index);
    m_history.erase(std::remove(m_history.begin(), m_history.end(), index), m_history.end());
    for (int &h : m_history)
        if (h > index)
            --h;
    m_hover = -1;
    if (index < m_current) {
        --m_current;
    } else if (wasCurrent) {
        m_current = -1;
        int next = -1;
        if (count() > 0) {
            switch (m_onRemove) {
            case SelectLeftTab:
                next = std::max(0, index - 1);
                break;
            case SelectRightTab:
                next = std::min(index, count() - 1);
                break;
            case SelectPreviousTab:
                next = m_history.empty() ? std::min(index, count() - 1) : m_history.back();
                break;
            }
        }
        if (next >= 0) {
            setCurrentIndex(next);
        } else {
            emit currentChanged(-1);
        }
    }
    tabRemoved(index);
    changed();
    autoHideChanged();
}

/*!
    Moves the tab at \a from to \a to and emits tabMoved(). The current tab stays current.
*/
void QTabBar::moveTab(int from, int to) {
    if (!valid(from) || !valid(to) || from == to)
        return;
    const Tab tab = m_tabs[static_cast<std::size_t>(from)];
    m_tabs.erase(m_tabs.begin() + from);
    m_tabs.insert(m_tabs.begin() + to, tab);
    const auto remap = [from, to](int i) {
        if (i == from)
            return to;
        if (from < to && i > from && i <= to)
            return i - 1;
        if (from > to && i >= to && i < from)
            return i + 1;
        return i;
    };
    m_current = remap(m_current);
    for (int &h : m_history)
        h = remap(h);
    emit tabMoved(from, to);
    changed();
}

/*!
    Returns whether the tab at \a index is enabled. A disabled tab cannot be made current by the user.

    \sa setTabEnabled()
*/
bool QTabBar::isTabEnabled(int index) const { return valid(index) && m_tabs[static_cast<std::size_t>(index)].enabled; }

/*!
    Enables the tab at \a index if \a enabled is \c true, and disables it otherwise.
*/
void QTabBar::setTabEnabled(int index, bool enabled) {
    if (!valid(index))
        return;
    m_tabs[static_cast<std::size_t>(index)].enabled = enabled;
    update();
}

/*!
    Returns the text of the tab at \a index.

    \sa setTabText()
*/
QString QTabBar::tabText(int index) const {
    return valid(index) ? m_tabs[static_cast<std::size_t>(index)].text : QString();
}

/*!
    Sets the text of the tab at \a index to \a text.
*/
void QTabBar::setTabText(int index, const QString &text) {
    if (!valid(index))
        return;
    m_tabs[static_cast<std::size_t>(index)].text = text;
    changed();
}

/*!
    Returns the tool tip of the tab at \a index.

    \sa setTabToolTip()
*/
QString QTabBar::tabToolTip(int index) const {
    return valid(index) ? m_tabs[static_cast<std::size_t>(index)].toolTip : QString();
}

/*!
    Sets the tool tip of the tab at \a index to \a tip.
*/
void QTabBar::setTabToolTip(int index, const QString &tip) {
    if (valid(index))
        m_tabs[static_cast<std::size_t>(index)].toolTip = tip;
}

/*!
    Returns the data of the tab at \a index.

    \sa setTabData()
*/
QVariant QTabBar::tabData(int index) const {
    return valid(index) ? m_tabs[static_cast<std::size_t>(index)].data : QVariant();
}

/*!
    Sets the data of the tab at \a index to \a data, for the application's own use.
*/
void QTabBar::setTabData(int index, const QVariant &data) {
    if (valid(index))
        m_tabs[static_cast<std::size_t>(index)].data = data;
}

/*!
    \since 6.0

    Returns the icon of the tab at \a index.
*/
QIcon QTabBar::tabIcon(int index) const { return valid(index) ? m_tabs[static_cast<std::size_t>(index)].icon : QIcon(); }

/*!
    \since 6.0

    Sets the icon of the tab at \a index to \a icon, drawn before its text in iconSize().
*/
void QTabBar::setTabIcon(int index, const QIcon &icon) {
    if (!valid(index))
        return;
    m_tabs[static_cast<std::size_t>(index)].icon = icon;
    changed();
}

/*!
    \since 6.0

    Returns the color of the text of the tab at \a index, or an invalid color if it uses the palette.
*/
QColor QTabBar::tabTextColor(int index) const {
    return valid(index) ? m_tabs[static_cast<std::size_t>(index)].textColor : QColor();
}

/*!
    \since 6.0

    Sets the color of the text of the tab at \a index to \a color; an invalid color uses the palette.
*/
void QTabBar::setTabTextColor(int index, const QColor &color) {
    if (!valid(index))
        return;
    m_tabs[static_cast<std::size_t>(index)].textColor = color;
    update();
}

/*!
    \since 6.0

    Returns the "What's This?" help of the tab at \a index.
*/
QString QTabBar::tabWhatsThis(int index) const {
    return valid(index) ? m_tabs[static_cast<std::size_t>(index)].whatsThis : QString();
}

/*!
    \since 6.0

    Sets the "What's This?" help of the tab at \a index to \a text.
*/
void QTabBar::setTabWhatsThis(int index, const QString &text) {
    if (valid(index))
        m_tabs[static_cast<std::size_t>(index)].whatsThis = text;
}

/*!
    \since 6.0

    Returns the name of the tab at \a index for accessibility tools.
*/
QString QTabBar::accessibleTabName(int index) const {
    return valid(index) ? m_tabs[static_cast<std::size_t>(index)].accessibleName : QString();
}

/*!
    \since 6.0

    Sets the name of the tab at \a index for accessibility tools to \a name.
*/
void QTabBar::setAccessibleTabName(int index, const QString &name) {
    if (valid(index))
        m_tabs[static_cast<std::size_t>(index)].accessibleName = name;
}

/*!
    \since 6.0

    Returns \c true if the tab at \a index is shown.
*/
bool QTabBar::isTabVisible(int index) const { return valid(index) && m_tabs[static_cast<std::size_t>(index)].visible; }

/*!
    \since 6.0

    Shows the tab at \a index if \a visible is \c true, and hides it otherwise. A hidden tab takes no room; if it
    was current, the next visible and enabled tab becomes current.
*/
void QTabBar::setTabVisible(int index, bool visible) {
    if (!valid(index) || m_tabs[static_cast<std::size_t>(index)].visible == visible)
        return;
    m_tabs[static_cast<std::size_t>(index)].visible = visible;
    if (!visible && index == m_current) {
        for (int i = index + 1; i < count(); ++i)
            if (isTabVisible(i) && isTabEnabled(i)) {
                setCurrentIndex(i);
                break;
            }
        if (m_current == index)
            for (int i = index - 1; i >= 0; --i)
                if (isTabVisible(i) && isTabEnabled(i)) {
                    setCurrentIndex(i);
                    break;
                }
    }
    changed();
}

/*!
    \fn QTabBar::Shape QTabBar::shape() const
    \since 6.0

    Returns the shape of the tabs, RoundedNorth by default.
*/

/*!
    \since 6.0

    Sets the shape of the tabs to \a shape: at which side of the pages they are.
*/
void QTabBar::setShape(Shape shape) {
    if (m_shape == shape)
        return;
    const bool wasVertical = vertical();
    m_shape = shape;
    if (wasVertical != vertical())
        setSizePolicy(vertical() ? QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred)
                                 : QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed));
    changed();
}

/*!
    \since 6.0

    Returns the size of the icons of the tabs: the one set with setIconSize(), or that of the style.
*/
QSize QTabBar::iconSize() const {
    if (m_iconSize.isValid())
        return m_iconSize;
    const int extent = style()->pixelMetric(QStyle::PM_TabBarIconSize, nullptr, this);
    return extent > 0 ? QSize(extent, extent) : QSize(16, 16);
}

/*!
    \since 6.0

    Sets the size of the icons of the tabs to \a size.
*/
void QTabBar::setIconSize(const QSize &size) {
    m_iconSize = size;
    changed();
}

/*!
    \fn Qt::TextElideMode QTabBar::elideMode() const
    \since 6.0

    Returns how the text of tabs that do not fit is shortened. The default is Qt::ElideNone: the tabs are cut off.
*/

/*!
    \since 6.0

    Sets how the text of tabs that do not fit is shortened to \a mode.
*/
void QTabBar::setElideMode(Qt::TextElideMode mode) {
    m_elideMode = mode;
    changed();
}

/*!
    \fn bool QTabBar::autoHide() const
    \since 6.0

    Returns \c true if the tab bar hides itself while it has fewer than two tabs.
*/

/*!
    \since 6.0

    Makes the tab bar hide itself while it has fewer than two tabs if \a hide is \c true.
*/
void QTabBar::setAutoHide(bool hide) {
    if (m_autoHide == hide)
        return;
    m_autoHide = hide;
    if (hide)
        autoHideChanged();
    else
        show();
}

void QTabBar::autoHideChanged() {
    if (m_autoHide)
        setVisible(count() > 1);
}

/*!
    \fn bool QTabBar::changeCurrentOnDrag() const
    \since 6.0

    Returns whether a drag over a tab makes it current. Nt keeps the value.
*/

/*!
    \fn void QTabBar::setChangeCurrentOnDrag(bool change)
    \since 6.0

    Sets whether a drag over a tab makes it current to \a change.
*/

/*!
    Returns the rectangle of the tab at \a index in the tab bar, or an empty rectangle if there is no such tab.
*/
QRect QTabBar::tabRect(int index) const {
    if (!valid(index))
        return QRect();
    return layoutTabs()[static_cast<std::size_t>(index)];
}

bool QTabBar::vertical() const {
    return m_shape == RoundedWest || m_shape == RoundedEast || m_shape == TriangularWest || m_shape == TriangularEast;
}

std::vector<QRect> QTabBar::layoutTabs() const {
    const bool vert = vertical();
    const int room = vert ? height() : width();
    const int thick = vert ? width() : height();
    std::vector<int> lengths(m_tabs.size(), 0);
    long long sum = 0;
    for (int i = 0; i < count(); ++i) {
        if (!m_tabs[static_cast<std::size_t>(i)].visible)
            continue;
        const QSize s = tabSizeHint(i);
        lengths[static_cast<std::size_t>(i)] = vert ? s.height() : s.width();
        sum += lengths[static_cast<std::size_t>(i)];
    }
    // Tabs that do not fit are shortened, down to their minimum, when their text may be elided.
    if (m_elideMode != Qt::ElideNone && room > 0 && sum > room) {
        for (int i = 0; i < count(); ++i) {
            int &length = lengths[static_cast<std::size_t>(i)];
            if (length == 0)
                continue;
            const QSize minimum = minimumTabSizeHint(i);
            length = std::max(vert ? minimum.height() : minimum.width(), static_cast<int>(length * room / sum));
        }
    }
    // Material's primary tabs share the width of their bar. Under other styles expanding() is only kept.
    if (m_expanding && !vert && room > sum && qobject_cast<const QMaterialStyle *>(style())) {
        const long long shown = std::count_if(lengths.begin(), lengths.end(), [](int length) { return length > 0; });
        if (shown > 0) {
            // What is left over goes to every tab alike, and the pixels that do not divide to the first ones.
            const long long share = (room - sum) / shown;
            long long odd = (room - sum) % shown;
            for (int &length : lengths) {
                if (length > 0)
                    length += static_cast<int>(share + (odd-- > 0 ? 1 : 0));
            }
        }
    }
    std::vector<QRect> rects;
    rects.reserve(m_tabs.size());
    int at = 0;
    for (const int length : lengths) {
        rects.push_back(vert ? QRect(0, at, thick, length) : QRect(at, 0, length, thick));
        at += length;
    }
    return rects;
}

QTransform QTabBar::tabTransform(const QRect &rect) const {
    QTransform t;
    switch (m_shape) {
    case RoundedWest:
    case TriangularWest:
        t.translate(rect.left(), rect.bottom() + 1);
        t.rotate(-90);
        break;
    case RoundedEast:
    case TriangularEast:
        t.translate(rect.right() + 1, rect.top());
        t.rotate(90);
        break;
    default:
        t.translate(rect.left(), rect.top());
        break;
    }
    return t;
}

/*!
    Returns the index of the tab at \a position, or -1 if there is none.
*/
int QTabBar::tabAt(const QPoint &position) const {
    const std::vector<QRect> rects = layoutTabs();
    for (int i = 0; i < count(); ++i)
        if (m_tabs[static_cast<std::size_t>(i)].visible && rects[static_cast<std::size_t>(i)].contains(position))
            return i;
    return -1;
}

/*!
    \fn int QTabBar::currentIndex() const

    Returns the index of the current tab, or -1 if there are no tabs.
*/

/*!
    \fn int QTabBar::count() const

    Returns the number of tabs.
*/

/*!
    \fn bool QTabBar::tabsClosable() const

    Returns whether the tabs have close buttons. The default is \c false.

    \sa setTabsClosable()
*/

/*!
    Sets whether the tabs have close buttons to \a closable.

    \sa tabCloseRequested()
*/
void QTabBar::setTabsClosable(bool closable) {
    if (m_closable == closable)
        return;
    m_closable = closable;
    changed();
}

/*!
    \fn bool QTabBar::isMovable() const

    Returns whether the user can drag the tabs into another order. The default is \c false.
*/

/*!
    \fn void QTabBar::setMovable(bool movable)

    Sets whether the user can drag the tabs into another order to \a movable.
*/

/*!
    \fn QTabBar::SelectionBehavior QTabBar::selectionBehaviorOnRemove() const

    Returns which tab becomes current when the current one is removed, SelectRightTab by default.
*/

/*!
    \fn void QTabBar::setSelectionBehaviorOnRemove(SelectionBehavior behavior)

    Sets which tab becomes current when the current one is removed to \a behavior.
*/

/*!
    \reimp
*/
QSize QTabBar::sizeHint() const {
    int width = 0;
    int height = 0;
    for (int i = 0; i < count(); ++i) {
        if (!m_tabs[static_cast<std::size_t>(i)].visible)
            continue;
        const QSize s = horizontalSize(i, false);
        width += s.width();
        height = std::max(height, s.height());
    }
    if (height == 0) {
        QStyleOptionTab option;
        option.initFrom(this);
        height =
            style()->sizeFromContents(QStyle::CT_TabBarTab, &option, QSize(0, fontMetrics().height()), this).height();
    }
    return vertical() ? QSize(height, width) : QSize(width, height);
}

/*!
    \reimp
*/
QSize QTabBar::minimumSizeHint() const {
    const QSize hint = sizeHint();
    return vertical() ? QSize(hint.width(), 0) : QSize(0, hint.height());
}

/*!
    Makes the tab at \a index current and emits currentChanged(), if it is another tab.

    \sa currentIndex()
*/
void QTabBar::setCurrentIndex(int index) {
    if (!valid(index) || index == m_current)
        return;
    if (m_current >= 0) {
        m_history.erase(std::remove(m_history.begin(), m_history.end(), m_current), m_history.end());
        m_history.push_back(m_current);
    }
    m_current = index;
    update();
    emit currentChanged(index);
}

/*!
    Returns the size of the tab at \a index. Reimplement it for tabs of another size.
*/
QSize QTabBar::tabSizeHint(int index) const {
    const QSize s = horizontalSize(index, false);
    return vertical() ? s.transposed() : s;
}

/*!
    \since 6.0

    Returns the smallest size of the tab at \a index, with its text shortened to three characters and an ellipsis,
    which a tab is not shortened below when the tabs do not fit.

    \sa elideMode()
*/
QSize QTabBar::minimumTabSizeHint(int index) const {
    const QSize s = horizontalSize(index, true);
    return vertical() ? s.transposed() : s;
}

// The size of the tab at `index` as if it were at the top, with its full text or the shortest one.
QSize QTabBar::horizontalSize(int index, bool minimum) const {
    QStyleOptionTab option;
    option.initFrom(this);
    option.text = tabText(index);
    if (minimum && option.text.size() > 3)
        option.text = option.text.left(3) + QChar(0x2026);
    QSize text = fontMetrics().size(Qt::TextSingleLine, option.text);
    if (valid(index) && !m_tabs[static_cast<std::size_t>(index)].icon.isNull()) {
        const QSize icon = iconSize();
        text.setWidth(text.width() + icon.width() + 4);
        text.setHeight(std::max(text.height(), icon.height()));
    }
    if (m_closable)
        text.setWidth(text.width() + kCloseSize + 4);
    // Room for the buttons of the tab, which are as high as they like up to the height of the bar.
    if (valid(index)) {
        for (const QWidget *button : {m_tabs[static_cast<std::size_t>(index)].left.data(),
                                      m_tabs[static_cast<std::size_t>(index)].right.data()}) {
            if (!button)
                continue;
            const QSize hint = button->sizeHint().isValid() ? button->sizeHint() : button->size();
            text.setWidth(text.width() + hint.width() + 4);
            text.setHeight(std::max(text.height(), hint.height()));
        }
    }
    return style()->sizeFromContents(QStyle::CT_TabBarTab, &option, text, this);
}

/*!
    Is called after a tab is inserted at \a index. The default implementation does nothing.
*/
void QTabBar::tabInserted(int index) { (void)index; }

/*!
    Is called after the tab at \a index is removed. The default implementation does nothing.
*/
void QTabBar::tabRemoved(int index) { (void)index; }

/*!
    Is called when the sizes of the tabs change. The default implementation does nothing.
*/
void QTabBar::tabLayoutChange() {}

void QTabBar::changed() {
    tabLayoutChange();
    updateGeometry();
    placeButtons();
    update();
}

// Puts the buttons of the tabs at the sides of their tabs, centred down.
void QTabBar::placeButtons() {
    const std::vector<QRect> rects = layoutTabs();
    const bool vert = vertical();
    for (int i = 0; i < count(); ++i) {
        const Tab &tab = m_tabs[static_cast<std::size_t>(i)];
        const int offset = i == m_dragging ? m_dragOffset : 0;
        const QRect rect = rects[static_cast<std::size_t>(i)].translated(vert ? 0 : offset, vert ? offset : 0);
        // The buttons are placed in the coordinates of the tab drawn at the top, then turned with the tab.
        const QTransform toBar = tabTransform(rect);
        const QSize local = vert ? rect.size().transposed() : rect.size();
        const auto place = [&](QWidget *button, bool atStart) {
            if (!button)
                return;
            if (!tab.visible) {
                button->hide();
                return;
            }
            const QSize size = button->sizeHint().isValid() ? button->sizeHint() : button->size();
            const int end = m_closable ? local.width() - kCloseSize - 6 : local.width() - 3;
            const QRect inTab(atStart ? 4 : end - size.width(), (local.height() - size.height()) / 2, size.width(),
                              size.height());
            const QPoint center = toBar.map(inTab.center());
            button->setGeometry(QRect(QPoint(center.x() - size.width() / 2, center.y() - size.height() / 2), size));
            button->show();
        };
        place(tab.left.data(), true);
        place(tab.right.data(), false);
    }
}

/*!
    \reimp
*/
void QTabBar::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    placeButtons();
}

/*!
    \since 6.0

    Returns the widget at the side \a position of the tab at \a index, or \c nullptr.
*/
QWidget *QTabBar::tabButton(int index, ButtonPosition position) const {
    if (!valid(index))
        return nullptr;
    const Tab &tab = m_tabs[static_cast<std::size_t>(index)];
    return position == LeftSide ? tab.left.data() : tab.right.data();
}

/*!
    \since 6.0

    Puts \a widget, such as a small tool button, at the side \a position of the tab at \a index. The tab bar takes
    ownership of it and deletes the widget that was there; \c nullptr removes it. The tab grows to hold it.
*/
void QTabBar::setTabButton(int index, ButtonPosition position, QWidget *widget) {
    if (!valid(index))
        return;
    QPointer<QWidget> &slot = position == LeftSide ? m_tabs[static_cast<std::size_t>(index)].left
                                                   : m_tabs[static_cast<std::size_t>(index)].right;
    if (slot == widget)
        return;
    delete slot.data();
    slot = widget;
    if (widget)
        widget->setParent(this);
    changed();
}

/*!
    \fn bool QTabBar::expanding() const
    \since 6.0

    Returns whether the tabs would stretch to fill the bar. Nt keeps the value; tabs keep their size.
*/

/*!
    \fn void QTabBar::setExpanding(bool enabled)
    \since 6.0

    Sets whether the tabs stretch to fill the bar to \a enabled.
*/

/*!
    \fn bool QTabBar::drawBase() const
    \since 6.0

    Returns whether a base line is drawn under the tabs. Nt keeps the value.
*/

/*!
    \fn void QTabBar::setDrawBase(bool drawTheBase)
    \since 6.0

    Sets whether a base line is drawn under the tabs to \a drawTheBase.
*/

/*!
    \fn bool QTabBar::documentMode() const
    \since 6.0

    Returns whether the tab bar is drawn as for the documents of a main window.
*/

/*!
    \since 6.0

    Sets whether the tab bar is drawn as for documents to \a set. Nt keeps the value and draws the tabs the same.
*/
void QTabBar::setDocumentMode(bool set) {
    m_documentMode = set;
    update();
}

/*!
    \fn bool QTabBar::usesScrollButtons() const
    \since 6.0

    Returns whether buttons scroll tabs that do not fit. Nt keeps the value; tabs that do not fit are cut off.
*/

/*!
    \fn void QTabBar::setUsesScrollButtons(bool useButtons)
    \since 6.0

    Sets whether buttons scroll tabs that do not fit to \a useButtons.
*/

QRect QTabBar::closeRect(int index) const {
    if (!m_closable || !valid(index) || !m_tabs[static_cast<std::size_t>(index)].visible)
        return QRect();
    const QRect tab = tabRect(index);
    const QSize local = vertical() ? tab.size().transposed() : tab.size();
    const QRect inTab(local.width() - kCloseSize - 4, (local.height() - kCloseSize) / 2, kCloseSize, kCloseSize);
    return tabTransform(tab).mapRect(inTab);
}

// Makes the next enabled tab in `direction` current.
void QTabBar::step(int direction) {
    for (int i = m_current + direction; valid(i); i += direction) {
        if (isTabEnabled(i) && isTabVisible(i)) {
            setCurrentIndex(i);
            return;
        }
    }
}

/*!
    \reimp

    A tool tip event over a tab shows the tool tip of the tab.
*/
bool QTabBar::event(QEvent *event) {
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        const int index = tabAt(help->pos());
        if (index >= 0 && !tabToolTip(index).isEmpty()) {
            QToolTip::showText(help->globalPos(), tabToolTip(index), this, tabRect(index));
            return true;
        }
        event->ignore();
        return true;
    }
    return QWidget::event(event);
}

/*!
    \reimp
*/
void QTabBar::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    painter.setFont(font());
    const std::vector<QRect> rects = layoutTabs();
    const bool vert = vertical();
    const bool south = m_shape == RoundedSouth || m_shape == TriangularSouth;
    // The current tab, and a tab that is dragged, are drawn last so that they overlap their neighbours. Each tab is
    // drawn as a tab at the top, in coordinates that tabTransform() turns to its side.
    const auto drawTab = [&](int i) {
        const Tab &tab = m_tabs[static_cast<std::size_t>(i)];
        if (!tab.visible)
            return;
        const int offset = i == m_dragging ? m_dragOffset : 0;
        const QRect rect = rects[static_cast<std::size_t>(i)].translated(vert ? 0 : offset, vert ? offset : 0);
        QStyleOptionTab option;
        initStyleOption(&option, i);
        option.rect = QRect(QPoint(0, 0), vert ? rect.size().transposed() : rect.size());
        painter.save();
        // The shape, without the text; a tab below the pages is drawn upside down.
        QStyleOptionTab shape = option;
        shape.text.clear();
        shape.icon = QIcon();
        if (south) {
            QTransform flip;
            flip.translate(rect.right() + 1, rect.bottom() + 1);
            flip.rotate(180);
            painter.setTransform(flip);
        } else {
            painter.setTransform(tabTransform(rect));
        }
        style()->drawControl(QStyle::CE_TabBarTab, &shape, &painter, this);
        painter.setTransform(tabTransform(rect));
        int leftRoom = 4;
        int rightRoom = m_closable ? kCloseSize + 6 : 4;
        if (tab.left)
            leftRoom += tab.left->width() + 4;
        if (tab.right)
            rightRoom += tab.right->width() + 4;
        QRect textRect = option.rect.adjusted(leftRoom, 0, -rightRoom, 0);
        const QPalette::ColorGroup group =
            (option.state & QStyle::State_Enabled) ? QPalette::Active : QPalette::Disabled;
        if (!tab.icon.isNull()) {
            const QSize icon = iconSize();
            const int textWidth = fontMetrics().horizontalAdvance(option.text);
            const int total = icon.width() + 4 + textWidth;
            const int x = textRect.left() + std::max(0, (textRect.width() - total) / 2);
            const QPixmap pixmap = tab.icon.pixmap(icon, (option.state & QStyle::State_Enabled) ? QIcon::Normal
                                                                                                 : QIcon::Disabled);
            painter.drawPixmap(x, textRect.top() + (textRect.height() - icon.height()) / 2, pixmap);
            textRect.setLeft(x + icon.width() + 4);
            textRect.setWidth(std::min(textRect.width(), textWidth));
        }
        QString text = option.text;
        if (m_elideMode != Qt::ElideNone)
            text = fontMetrics().elidedText(text, m_elideMode, textRect.width());
        QColor ink = palette().color(group, QPalette::WindowText);
        // Material gives the selected tab's label the accent color, which a palette has no role for.
        if (const auto *material = qobject_cast<const QMaterialStyle *>(style()); material && group == QPalette::Active)
            ink = material->color(i == m_current ? QMaterialStyle::Role::Primary
                                                 : QMaterialStyle::Role::OnSurfaceVariant);
        painter.setPen(tab.textColor.isValid() ? tab.textColor : ink);
        painter.drawText(textRect, static_cast<int>(Qt::AlignCenter) | Qt::TextSingleLine, text);
        if (m_closable) {
            const QRect close(option.rect.width() - kCloseSize - 4, (option.rect.height() - kCloseSize) / 2,
                              kCloseSize, kCloseSize);
            if (i == m_pressedClose)
                painter.fillRect(close, palette().color(group, QPalette::Mid));
            painter.setPen(palette().color(group, QPalette::ButtonText));
            painter.drawLine(close.left() + 3, close.top() + 3, close.right() - 3, close.bottom() - 3);
            painter.drawLine(close.right() - 3, close.top() + 3, close.left() + 3, close.bottom() - 3);
        }
        painter.restore();
    };
    for (int i = 0; i < count(); ++i)
        if (i != m_current && i != m_dragging)
            drawTab(i);
    if (m_current >= 0 && m_current != m_dragging)
        drawTab(m_current);
    if (m_dragging >= 0)
        drawTab(m_dragging);
}

/*!
    \since 6.0

    Fills \a option with the state of the tab at \a tabIndex, for the style to draw it.
*/
void QTabBar::initStyleOption(QStyleOptionTab *option, int tabIndex) const {
    if (!option || !valid(tabIndex))
        return;
    option->initFrom(this);
    option->rect = tabRect(tabIndex);
    option->text = tabText(tabIndex);
    option->icon = tabIcon(tabIndex);
    option->iconSize = iconSize();
    option->shape = m_shape;
    option->documentMode = m_documentMode;
    option->tabIndex = tabIndex;
    option->position = count() == 1 ? QStyleOptionTab::OnlyOneTab
                       : tabIndex == 0 ? QStyleOptionTab::Beginning
                       : tabIndex == count() - 1 ? QStyleOptionTab::End
                                                 : QStyleOptionTab::Middle;
    option->state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver | QStyle::State_Enabled);
    if (isTabEnabled(tabIndex) && isEnabled())
        option->state |= QStyle::State_Enabled;
    if (tabIndex == m_current) {
        option->state |= QStyle::State_Selected;
        if (hasFocus())
            option->state |= QStyle::State_HasFocus;
    }
    if (tabIndex == m_hover)
        option->state |= QStyle::State_MouseOver;
    const QColor color = m_tabs[static_cast<std::size_t>(tabIndex)].textColor;
    if (color.isValid())
        option->palette.setColor(QPalette::WindowText, color);
}

/*!
    \reimp
*/
void QTabBar::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    const int index = tabAt(event->pos());
    emit tabBarClicked(index);
    if (index < 0) {
        event->accept();
        return;
    }
    if (closeRect(index).contains(event->pos())) {
        m_pressedClose = index;
        update();
        event->accept();
        return;
    }
    if (isTabEnabled(index))
        setCurrentIndex(index);
    if (m_movable) {
        m_dragging = index;
        m_pressX = along(event->pos());
        m_dragOffset = 0;
        placeButtons();
    }
    event->accept();
}

/*!
    \reimp

    Dragging a tab of a movable tab bar past the middle of its neighbour swaps them.
*/
void QTabBar::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging >= 0 && (event->buttons() & Qt::LeftButton)) {
        int offset = along(event->pos()) - m_pressX;
        if (std::abs(offset) < kDragDistance && m_dragOffset == 0) {
            event->accept();
            return;
        }
        while (offset > 0 && valid(m_dragging + 1) && offset > lengthOf(tabRect(m_dragging + 1)) / 2) {
            const int length = lengthOf(tabRect(m_dragging + 1));
            moveTab(m_dragging, m_dragging + 1);
            ++m_dragging;
            m_pressX += length;
            offset -= length;
        }
        while (offset < 0 && valid(m_dragging - 1) && -offset > lengthOf(tabRect(m_dragging - 1)) / 2) {
            const int length = lengthOf(tabRect(m_dragging - 1));
            moveTab(m_dragging, m_dragging - 1);
            --m_dragging;
            m_pressX -= length;
            offset += length;
        }
        m_dragOffset = offset;
        placeButtons();
        update();
        event->accept();
        return;
    }
    const int index = tabAt(event->pos());
    if (index != m_hover) {
        m_hover = index;
        update();
    }
    event->accept();
}

/*!
    \reimp

    Releasing the mouse on the close button that was pressed emits tabCloseRequested().
*/
void QTabBar::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    if (m_pressedClose >= 0) {
        const int index = m_pressedClose;
        m_pressedClose = -1;
        update();
        if (closeRect(index).contains(event->pos()))
            emit tabCloseRequested(index);
    }
    if (m_dragging >= 0) {
        m_dragging = -1;
        m_dragOffset = 0;
        placeButtons();
        update();
    }
    event->accept();
}

/*!
    \reimp
*/
void QTabBar::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    emit tabBarDoubleClicked(tabAt(event->pos()));
    event->accept();
}

/*!
    \reimp

    Left and Right move to the previous and next enabled tab, Home and End to the first and last.
*/
void QTabBar::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Up:
        step(-1);
        break;
    case Qt::Key_Right:
    case Qt::Key_Down:
        step(1);
        break;
    case Qt::Key_Home:
        for (int i = 0; i < count(); ++i)
            if (isTabEnabled(i) && isTabVisible(i)) {
                setCurrentIndex(i);
                break;
            }
        break;
    case Qt::Key_End:
        for (int i = count() - 1; i >= 0; --i)
            if (isTabEnabled(i) && isTabVisible(i)) {
                setCurrentIndex(i);
                break;
            }
        break;
    default:
        event->ignore();
        return;
    }
    event->accept();
}

/*!
    \reimp

    The wheel moves to the previous or next enabled tab.
*/
void QTabBar::wheelEvent(QWheelEvent *event) {
    const int notches = event->angleDelta().y() / 120;
    if (notches == 0) {
        event->ignore();
        return;
    }
    step(notches > 0 ? -1 : 1);
    event->accept();
}

/*!
    \reimp
*/
void QTabBar::leaveEvent(QEvent *event) {
    m_hover = -1;
    update();
    QWidget::leaveEvent(event);
}

/*!
    \reimp
*/
void QTabBar::changeEvent(QEvent *event) {
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
        changed();
    QWidget::changeEvent(event);
}

#include "moc_qtabbar.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
