// MIT License - see LICENSE
#include <QtWidgets/qlistview.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qtooltip.h>

#include <algorithm>
#include <climits>
#include <cstdlib>

/*!
    \class QListView
    \brief The QListView class provides a list or icon view onto a model.

    A QListView shows one column of a model as a list: the items one under the other, as in a QListWidget, or,
    in the icon mode, as large icons with their text below, side by side in rows that wrap.

    \code
    auto *model = new QStringListModel({"Hazel", "Walnut", "Pecan", "Cashew"}, this);
    auto *view = new QListView;
    view->setModel(model);
    connect(view, &QListView::activated, this, &Window::open);
    \endcode

    For the icon mode, call setViewMode(QListView::IconMode). It lays the items out from left to right, wraps
    them into rows that fit the width of the view, and draws the decoration of each item (Qt::DecorationRole)
    above its text, in iconSize(), 32 by 32 pixels unless set. setGridSize() puts every item in a cell of the
    same size, which keeps the rows straight:

    \code
    view->setViewMode(QListView::IconMode);
    view->setIconSize(QSize(48, 48));
    view->setGridSize(QSize(96, 80));
    \endcode

    The layout is controlled in detail by flow() (down or across), isWrapping() (into columns or rows),
    spacing() (around each item) and gridSize(). modelColumn() is the column of the model that is shown, and
    rows can be hidden with setRowHidden().

    The arrow keys move the current item to the neighbour in that direction, Home and End to the first and last
    item, and the page keys a page at a time. Items are selected as in the other views, with the selection
    mode and the Shift and Control keys; in the extended and contiguous modes dragging the mouse over the items
    selects them. Items are drawn and edited by the delegate; see QAbstractItemView and QStyledItemDelegate.

    \differences
    Items cannot be dragged: movement() is kept but items stay where the layout puts them, so there is no
    \c indexesMoved() signal and no rubber band for selectionRectVisible(). The view always lays the items out
    again when it is resized, as with the \c Adjust resize mode, and all at once, whatever layoutMode() and
    batchSize() say. itemAlignment() is kept but not used. In a grid every item takes the whole cell, and
    spacing() is ignored. wordWrap() asks the delegate to wrap the text, but QStyledItemDelegate draws one line.
    \enddifferences

    \sa QListWidget, QTreeView, QTableView, QAbstractItemView, QStringListModel
*/

/*!
    \enum QListView::Movement

    This enum says how the user may move the items. Nt keeps the value, but items cannot be moved.

    \value Static The items cannot be moved.
    \value Free The items can be moved anywhere.
    \value Snap The items snap to the grid when they are moved.
*/

/*!
    \enum QListView::Flow

    This enum says in which direction the items are laid out.

    \value LeftToRight Side by side, from left to right; with wrapping, in rows.
    \value TopToBottom One under the other, from top to bottom; with wrapping, in columns.
*/

/*!
    \enum QListView::ResizeMode

    This enum says whether the items are laid out again when the view is resized. Nt always lays them out again.

    \value Fixed Only when the items change.
    \value Adjust Also when the view is resized.
*/

/*!
    \enum QListView::LayoutMode

    This enum says whether the items are laid out all at once or in batches. Nt lays them out all at once.

    \value SinglePass All at once.
    \value Batched In batches of batchSize() items, from the event loop.
*/

/*!
    \enum QListView::ViewMode

    This enum says how the items are shown.

    \value ListMode As a list: small decorations left of the text, one item under the other.
    \value IconMode As icons: large decorations above the text, side by side in rows that wrap.
*/

namespace {
constexpr QSize kIconModeSize(32, 32);
} // namespace

/*!
    Constructs a list view with the given \a parent, in the list mode.
*/
QListView::QListView(QWidget *parent) : QAbstractItemView(parent) { setFocusPolicy(Qt::StrongFocus); }

/*!
    Destroys the view.
*/
QListView::~QListView() {
    for (const QMetaObject::Connection &c : m_connections)
        QObject::disconnect(c);
}

/*!
    \reimp

    Makes the view show \a model. No row is hidden.
*/
void QListView::setModel(QAbstractItemModel *model) {
    if (model == this->model())
        return;
    for (const QMetaObject::Connection &c : m_connections)
        QObject::disconnect(c);
    m_connections.clear();
    m_items.clear();
    m_hidden.clear();
    m_anchor = QPersistentModelIndex();
    QAbstractItemView::setModel(model);
    if (model) {
        const auto again = [this] { updateGeometries(); };
        m_connections = {
            connect(model, &QAbstractItemModel::rowsRemoved, this, [again](const QModelIndex &, int, int) { again(); }),
            connect(model, &QAbstractItemModel::rowsMoved, this,
                    [again](const QModelIndex &, int, int, const QModelIndex &, int) { again(); }),
            connect(model, &QAbstractItemModel::layoutChanged, this,
                    [again](const QList<QPersistentModelIndex> &, QAbstractItemModel::LayoutChangeHint) { again(); }),
            connect(model, &QAbstractItemModel::columnsInserted, this,
                    [again](const QModelIndex &, int, int) { again(); }),
            connect(model, &QAbstractItemModel::columnsRemoved, this,
                    [again](const QModelIndex &, int, int) { again(); }),
        };
    }
    updateGeometries();
}

