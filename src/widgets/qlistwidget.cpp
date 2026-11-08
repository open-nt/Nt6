// MIT License - see LICENSE
#include <QtWidgets/qlistwidget.h>

#include <QtCore/qdebug.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qtooltip.h>

#include <algorithm>

namespace {
constexpr int textMargin = 3;
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
    \c setSizeHint(), no \c setHidden(), and no \c clone(). An item cannot be edited, so there is no \c ItemIsEditable
    flag. An item cannot be copied.
    \enddifferences

    \sa QListWidget
*/

/*!
    Constructs an item with no text.
*/
QListWidgetItem::QListWidgetItem() = default;

/*!
    Constructs an item with the given \a text.
*/
QListWidgetItem::QListWidgetItem(const QString &text) : m_text(text) {}

/*!
    Constructs an item with the given \a text and adds it at the end of \a listWidget.
*/
QListWidgetItem::QListWidgetItem(const QString &text, QListWidget *listWidget) : m_text(text) {
    if (listWidget)
        listWidget->addItem(this);
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
        m_list->itemChanged(this);
}

/*!
    \fn QString QListWidgetItem::text() const

    Returns the text of the item.

    \sa setText()
*/

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
    There is no model and no \c QListView: the list keeps its items itself. There is no editing, no drag and drop, no
    icons and no icon or grid view mode, no \c setItemWidget(), no sorting while items are added (\c setSortingEnabled()),
    and no \c findItems(). Dragging does not select a range, and typing a letter looks at one letter at a time. The rows
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
    Returns the height of every row: that of a line of text in the font of the list.
*/
int QListWidget::rowHeight() const { return fontMetrics().height(); }

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
    viewport()->update();
    if (m_current < 0 && count() == 1)
        moveCurrent(0);
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
    viewport()->update();
    return item;
}

void QListWidget::forget(QListWidgetItem *item) { takeItem(row(item)); }

void QListWidget::itemChanged(QListWidgetItem *item) {
    const int r = row(item);
    if (r >= 0)
        viewport()->update(visualItemRect(item));
}

/*!
    Removes and deletes every item.
*/
void QListWidget::clear() {
    const bool hadSelection = !selectedItems().isEmpty();
    for (QListWidgetItem *&item : m_items) {
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
    return it && (it->flags() & Qt::ItemIsEnabled);
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
    return item(rowAtY(pos.y()));
}

/*!
    \fn QListWidgetItem *QListWidget::itemAt(int x, int y) const
    \overload

    Returns the item at (\a x, \a y) in the viewport.
*/

int QListWidget::rowAtY(int y) const {
    const int h = std::max(1, rowHeight());
    const int row = (y + verticalScrollBar()->value()) / h;
    return y + verticalScrollBar()->value() >= 0 && row < count() ? row : -1;
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
    const int h = rowHeight();
    return QRect(0, r * h - verticalScrollBar()->value(), viewport()->width(), h);
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
    const int h = rowHeight();
    const int top = row * h, bottom = top + h;
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
    for (int pass = 0; pass < 4; ++pass) {
        m_updateAgain = false;
        const int h = rowHeight();
        bar->setSingleStep(h);
        bar->setPageStep(std::max(1, viewport()->height()));
        bar->setRange(0, std::max(0, count() * h - viewport()->height()));
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
    updateScrollBars();
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
    const int h = std::max(1, rowHeight());
    const int offset = verticalScrollBar()->value();
    const QColor highlight = pal.color(group, QPalette::Highlight);
    for (int r = std::max(0, offset / h); r < count(); ++r) {
        const QRect rect(0, r * h - offset, all.width(), h);
        if (rect.top() > all.bottom())
            break;
        const QListWidgetItem *it = m_items[static_cast<std::size_t>(r)];
        const bool selected = it->isSelected();
        if (selected)
            painter.fillRect(rect, highlight);
        else if (it->background().style() != Qt::NoBrush)
            painter.fillRect(rect, it->background().color());
        else if (alternatingRowColors() && (r & 1))
            painter.fillRect(rect, pal.color(group, QPalette::AlternateBase));
        if (!it->text().isEmpty()) {
            const bool on = isEnabled() && (it->flags() & Qt::ItemIsEnabled);
            QColor color = pal.color(on ? QPalette::Active : QPalette::Disabled, QPalette::Text);
            if (selected)
                color = pal.color(group, QPalette::HighlightedText);
            else if (it->foreground().style() != Qt::NoBrush && on)
                color = it->foreground().color();
            const QFont font = it->font();
            const QRect textRect = rect.adjusted(textMargin, 0, -textMargin, 0);
            painter.setFont(font);
            painter.setPen(color);
            painter.setClipRect(rect);
            painter.drawText(textRect, static_cast<int>(it->textAlignment()) | Qt::TextSingleLine,
                             QFontMetrics(font).elidedText(it->text(), Qt::ElideRight, textRect.width()));
            painter.setClipping(false);
        }
        if (r == m_current && hasFocus()) {
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
    const int r = rowAtY(event->pos().y());
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
    const int r = rowAtY(event->pos().y());
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
    const int r = rowAtY(event->pos().y());
    if (enabled(r)) {
        moveCurrent(r);
        emit itemDoubleClicked(item(r));
        emit itemActivated(item(r));
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
    int target = -1;
    switch (event->key()) {
    case Qt::Key_Up:
        target = step(m_current < 0 ? count() : m_current, -1);
        break;
    case Qt::Key_Down:
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
            if (selectionMode() == MultiSelection || (selectionMode() == ExtendedSelection && (modifiers & Qt::ControlModifier)))
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

#include "moc_qlistwidget.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
