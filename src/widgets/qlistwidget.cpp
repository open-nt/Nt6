// MIT License - see LICENSE
#include <QtWidgets/qlistwidget.h>

#include <QtCore/qdatastream.h>

#include <QtCore/qmimedata.h>

#include <QtCore/qdebug.h>
#include <QtGui/qevent.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qmaterialstyle.h>

#include "qitemtextmatch_p.h"

#include <functional>
#include <QtGui/qpainter.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qtooltip.h>

#include <algorithm>

namespace {
constexpr int textMargin = 3;
// As QStyledItemDelegate has them for the rows of a list in the Material style.
constexpr int materialRowHeight = 56, materialMargin = 16, materialSpacing = 12;
constexpr int materialDrawerMargin = 28; // inside the pill of a row of a navigation drawer
}

/*!
    \class QListWidgetItem
    \brief The QListWidgetItem class is a row of a QListWidget.

    An item has a text, and how it is drawn: its alignment, colors and font. Create an item and add it to a list,
    which takes it over:

    \code
    QListWidgetItem *item = new QListWidgetItem("Inbox");
    item->setToolTip("Mail that has not been sorted");
    list->addItem(item);
    \endcode

    Deleting an item takes it out of its list.

    \differences
    An item holds a text, not a \c QVariant: there is no \c data(), \c setData(), \c setIcon(), \c setCheckState() or
    \c setSizeHint(), no \c setHidden(), and no \c clone(). An item with Qt::ItemIsEditable is edited
    with a line edit, by QListWidget::editItem() or a double click. An item cannot be copied.
    \enddifferences

    \sa QListWidget
*/

/*!
    \enum QListWidgetItem::ItemType
    \since 6.0

    The type() of an item.

    \value Type A QListWidgetItem.
    \value UserType The first type for subclasses.
*/

/*!
    Constructs an item with no text, of the type \a type, and adds it at the end of \a listview if it is not
    \c nullptr.
*/
QListWidgetItem::QListWidgetItem(QListWidget *listview, int type) : m_type(type) {
    if (listview)
        listview->addItem(this);
}

/*!
    Constructs an item with the given \a text, of the type \a type, and adds it at the end of \a listview if it is
    not \c nullptr.
*/
QListWidgetItem::QListWidgetItem(const QString &text, QListWidget *listview, int type) : m_text(text), m_type(type) {
    if (listview)
        listview->addItem(this);
}

/*!
    \fn int QListWidgetItem::type() const
    \since 6.0

    Returns the type that the item was constructed with.
*/

/*!
    \fn bool QListWidgetItem::isHidden() const
    \since 6.0

    Returns \c true if the item is hidden.
*/

/*!
    \since 6.0

    Hides the item if \a hide is \c true, or shows it. A hidden item takes no room and cannot be chosen.
*/
void QListWidgetItem::setHidden(bool hide) {
    if (hide == m_hidden)
        return;
    m_hidden = hide;
    if (m_list) {
        if (hide && m_widget)
            m_widget->hide();
        m_list->invalidateLayout();
        m_list->updateScrollBars();
        m_list->placeItemWidgets();
        m_list->viewport()->update();
    }
}

/*!
    Destroys the item, and removes it from its list.
*/
QListWidgetItem::~QListWidgetItem() {
    if (m_list)
        m_list->forget(this);
}

/*!
    \fn QListWidget *QListWidgetItem::listWidget() const

    Returns the list that has the item, or \c nullptr if it is in none.
*/

void QListWidgetItem::changed() {
    if (m_list)
        m_list->itemDidChange(this);
}

/*!
    \fn QString QListWidgetItem::text() const

    Returns the text of the item.

    \sa setText()
*/

/*!
    \since 6.0

    Constructs an item with \a icon and \a text, of the type \a type, and adds it to the end of \a listview if it
    is not \c nullptr.
*/
QListWidgetItem::QListWidgetItem(const QIcon &icon, const QString &text, QListWidget *listview, int type)
    : m_text(text), m_icon(icon), m_type(type) {
    if (listview)
        listview->addItem(this);
}

/*!
    \fn QIcon QListWidgetItem::icon() const
    \since 6.0

    Returns the icon of the item, shown before its text; a null icon if it has none.
*/

/*!
    \since 6.0

    Sets the icon of the item to \a icon, which the list shows in its icon size.

    \sa QAbstractItemView::setIconSize()
*/
void QListWidgetItem::setIcon(const QIcon &icon) {
    m_icon = icon;
    if (m_list) {
        m_list->updateScrollBars();
        m_list->placeItemWidgets();
    }
    changed();
}

/*!
    Sets the text of the item to \a text.

    \sa text()
*/
void QListWidgetItem::setText(const QString &text) {
    if (m_text == text)
        return;
    m_text = text;
    changed();
}

/*!
    \fn QString QListWidgetItem::toolTip() const

    Returns the tool tip of the item, which the list shows when the mouse rests over the item.

    \sa setToolTip()
*/

/*!
    \fn void QListWidgetItem::setToolTip(const QString &toolTip)

    Sets the tool tip of the item to \a toolTip. Without one, the item shows the tool tip of the list.

    \sa toolTip()
*/

/*!
    \fn Qt::Alignment QListWidgetItem::textAlignment() const

    Returns how the text is placed in the row. The default is \c{Qt::AlignLeft | Qt::AlignVCenter}.

    \sa setTextAlignment()
*/

/*!
    Sets how the text is placed in the row to \a alignment.

    \sa textAlignment()
*/
void QListWidgetItem::setTextAlignment(Qt::Alignment alignment) {
    m_alignment = alignment;
    changed();
}

/*!
    \fn QBrush QListWidgetItem::foreground() const

    Returns the brush of the text. A brush with no style (the default) stands for the text color of the palette.

    \sa setForeground()
*/

/*!
    Sets the brush of the text to \a brush. Only its color is used.

    \sa foreground()
*/
void QListWidgetItem::setForeground(const QBrush &brush) {
    m_foreground = brush;
    changed();
}

/*!
    \fn QBrush QListWidgetItem::background() const

    Returns the brush that fills the row. A brush with no style (the default) leaves it to the list.

    \sa setBackground()
*/

/*!
    Sets the brush that fills the row to \a brush. Only its color is used.

    \sa background()
*/
void QListWidgetItem::setBackground(const QBrush &brush) {
    m_background = brush;
    changed();
}

/*!
    Returns the font of the text: the one that was set, or the font of the list.

    \sa setFont()
*/
QFont QListWidgetItem::font() const { return m_hasFont || !m_list ? m_font : m_list->font(); }

/*!
    Sets the font of the text to \a font.

    \sa font()
*/
void QListWidgetItem::setFont(const QFont &font) {
    m_font = font;
    m_hasFont = true;
    changed();
}

/*!
    \fn Qt::ItemFlags QListWidgetItem::flags() const

    Returns what the user can do with the item. The default is that it is enabled and selectable.

    \sa setFlags()
*/

/*!
    Sets what the user can do with the item to \a flags. An item that is not enabled is drawn in the disabled colors,
    and the user cannot select it or make it the current item.

    \sa flags()
*/
void QListWidgetItem::setFlags(Qt::ItemFlags flags) {
    m_flags = flags;
    changed();
}