/*!
    \reimp

    Shows the children of \a index as the items of the view.
*/
void QListView::setRootIndex(const QModelIndex &index) {
    m_hidden.clear();
    QAbstractItemView::setRootIndex(index);
    updateGeometries();
}

/*!
    \fn QListView::Movement QListView::movement() const

    Returns how the user may move the items. The default is \c Static, and \c Free in the icon mode.

    \sa setMovement()
*/

/*!
    Keeps \a movement for movement(). Items cannot be moved in Nt.
*/
void QListView::setMovement(Movement movement) { m_movement = movement; }

/*!
    \fn QListView::Flow QListView::flow() const

    Returns in which direction the items are laid out. The default is \c TopToBottom, and \c LeftToRight in the
    icon mode.

    \sa setFlow(), isWrapping()
*/

/*!
    Lays the items out in the direction \a flow.

    \sa flow()
*/
void QListView::setFlow(Flow flow) {
    if (m_flow == flow)
        return;
    m_flow = flow;
    updateGeometries();
}

/*!
    \fn bool QListView::isWrapping() const

    Returns whether the items wrap into more rows, or columns, when there is no room for them in one. The
    default is \c false, and \c true in the icon mode.

    \sa setWrapping(), flow()
*/

/*!
    Sets whether the items wrap into more rows or columns to \a enable.

    \sa isWrapping()
*/
void QListView::setWrapping(bool enable) {
    if (m_wrapping == enable)
        return;
    m_wrapping = enable;
    updateGeometries();
}

/*!
    \fn QListView::ResizeMode QListView::resizeMode() const

    Returns the resize mode that setResizeMode() set. The default is \c Fixed.
*/

/*!
    Keeps \a mode for resizeMode(). Nt lays the items out again whenever the view is resized.
*/
void QListView::setResizeMode(ResizeMode mode) { m_resizeMode = mode; }

/*!
    \fn QListView::LayoutMode QListView::layoutMode() const

    Returns the layout mode that setLayoutMode() set. The default is \c SinglePass.
*/

/*!
    Keeps \a mode for layoutMode(). Nt lays all items out at once.
*/
void QListView::setLayoutMode(LayoutMode mode) { m_layoutMode = mode; }

/*!
    \fn int QListView::spacing() const

    Returns the room, in pixels, that is left around each item. The default is 0.

    \sa setSpacing()
*/

/*!
    Leaves \a space pixels around each item. It is not used while there is a gridSize().

    \sa spacing()
*/
void QListView::setSpacing(int space) {
    space = std::max(0, space);
    if (m_spacing == space)
        return;
    m_spacing = space;
    updateGeometries();
}

/*!
    \fn int QListView::batchSize() const

    Returns the batch size that setBatchSize() set. The default is 100.
*/

/*!
    Keeps \a batchSize for batchSize(). Nt lays all items out at once.
*/
void QListView::setBatchSize(int batchSize) { m_batchSize = std::max(1, batchSize); }

/*!
    \fn QSize QListView::gridSize() const

    Returns the size of the cells that the items are laid out in, or an invalid size if each item has its own
    size, which is the default.

    \sa setGridSize()
*/

/*!
    Puts every item in a cell of \a size. An invalid \a size gives each item the size that its delegate wants.

    \sa gridSize()
*/
void QListView::setGridSize(const QSize &size) {
    if (m_gridSize == size)
        return;
    m_gridSize = size;
    updateGeometries();
}

/*!
    \fn QListView::ViewMode QListView::viewMode() const

    Returns how the items are shown. The default is \c ListMode.

    \sa setViewMode()
*/

/*!
    Shows the items in the way \a mode says. Like Qt, this sets the other properties to what suits the mode:
    \c IconMode lays the items out from left to right, wrapping, with \c Free movement; \c ListMode lays them
    out from top to bottom, without wrapping, with \c Static movement.

    \sa viewMode(), setIconSize()
*/
void QListView::setViewMode(ViewMode mode) {
    m_viewMode = mode;
    if (mode == IconMode) {
        m_movement = Free;
        m_flow = LeftToRight;
        m_wrapping = true;
    } else {
        m_movement = Static;
        m_flow = TopToBottom;
        m_wrapping = false;
    }
    updateGeometries();
}

/*!
    \fn int QListView::modelColumn() const

    Returns the column of the model whose items the view shows. The default is 0.

    \sa setModelColumn()
*/

/*!
    Shows the items of the column \a column of the model.

    \sa modelColumn()
*/
void QListView::setModelColumn(int column) {
    column = std::max(0, column);
    if (m_modelColumn == column)
        return;
    m_modelColumn = column;
    updateGeometries();
}

