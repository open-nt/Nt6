// MIT License - see LICENSE
#include <QtWidgets/qcolumnview.h>

#include <QtCore/qitemselectionmodel.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qscrollbar.h>

#include <algorithm>

namespace {
constexpr int kDefaultWidth = 200;
}

/*!
    \class QColumnView
    \inmodule NtWidgets
    \since 6.0
    \brief The QColumnView class shows a tree model as columns, one for each level that is open.

    The first column shows the items of the root; choosing an item that has children opens a column with them to
    its right, and choosing an item without children shows the preview widget, if there is one. The columns scroll
    sideways:

    \code
    auto *view = new QColumnView;
    view->setModel(fileSystemModel);
    view->setRootIndex(fileSystemModel->index(QDir::homePath()));
    \endcode

    A column view of a small project tree, with a file in the src folder selected.

    \image widgets/qcolumnview.png QColumnView in the light color scheme

    \differences
    Each column is a QListView with a selection of its own: the view has no selection model of its own across the
    columns, and the user cannot resize the columns, so resizeGripsVisible() has no effect.
    \enddifferences

    \sa QTreeView, QListView
*/

/*!
    Constructs a column view with the given \a parent.
*/
QColumnView::QColumnView(QWidget *parent) : QAbstractItemView(parent) {
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

/*!
    Destroys the column view.
*/
QColumnView::~QColumnView() = default;

/*!
    \reimp
*/
void QColumnView::setModel(QAbstractItemModel *model) {
    truncate(0);
    QAbstractItemView::setModel(model);
    if (model)
        openFrom(-1, rootIndex());
}

/*!
    \reimp
*/
void QColumnView::setRootIndex(const QModelIndex &index) {
    QAbstractItemView::setRootIndex(index);
    truncate(0);
    if (model())
        openFrom(-1, index);
}

/*!
    Returns a new column that shows the children of \a rootIndex, a QListView by default. Reimplement it to give the
    view columns of your own; call initializeColumn() on them.
*/
QAbstractItemView *QColumnView::createColumn(const QModelIndex &rootIndex) {
    auto *column = new QListView(viewport());
    initializeColumn(column);
    column->setRootIndex(rootIndex);
    return column;
}

/*!
    Gives \a column the model, the settings and the connections of the view.
*/
void QColumnView::initializeColumn(QAbstractItemView *column) const {
    column->setModel(model());
    column->setSelectionMode(selectionMode());
    column->setIconSize(iconSize());
    column->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *self = const_cast<QColumnView *>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast): connects to itself
    QObject::connect(column, &QAbstractItemView::clicked, self, [self, column](const QModelIndex &index) {
        for (std::size_t i = 0; i < self->m_columns.size(); ++i)
            if (self->m_columns[i] == column)
                self->openFrom(static_cast<int>(i), index);
        self->setCurrentIndex(index);
        emit self->clicked(index);
    });
    QObject::connect(column, &QAbstractItemView::activated, self,
                     [self](const QModelIndex &index) { emit self->activated(index); });
}

void QColumnView::truncate(int keep) {
    while (static_cast<int>(m_columns.size()) > keep) {
        if (QAbstractItemView *column = m_columns.back().data())
            column->deleteLater();
        m_columns.pop_back();
    }
    if (m_previewHolder) {
        if (m_preview)
            m_preview->setParent(nullptr);
        m_previewHolder->deleteLater();
        m_previewHolder = nullptr;
    }
}

void QColumnView::openFrom(int columnIndex, const QModelIndex &index) {
    if (m_opening || !model())
        return;
    m_opening = true;
    truncate(columnIndex + 1);
    if (columnIndex < 0 || model()->hasChildren(index)) {
        QAbstractItemView *column = createColumn(index);
        column->show();
        m_columns.emplace_back(column);
    } else if (m_preview) {
        // An item without children: the preview widget, in a column of its own.
        auto *holder = new QWidget(viewport());
        m_preview->setParent(holder);
        m_preview->move(0, 0);
        m_preview->show();
        holder->show();
        m_previewHolder = holder;
        emit updatePreviewWidget(index);
    }
    m_opening = false;
    placeColumns();
    // The new column is scrolled into view.
    horizontalScrollBar()->setValue(horizontalScrollBar()->maximum());
}

void QColumnView::placeColumns() {
    const int height = viewport()->height();
    int x = -horizontalScrollBar()->value();
    int total = 0;
    for (std::size_t i = 0; i < m_columns.size(); ++i) {
        const int width = static_cast<int>(i) < m_widths.size() ? m_widths.at(static_cast<int>(i)) : kDefaultWidth;
        if (QAbstractItemView *column = m_columns[i].data())
            column->setGeometry(x, 0, width, height);
        x += width;
        total += width;
    }
    if (m_previewHolder) {
        const int width = m_preview ? std::max(kDefaultWidth, m_preview->sizeHint().width()) : kDefaultWidth;
        m_previewHolder->setGeometry(x, 0, width, height);
        if (m_preview)
            m_preview->resize(width, height);
        total += width;
    }
    QScrollBar *bar = horizontalScrollBar();
    bar->setPageStep(std::max(1, viewport()->width()));
    bar->setSingleStep(20);
    bar->setRange(0, std::max(0, total - viewport()->width()));
}