/*!
    \fn bool QListWidgetItem::isSelected() const

    Returns whether the item is selected.

    \sa setSelected()
*/

/*!
    Selects the item if \a select is \c true, or deselects it. In a list that allows one selected item, selecting an
    item deselects the one that was selected. It does nothing for an item that is in no list, that cannot be selected, or
    in a list with no selection.

    \sa isSelected()
*/
void QListWidgetItem::setSelected(bool select) {
    if (!m_list)
        return;
    const int row = m_list->row(this);
    if (select && !m_list->selectable(row))
        return;
    if (m_list->selectionMode() == QAbstractItemView::NoSelection)
        return;
    if (select && m_list->selectionMode() == QAbstractItemView::SingleSelection)
        m_list->clearSelection(row);
    m_list->setRowSelected(row, select);
    m_list->finishSelection();
}

/*!
    \fn QSize QListWidgetItem::sizeHint() const
    \since 6.0

    Returns the size hint of the item; an invalid size if none was set.

    \sa setSizeHint()
*/

/*!
    \since 6.0

    Sets the size hint of the item to \a size. A row is as high as the height of the hint if that is more than the
    height of a line of text, which makes room for an item widget.

    \sa QListWidget::setItemWidget()
*/
void QListWidgetItem::setSizeHint(const QSize &size) {
    m_sizeHint = size;
    if (m_list) {
        m_list->updateScrollBars();
        m_list->placeItemWidgets();
        m_list->viewport()->update();
    }
}

/*!
    \since 6.0

    Returns the data of the item for \a role: the text for Qt::DisplayRole and Qt::EditRole, the tool tip, the
    alignment, the brushes, the font and the size hint for their roles, and what setData() stored for the other
    roles. Returns an invalid QVariant for a role that has nothing.

    \sa setData()
*/
QVariant QListWidgetItem::data(int role) const {
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return m_text;
    case Qt::ToolTipRole:
        return m_toolTip.isEmpty() ? QVariant() : QVariant(m_toolTip);
    case Qt::TextAlignmentRole:
        return QVariant(static_cast<int>(m_alignment));
    case Qt::ForegroundRole:
        return m_foreground.style() == Qt::NoBrush ? QVariant() : QVariant::fromValue(m_foreground);
    case Qt::BackgroundRole:
        return m_background.style() == Qt::NoBrush ? QVariant() : QVariant::fromValue(m_background);
    case Qt::FontRole:
        return m_hasFont ? QVariant::fromValue(m_font) : QVariant();
    case Qt::SizeHintRole:
        return m_sizeHint.isValid() ? QVariant(m_sizeHint) : QVariant();
    case Qt::DecorationRole:
        return m_icon.isNull() ? QVariant() : QVariant::fromValue(m_icon);
    default:
        return m_data.value(role);
    }
}

/*!
    \since 6.0

    Sets the data of the item for \a role to \a value. The roles that the item shows, such as Qt::DisplayRole,
    change it; the other roles, such as Qt::UserRole, are stored for data() to return.
*/
void QListWidgetItem::setData(int role, const QVariant &value) {
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        setText(value.toString());
        return;
    case Qt::ToolTipRole:
        setToolTip(value.toString());
        return;
    case Qt::TextAlignmentRole:
        setTextAlignment(Qt::Alignment::fromInt(value.toInt()));
        return;
    case Qt::ForegroundRole:
        setForeground(value.value<QBrush>());
        return;
    case Qt::BackgroundRole:
        setBackground(value.value<QBrush>());
        return;
    case Qt::FontRole:
        setFont(value.value<QFont>());
        return;
    case Qt::SizeHintRole:
        setSizeHint(value.toSize());
        return;
    case Qt::DecorationRole:
        setIcon(value.value<QIcon>());
        return;
    default:
        if (value.isValid())
            m_data.insert(role, value);
        else
            m_data.remove(role);
        changed();
    }
}

/*!
    \class QListWidget
    \brief The QListWidget class provides a list of items, one on each row.

    A list shows its items as rows of text, and the user selects with the mouse or the keys. It scrolls when there are
    more rows than fit. Add items with addItem() and read the choice from currentItem(), or connect to the signals:

    \code
    QListWidget *list = new QListWidget(window);
    list->addItems({"Inbox", "Drafts", "Sent", "Archive"});
    list->setCurrentRow(0);
    connect(list, &QListWidget::currentTextChanged, [](const QString &text) { qDebug() << text; });
    \endcode

    The list allows one selected item. Set the selection mode with setSelectionMode(): with \c MultiSelection a
    click toggles an item, and with \c ExtendedSelection the Control and Shift keys add items and ranges, as in
    other applications. The arrow keys, Page Up, Page Down, Home and End move the current item, and typing a letter
    goes to the next item that starts with it.

    A list with a selected item, an item that is current, a colored item, one that is disabled, and a tool tip.

    \image widgets/qlistwidget.png QListWidget in the light color scheme

    \differences
    There is no model and no \c QListView: the list keeps its items itself, so indexFromItem() returns an invalid
    index and items() of MIME data is empty. supportedDragActions() is only kept. Dragging does not select a range,
    and typing a letter looks at one letter at a time. The rows
    are as wide as the viewport and long text is elided, so the list has no horizontal scroll bar. The list scrolls by
    pixels. The first item added becomes the current item, as in Qt.
    \enddifferences

    \sa QListWidgetItem, QAbstractItemView, QComboBox
*/

/*!
    Constructs an empty list with the given \a parent.
*/
QListWidget::QListWidget(QWidget *parent) : QAbstractItemView(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    verticalScrollBar()->setSingleStep(rowHeight());
}

/*!
    Destroys the list, and the items in it.
*/
QListWidget::~QListWidget() {
    for (QListWidgetItem *&item : m_items) {
        item->m_list = nullptr;
        delete item;
        item = nullptr;
    }
}

/*!
    Returns the height of a row: that of a line of text in the font of the list, or of an icon in iconSize() if an
    item has one.
*/
int QListWidget::rowHeight() const {
    int height = fontMetrics().height();
    for (const QListWidgetItem *item : m_items) {
        if (!item->m_icon.isNull()) {
            height = std::max(height, (iconSize().isValid() ? iconSize().height() : 16) + 2);
            break;
        }
    }
    // The Material style's rows are for a finger.
    if (m_viewMode == QListView::ListMode && qobject_cast<const QMaterialStyle *>(style()))
        height = std::max(height, materialRowHeight);
    return height;
}

/*!
    \fn int QListWidget::count() const

    Returns the number of items.
*/

/*!
    Returns the item at \a row, or \c nullptr if there is none.

    \sa row()
*/
QListWidgetItem *QListWidget::item(int row) const {
    return row >= 0 && row < count() ? m_items[static_cast<std::size_t>(row)] : nullptr;
}

/*!
    Returns the row of \a item, or -1 if it is not in this list.

    \sa item()
*/
int QListWidget::row(const QListWidgetItem *item) const {
    for (int i = 0; i < count(); ++i)
        if (m_items[static_cast<std::size_t>(i)] == item)
            return i;
    return -1;
}