/*!
    \fn bool QListView::uniformItemSizes() const

    Returns whether all items are taken to have the size of the first one. The default is \c false.

    \sa setUniformItemSizes()
*/

/*!
    Sets whether all items have the size of the first one to \a enable, so that the view asks the delegate only
    once. This makes a long list faster.

    \sa uniformItemSizes()
*/
void QListView::setUniformItemSizes(bool enable) {
    if (m_uniform == enable)
        return;
    m_uniform = enable;
    updateGeometries();
}

/*!
    \fn bool QListView::wordWrap() const

    Returns whether the text of an item may be broken into lines. The default is \c false.

    \sa setWordWrap()
*/

/*!
    Sets whether the text of an item may be broken into lines to \a on. The view tells the delegate through
    QStyleOptionViewItem::WrapText; QStyledItemDelegate draws one line.
*/
void QListView::setWordWrap(bool on) {
    m_wordWrap = on;
    updateGeometries();
}

/*!
    \fn bool QListView::isSelectionRectVisible() const

    Returns the value that setSelectionRectVisible() set. The default is \c false.
*/

/*!
    Keeps \a show for isSelectionRectVisible(). Nt draws no rubber band while the mouse selects items.
*/
void QListView::setSelectionRectVisible(bool show) { m_selectionRectVisible = show; }

/*!
    \fn Qt::Alignment QListView::itemAlignment() const

    Returns the alignment that setItemAlignment() set. The default is none.
*/

/*!
    Keeps \a alignment for itemAlignment(). Nt does not use it.
*/
void QListView::setItemAlignment(Qt::Alignment alignment) { m_itemAlignment = alignment; }

/*!
    Returns \c true if the row \a row is hidden; otherwise returns \c false.

    \sa setRowHidden()
*/
bool QListView::isRowHidden(int row) const {
    if (!model())
        return false;
    const QModelIndex index = model()->index(row, 0, rootIndex());
    return index.isValid() && m_hidden.contains(QPersistentModelIndex(index));
}

/*!
    Hides the row \a row if \a hide is \c true, and shows it again otherwise. A hidden row stays hidden while
    other rows come and go, until the model is reset.

    \sa isRowHidden(), clearPropertyFlags()
*/
void QListView::setRowHidden(int row, bool hide) {
    if (!model())
        return;
    const QModelIndex index = model()->index(row, 0, rootIndex());
    if (!index.isValid() || isRowHidden(row) == hide)
        return;
    if (hide)
        m_hidden.append(QPersistentModelIndex(index));
    else
        m_hidden.removeAll(QPersistentModelIndex(index));
    updateGeometries();
}

/*!
    Shows all rows that were hidden.

    \sa setRowHidden()
*/
void QListView::clearPropertyFlags() {
    if (m_hidden.isEmpty())
        return;
    m_hidden.clear();
    updateGeometries();
}

// Where the contents are scrolled to.
QPoint QListView::offset() const { return QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value()); }

// The view scrolls in the direction in which the lines of items follow each other.
bool QListView::scrollsVertically() const { return (m_flow == TopToBottom) != m_wrapping; }

// Lays the items out: their rectangles in the coordinates of the contents.
void QListView::doLayout() {
    m_items.clear();
    m_contents = QSize();
    if (!model())
        return;
    const QModelIndex root = rootIndex();
    const int rows = model()->rowCount(root);
    if (m_modelColumn >= model()->columnCount(root))
        return;
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    const bool grid = m_gridSize.isValid() && !m_gridSize.isEmpty();
    const int space = grid ? 0 : m_spacing;
    const QSize view = viewport()->size();
    m_items.reserve(static_cast<std::size_t>(std::max(0, rows)));
    QSize uniform;
    int widest = 0;
    for (int r = 0; r < rows; ++r) {
        const QModelIndex index = model()->index(r, m_modelColumn, root);
        if (!m_hidden.isEmpty() && m_hidden.contains(QPersistentModelIndex(index.sibling(r, 0))))
            continue;
        QSize size;
        if (grid)
            size = m_gridSize;
        else if (uniform.isValid())
            size = uniform;
        else
            size = itemDelegateForIndex(index)->sizeHint(option, index).expandedTo(QSize(1, 1));
        if (m_uniform && !uniform.isValid())
            uniform = size;
        widest = std::max(widest, size.width());
        m_items.push_back({index, QRect(QPoint(0, 0), size)});
    }
    // A list is as wide as the view, so that the highlight of an item goes across.
    const bool stretch = m_viewMode == ListMode && m_flow == TopToBottom && !m_wrapping && !grid;
    const int stretched = std::max(widest, view.width() - 2 * space);
    int x = space, y = space, line = 0; // `line`: the height of the row, or the width of the column, so far
    int right = 0, bottom = 0;
    for (Item &item : m_items) {
        QSize size = item.rect.size();
        if (stretch)
            size.setWidth(stretched);
        if (m_flow == LeftToRight) {
            if (m_wrapping && x > space && x + size.width() + space > view.width()) {
                y += line + space;
                x = space;
                line = 0;
            }
            item.rect = QRect(QPoint(x, y), size);
            x += size.width() + space;
            line = std::max(line, size.height());
        } else {
            if (m_wrapping && y > space && y + size.height() + space > view.height()) {
                x += line + space;
                y = space;
                line = 0;
            }
            item.rect = QRect(QPoint(x, y), size);
            y += size.height() + space;
            line = std::max(line, size.width());
        }
        right = std::max(right, item.rect.right() + 1);
        bottom = std::max(bottom, item.rect.bottom() + 1);
    }
    m_contents = QSize(right + space, bottom + space);
}