/*!
    \reimp
*/
void QColumnView::resizeEvent(QResizeEvent *event) {
    QAbstractItemView::resizeEvent(event);
    placeColumns();
}

/*!
    \reimp
*/
void QColumnView::scrollContentsBy(int dx, int dy) {
    (void)dx;
    (void)dy;
    placeColumns();
}

/*!
    \reimp

    Shows the path to \a current: the columns of its parents, with them chosen.
*/
void QColumnView::currentChanged(const QModelIndex &current, const QModelIndex &previous) {
    QAbstractItemView::currentChanged(current, previous);
    if (!current.isValid() || m_opening || !model())
        return;
    // The chain from the root down to the item.
    QList<QModelIndex> path;
    for (QModelIndex i = current; i.isValid() && i != rootIndex(); i = i.parent())
        path.prepend(i);
    for (int level = 0; level < path.size(); ++level) {
        const QModelIndex at = path.at(level);
        if (level >= static_cast<int>(m_columns.size()) || !m_columns[static_cast<std::size_t>(level)])
            break;
        QAbstractItemView *column = m_columns[static_cast<std::size_t>(level)].data();
        if (column->currentIndex() != at) {
            column->setCurrentIndex(at);
            openFrom(level, at);
        } else if (level + 1 == path.size() && level + 1 >= static_cast<int>(m_columns.size())) {
            openFrom(level, at);
        }
    }
}

/*!
    \reimp
*/
QModelIndex QColumnView::indexAt(const QPoint &point) const {
    for (const QPointer<QAbstractItemView> &column : m_columns) {
        if (column && column->geometry().contains(point))
            return column->indexAt(column->viewport()->mapFrom(viewport(), point));
    }
    return QModelIndex();
}

/*!
    \reimp
*/
QRect QColumnView::visualRect(const QModelIndex &index) const {
    for (const QPointer<QAbstractItemView> &column : m_columns) {
        if (column && column->rootIndex() == index.parent()) {
            const QRect inner = column->visualRect(index);
            return inner.isValid() ? QRect(column->viewport()->mapTo(viewport(), inner.topLeft()), inner.size())
                                   : QRect();
        }
    }
    return QRect();
}

/*!
    \reimp

    Opens the columns of the parents of \a index, and scrolls them so that it is in view; \a hint is passed to the
    column of \a index.
*/
void QColumnView::scrollTo(const QModelIndex &index, ScrollHint hint) {
    setCurrentIndex(index);
    for (const QPointer<QAbstractItemView> &column : m_columns)
        if (column && column->rootIndex() == index.parent())
            column->scrollTo(index, hint);
}

/*!
    \reimp

    Selects all items of the last column.
*/
void QColumnView::selectAll() {
    if (!m_columns.empty() && m_columns.back())
        m_columns.back()->selectAll();
}

/*!
    \reimp
*/
QSize QColumnView::sizeHint() const { return QSize(3 * kDefaultWidth, 300); }

/*!
    Returns the widths of the columns that are open.
*/
QList<int> QColumnView::columnWidths() const {
    QList<int> widths;
    for (std::size_t i = 0; i < m_columns.size(); ++i)
        widths.append(static_cast<int>(i) < m_widths.size() ? m_widths.at(static_cast<int>(i)) : kDefaultWidth);
    return widths;
}

/*!
    Sets the widths of the columns, from the first, to \a list; the others are 200 pixels wide.
*/
void QColumnView::setColumnWidths(const QList<int> &list) {
    m_widths = list;
    placeColumns();
}

/*!
    \fn QWidget *QColumnView::previewWidget() const

    Returns the preview widget, or \c nullptr.
*/

/*!
    Sets the widget shown in the last column when an item without children is chosen to \a widget. The view takes
    ownership of it.
*/
void QColumnView::setPreviewWidget(QWidget *widget) {
    if (m_preview && m_preview != widget)
        m_preview->deleteLater();
    m_preview = widget;
    if (widget) {
        widget->setParent(this);
        widget->hide();
    }
}

/*!
    \fn bool QColumnView::resizeGripsVisible() const

    Returns whether the columns have grips to resize them; Nt has none.
*/

/*!
    \fn void QColumnView::setResizeGripsVisible(bool visible)

    Sets whether the columns have grips to resize them to \a visible; Nt keeps the value.
*/

#include "moc_qcolumnview.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