/*!
    Adds \a item at the end of the list, which takes it over.

    \sa insertItem()
*/
void QListWidget::addItem(QListWidgetItem *item) { insertItem(count(), item); }

/*!
    \overload

    Adds an item with the given \a text at the end of the list.
*/
void QListWidget::addItem(const QString &text) { insertItem(count(), text); }

/*!
    Adds an item for each of the \a labels at the end of the list.
*/
void QListWidget::addItems(const QStringList &labels) { insertItems(count(), labels); }

/*!
    Inserts \a item at \a row, which is brought into the range 0 to count(); the items from \a row on move down. An
    item that is in another list or in this one moves.

    \sa addItem(), takeItem()
*/
void QListWidget::insertItem(int row, QListWidgetItem *item) {
    if (!item)
        return;
    if (item->m_list) {
        QListWidget *other = item->m_list;
        const int old = other->row(item);
        other->takeItem(old);
        if (other == this && old < row)
            --row;
    }
    row = std::clamp(row, 0, count());
    m_items.insert(m_items.begin() + row, item);
    item->m_list = this;
    if (m_current >= row)
        ++m_current;
    if (m_anchor >= row)
        ++m_anchor;
    if (item->m_selected) {
        // A selected item that comes in is kept only if the mode allows it.
        const bool allowed = selectionMode() != NoSelection && selectable(row);
        item->m_selected = false;
        if (allowed)
            setRowSelected(row, true);
        finishSelection();
    }
    updateScrollBars();
    placeItemWidgets();
    viewport()->update();
    if (m_current < 0 && count() == 1)
        moveCurrent(0);
    if (m_sorting)
        sortItems(Qt::AscendingOrder);
}

/*!
    \overload

    Inserts an item with the given \a text at \a row.
*/
void QListWidget::insertItem(int row, const QString &text) { insertItem(row, new QListWidgetItem(text)); }

/*!
    Inserts an item for each of the \a labels at \a row.
*/
void QListWidget::insertItems(int row, const QStringList &labels) {
    row = std::clamp(row, 0, count());
    for (const QString &label : labels)
        insertItem(row++, label);
}

/*!
    Removes the item at \a row from the list and returns it; the caller owns it. Returns \c nullptr if there is no such
    row. If it was the current item, the one that takes its place is.

    \sa insertItem()
*/
QListWidgetItem *QListWidget::takeItem(int row) {
    QListWidgetItem *item = this->item(row);
    if (!item)
        return nullptr;
    const bool wasSelected = item->m_selected;
    const bool wasCurrent = row == m_current;
    m_items.erase(m_items.begin() + row);
    if (item->m_widget) {
        item->m_widget->deleteLater();
        item->m_widget = nullptr;
    }
    item->m_list = nullptr;
    item->m_selected = false;
    if (m_anchor == row)
        m_anchor = -1;
    else if (m_anchor > row)
        --m_anchor;
    if (wasSelected)
        emit itemSelectionChanged();
    if (wasCurrent) {
        m_current = std::min(row, count() - 1);
        QListWidgetItem *now = currentItem();
        emit currentItemChanged(now, item);
        emit currentRowChanged(m_current);
        emit currentTextChanged(now ? now->text() : QString());
    } else if (m_current > row) {
        --m_current;
    }
    updateScrollBars();
    placeItemWidgets();
    viewport()->update();
    return item;
}

void QListWidget::forget(QListWidgetItem *item) { takeItem(row(item)); }

void QListWidget::itemDidChange(QListWidgetItem *item) {
    const int r = row(item);
    if (r >= 0)
        viewport()->update(visualItemRect(item));
    if (r >= 0)
        emit itemChanged(item);
}

/*!
    Removes and deletes every item.
*/
void QListWidget::clear() {
    const bool hadSelection = !selectedItems().isEmpty();
    for (QListWidgetItem *&item : m_items) {
        if (item->m_widget)
            item->m_widget->deleteLater();
        item->m_list = nullptr;
        delete item;
    }
    m_items.clear();
    const bool hadCurrent = m_current >= 0;
    m_current = m_anchor = m_pressed = -1;
    if (hadSelection)
        emit itemSelectionChanged();
    if (hadCurrent) {
        emit currentItemChanged(nullptr, nullptr);
        emit currentRowChanged(-1);
        emit currentTextChanged(QString());
    }
    updateScrollBars();
    viewport()->update();
}

/*!
    Sorts the items by their text: from the smallest to the largest if \a order is \c Qt::AscendingOrder, otherwise the
    other way. Equal items keep their order. The current item stays the current item.

    \sa insertItem()
*/
void QListWidget::sortItems(Qt::SortOrder order) {
    const QListWidgetItem *current = currentItem();
    const QListWidgetItem *anchor = item(m_anchor);
    std::stable_sort(m_items.begin(), m_items.end(), [order](const QListWidgetItem *a, const QListWidgetItem *b) {
        return order == Qt::AscendingOrder ? a->text() < b->text() : b->text() < a->text();
    });
    m_current = current ? row(current) : -1;
    m_anchor = anchor ? row(anchor) : -1;
    placeItemWidgets();
    viewport()->update();
}

/*!
    Returns the current item, or \c nullptr if there is none.

    \sa setCurrentItem(), currentRow()
*/
QListWidgetItem *QListWidget::currentItem() const { return item(m_current); }

/*!
    \fn int QListWidget::currentRow() const

    Returns the row of the current item, or -1 if there is none.

    \sa setCurrentRow()
*/

/*!
    Makes \a item the current item, which selects it if the list allows that, and scrolls it into view. Does nothing
    if \a item is not in this list, or the user cannot make it current.

    \sa currentItem()
*/
void QListWidget::setCurrentItem(QListWidgetItem *item) {
    if (item && item->m_list == this)
        setCurrentRow(row(item));
}

/*!
    Makes the item at \a row the current item, which selects it if the list allows that, and scrolls it into view.
    Does nothing if there is no such row, or the user cannot make it current.

    \sa currentRow()
*/
void QListWidget::setCurrentRow(int row) {
    if (!enabled(row))
        return;
    moveCurrent(row);
    if (selectionMode() == SingleSelection || selectionMode() == ExtendedSelection) {
        select(row, Qt::NoModifier);
    }
    scrollToRow(row);
}

/*!
    Returns the selected items, in the order of the list.
*/
QList<QListWidgetItem *> QListWidget::selectedItems() const {
    QList<QListWidgetItem *> items;
    for (QListWidgetItem *item : m_items)
        if (item->m_selected)
            items.append(item);
    return items;
}

/*!
    Selects all the items that can be selected, if the selection mode allows more than one selected item.
*/
void QListWidget::selectAll() {
    if (selectionMode() != MultiSelection && selectionMode() != ExtendedSelection)
        return;
    for (int r = 0; r < count(); ++r)
        if (selectable(r))
            setRowSelected(r, true);
    finishSelection();
}

bool QListWidget::enabled(int row) const {
    const QListWidgetItem *it = item(row);
    return it && !it->m_hidden && (it->flags() & Qt::ItemIsEnabled);
}