// Sets the ranges of the scroll bars for the contents.
void QListView::setRanges() {
    const QSize view = viewport()->size();
    QScrollBar *hbar = horizontalScrollBar(), *vbar = verticalScrollBar();
    const int step =
        m_items.empty() ? 20 : (scrollsVertically() ? m_items.front().rect.height() : m_items.front().rect.width());
    hbar->setPageStep(std::max(1, view.width()));
    hbar->setSingleStep(scrollsVertically() ? 20 : std::max(1, step));
    hbar->setRange(0, std::max(0, m_contents.width() - view.width()));
    vbar->setPageStep(std::max(1, view.height()));
    vbar->setSingleStep(scrollsVertically() ? std::max(1, step) : 20);
    vbar->setRange(0, std::max(0, m_contents.height() - view.height()));
}

/*!
    \reimp

    Lays the items out again and sets the ranges of the scroll bars. A scroll bar that comes or goes changes
    the room for the items, so this goes on until the layout fits.
*/
void QListView::updateGeometries() {
    if (m_inLayout) {
        m_layoutAgain = true;
        return;
    }
    m_inLayout = true;
    for (int pass = 0; pass < 6; ++pass) {
        m_layoutAgain = false;
        doLayout();
        setRanges();
        if (!m_layoutAgain)
            break;
    }
    m_inLayout = false;
    updateEditorGeometries();
    viewport()->update();
}

/*!
    Returns the size of the contents of the view: of all the items as they are laid out, with the spacing.
*/
QSize QListView::contentsSize() const { return m_contents; }

// The position in m_items of the item `index`, or -1 if it does not show.
int QListView::positionOf(const QModelIndex &index) const {
    if (!index.isValid() || index.model() != model() || index.column() != m_modelColumn ||
        index.parent() != rootIndex())
        return -1;
    // The items are in the order of their rows.
    const auto it = std::lower_bound(m_items.begin(), m_items.end(), index.row(),
                                     [](const Item &item, int row) { return item.index.row() < row; });
    if (it == m_items.end() || it->index.row() != index.row())
        return -1;
    return static_cast<int>(it - m_items.begin());
}

QModelIndex QListView::itemIndex(int position) const {
    if (position < 0 || position >= static_cast<int>(m_items.size()))
        return QModelIndex();
    return m_items[static_cast<std::size_t>(position)].index;
}

// The position of the item at `contentsPoint`, or -1.
int QListView::itemAt(const QPoint &contentsPoint) const {
    if (m_flow == TopToBottom && !m_wrapping) {
        // One under the other: the bottoms grow.
        const auto it = std::lower_bound(m_items.begin(), m_items.end(), contentsPoint.y(),
                                         [](const Item &item, int y) { return item.rect.bottom() < y; });
        if (it != m_items.end() && it->rect.contains(contentsPoint))
            return static_cast<int>(it - m_items.begin());
        return -1;
    }
    for (std::size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].rect.contains(contentsPoint))
            return static_cast<int>(i);
    }
    return -1;
}

/*!
    Returns the rectangle of the item \a index in the coordinates of the contents, before they are scrolled.
    It is empty if the item does not show.

    \sa visualRect()
*/
QRect QListView::rectForIndex(const QModelIndex &index) const {
    const int position = positionOf(index);
    return position < 0 ? QRect() : m_items[static_cast<std::size_t>(position)].rect;
}

/*!
    \reimp

    Returns the rectangle of the item \a index on the viewport, or an empty one if the item does not show: it
    is hidden, in another column, or not a child of the root.
*/
QRect QListView::visualRect(const QModelIndex &index) const {
    const QRect rect = rectForIndex(index);
    return rect.isEmpty() ? QRect() : rect.translated(-offset());
}

/*!
    \reimp

    Scrolls so that the item \a index shows, where \a hint says.
*/
void QListView::scrollTo(const QModelIndex &index, ScrollHint hint) {
    const QRect rect = rectForIndex(index);
    if (rect.isEmpty())
        return;
    const auto scroll = [hint](QScrollBar *bar, int start, int length, int view) {
        switch (hint) {
        case PositionAtTop:
            bar->setValue(start);
            break;
        case PositionAtBottom:
            bar->setValue(start + length - view);
            break;
        case PositionAtCenter:
            bar->setValue(start - (view - length) / 2);
            break;
        case EnsureVisible:
            if (start < bar->value() || length > view)
                bar->setValue(start);
            else if (start + length > bar->value() + view)
                bar->setValue(start + length - view);
            break;
        }
    };
    const QSize view = viewport()->size();
    if (scrollsVertically()) {
        scroll(verticalScrollBar(), rect.top(), rect.height(), view.height());
        QScrollBar *hbar = horizontalScrollBar();
        if (rect.left() < hbar->value())
            hbar->setValue(rect.left());
        else if (rect.right() + 1 > hbar->value() + view.width())
            hbar->setValue(rect.right() + 1 - view.width());
    } else {
        scroll(horizontalScrollBar(), rect.left(), rect.width(), view.width());
        QScrollBar *vbar = verticalScrollBar();
        if (rect.top() < vbar->value())
            vbar->setValue(rect.top());
        else if (rect.bottom() + 1 > vbar->value() + view.height())
            vbar->setValue(rect.bottom() + 1 - view.height());
    }
    viewport()->update();
}

/*!
    \reimp

    Returns the item at \a point of the viewport, or an invalid index if there is none there.
*/
QModelIndex QListView::indexAt(const QPoint &point) const { return itemIndex(itemAt(point + offset())); }

/*!
    \reimp

    Shows all rows again and lays the items of the model out anew.
*/
void QListView::reset() {
    m_hidden.clear();
    m_anchor = QPersistentModelIndex();
    m_pressed = QPersistentModelIndex();
    QAbstractItemView::reset();
    updateGeometries();
}

/*!
    \reimp

    Selects all the items that show, if the selection mode allows several.
*/
void QListView::selectAll() {
    if (!selectionModel() || m_items.empty())
        return;
    if (selectionMode() == NoSelection || selectionMode() == SingleSelection)
        return;
    selectRange(0, static_cast<int>(m_items.size()) - 1);
}

/*!
    \reimp

    Returns a size for a view of a few items.
*/
QSize QListView::sizeHint() const { return QSize(256, 192); }

/*!
    \reimp

    Fills \a option as the base class does; in the icon mode the decoration goes above the text, centered, in
    iconSize() or 32 by 32 pixels.
*/
void QListView::initViewItemOption(QStyleOptionViewItem *option) const {
    QAbstractItemView::initViewItemOption(option);
    if (m_wordWrap)
        option->features |= QStyleOptionViewItem::WrapText;
    if (m_viewMode == IconMode) {
        option->decorationPosition = QStyleOptionViewItem::Top;
        option->decorationAlignment = Qt::AlignCenter;
        option->displayAlignment = Qt::AlignHCenter | Qt::AlignTop;
        option->showDecorationSelected = true;
        if (!iconSize().isValid())
            option->decorationSize = kIconModeSize;
    }
}

// Makes the items from `fromPosition` to `toPosition` of the layout the selection.
void QListView::selectRange(int fromPosition, int toPosition) {
    if (!selectionModel())
        return;
    const int first = std::max(0, std::min(fromPosition, toPosition));
    const int last = std::min(static_cast<int>(m_items.size()) - 1, std::max(fromPosition, toPosition));
    QItemSelection selection;
    // Rows that follow each other make one range.
    int start = first;
    for (int p = first; p <= last; ++p) {
        const bool breaks = p == last || itemIndex(p + 1).row() != itemIndex(p).row() + 1;
        if (breaks) {
            selection.select(itemIndex(start), itemIndex(p));
            start = p + 1;
        }
    }
    selectionModel()->select(selection, QItemSelectionModel::SelectionFlags(QItemSelectionModel::ClearAndSelect));
}

// Makes `index` the current item and changes the selection the way the selection mode and the modifier keys
// say: for a click, or for a move with the keyboard.
void QListView::selectWith(const QModelIndex &index, Qt::KeyboardModifiers modifiers, bool keyboard) {
    QItemSelectionModel *selection = selectionModel();
    if (!selection || !index.isValid())
        return;
    const bool shift = modifiers & Qt::ShiftModifier;
    const bool control = modifiers & Qt::ControlModifier;
    const SelectionMode mode = selectionMode();
    switch (mode) {
    case NoSelection:
        selection->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        return;
    case SingleSelection:
        selection->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
        m_anchor = QPersistentModelIndex(index);
        return;
    case MultiSelection:
        selection->setCurrentIndex(index, keyboard ? QItemSelectionModel::NoUpdate : QItemSelectionModel::Toggle);
        m_anchor = QPersistentModelIndex(index);
        return;
    case ExtendedSelection:
    case ContiguousSelection:
        if (shift && m_anchor.isValid() && positionOf(m_anchor) >= 0) {
            selection->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
            selectRange(positionOf(m_anchor), positionOf(index));
            return;
        }
        if (control && mode == ExtendedSelection) {
            selection->setCurrentIndex(index, keyboard ? QItemSelectionModel::NoUpdate : QItemSelectionModel::Toggle);
            m_anchor = QPersistentModelIndex(index);
            return;
        }
        selection->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
        m_anchor = QPersistentModelIndex(index);
        return;
    }
}