bool QListWidget::selectable(int row) const {
    const QListWidgetItem *it = item(row);
    return it && (it->flags() & Qt::ItemIsEnabled) && (it->flags() & Qt::ItemIsSelectable);
}

void QListWidget::moveCurrent(int row) {
    if (row == m_current)
        return;
    QListWidgetItem *previous = currentItem();
    viewport()->update(visualItemRect(previous));
    m_current = row;
    QListWidgetItem *now = currentItem();
    viewport()->update(visualItemRect(now));
    emit currentItemChanged(now, previous);
    emit currentRowChanged(m_current);
    emit currentTextChanged(now ? now->text() : QString());
}

void QListWidget::setRowSelected(int row, bool selected) {
    QListWidgetItem *it = item(row);
    if (!it || it->m_selected == selected)
        return;
    it->m_selected = selected;
    m_selectionDirty = true;
    viewport()->update(visualItemRect(it));
}

// Deselects every item but the one at `except`; returns whether any was.
bool QListWidget::clearSelection(int except) {
    bool changed = false;
    for (int r = 0; r < count(); ++r)
        if (r != except && m_items[static_cast<std::size_t>(r)]->m_selected) {
            setRowSelected(r, false);
            changed = true;
        }
    return changed;
}

void QListWidget::finishSelection() {
    if (!m_selectionDirty)
        return;
    m_selectionDirty = false;
    emit itemSelectionChanged();
}

// Selects the item at `row` as the mode and the modifiers of a click or a key say.
void QListWidget::select(int row, Qt::KeyboardModifiers modifiers) {
    if (selectionMode() == NoSelection || !selectable(row))
        return;
    const bool control = modifiers & Qt::ControlModifier, shift = modifiers & Qt::ShiftModifier;
    switch (selectionMode()) {
    case NoSelection:
        break;
    case SingleSelection:
        clearSelection(row);
        setRowSelected(row, true);
        m_anchor = row;
        break;
    case MultiSelection:
        setRowSelected(row, !item(row)->m_selected);
        m_anchor = row;
        break;
    case ContiguousSelection: // as ExtendedSelection; Control is not told apart here
    case ExtendedSelection:
        if (shift && m_anchor >= 0) {
            const int from = std::min(m_anchor, row), to = std::max(m_anchor, row);
            if (!control)
                for (int r = 0; r < count(); ++r)
                    if (r < from || r > to)
                        setRowSelected(r, false);
            for (int r = from; r <= to; ++r)
                if (selectable(r))
                    setRowSelected(r, true);
        } else if (control) {
            setRowSelected(row, !item(row)->m_selected);
            m_anchor = row;
        } else {
            clearSelection(row);
            setRowSelected(row, true);
            m_anchor = row;
        }
        break;
    }
    finishSelection();
}

void QListWidget::selectionModeChanged() {
    bool changed = false;
    if (selectionMode() == NoSelection) {
        changed = clearSelection(-1);
    } else if (selectionMode() == SingleSelection) {
        // Only one stays: the current item if it is selected, else the first.
        int keep = -1;
        if (m_current >= 0 && item(m_current)->m_selected)
            keep = m_current;
        else
            for (int r = 0; r < count() && keep < 0; ++r)
                if (m_items[static_cast<std::size_t>(r)]->m_selected)
                    keep = r;
        changed = clearSelection(keep);
    }
    (void)changed;
    finishSelection();
}

/*!
    Returns the item at \a pos, a position in the viewport, or \c nullptr if there is none.

    \sa visualItemRect()
*/
QListWidgetItem *QListWidget::itemAt(const QPoint &pos) const {
    if (pos.x() < 0 || pos.x() >= viewport()->width())
        return nullptr;
    return item(rowAt(pos));
}

/*!
    \fn QListWidgetItem *QListWidget::itemAt(int x, int y) const
    \overload

    Returns the item at (\a x, \a y) in the viewport.
*/

int QListWidget::rowAt(const QPoint &pos) const {
    layoutItems();
    const QPoint at = pos + QPoint(0, verticalScrollBar()->value());
    for (std::size_t r = 0; r < m_rects.size(); ++r) {
        const QRect &rect = m_rects[r];
        if (rect.isEmpty())
            continue;
        // In list mode a row reaches across the viewport.
        const bool inside = m_viewMode == QListView::ListMode ? at.y() >= rect.top() && at.y() <= rect.bottom()
                                                              : rect.contains(at);
        if (inside)
            return static_cast<int>(r);
    }
    return -1;
}

QSize QListWidget::iconCell() const {
    if (m_gridSize.isValid())
        return m_gridSize;
    const QSize icon = iconSize().isValid() ? iconSize() : QSize(32, 32);
    const QFontMetrics fm = fontMetrics();
    // As wide as the icon, or as the widest text up to twice that and at least 64 pixels.
    const int widest = std::max(icon.width() * 2, 64);
    int text = 0;
    for (const QListWidgetItem *it : m_items)
        text = std::max(text, std::min(widest, QFontMetrics(it->font()).horizontalAdvance(it->text())));
    const int lines = m_wordWrap ? 2 : 1;
    return QSize(std::max(icon.width(), text) + 2 * textMargin, icon.height() + 4 + lines * fm.height() + 2 * textMargin);
}

void QListWidget::invalidateLayout() { m_layoutValid = false; }

void QListWidget::layoutItems() const {
    const int width = viewport()->width();
    if (m_layoutValid && width == m_layoutWidth && m_rects.size() == m_items.size())
        return;
    m_layoutValid = true;
    m_layoutWidth = width;
    m_rects.assign(m_items.size(), QRect());
    if (m_viewMode == QListView::IconMode) {
        // A grid of cells, left to right and wrapping at the edge of the viewport.
        const QSize cell = iconCell();
        const int columns = std::max(1, (width - m_spacing) / std::max(1, cell.width() + m_spacing));
        int k = 0;
        int bottom = 0;
        for (std::size_t r = 0; r < m_items.size(); ++r) {
            if (m_items[r]->m_hidden)
                continue;
            const QSize size = m_items[r]->m_sizeHint.isValid() ? m_items[r]->m_sizeHint : cell;
            const int x = m_spacing + (k % columns) * (cell.width() + m_spacing);
            const int y = m_spacing + (k / columns) * (cell.height() + m_spacing);
            m_rects[r] = QRect(x, y, size.width(), size.height());
            bottom = std::max(bottom, y + size.height());
            ++k;
        }
        m_contentHeight = bottom + m_spacing;
        return;
    }
    // Rows one under the other, rowHeight() high or as high as the size hint of their item.
    const int base = std::max(1, rowHeight());
    int top = m_spacing;
    for (std::size_t r = 0; r < m_items.size(); ++r) {
        if (m_items[r]->m_hidden)
            continue;
        const int h = std::max(base, m_items[r]->m_sizeHint.height());
        m_rects[r] = QRect(m_spacing, top, std::max(1, width - 2 * m_spacing), h);
        top += h + m_spacing;
    }
    m_contentHeight = top;
}

QRect QListWidget::itemRect(int row) const {
    layoutItems();
    return row >= 0 && row < static_cast<int>(m_rects.size()) ? m_rects[static_cast<std::size_t>(row)] : QRect();
}

int QListWidget::contentHeight() const {
    layoutItems();
    return m_contentHeight;
}

void QListWidget::placeItemWidgets() {
    for (QListWidgetItem *item : m_items) {
        if (!item->m_widget || item->m_hidden)
            continue;
        item->m_widget->setGeometry(visualItemRect(item));
        item->m_widget->show();
    }
}

/*!
    \fn void QListWidget::removeItemWidget(QListWidgetItem *item)
    \since 6.0

    Removes and deletes the widget shown over \a item.
*/

/*!
    \since 6.0

    Returns the widget shown over \a item, or \c nullptr if it has none.

    \sa setItemWidget()
*/
QWidget *QListWidget::itemWidget(QListWidgetItem *item) const { return item ? item->m_widget.data() : nullptr; }

/*!
    \since 6.0

    Shows \a widget over \a item, in place of how the item is drawn, and takes ownership of it. A widget the item
    had before is deleted. The widget follows the item when the list scrolls and is deleted with it. Give the item a
    size hint as high as the widget so the row fits it. A \c nullptr \a widget removes the one the item has.

    \sa itemWidget(), QListWidgetItem::setSizeHint()
*/
void QListWidget::setItemWidget(QListWidgetItem *item, QWidget *widget) {
    if (!item || item->m_list != this)
        return;
    if (item->m_widget && item->m_widget != widget)
        item->m_widget->deleteLater();
    item->m_widget = widget;
    if (widget) {
        widget->setParent(viewport());
        widget->setGeometry(visualItemRect(item));
        widget->show();
    }
    viewport()->update();
}

/*!
    Returns the rectangle of \a item in the viewport, whether it is in view or not. It is empty if \a item is not in
    this list.

    \sa itemAt()
*/
QRect QListWidget::visualItemRect(const QListWidgetItem *item) const {
    const int r = row(item);
    if (r < 0)
        return QRect();
    return itemRect(r).translated(0, -verticalScrollBar()->value());
}

/*!
    Scrolls the list so that \a item is in view. Does nothing if \a item is not in this list.

    \sa setCurrentItem()
*/
void QListWidget::scrollToItem(const QListWidgetItem *item) { scrollToRow(row(item)); }

void QListWidget::scrollToRow(int row) {
    if (row < 0 || row >= count())
        return;
    // Before the list is shown it has no size to scroll in: the scrolling waits until it has.
    if (!isVisible()) {
        m_scrollRow = row;
        return;
    }
    m_scrollRow = -1;
    QScrollBar *bar = verticalScrollBar();
    const QRect rect = itemRect(row);
    if (rect.isEmpty())
        return;
    const int top = rect.top(), bottom = rect.bottom() + 1;
    if (top < bar->value())
        bar->setValue(top);
    else if (bottom > bar->value() + viewport()->height())
        bar->setValue(std::min(top, bottom - viewport()->height()));
}

// Sets the range of the scroll bar from the number of rows. A bar that comes or goes changes the viewport, so this
// goes on until the sizes agree.
void QListWidget::updateScrollBars() {
    if (m_updating) {
        m_updateAgain = true;
        return;
    }
    m_updating = true;
    QScrollBar *bar = verticalScrollBar();
    invalidateLayout();
    for (int pass = 0; pass < 4; ++pass) {
        m_updateAgain = false;
        const int h = rowHeight();
        bar->setSingleStep(h);
        bar->setPageStep(std::max(1, viewport()->height()));
        bar->setRange(0, std::max(0, contentHeight() - viewport()->height()));
        if (!m_updateAgain)
            break;
    }
    m_updating = false;
}

/*!
    \reimp
*/
void QListWidget::resizeEvent(QResizeEvent *event) {
    (void)event;
    invalidateLayout();
    updateScrollBars();
    placeItemWidgets();
    viewport()->update();
    if (m_scrollRow >= 0 && isVisible())
        scrollToRow(m_scrollRow);
}

/*!
    \reimp
*/
void QListWidget::showEvent(QShowEvent *event) {
    (void)event;
    updateScrollBars();
    if (m_scrollRow >= 0)
        scrollToRow(m_scrollRow);
}

/*!
    \reimp
*/
void QListWidget::scrollContentsBy(int dx, int dy) {
    (void)dx;
    (void)dy;
    placeItemWidgets();
    viewport()->update();
}

/*!
    \reimp
*/
void QListWidget::changeEvent(QEvent *event) {
    updateScrollBars();
    viewport()->update();
    QAbstractItemView::changeEvent(event);
}

/*!
    \reimp
*/
void QListWidget::focusInEvent(QFocusEvent *event) {
    (void)event;
    viewport()->update();
}

/*!
    \reimp
*/
void QListWidget::focusOutEvent(QFocusEvent *event) {
    (void)event;
    viewport()->update();
}

/*!
    \reimp
*/
void QListWidget::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(viewport());
    const QPalette &pal = palette();
    const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    const QRect all = viewport()->rect();
    painter.fillRect(all, pal.color(group, QPalette::Base));
    const int offset = verticalScrollBar()->value();
    const QColor highlight = pal.color(group, QPalette::Highlight);
    const bool iconMode = m_viewMode == QListView::IconMode;
    layoutItems();
    for (int r = 0; r < count(); ++r) {
        const QRect rect = m_rects[static_cast<std::size_t>(r)].translated(0, -offset);
        if (rect.isEmpty() || !rect.intersects(all))
            continue;
        const QListWidgetItem *it = m_items[static_cast<std::size_t>(r)];
        const bool selected = it->isSelected();
        const bool on = isEnabled() && (it->flags() & Qt::ItemIsEnabled);
        const auto *material = iconMode ? nullptr : qobject_cast<const QMaterialStyle *>(style());
        if (material)
            material->drawListRow(&painter, rect, selected, on,
                                  it->background().style() != Qt::NoBrush ? QBrush(it->background().color()) : QBrush(),
                                  this);
        else if (selected)
            painter.fillRect(rect, highlight);
        else if (it->background().style() != Qt::NoBrush)
            painter.fillRect(rect, it->background().color());
        else if (alternatingRowColors() && (r & 1) && !iconMode)
            painter.fillRect(rect, pal.color(group, QPalette::AlternateBase));
        const QIcon::Mode iconState = !on ? QIcon::Disabled : selected ? QIcon::Selected : QIcon::Normal;
        QColor color = pal.color(on ? QPalette::Active : QPalette::Disabled, QPalette::Text);
        if (selected)
            color = pal.color(group, QPalette::HighlightedText);
        else if (it->foreground().style() != Qt::NoBrush && on)
            color = it->foreground().color();
        const QFont font = it->font();
        if (iconMode) {
            // The icon at the top of the cell, and the text centred under it.
            const QSize size = iconSize().isValid() ? iconSize() : QSize(32, 32);
            const QRect iconRect(rect.left() + (rect.width() - size.width()) / 2, rect.top() + textMargin, size.width(),
                                 size.height());
            if (!it->icon().isNull())
                it->icon().paint(&painter, iconRect, Qt::AlignCenter, iconState);
            const QRect textRect(rect.left() + textMargin, iconRect.bottom() + 4, rect.width() - 2 * textMargin,
                                 rect.bottom() - iconRect.bottom() - 4);
            painter.setFont(font);
            painter.setPen(color);
            painter.setClipRect(rect);
            const QString text = m_wordWrap ? it->text()
                                            : QFontMetrics(font).elidedText(it->text(), Qt::ElideRight, textRect.width());
            painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignTop | (m_wordWrap ? Qt::TextWordWrap : 0), text);
            painter.setClipping(false);
        } else {
            const int margin = !material                            ? textMargin
                               : QMaterialStyle::isDrawerList(this) ? materialDrawerMargin
                                                                    : materialMargin;
            const int gap = material ? materialSpacing : textMargin;
            int iconWidth = 0;
            if (!it->icon().isNull()) {
                const QSize size = iconSize().isValid() ? iconSize() : QSize(16, 16);
                const QRect room(rect.left() + margin, rect.top(), size.width(), rect.height());
                if (material)
                    material->drawRowIcon(
                        &painter, QRect(QPoint(room.left(), room.top() + (room.height() - size.height()) / 2), size),
                        it->icon(), iconState, selected, on);
                else
                    it->icon().paint(&painter, room, Qt::AlignCenter, iconState);
                iconWidth = size.width() + gap;
            }
            if (!it->text().isEmpty()) {
                const QRect textRect = rect.adjusted(margin + iconWidth, 0, -margin, 0);
                painter.setFont(font);
                painter.setPen(color);
                painter.setClipRect(rect);
                if (m_wordWrap)
                    painter.drawText(textRect, static_cast<int>(it->textAlignment()) | Qt::TextWordWrap, it->text());
                else
                    painter.drawText(textRect, static_cast<int>(it->textAlignment()) | Qt::TextSingleLine,
                                     QFontMetrics(font).elidedText(it->text(), Qt::ElideRight, textRect.width()));
                painter.setClipping(false);
            }
        }
        if (r == m_current && hasFocus() && material) {
            // The style says when a ring shows: with the keyboard, not after a tap.
            QStyleOption focus;
            focus.initFrom(this);
            focus.rect = rect;
            style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &painter, this);
        } else if (r == m_current && hasFocus()) {
            painter.setPen(selected ? highlight.lighter(150) : highlight);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(rect.adjusted(0, 0, -1, -1));
        }
    }
}