/*!
    \reimp
*/
void QListView::paintEvent(QPaintEvent *event) {
    QPainter painter(viewport());
    QStyleOptionViewItem options;
    initViewItemOption(&options);
    const QPalette::ColorGroup group = !isEnabled()                             ? QPalette::Disabled
                                       : (options.state & QStyle::State_Active) ? QPalette::Active
                                                                                : QPalette::Inactive;
    painter.fillRect(event->rect(), options.palette.color(group, QPalette::Base));
    if (!model() || m_items.empty())
        return;
    const QPoint shift = offset();
    const QRect area = event->rect().translated(shift);
    const QItemSelectionModel *selection = selectionModel();
    const QModelIndex current = currentIndex();
    std::size_t first = 0;
    if (m_flow == TopToBottom && !m_wrapping) {
        const auto it = std::lower_bound(m_items.begin(), m_items.end(), area.top(),
                                         [](const Item &item, int y) { return item.rect.bottom() < y; });
        first = static_cast<std::size_t>(it - m_items.begin());
    }
    for (std::size_t i = first; i < m_items.size(); ++i) {
        const Item &item = m_items[i];
        if (!item.rect.intersects(area)) {
            if (m_flow == TopToBottom && !m_wrapping && item.rect.top() > area.bottom())
                break;
            continue;
        }
        QStyleOptionViewItem option = options;
        option.rect = item.rect.translated(-shift);
        if (alternatingRowColors() && (i % 2) == 1) {
            option.features |= QStyleOptionViewItem::Alternate;
            painter.fillRect(option.rect, options.palette.color(group, QPalette::AlternateBase));
        }
        if (selection && selection->isSelected(item.index))
            option.state |= QStyle::State_Selected;
        if (!(model()->flags(item.index) & Qt::ItemIsEnabled))
            option.state &= ~QStyle::State(QStyle::State_Enabled);
        const bool isCurrent = item.index == current && hasFocus();
        if (isCurrent)
            option.state |= QStyle::State_HasFocus;
        itemDelegateForIndex(item.index)->paint(&painter, option, item.index);
        if (isCurrent) {
            QStyleOption focus;
            focus.initFrom(this);
            focus.rect = option.rect;
            style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &painter, this);
        }
    }
}

/*!
    \reimp

    A press on an item makes it the current item and selects it the way the selection mode and the Shift and
    Control keys say; a press beside the items takes the selection away.
*/
void QListView::mousePressEvent(QMouseEvent *event) {
    m_pressed = QPersistentModelIndex();
    m_dragging = false;
    if (!model()) {
        event->ignore();
        return;
    }
    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        if (event->button() == Qt::LeftButton && selectionModel() && selectionMode() != NoSelection &&
            !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
            selectionModel()->clearSelection();
        return;
    }
    if (!(model()->flags(index) & Qt::ItemIsEnabled))
        return;
    m_pressed = QPersistentModelIndex(index);
    m_pressedWasSelected = selectionModel() && selectionModel()->isSelected(index) && currentIndex() == index;
    emit pressed(index);
    if (event->button() == Qt::LeftButton || !(selectionModel() && selectionModel()->isSelected(index)))
        selectWith(index, event->modifiers(), false);
    m_dragging = event->button() == Qt::LeftButton;
}

/*!
    \reimp

    In the extended and contiguous selection modes, dragging with the left button selects the items from the
    one that was pressed to the one under the mouse.
*/
void QListView::mouseMoveEvent(QMouseEvent *event) {
    if (!m_dragging || !(event->buttons() & Qt::LeftButton) || !selectionModel() || !m_anchor.isValid() ||
        (selectionMode() != ExtendedSelection && selectionMode() != ContiguousSelection)) {
        event->ignore();
        return;
    }
    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid() || index == currentIndex())
        return;
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
    selectRange(positionOf(m_anchor), positionOf(index));
}

/*!
    \reimp

    A release on the item that was pressed is a click: clicked() is emitted, the delegate gets the event, for
    a check box, and with the \c SelectedClicked trigger an item that was selected already is edited.
*/
void QListView::mouseReleaseEvent(QMouseEvent *event) {
    m_dragging = false;
    const QModelIndex pressedIndex = m_pressed;
    m_pressed = QPersistentModelIndex();
    if (!model() || !pressedIndex.isValid() || indexAt(event->pos()) != pressedIndex ||
        event->button() != Qt::LeftButton)
        return;
    emit clicked(pressedIndex);
    const bool plain = !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier));
    edit(pressedIndex, m_pressedWasSelected && plain ? SelectedClicked : NoEditTriggers, event);
}

/*!
    \reimp

    A double click on an item emits doubleClicked(), and edits the item if \c DoubleClicked is one of the edit
    triggers and the item can be edited; otherwise it emits activated().
*/
void QListView::mouseDoubleClickEvent(QMouseEvent *event) {
    if (!model() || event->button() != Qt::LeftButton)
        return;
    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid() || !(model()->flags(index) & Qt::ItemIsEnabled))
        return;
    const QPersistentModelIndex keep(index);
    emit doubleClicked(index);
    if (!keep.isValid() || edit(keep, DoubleClicked, event))
        return;
    emit activated(keep);
}

// The item nearest to the item at `from` in the direction of `direction`: in the next line of items that way,
// the one closest across. -1 if there is none.
QModelIndex QListView::nearest(int from, CursorAction direction) const {
    const QRect here = m_items[static_cast<std::size_t>(from)].rect;
    int best = -1;
    long long bestAlong = LLONG_MAX, bestAcross = LLONG_MAX;
    for (std::size_t i = 0; i < m_items.size(); ++i) {
        const QRect there = m_items[i].rect;
        long long along = 0, across = 0;
        switch (direction) {
        case MoveUp:
            along = here.top() - there.top();
            across = std::abs(there.center().x() - here.center().x());
            break;
        case MoveDown:
            along = there.top() - here.top();
            across = std::abs(there.center().x() - here.center().x());
            break;
        case MoveLeft:
            along = here.left() - there.left();
            across = std::abs(there.center().y() - here.center().y());
            break;
        case MoveRight:
            along = there.left() - here.left();
            across = std::abs(there.center().y() - here.center().y());
            break;
        default:
            return QModelIndex();
        }
        if (along <= 0)
            continue;
        if (along < bestAlong || (along == bestAlong && across < bestAcross)) {
            best = static_cast<int>(i);
            bestAlong = along;
            bestAcross = across;
        }
    }
    return itemIndex(best);
}

/*!
    \reimp

    Returns the item that \a cursorAction leads to from the current item: the neighbour in that direction for
    the arrow keys, the first or last item for Home and End, a page further for the page keys, and the next or
    previous row for \c MoveNext and \c MovePrevious. \a modifiers is not used.
*/
QModelIndex QListView::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) {
    static_cast<void>(modifiers);
    if (!model() || m_items.empty())
        return QModelIndex();
    const int count = static_cast<int>(m_items.size());
    const int from = positionOf(currentIndex());
    if (from < 0)
        return itemIndex(0);
    switch (cursorAction) {
    case MoveUp:
    case MoveDown:
    case MoveLeft:
    case MoveRight: {
        const QModelIndex next = nearest(from, cursorAction);
        return next.isValid() ? next : itemIndex(from);
    }
    case MoveHome:
        return itemIndex(0);
    case MoveEnd:
        return itemIndex(count - 1);
    case MoveNext:
        return itemIndex(std::min(count - 1, from + 1));
    case MovePrevious:
        return itemIndex(std::max(0, from - 1));
    case MovePageUp:
    case MovePageDown: {
        // The farthest item that is no more than a page away, in the direction that the view scrolls.
        const bool down = cursorAction == MovePageDown;
        const bool vertical = scrollsVertically();
        const QRect here = m_items[static_cast<std::size_t>(from)].rect;
        const int page = vertical ? viewport()->height() : viewport()->width();
        const int start = vertical ? here.center().y() : here.center().x();
        int best = from;
        int bestDistance = 0;
        for (int i = 0; i < count; ++i) {
            const QRect there = m_items[static_cast<std::size_t>(i)].rect;
            const int distance = ((vertical ? there.center().y() : there.center().x()) - start) * (down ? 1 : -1);
            if (distance <= 0 || distance > page)
                continue;
            const int across = vertical ? std::abs(there.center().x() - here.center().x())
                                        : std::abs(there.center().y() - here.center().y());
            const int bestAcross =
                vertical ? std::abs(m_items[static_cast<std::size_t>(best)].rect.center().x() - here.center().x())
                         : std::abs(m_items[static_cast<std::size_t>(best)].rect.center().y() - here.center().y());
            if (distance > bestDistance || (distance == bestDistance && across < bestAcross)) {
                best = i;
                bestDistance = distance;
            }
        }
        if (best == from) // not even one item within a page: the next one
            return itemIndex(down ? std::min(count - 1, from + 1) : std::max(0, from - 1));
        return itemIndex(best);
    }
    }
    return itemIndex(from);
}