/*!
    \reimp
*/
bool QListWidget::viewportEvent(QEvent *event) {
    if (event->type() == QEvent::MouseMove && (viewport()->hasMouseTracking() || hasMouseTracking())) {
        const int r = rowAt(static_cast<QMouseEvent *>(event)->pos());
        if (r != m_hover) {
            m_hover = r;
            if (r >= 0)
                emit itemEntered(item(r));
            else
                emit viewportEntered();
        }
    } else if (event->type() == QEvent::Leave) {
        m_hover = -1;
    }
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        const QListWidgetItem *it = itemAt(help->pos());
        if (it && !it->toolTip().isEmpty()) {
            QToolTip::showText(help->globalPos(), it->toolTip(), viewport(), visualItemRect(it));
            event->accept();
            return true;
        }
    }
    return QAbstractItemView::viewportEvent(event);
}

/*!
    \reimp
*/
void QListWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    const int r = rowAt(event->pos());
    m_pressed = r;
    if (r < 0) {
        if (selectionMode() == ExtendedSelection && !(event->modifiers() & Qt::ControlModifier)) {
            clearSelection(-1);
            finishSelection();
        }
    } else if (enabled(r)) {
        moveCurrent(r);
        select(r, event->modifiers());
        scrollToRow(r);
        emit itemPressed(item(r));
    }
    event->accept();
}

/*!
    \reimp
*/
void QListWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    const int r = rowAt(event->pos());
    const bool clicked = r >= 0 && r == m_pressed && enabled(r);
    m_pressed = -1;
    if (clicked)
        emit itemClicked(item(r));
    event->accept();
}

/*!
    \reimp
*/
void QListWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    const int r = rowAt(event->pos());
    if (enabled(r)) {
        moveCurrent(r);
        emit itemDoubleClicked(item(r));
        emit itemActivated(item(r));
        if ((editTriggers() & DoubleClicked) && (item(r)->flags() & Qt::ItemIsEditable))
            editItem(item(r));
    }
    event->accept();
}

// Moves the current item to `target`, and selects it as the mode and the modifiers say.
void QListWidget::navigate(int target, Qt::KeyboardModifiers modifiers) {
    if (!enabled(target))
        return;
    if (m_anchor < 0)
        m_anchor = m_current;
    moveCurrent(target);
    scrollToRow(target);
    const bool control = modifiers & Qt::ControlModifier;
    switch (selectionMode()) {
    case NoSelection:
    case MultiSelection:
        break;
    case SingleSelection:
        select(target, Qt::NoModifier);
        break;
    case ContiguousSelection:
    case ExtendedSelection:
        if (!control || (modifiers & Qt::ShiftModifier))
            select(target, modifiers & Qt::ShiftModifier ? Qt::ShiftModifier : Qt::NoModifier);
        break;
    }
}

// Goes to the next item, after the current one and wrapping around, whose text starts with the letter in `text`.
void QListWidget::search(const QString &text) {
    if (text.isEmpty() || count() == 0)
        return;
    const QString letter = text.left(1).toLower();
    for (int i = 1; i <= count(); ++i) {
        const int r = (std::max(m_current, -1) + i) % count();
        if (enabled(r) && item(r)->text().toLower().startsWith(letter)) {
            navigate(r, Qt::NoModifier);
            return;
        }
    }
}