/*!
    \reimp

    The arrow, Home, End and page keys move the current item; Return and Enter activate it; F2 edits it, as
    typing does with the \c AnyKeyPressed trigger; Space toggles a check box or the selection; Control+A selects
    everything.
*/
void QListView::keyPressEvent(QKeyEvent *event) {
    if (!model() || m_items.empty()) {
        QAbstractItemView::keyPressEvent(event);
        return;
    }
    const QModelIndex current = currentIndex();
    CursorAction action = MoveDown;
    bool moves = true;
    switch (event->key()) {
    case Qt::Key_Up:
        action = MoveUp;
        break;
    case Qt::Key_Down:
        action = MoveDown;
        break;
    case Qt::Key_Left:
        action = MoveLeft;
        break;
    case Qt::Key_Right:
        action = MoveRight;
        break;
    case Qt::Key_Home:
        action = MoveHome;
        break;
    case Qt::Key_End:
        action = MoveEnd;
        break;
    case Qt::Key_PageUp:
        action = MovePageUp;
        break;
    case Qt::Key_PageDown:
        action = MovePageDown;
        break;
    default:
        moves = false;
        break;
    }
    if (moves) {
        const QModelIndex target = moveCursor(action, event->modifiers());
        if (target.isValid() && target != current)
            selectWith(target, event->modifiers(), true);
        else if (target.isValid() && !current.isValid())
            selectWith(target, Qt::NoModifier, true);
        event->accept();
        return;
    }
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (current.isValid() && (model()->flags(current) & Qt::ItemIsEnabled))
            emit activated(current);
        break;
    case Qt::Key_F2:
        if (!edit(current, EditKeyPressed, event))
            event->ignore();
        return;
    case Qt::Key_Space:
        if (current.isValid() && !edit(current, NoEditTriggers, event) && selectionModel() &&
            selectionMode() != NoSelection) {
            selectionModel()->select(current, selectionMode() == SingleSelection ? QItemSelectionModel::ClearAndSelect
                                                                                 : QItemSelectionModel::Toggle);
        }
        break;
    case Qt::Key_A:
        if (event->modifiers() & Qt::ControlModifier) {
            selectAll();
            break;
        }
        [[fallthrough]];
    default: {
        const QString text = event->text();
        const bool printable =
            !text.isEmpty() && text.at(0).isPrint() && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier));
        if (printable && current.isValid() && edit(current, AnyKeyPressed, event) && state() == EditingState) {
            if (QWidget *editor = QApplication::focusWidget(); editor && editor != this)
                QCoreApplication::sendEvent(editor, event);
            event->accept();
            return;
        }
        QAbstractItemView::keyPressEvent(event);
        return;
    }
    }
    event->accept();
}

/*!
    \reimp
*/
void QListView::resizeEvent(QResizeEvent *event) {
    static_cast<void>(event);
    updateGeometries();
}

/*!
    \reimp
*/
void QListView::showEvent(QShowEvent *event) {
    static_cast<void>(event);
    updateGeometries();
}

/*!
    \reimp

    Repaints the items that scrolled into view, and asks a model that hands out its rows in steps for more when
    the end is reached; \a dx and \a dy are not used.
*/
void QListView::scrollContentsBy(int dx, int dy) {
    static_cast<void>(dx);
    static_cast<void>(dy);
    viewport()->update();
    updateEditorGeometries();
    QScrollBar *bar = scrollsVertically() ? verticalScrollBar() : horizontalScrollBar();
    if (model() && bar->value() >= bar->maximum() && model()->canFetchMore(rootIndex()))
        model()->fetchMore(rootIndex());
}

/*!
    \reimp

    Shows the tool tip of the item under the mouse, which the model returns for Qt::ToolTipRole; other
    \a event are handled as in the base class.
*/
bool QListView::viewportEvent(QEvent *event) {
    if (event->type() == QEvent::ToolTip && model()) {
        auto *help = static_cast<QHelpEvent *>(event);
        const QModelIndex index = indexAt(help->pos());
        const QString tip = index.isValid() ? index.data(Qt::ToolTipRole).toString() : QString();
        if (!tip.isEmpty()) {
            QToolTip::showText(help->globalPos(), tip, viewport(), visualRect(index));
            event->accept();
            return true;
        }
    }
    return QAbstractItemView::viewportEvent(event);
}

/*!
    \reimp

    Lays the items out again when the data of the items from \a topLeft to \a bottomRight changed for \a roles,
    since their sizes may have changed, unless all items have the same size.
*/
void QListView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles) {
    QAbstractItemView::dataChanged(topLeft, bottomRight, roles);
    if (!m_uniform && !(m_gridSize.isValid() && !m_gridSize.isEmpty()) && topLeft.parent() == rootIndex() &&
        topLeft.column() <= m_modelColumn && bottomRight.column() >= m_modelColumn)
        updateGeometries();
}

/*!
    \reimp

    Lays the items out again after the rows \a start to \a end were inserted under \a parent.
*/
void QListView::rowsInserted(const QModelIndex &parent, int start, int end) {
    QAbstractItemView::rowsInserted(parent, start, end);
    if (parent == rootIndex())
        updateGeometries();
}

/*!
    \reimp

    Moves the current item off the rows \a start to \a end of \a parent before they are removed: to the row
    after them, or else the row before them.
*/
void QListView::rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end) {
    QAbstractItemView::rowsAboutToBeRemoved(parent, start, end);
    const QModelIndex current = currentIndex();
    if (parent != rootIndex() || !current.isValid() || current.parent() != parent || current.row() < start ||
        current.row() > end)
        return;
    QModelIndex next;
    if (end + 1 < model()->rowCount(parent))
        next = model()->index(end + 1, current.column(), parent);
    else if (start > 0)
        next = model()->index(start - 1, current.column(), parent);
    if (selectionModel())
        selectionModel()->setCurrentIndex(next, QItemSelectionModel::NoUpdate);
}

#include "moc_qlistview.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