/*!
    \reimp

    The arrow keys, Page Up, Page Down, Home and End move the current item, skipping items that are not enabled;
    Space selects it, or toggles it in a list with \c MultiSelection or with Control in an \c ExtendedSelection list;
    Control+A selects all; Return and Enter activate it, and a letter goes to the next item that starts with it.
*/
void QListWidget::keyPressEvent(QKeyEvent *event) {
    if (count() == 0) {
        event->ignore();
        return;
    }
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    const int pageRows = std::max(1, viewport()->height() / std::max(1, rowHeight()) - 1);
    auto step = [this](int from, int direction) {
        for (int r = from + direction; r >= 0 && r < count(); r += direction)
            if (enabled(r))
                return r;
        return -1;
    };
    // In icon mode Left and Right go to the item before and after, and Up and Down to the one above and below.
    const bool iconMode = m_viewMode == QListView::IconMode;
    const auto vertical = [&](int direction) {
        if (m_current < 0)
            return step(direction < 0 ? count() : -1, direction);
        const QRect from = itemRect(m_current);
        int best = -1, bestDistance = 0;
        for (int r = 0; r < count(); ++r) {
            const QRect rect = itemRect(r);
            if (!enabled(r) || rect.isEmpty() || (direction < 0 ? rect.top() >= from.top() : rect.top() <= from.top()))
                continue;
            const int distance = std::abs(rect.top() - from.top()) * 4 + std::abs(rect.center().x() - from.center().x());
            if (best < 0 || distance < bestDistance) {
                best = r;
                bestDistance = distance;
            }
        }
        return best;
    };
    int target = -1;
    switch (event->key()) {
    case Qt::Key_Up:
        target = iconMode ? vertical(-1) : step(m_current < 0 ? count() : m_current, -1);
        break;
    case Qt::Key_Down:
        target = iconMode ? vertical(1) : step(m_current < 0 ? -1 : m_current, 1);
        break;
    case Qt::Key_Left:
        if (!iconMode) {
            event->ignore();
            return;
        }
        target = step(m_current < 0 ? count() : m_current, -1);
        break;
    case Qt::Key_Right:
        if (!iconMode) {
            event->ignore();
            return;
        }
        target = step(m_current < 0 ? -1 : m_current, 1);
        break;
    case Qt::Key_PageUp:
        target = m_current < 0 ? step(count(), -1) : std::max(0, m_current - pageRows);
        break;
    case Qt::Key_PageDown:
        target = m_current < 0 ? step(-1, 1) : std::min(count() - 1, m_current + pageRows);
        break;
    case Qt::Key_Home:
        target = step(-1, 1);
        break;
    case Qt::Key_End:
        target = step(count(), -1);
        break;
    case Qt::Key_Space:
        if (m_current >= 0) {
            if (selectionMode() == MultiSelection ||
                (selectionMode() == ExtendedSelection && (modifiers & Qt::ControlModifier)))
                select(m_current, Qt::ControlModifier);
            else
                select(m_current, Qt::NoModifier);
        }
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_current >= 0)
            emit itemActivated(currentItem());
        event->accept();
        return;
    case Qt::Key_A:
        if (modifiers & Qt::ControlModifier) {
            selectAll();
            event->accept();
            return;
        }
        [[fallthrough]];
    default:
        if (!(modifiers & (Qt::ControlModifier | Qt::AltModifier)) && !event->text().isEmpty() &&
            event->text().at(0).isPrint()) {
            search(event->text());
            event->accept();
            return;
        }
        event->ignore();
        return;
    }
    event->accept();
    if (target >= 0)
        navigate(target, modifiers);
}

/*!
    \fn QListView::ViewMode QListWidget::viewMode() const
    \since 6.0

    Returns how the items are shown: QListView::ListMode, the default, or QListView::IconMode.
*/

/*!
    \since 6.0

    Shows the items as \a mode says: in rows, or in QListView::IconMode as a grid of cells with the icon above the
    text, wrapping at the edge of the viewport.

    \code
    gallery->setViewMode(QListView::IconMode);
    gallery->setIconSize(QSize(64, 64));
    gallery->setGridSize(QSize(96, 96));
    \endcode
*/
void QListWidget::setViewMode(QListView::ViewMode mode) {
    if (mode == m_viewMode)
        return;
    m_viewMode = mode;
    updateScrollBars();
    placeItemWidgets();
    viewport()->update();
}

/*!
    \fn QListView::Movement QListWidget::movement() const
    \since 6.0

    Returns whether the user may move the items. Nt keeps the value; items are not moved.
*/

/*!
    \fn void QListWidget::setMovement(QListView::Movement movement)
    \since 6.0

    Sets whether the user may move the items to \a movement.
*/

/*!
    \fn QListView::ResizeMode QListWidget::resizeMode() const
    \since 6.0

    Returns whether the items are laid out again when the list is resized. Nt always lays them out again.
*/

/*!
    \fn void QListWidget::setResizeMode(QListView::ResizeMode mode)
    \since 6.0

    Sets whether the items are laid out again when the list is resized to \a mode.
*/

/*!
    \fn QListView::Flow QListWidget::flow() const
    \since 6.0

    Returns the direction of the items. Nt lays rows out top to bottom and cells left to right.
*/

/*!
    \since 6.0

    Sets the direction of the items to \a flow; it is kept.
*/
void QListWidget::setFlow(QListView::Flow flow) { m_flow = flow; }

/*!
    \fn bool QListWidget::isWrapping() const
    \since 6.0

    Returns whether the items wrap at the edge of the viewport; icon mode always wraps.
*/

/*!
    \since 6.0

    Sets whether the items wrap at the edge of the viewport to \a enable; it is kept.
*/
void QListWidget::setWrapping(bool enable) { m_wrapping = enable; }

/*!
    \fn int QListWidget::spacing() const
    \since 6.0

    Returns the room around each item, 0 by default.
*/

/*!
    \since 6.0

    Sets the room around each item to \a space pixels.
*/
void QListWidget::setSpacing(int space) {
    m_spacing = std::max(0, space);
    updateScrollBars();
    placeItemWidgets();
    viewport()->update();
}

/*!
    \fn QSize QListWidget::gridSize() const
    \since 6.0

    Returns the size of a cell in icon mode, or an invalid size for one that fits the items.
*/

/*!
    \since 6.0

    Sets the size of a cell in icon mode to \a size.
*/
void QListWidget::setGridSize(const QSize &size) {
    m_gridSize = size;
    updateScrollBars();
    placeItemWidgets();
    viewport()->update();
}

/*!
    \fn bool QListWidget::uniformItemSizes() const
    \since 6.0

    Returns whether all items are assumed to be as large. Nt keeps the value.
*/

/*!
    \fn void QListWidget::setUniformItemSizes(bool enable)
    \since 6.0

    Sets whether all items are assumed to be as large to \a enable.
*/

/*!
    \fn bool QListWidget::wordWrap() const
    \since 6.0

    Returns whether the texts of the items wrap.
*/

/*!
    \since 6.0

    Sets whether the texts of the items wrap to \a on. A wrapped text needs a size hint, or a grid size in icon mode,
    high enough for its lines.
*/
void QListWidget::setWordWrap(bool on) {
    m_wordWrap = on;
    updateScrollBars();
    viewport()->update();
}

/*!
    \since 6.0

    Returns \c true if the item at \a row is hidden.
*/
bool QListWidget::isRowHidden(int row) const {
    const QListWidgetItem *it = item(row);
    return it && it->isHidden();
}

/*!
    \since 6.0

    Hides the item at \a row if \a hide is \c true, or shows it.
*/
void QListWidget::setRowHidden(int row, bool hide) {
    if (QListWidgetItem *it = item(row))
        it->setHidden(hide);
}

/*!
    \since 6.0

    Deselects all items.
*/
void QListWidget::clearSelection() {
    if (clearSelection(-1))
        finishSelection();
}

/*!
    \since 6.0

    Returns the MIME types that mimeData() gives: \c application/x-qabstractitemmodeldatalist by default.
*/
QStringList QListWidget::mimeTypes() const { return {QStringLiteral("application/x-qabstractitemmodeldatalist")}; }

/*!
    \since 6.0

    Returns new data that stands for \a items, for a drag from the list: by default their texts, one per line, as
    plain text. Reimplement it to drag data of your own; the caller owns the data.
*/
QMimeData *QListWidget::mimeData(const QList<QListWidgetItem *> &items) const {
    auto *data = new QMimeData;
    QStringList texts;
    for (const QListWidgetItem *it : items)
        if (it)
            texts.append(it->text());
    data->setText(texts.join(QChar('\n')));
    return data;
}

/*!
    \since 6.0

    Handles \a data dropped with \a action before the item at \a index, and returns \c true if it was taken. The
    default takes nothing; reimplement it to accept drops.
*/
bool QListWidget::dropMimeData(int index, const QMimeData *data, Qt::DropAction action) {
    (void)index;
    (void)data;
    (void)action;
    return false;
}

/*!
    \since 6.0

    Returns the actions that drops on the list may take: Qt::CopyAction by default.
*/
Qt::DropActions QListWidget::supportedDropActions() const { return Qt::CopyAction; }

namespace {
// Closes the editor of an item without storing its text when Escape is pressed in it.
class ListEscapeFilter : public QObject {
public:
    ListEscapeFilter(std::function<void()> cancel, QObject *parent) : QObject(parent), m_cancel(std::move(cancel)) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            m_cancel();
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void()> m_cancel;
};
} // namespace

/*!
    \since 6.0

    Turns sorting on if \a enable is \c true: the items are sorted now, in ascending order, and items that are added
    later take their place in the order.

    \sa sortItems()
*/
void QListWidget::setSortingEnabled(bool enable) {
    m_sorting = enable;
    if (enable)
        sortItems(Qt::AscendingOrder);
}

/*!
    \fn bool QListWidget::isSortingEnabled() const
    \since 6.0

    Returns whether the items are kept sorted. The default is \c false.
*/

/*!
    \since 6.0

    Returns the items whose text matches \a text the way \a flags says, in the order of the list.
*/
QList<QListWidgetItem *> QListWidget::findItems(const QString &text, Qt::MatchFlags flags) const {
    const QtPrivate::ItemTextMatcher matches(text, flags);
    QList<QListWidgetItem *> found;
    for (QListWidgetItem *it : m_items)
        if (matches(it->text()))
            found.append(it);
    return found;
}

/*!
    \since 6.0

    Opens a line edit over \a item, if it has Qt::ItemIsEditable, with its text. Return or leaving the editor stores
    the text and emits itemChanged(); Escape closes it without storing.
*/
void QListWidget::editItem(QListWidgetItem *item) {
    if (!item || item->m_list != this || !(item->flags() & Qt::ItemIsEditable) ||
        !(item->flags() & Qt::ItemIsEnabled))
        return;
    closeItemEditor(true);
    scrollToItem(item);
    auto *editor = new QLineEdit(viewport());
    editor->setText(item->text());
    editor->setFrame(false);
    editor->setGeometry(visualItemRect(item));
    editor->installEventFilter(new ListEscapeFilter([this] { closeItemEditor(false); }, editor));
    QObject::connect(editor, &QLineEdit::editingFinished, this, [this] { closeItemEditor(true); });
    m_editor = editor;
    m_editing = item;
    editor->show();
    editor->selectAll();
    editor->setFocus();
}

void QListWidget::closeItemEditor(bool commit) {
    QPointer<QWidget> editor = m_editor;
    QListWidgetItem *item = m_editing;
    m_editor = nullptr;
    m_editing = nullptr;
    if (!editor)
        return;
    if (commit && item && item->m_list == this)
        if (auto *line = qobject_cast<QLineEdit *>(editor.data()))
            item->setText(line->text());
    editor->hide();
    editor->deleteLater();
    if (hasFocus() || !QApplication::focusWidget() || QApplication::focusWidget() == editor)
        setFocus();
}

/*!
    \since 6.0

    Returns an invalid index: the list keeps its items without a model, so \a item has no model index.
*/
QModelIndex QListWidget::indexFromItem(const QListWidgetItem *item) const {
    (void)item;
    return QModelIndex();
}

/*!
    \since 6.0

    Returns the item at the row of \a index, or \c nullptr. The list has no model, so only the row of \a index is
    looked at.
*/
QListWidgetItem *QListWidget::itemFromIndex(const QModelIndex &index) const {
    return index.isValid() ? item(index.row()) : nullptr;
}

/*!
    \fn Qt::DropActions QListWidget::supportedDragActions() const
    \since 6.0

    Returns the actions a drag from the list allows.
*/

/*!
    \fn void QListWidget::setSupportedDragActions(Qt::DropActions actions)
    \since 6.0

    Sets the actions a drag from the list allows to \a actions. Nt keeps the value.
*/

/*!
    \since 6.0

    Returns the items of the list that \a data was made from; empty, as the items are not kept in MIME data.
*/
QList<QListWidgetItem *> QListWidget::items(const QMimeData *data) const {
    (void)data;
    return {};
}

/*!
    \fn QString QListWidgetItem::statusTip() const
    \since 6.0

    Returns the status tip of the item.
*/

/*!
    \fn void QListWidgetItem::setStatusTip(const QString &statusTip)
    \since 6.0

    Sets the status tip of the item to \a statusTip.
*/

/*!
    \fn QString QListWidgetItem::whatsThis() const
    \since 6.0

    Returns the "What's This?" help of the item.
*/

/*!
    \fn void QListWidgetItem::setWhatsThis(const QString &whatsThis)
    \since 6.0

    Sets the "What's This?" help of the item to \a whatsThis.
*/

/*!
    \fn Qt::CheckState QListWidgetItem::checkState() const
    \since 6.0

    Returns the check state of the item.
*/

/*!
    \fn void QListWidgetItem::setCheckState(Qt::CheckState state)
    \since 6.0

    Sets the check state of the item to \a state. The list keeps it; it draws no check box.
*/

/*!
    \since 6.0

    Returns a new item, without a list, with the same data and flags as this one.
*/
QListWidgetItem *QListWidgetItem::clone() const {
    auto *copy = new QListWidgetItem(m_icon, m_text, nullptr, m_type);
    copy->m_toolTip = m_toolTip;
    copy->m_alignment = m_alignment;
    copy->m_foreground = m_foreground;
    copy->m_background = m_background;
    copy->m_font = m_font;
    copy->m_hasFont = m_hasFont;
    copy->m_flags = m_flags;
    copy->m_sizeHint = m_sizeHint;
    copy->m_hidden = m_hidden;
    copy->m_data = m_data;
    return copy;
}

/*!
    \since 6.0

    Reads the data of the item from \a in, as write() wrote it.
*/
void QListWidgetItem::read(QDataStream &in) {
    quint32 count = 0;
    in >> count;
    for (quint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i) {
        qint32 role = 0;
        QString value;
        in >> role >> value;
        setData(role, value);
    }
}

/*!
    \since 6.0

    Writes the data of the item to \a out: the text, the tool tip and the roles whose value is a string. Other
    values, such as icons, are not written.
*/
void QListWidgetItem::write(QDataStream &out) const {
    QList<std::pair<int, QString>> values;
    if (!m_text.isEmpty())
        values.append({Qt::DisplayRole, m_text});
    if (!m_toolTip.isEmpty())
        values.append({Qt::ToolTipRole, m_toolTip});
    for (auto it = m_data.constBegin(); it != m_data.constEnd(); ++it)
        if (it.value().typeId() == QMetaType::QString)
            values.append({it.key(), it.value().toString()});
    out << static_cast<quint32>(values.size());
    for (const auto &[role, value] : values)
        out << static_cast<qint32>(role) << value;
}

#include "moc_qlistwidget.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
