// MIT License - see LICENSE
#include <QtGui/qstandarditemmodel.h>

#include <QtCore/qcompare.h>
#include <QtCore/qregularexpression.h>

#include <algorithm>
#include <functional>
#include <numeric>
#include <vector>

namespace {

// Whether `a` sorts before `b`: numbers and dates by value, everything else as text.
bool variantLess(const QVariant &a, const QVariant &b) {
    const QPartialOrdering order = QVariant::compare(a, b);
    if (order == QPartialOrdering::Less)
        return true;
    if (order != QPartialOrdering::Unordered)
        return false;
    return QString::localeAwareCompare(a.toString(), b.toString()) < 0;
}

bool textMatches(const QString &value, const QString &text, Qt::MatchFlags flags) {
    const Qt::CaseSensitivity cs = flags & Qt::MatchCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    switch (static_cast<int>(flags & 0x0f)) {
    case Qt::MatchExactly:
        return value == text;
    case Qt::MatchContains:
        return value.contains(text, cs);
    case Qt::MatchStartsWith:
        return value.startsWith(text, cs);
    case Qt::MatchEndsWith:
        return value.endsWith(text, cs);
    case Qt::MatchRegularExpression:
    case Qt::MatchWildcard: {
        const QString pattern = (flags & 0x0f) == Qt::MatchWildcard
                                    ? QRegularExpression::wildcardToRegularExpression(text)
                                    : QRegularExpression::anchoredPattern(text);
        const QRegularExpression re(pattern, cs == Qt::CaseSensitive ? QRegularExpression::NoPatternOption
                                                                     : QRegularExpression::CaseInsensitiveOption);
        return re.match(value).hasMatch();
    }
    default: // Qt::MatchFixedString
        return value.compare(text, cs) == 0;
    }
}

} // namespace

/*!
    \class QStandardItem
    \inmodule NtGui
    \since 6.0
    \brief The QStandardItem class is an item of a QStandardItemModel.

    An item holds data for any number of roles (text(), icon(), checkState() and the others are shortcuts for
    data()), its flags(), and a table of child items. Put items into a model with QStandardItemModel::appendRow()
    or setItem(), and give them children with appendRow() and setChild(); the model owns the items it holds.

    \code
    auto *fruit = new QStandardItem("Fruit");
    fruit->appendRow(new QStandardItem("Apple"));
    fruit->appendRow(new QStandardItem("Cherry"));
    model->appendRow(fruit);
    \endcode

    \differences
    There is no QDataStream support (\c read(), \c write()), and no \c multiData(). Auto-tristate items do not
    compute their check state from their children.
    \enddifferences

    \sa QStandardItemModel
*/

/*!
    \enum QStandardItem::ItemType

    The type() of an item.

    \value Type A QStandardItem.
    \value UserType The first type for subclasses.
*/

/*!
    Constructs an item without data or children.
*/
QStandardItem::QStandardItem() = default;

/*!
    Constructs an item with the text \a text.
*/
QStandardItem::QStandardItem(const QString &text) { m_values.insert(Qt::DisplayRole, text); }

/*!
    Constructs an item with the icon \a icon and the text \a text.
*/
QStandardItem::QStandardItem(const QIcon &icon, const QString &text) : QStandardItem(text) {
    m_values.insert(Qt::DecorationRole, QVariant::fromValue(icon));
}

/*!
    Constructs an item with a table of \a rows by \a columns empty children.
*/
QStandardItem::QStandardItem(int rows, int columns)
    : m_children(static_cast<qsizetype>(std::max(0, rows)) * std::max(0, columns), nullptr), m_rows(std::max(0, rows)),
      m_columns(std::max(0, columns)) {}

/*!
    Constructs an item with the data and the flags of \a other, but no children.
*/
QStandardItem::QStandardItem(const QStandardItem &other) : m_values(other.m_values), m_flags(other.m_flags) {}

/*!
    Sets the data and the flags of this item to those of \a other, and keeps its children.
*/
QStandardItem &QStandardItem::operator=(const QStandardItem &other) {
    if (this != &other) {
        m_values = other.m_values;
        m_flags = other.m_flags;
        emitDataChanged();
    }
    return *this;
}

/*!
    Destroys the item and its children.
*/
QStandardItem::~QStandardItem() {
    for (QStandardItem *child : std::as_const(m_children)) {
        if (child) {
            child->m_parent = nullptr;
            delete child;
        }
    }
}

/*!
    Returns the data of the item for \a role; Qt::EditRole gives the Qt::DisplayRole data.
*/
QVariant QStandardItem::data(int role) const {
    return m_values.value(role == Qt::EditRole ? static_cast<int>(Qt::DisplayRole) : role);
}

/*!
    Sets the data of the item for \a role to \a value; Qt::EditRole sets the Qt::DisplayRole data. An invalid
    \a value removes the data. The model emits QAbstractItemModel::dataChanged() and
    QStandardItemModel::itemChanged().
*/
void QStandardItem::setData(const QVariant &value, int role) {
    const int key = role == Qt::EditRole ? static_cast<int>(Qt::DisplayRole) : role;
    const auto it = m_values.constFind(key);
    if (!value.isValid()) {
        if (it == m_values.constEnd())
            return;
        m_values.remove(key);
    } else {
        if (it != m_values.constEnd() && *it == value && it->metaType() == value.metaType())
            return;
        m_values.insert(key, value);
    }
    if (m_model) {
        const QList<int> roles = key == Qt::DisplayRole ? QList<int>{Qt::DisplayRole, Qt::EditRole} : QList<int>{key};
        m_model->itemDataChanged(this, roles);
    }
}

/*!
    Removes all of the data of the item.
*/
void QStandardItem::clearData() {
    if (m_values.isEmpty())
        return;
    m_values.clear();
    emitDataChanged();
}

/*!
    \fn QString QStandardItem::text() const

    Returns the text of the item, its Qt::DisplayRole data.
*/

/*!
    \fn void QStandardItem::setText(const QString &text)

    Sets the text of the item to \a text.
*/

/*!
    \fn QIcon QStandardItem::icon() const

    Returns the icon of the item, its Qt::DecorationRole data.
*/

/*!
    \fn void QStandardItem::setIcon(const QIcon &icon)

    Sets the icon of the item to \a icon.
*/

/*!
    \fn QString QStandardItem::toolTip() const

    Returns the tool tip of the item.
*/

/*!
    \fn void QStandardItem::setToolTip(const QString &toolTip)

    Sets the tool tip of the item to \a toolTip.
*/

/*!
    \fn QString QStandardItem::statusTip() const

    Returns the status tip of the item.
*/

/*!
    \fn void QStandardItem::setStatusTip(const QString &statusTip)

    Sets the status tip of the item to \a statusTip.
*/

/*!
    \fn QString QStandardItem::whatsThis() const

    Returns the "What's This?" help of the item.
*/

/*!
    \fn void QStandardItem::setWhatsThis(const QString &whatsThis)

    Sets the "What's This?" help of the item to \a whatsThis.
*/

/*!
    \fn QSize QStandardItem::sizeHint() const

    Returns the size hint of the item, or an invalid size if it has none.
*/

/*!
    \fn void QStandardItem::setSizeHint(const QSize &size)

    Sets the size hint of the item to \a size.
*/

/*!
    \fn QFont QStandardItem::font() const

    Returns the font of the item.
*/

/*!
    \fn void QStandardItem::setFont(const QFont &font)

    Sets the font of the item to \a font.
*/

/*!
    \fn Qt::Alignment QStandardItem::textAlignment() const

    Returns the alignment of the text of the item.
*/

/*!
    \fn void QStandardItem::setTextAlignment(Qt::Alignment alignment)

    Sets the alignment of the text of the item to \a alignment.
*/

/*!
    \fn QBrush QStandardItem::background() const

    Returns the brush that the background of the item is filled with.
*/

/*!
    \fn void QStandardItem::setBackground(const QBrush &brush)

    Sets the brush that the background of the item is filled with to \a brush.
*/

/*!
    \fn QBrush QStandardItem::foreground() const

    Returns the brush that the text of the item is drawn with.
*/

/*!
    \fn void QStandardItem::setForeground(const QBrush &brush)

    Sets the brush that the text of the item is drawn with to \a brush.
*/

/*!
    \fn Qt::CheckState QStandardItem::checkState() const

    Returns the check state of the item.
*/

/*!
    \fn void QStandardItem::setCheckState(Qt::CheckState state)

    Sets the check state of the item to \a state.
*/

/*!
    \fn QString QStandardItem::accessibleText() const

    Returns the text of the item for accessibility tools.
*/

/*!
    \fn void QStandardItem::setAccessibleText(const QString &text)

    Sets the text of the item for accessibility tools to \a text.
*/

/*!
    \fn QString QStandardItem::accessibleDescription() const

    Returns the description of the item for accessibility tools.
*/

/*!
    \fn void QStandardItem::setAccessibleDescription(const QString &text)

    Sets the description of the item for accessibility tools to \a text.
*/

/*!
    \fn Qt::ItemFlags QStandardItem::flags() const

    Returns the flags of the item: by default it is selectable, enabled, editable and can be dragged and dropped on.
*/

/*!
    Sets the flags of the item to \a flags.
*/
void QStandardItem::setFlags(Qt::ItemFlags flags) {
    if (flags == m_flags)
        return;
    m_flags = flags;
    emitDataChanged();
}

/*!
    \fn bool QStandardItem::isEnabled() const

    Returns \c true if the user can interact with the item.
*/

/*!
    Enables the item if \a enabled is \c true, or disables it.
*/
void QStandardItem::setEnabled(bool enabled) { setFlags(Qt::ItemFlags(m_flags).setFlag(Qt::ItemIsEnabled, enabled)); }

/*!
    \fn bool QStandardItem::isEditable() const

    Returns \c true if the user can edit the item.
*/

/*!
    Makes the item editable if \a editable is \c true.
*/
void QStandardItem::setEditable(bool editable) {
    setFlags(Qt::ItemFlags(m_flags).setFlag(Qt::ItemIsEditable, editable));
}

/*!
    \fn bool QStandardItem::isSelectable() const

    Returns \c true if the user can select the item.
*/

/*!
    Makes the item selectable if \a selectable is \c true.
*/
void QStandardItem::setSelectable(bool selectable) {
    setFlags(Qt::ItemFlags(m_flags).setFlag(Qt::ItemIsSelectable, selectable));
}

/*!
    \fn bool QStandardItem::isCheckable() const

    Returns \c true if the user can check the item.
*/

/*!
    Makes the item checkable if \a checkable is \c true; it is then unchecked if it had no check state.
*/
void QStandardItem::setCheckable(bool checkable) {
    if (checkable && !data(Qt::CheckStateRole).isValid())
        setCheckState(Qt::Unchecked);
    setFlags(Qt::ItemFlags(m_flags).setFlag(Qt::ItemIsUserCheckable, checkable));
}

/*!
    \fn bool QStandardItem::isAutoTristate() const

    Returns \c true if the check state of the item is meant to follow those of its children.
*/

/*!
    Sets whether the check state of the item follows those of its children to \a tristate.
*/
void QStandardItem::setAutoTristate(bool tristate) {
    setFlags(Qt::ItemFlags(m_flags).setFlag(Qt::ItemIsAutoTristate, tristate));
}

/*!
    \fn bool QStandardItem::isUserTristate() const

    Returns \c true if the user can give the item the partially checked state.
*/

/*!
    Sets whether the user can give the item the partially checked state to \a tristate.
*/
void QStandardItem::setUserTristate(bool tristate) {
    setFlags(Qt::ItemFlags(m_flags).setFlag(Qt::ItemIsUserTristate, tristate));
}

/*!
    \fn bool QStandardItem::isDragEnabled() const

    Returns \c true if the user can drag the item.
*/

/*!
    Sets whether the user can drag the item to \a dragEnabled.
*/
void QStandardItem::setDragEnabled(bool dragEnabled) {
    setFlags(Qt::ItemFlags(m_flags).setFlag(Qt::ItemIsDragEnabled, dragEnabled));
}

/*!
    \fn bool QStandardItem::isDropEnabled() const

    Returns \c true if data can be dropped on the item.
*/

/*!
    Sets whether data can be dropped on the item to \a dropEnabled.
*/
void QStandardItem::setDropEnabled(bool dropEnabled) {
    setFlags(Qt::ItemFlags(m_flags).setFlag(Qt::ItemIsDropEnabled, dropEnabled));
}

/*!
    Returns the parent of the item, or \c nullptr for a top-level item of a model and an item outside any.
*/
QStandardItem *QStandardItem::parent() const {
    if (m_model && m_parent == m_model->invisibleRootItem())
        return nullptr;
    return m_parent;
}

/*!
    Returns the row of the item in its parent, or -1 if it has none.
*/
int QStandardItem::row() const {
    if (!m_parent || m_parent->m_columns == 0)
        return -1;
    const QList<QStandardItem *> &siblings = m_parent->m_children;
    if (m_lastIndex < 0 || m_lastIndex >= siblings.size() || siblings.at(m_lastIndex) != this)
        m_lastIndex = static_cast<int>(siblings.indexOf(const_cast<QStandardItem *>(this)));
    return m_lastIndex < 0 ? -1 : m_lastIndex / m_parent->m_columns;
}

/*!
    Returns the column of the item in its parent, or -1 if it has none.
*/
int QStandardItem::column() const {
    if (row() < 0)
        return -1;
    return m_lastIndex % m_parent->m_columns;
}

/*!
    Returns the index of the item in its model, or an invalid index if it is in none.
*/
QModelIndex QStandardItem::index() const { return m_model ? m_model->indexFromItem(this) : QModelIndex(); }

/*!
    \fn QStandardItemModel *QStandardItem::model() const

    Returns the model that holds the item, or \c nullptr.
*/

/*!
    \fn int QStandardItem::rowCount() const

    Returns the number of rows of children.
*/

/*!
    \fn int QStandardItem::columnCount() const

    Returns the number of columns of children.
*/

/*!
    \fn bool QStandardItem::hasChildren() const

    Returns \c true if the item has rows and columns of children.
*/

/*!
    Sets the number of rows of children to \a rows, adding empty rows or removing the last ones.
*/
void QStandardItem::setRowCount(int rows) {
    if (rows > m_rows)
        insertRows(m_rows, rows - m_rows);
    else if (rows >= 0 && rows < m_rows)
        removeRows(rows, m_rows - rows);
}

/*!
    Sets the number of columns of children to \a columns, adding empty columns or removing the last ones.
*/
void QStandardItem::setColumnCount(int columns) {
    if (columns > m_columns)
        insertColumns(m_columns, columns - m_columns);
    else if (columns >= 0 && columns < m_columns)
        removeColumns(columns, m_columns - columns);
}

/*!
    Returns the child at \a row and \a column, or \c nullptr if there is none.
*/
QStandardItem *QStandardItem::child(int row, int column) const {
    if (row < 0 || column < 0 || row >= m_rows || column >= m_columns)
        return nullptr;
    return m_children.at(slot(row, column));
}

void QStandardItem::adopt(QStandardItem *item, int row, int column) {
    const qsizetype at = slot(row, column);
    m_children[at] = item;
    if (!item)
        return;
    item->m_parent = this;
    item->m_lastIndex = static_cast<int>(at);
    item->setModel(m_model);
}

void QStandardItem::setModel(QStandardItemModel *model) {
    m_model = model;
    for (QStandardItem *child : std::as_const(m_children))
        if (child)
            child->setModel(model);
}

QModelIndex QStandardItem::childParentIndex() const {
    if (!m_model || this == m_model->invisibleRootItem())
        return QModelIndex();
    return index();
}

/*!
    Sets the child at \a row and \a column to \a item, and deletes the child that was there. The table of children
    grows to hold it. The item takes ownership of \a item, which must not have a parent.
*/
void QStandardItem::setChild(int row, int column, QStandardItem *item) {
    if (row < 0 || column < 0 || item == this || (item && item->m_parent))
        return;
    if (row >= m_rows)
        setRowCount(row + 1);
    if (column >= m_columns)
        setColumnCount(column + 1);
    QStandardItem *old = m_children.at(slot(row, column));
    if (old == item)
        return;
    if (old) {
        old->m_parent = nullptr;
        delete old;
    }
    adopt(item, row, column);
    if (m_model) {
        const QModelIndex changed = m_model->index(row, column, childParentIndex());
        emit m_model->dataChanged(changed, changed);
    }
}

/*!
    \fn void QStandardItem::setChild(int row, QStandardItem *item)
    \overload

    Sets the child at \a row in the first column to \a item.
*/

/*!
    Inserts a row at \a row with \a items as its columns. The number of columns grows to hold them.
*/
void QStandardItem::insertRow(int row, const QList<QStandardItem *> &items) {
    if (row < 0 || row > m_rows)
        return;
    if (items.size() > m_columns)
        setColumnCount(static_cast<int>(items.size()));
    if (m_model)
        m_model->notifyInsertRows(childParentIndex(), row, row, true);
    m_children.insert(slot(row, 0), m_columns, nullptr);
    ++m_rows;
    for (qsizetype i = 0; i < items.size(); ++i)
        if (items.at(i) && !items.at(i)->m_parent)
            adopt(items.at(i), row, static_cast<int>(i));
    if (m_model) {
        if (this == m_model->invisibleRootItem() && row < m_model->m_verticalHeader.size())
            m_model->m_verticalHeader.insert(row, nullptr);
        m_model->notifyInsertRows(childParentIndex(), row, row, false);
    }
}

/*!
    \fn void QStandardItem::insertRow(int row, QStandardItem *item)
    \overload

    Inserts a row at \a row with \a item in its first column.
*/

/*!
    Inserts a column at \a column with \a items as its rows. The number of rows grows to hold them.
*/
void QStandardItem::insertColumn(int column, const QList<QStandardItem *> &items) {
    if (column < 0 || column > m_columns)
        return;
    if (items.size() > m_rows)
        setRowCount(static_cast<int>(items.size()));
    insertColumns(column, 1);
    for (qsizetype i = 0; i < items.size(); ++i)
        if (items.at(i) && !items.at(i)->m_parent)
            adopt(items.at(i), static_cast<int>(i), column);
    if (m_model && !items.isEmpty()) {
        const QModelIndex parent = childParentIndex();
        emit m_model->dataChanged(m_model->index(0, column, parent),
                                  m_model->index(static_cast<int>(items.size()) - 1, column, parent));
    }
}

/*!
    Inserts a row at \a row for each of \a items, which go into the first column.
*/
void QStandardItem::insertRows(int row, const QList<QStandardItem *> &items) {
    if (row < 0 || row > m_rows || items.isEmpty())
        return;
    if (m_columns == 0)
        setColumnCount(1);
    const int count = static_cast<int>(items.size());
    if (m_model)
        m_model->notifyInsertRows(childParentIndex(), row, row + count - 1, true);
    m_children.insert(slot(row, 0), static_cast<qsizetype>(count) * m_columns, nullptr);
    m_rows += count;
    for (int i = 0; i < count; ++i)
        if (items.at(i) && !items.at(i)->m_parent)
            adopt(items.at(i), row + i, 0);
    if (m_model) {
        if (this == m_model->invisibleRootItem() && row < m_model->m_verticalHeader.size())
            m_model->m_verticalHeader.insert(row, count, nullptr);
        m_model->notifyInsertRows(childParentIndex(), row, row + count - 1, false);
    }
}

/*!
    Inserts \a count empty rows at \a row.
*/
void QStandardItem::insertRows(int row, int count) {
    if (row < 0 || row > m_rows || count < 1)
        return;
    if (m_model)
        m_model->notifyInsertRows(childParentIndex(), row, row + count - 1, true);
    m_children.insert(slot(row, 0), static_cast<qsizetype>(count) * m_columns, nullptr);
    m_rows += count;
    for (qsizetype i = 0; i < m_children.size(); ++i) // the positions of the later children changed
        if (m_children.at(i))
            m_children.at(i)->m_lastIndex = static_cast<int>(i);
    if (m_model) {
        if (this == m_model->invisibleRootItem() && row < m_model->m_verticalHeader.size())
            m_model->m_verticalHeader.insert(row, count, nullptr);
        m_model->notifyInsertRows(childParentIndex(), row, row + count - 1, false);
    }
}

/*!
    Inserts \a count empty columns at \a column.
*/
void QStandardItem::insertColumns(int column, int count) {
    if (column < 0 || column > m_columns || count < 1)
        return;
    if (m_model)
        m_model->notifyInsertColumns(childParentIndex(), column, column + count - 1, true);
    for (int r = m_rows - 1; r >= 0; --r)
        m_children.insert(slot(r, column), count, nullptr);
    m_columns += count;
    for (qsizetype i = 0; i < m_children.size(); ++i)
        if (m_children.at(i))
            m_children.at(i)->m_lastIndex = static_cast<int>(i);
    if (m_model) {
        if (this == m_model->invisibleRootItem() && column < m_model->m_horizontalHeader.size())
            m_model->m_horizontalHeader.insert(column, count, nullptr);
        m_model->notifyInsertColumns(childParentIndex(), column, column + count - 1, false);
    }
}

/*!
    \fn void QStandardItem::removeRow(int row)

    Removes the row \a row and deletes its items.
*/

/*!
    \fn void QStandardItem::removeColumn(int column)

    Removes the column \a column and deletes its items.
*/

/*!
    Removes \a count rows from \a row on and deletes their items.
*/
void QStandardItem::removeRows(int row, int count) {
    if (row < 0 || count < 1 || row + count > m_rows)
        return;
    if (m_model)
        m_model->notifyRemoveRows(childParentIndex(), row, row + count - 1, true);
    const qsizetype first = slot(row, 0);
    const qsizetype n = static_cast<qsizetype>(count) * m_columns;
    for (qsizetype i = first; i < first + n; ++i) {
        if (QStandardItem *item = m_children.at(i)) {
            item->m_parent = nullptr;
            delete item;
        }
    }
    m_children.remove(first, n);
    m_rows -= count;
    if (m_model) {
        if (this == m_model->invisibleRootItem() && row < m_model->m_verticalHeader.size()) {
            const qsizetype end = std::min<qsizetype>(row + count, m_model->m_verticalHeader.size());
            for (qsizetype i = row; i < end; ++i)
                delete m_model->m_verticalHeader.at(i);
            m_model->m_verticalHeader.remove(row, end - row);
        }
        m_model->notifyRemoveRows(childParentIndex(), row, row + count - 1, false);
    }
}

/*!
    Removes \a count columns from \a column on and deletes their items.
*/
void QStandardItem::removeColumns(int column, int count) {
    if (column < 0 || count < 1 || column + count > m_columns)
        return;
    if (m_model)
        m_model->notifyRemoveColumns(childParentIndex(), column, column + count - 1, true);
    for (int r = m_rows - 1; r >= 0; --r) {
        const qsizetype first = slot(r, column);
        for (qsizetype i = first; i < first + count; ++i) {
            if (QStandardItem *item = m_children.at(i)) {
                item->m_parent = nullptr;
                delete item;
            }
        }
        m_children.remove(first, count);
    }
    m_columns -= count;
    if (m_model) {
        if (this == m_model->invisibleRootItem() && column < m_model->m_horizontalHeader.size()) {
            const qsizetype end = std::min<qsizetype>(column + count, m_model->m_horizontalHeader.size());
            for (qsizetype i = column; i < end; ++i)
                delete m_model->m_horizontalHeader.at(i);
            m_model->m_horizontalHeader.remove(column, end - column);
        }
        m_model->notifyRemoveColumns(childParentIndex(), column, column + count - 1, false);
    }
}

/*!
    \fn void QStandardItem::appendRow(const QList<QStandardItem *> &items)

    Adds a row at the end with \a items as its columns.
*/

/*!
    \fn void QStandardItem::appendRow(QStandardItem *item)
    \overload

    Adds a row at the end with \a item in its first column.
*/

/*!
    \fn void QStandardItem::appendRows(const QList<QStandardItem *> &items)

    Adds a row at the end for each of \a items.
*/

/*!
    \fn void QStandardItem::appendColumn(const QList<QStandardItem *> &items)

    Adds a column at the end with \a items as its rows.
*/

/*!
    Removes the child at \a row and \a column without deleting it, and returns it; the caller owns it. The row and
    the column stay.
*/
QStandardItem *QStandardItem::takeChild(int row, int column) {
    QStandardItem *item = child(row, column);
    if (!item)
        return nullptr;
    m_children[slot(row, column)] = nullptr;
    item->m_parent = nullptr;
    item->setModel(nullptr);
    if (m_model) {
        const QModelIndex changed = m_model->index(row, column, childParentIndex());
        emit m_model->dataChanged(changed, changed);
    }
    return item;
}

/*!
    Removes the row \a row and returns its items, which the caller owns; an empty column gives a \c nullptr.
*/
QList<QStandardItem *> QStandardItem::takeRow(int row) {
    QList<QStandardItem *> items;
    if (row < 0 || row >= m_rows)
        return items;
    if (m_model)
        m_model->notifyRemoveRows(childParentIndex(), row, row, true);
    const qsizetype first = slot(row, 0);
    for (int c = 0; c < m_columns; ++c) {
        QStandardItem *item = m_children.at(first + c);
        if (item) {
            item->m_parent = nullptr;
            item->setModel(nullptr);
        }
        items.append(item);
    }
    m_children.remove(first, m_columns);
    --m_rows;
    if (m_model) {
        if (this == m_model->invisibleRootItem() && row < m_model->m_verticalHeader.size()) {
            delete m_model->m_verticalHeader.at(row);
            m_model->m_verticalHeader.removeAt(row);
        }
        m_model->notifyRemoveRows(childParentIndex(), row, row, false);
    }
    return items;
}

/*!
    Removes the column \a column and returns its items, which the caller owns; an empty row gives a \c nullptr.
*/
QList<QStandardItem *> QStandardItem::takeColumn(int column) {
    QList<QStandardItem *> items;
    if (column < 0 || column >= m_columns)
        return items;
    if (m_model)
        m_model->notifyRemoveColumns(childParentIndex(), column, column, true);
    for (int r = 0; r < m_rows; ++r) {
        QStandardItem *item = m_children.at(slot(r, column));
        if (item) {
            item->m_parent = nullptr;
            item->setModel(nullptr);
        }
        items.append(item);
    }
    for (int r = m_rows - 1; r >= 0; --r)
        m_children.removeAt(slot(r, column));
    --m_columns;
    if (m_model) {
        if (this == m_model->invisibleRootItem() && column < m_model->m_horizontalHeader.size()) {
            delete m_model->m_horizontalHeader.at(column);
            m_model->m_horizontalHeader.removeAt(column);
        }
        m_model->notifyRemoveColumns(childParentIndex(), column, column, false);
    }
    return items;
}

void QStandardItem::sortRecursive(int column, Qt::SortOrder order, int role) {
    (void)role;
    if (column >= 0 && column < m_columns && m_rows > 1) {
        std::vector<int> rows(static_cast<std::size_t>(m_rows));
        std::iota(rows.begin(), rows.end(), 0);
        // Rows without an item in the column go last, in either order.
        std::stable_sort(rows.begin(), rows.end(), [&](int a, int b) {
            const QStandardItem *x = child(a, column);
            const QStandardItem *y = child(b, column);
            if (!x || !y)
                return x != nullptr;
            return order == Qt::AscendingOrder ? *x < *y : *y < *x;
        });
        QList<QStandardItem *> sorted;
        sorted.reserve(m_children.size());
        for (int r : rows)
            for (int c = 0; c < m_columns; ++c)
                sorted.append(m_children.at(slot(r, c)));
        m_children = sorted;
        for (qsizetype i = 0; i < m_children.size(); ++i)
            if (m_children.at(i))
                m_children.at(i)->m_lastIndex = static_cast<int>(i);
    }
    for (QStandardItem *child : std::as_const(m_children))
        if (child)
            child->sortRecursive(column, order, role);
}

/*!
    Sorts the rows of children, and theirs, by their items in \a column, in \a order, as the less-than operator of
    the items compares them.
*/
void QStandardItem::sortChildren(int column, Qt::SortOrder order) {
    if (!m_model) {
        sortRecursive(column, order, Qt::DisplayRole);
        return;
    }
    emit m_model->layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);
    // Remember the items that the persistent indexes refer to, by item where there is one and by the item of their
    // row otherwise, to point the indexes at them again.
    const QModelIndexList from = m_model->persistentIndexList();
    struct Place {
        QStandardItem *parent;
        QStandardItem *anchor; // an item in the same row
        int column;
    };
    QList<Place> places;
    for (const QModelIndex &index : from) {
        auto *parent = static_cast<QStandardItem *>(index.internalPointer());
        QStandardItem *anchor = nullptr;
        for (int c = 0; c < parent->m_columns && !anchor; ++c)
            anchor = parent->child(index.row(), c);
        places.append(Place{parent, anchor, index.column()});
    }
    sortRecursive(column, order, m_model->sortRole());
    QModelIndexList to;
    for (qsizetype i = 0; i < from.size(); ++i) {
        const Place &p = places.at(i);
        to.append(p.anchor ? m_model->createIndex(p.anchor->row(), p.column, p.parent) : from.at(i));
    }
    m_model->changePersistentIndexList(from, to);
    emit m_model->layoutChanged({}, QAbstractItemModel::VerticalSortHint);
}

/*!
    Returns a new item with the data and the flags of this one, without children. A subclass reimplements it to
    return its own type, for QStandardItemModel::setItemPrototype().
*/
QStandardItem *QStandardItem::clone() const { return new QStandardItem(*this); }

/*!
    Returns the type of the item: Type, or UserType or more for subclasses.
*/
int QStandardItem::type() const { return Type; }

/*!
    Returns \c true if this item sorts before \a other: compares their data for the sort role of the model, numbers
    and dates by value and anything else as text.
*/
bool QStandardItem::operator<(const QStandardItem &other) const {
    const int role = m_model ? m_model->sortRole() : static_cast<int>(Qt::DisplayRole);
    return variantLess(data(role), other.data(role));
}

/*!
    Tells the model that the data of the item changed, for a subclass whose data() changes on its own.
*/
void QStandardItem::emitDataChanged() {
    if (m_model)
        m_model->itemDataChanged(this, {});
}

/*!
    \class QStandardItemModel
    \inmodule NtGui
    \since 6.0
    \brief The QStandardItemModel class is a model of QStandardItem objects.

    The model holds a tree of items, each with data for any role and a table of children. It is the quickest way
    to give a view a model of its own:

    \code
    auto *model = new QStandardItemModel(0, 2, this);
    model->setHorizontalHeaderLabels({"Name", "Size"});
    model->appendRow({new QStandardItem("notes.txt"), new QStandardItem("4 KB")});
    view->setModel(model);
    \endcode

    The items are also reached through the QAbstractItemModel API with indexes; itemFromIndex() and
    indexFromItem() convert between the two. QComboBox keeps its items in a QStandardItemModel, so
    \c{qobject_cast<QStandardItemModel *>(combo->model())->item(1)->setEnabled(false)} disables an entry.

    \differences
    There is no drag and drop of items (\c mimeData(), \c dropMimeData()) and no QDataStream support.
    \enddifferences

    \sa QStandardItem
*/

/*!
    Constructs an empty model with the given \a parent.
*/
QStandardItemModel::QStandardItemModel(QObject *parent)
    : QAbstractItemModel(parent), m_root(std::make_unique<QStandardItem>()) {
    m_root->m_model = this;
    m_root->m_flags = Qt::ItemIsDropEnabled;
}

/*!
    Constructs a model with the given \a parent and a table of \a rows by \a columns empty items.
*/
QStandardItemModel::QStandardItemModel(int rows, int columns, QObject *parent)
    : QAbstractItemModel(parent), m_root(std::make_unique<QStandardItem>(rows, columns)) {
    m_root->m_model = this;
    m_root->m_flags = Qt::ItemIsDropEnabled;
}

/*!
    Destroys the model and its items.
*/
QStandardItemModel::~QStandardItemModel() {
    m_root->setModel(nullptr);
    m_root.reset();
    qDeleteAll(m_horizontalHeader);
    qDeleteAll(m_verticalHeader);
    delete m_prototype;
}

/*!
    Sets the names of the roles to \a roleNames.
*/
void QStandardItemModel::setItemRoleNames(const QHash<int, QByteArray> &roleNames) { m_roleNames = roleNames; }

/*!
    \reimp
*/
QHash<int, QByteArray> QStandardItemModel::roleNames() const {
    return m_roleNames.isEmpty() ? QAbstractItemModel::roleNames() : m_roleNames;
}

/*!
    \reimp
*/
QModelIndex QStandardItemModel::index(int row, int column, const QModelIndex &parent) const {
    const QStandardItem *parentItem = parent.isValid() ? itemFromIndex(parent) : m_root.get();
    if (!parentItem || row < 0 || column < 0 || row >= parentItem->m_rows || column >= parentItem->m_columns)
        return QModelIndex();
    return createIndex(row, column, parentItem);
}

/*!
    \reimp
*/
QModelIndex QStandardItemModel::parent(const QModelIndex &child) const {
    if (!child.isValid() || child.model() != this)
        return QModelIndex();
    const auto *parentItem = static_cast<const QStandardItem *>(child.internalPointer());
    return parentItem == m_root.get() ? QModelIndex() : indexFromItem(parentItem);
}

/*!
    \reimp
*/
QModelIndex QStandardItemModel::sibling(int row, int column, const QModelIndex &idx) const {
    if (!idx.isValid())
        return QModelIndex();
    const auto *parentItem = static_cast<const QStandardItem *>(idx.internalPointer());
    if (row < 0 || column < 0 || row >= parentItem->m_rows || column >= parentItem->m_columns)
        return QModelIndex();
    return createIndex(row, column, parentItem);
}

/*!
    \reimp
*/
int QStandardItemModel::rowCount(const QModelIndex &parent) const {
    const QStandardItem *item = parent.isValid() ? itemFromIndex(parent) : m_root.get();
    return item ? item->m_rows : 0;
}

/*!
    \reimp
*/
int QStandardItemModel::columnCount(const QModelIndex &parent) const {
    const QStandardItem *item = parent.isValid() ? itemFromIndex(parent) : m_root.get();
    return item ? item->m_columns : 0;
}

/*!
    \reimp
*/
bool QStandardItemModel::hasChildren(const QModelIndex &parent) const {
    const QStandardItem *item = parent.isValid() ? itemFromIndex(parent) : m_root.get();
    return item && item->hasChildren();
}

/*!
    \reimp
*/
QVariant QStandardItemModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this)
        return QVariant();
    const auto *parentItem = static_cast<const QStandardItem *>(index.internalPointer());
    const QStandardItem *item = parentItem->child(index.row(), index.column());
    return item ? item->data(role) : QVariant();
}

/*!
    \reimp
*/
bool QStandardItemModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    QStandardItem *item = ensureItem(index);
    if (!item)
        return false;
    item->setData(value, role);
    return true;
}

/*!
    \reimp
*/
bool QStandardItemModel::clearItemData(const QModelIndex &index) {
    QStandardItem *item = ensureItem(index);
    if (!item)
        return false;
    item->clearData();
    return true;
}

/*!
    \reimp

    Returns the data of the header item of \a section for \a role, or the number of the section, counted from 1,
    for Qt::DisplayRole if there is no item.
*/
QVariant QStandardItemModel::headerData(int section, Qt::Orientation orientation, int role) const {
    const int count = orientation == Qt::Horizontal ? columnCount() : rowCount();
    if (section < 0 || section >= count)
        return QVariant();
    const QStandardItem *item =
        orientation == Qt::Horizontal ? horizontalHeaderItem(section) : verticalHeaderItem(section);
    if (item)
        return item->data(role);
    if (role == Qt::DisplayRole)
        return QString::number(section + 1);
    return QAbstractItemModel::headerData(section, orientation, role);
}

/*!
    \reimp
*/
bool QStandardItemModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role) {
    const int count = orientation == Qt::Horizontal ? columnCount() : rowCount();
    if (section < 0 || section >= count)
        return false;
    QStandardItem *item = orientation == Qt::Horizontal ? horizontalHeaderItem(section) : verticalHeaderItem(section);
    if (!item) {
        item = newItem();
        if (orientation == Qt::Horizontal)
            setHorizontalHeaderItem(section, item);
        else
            setVerticalHeaderItem(section, item);
    }
    item->setData(value, role);
    return true;
}

/*!
    \reimp
*/
QMap<int, QVariant> QStandardItemModel::itemData(const QModelIndex &index) const {
    const QStandardItem *item =
        index.isValid() && index.model() == this
            ? static_cast<const QStandardItem *>(index.internalPointer())->child(index.row(), index.column())
            : nullptr;
    return item ? item->m_values : QMap<int, QVariant>();
}

/*!
    \reimp
*/
bool QStandardItemModel::setItemData(const QModelIndex &index, const QMap<int, QVariant> &roles) {
    QStandardItem *item = ensureItem(index);
    if (!item)
        return false;
    QList<int> changed;
    for (auto it = roles.constBegin(); it != roles.constEnd(); ++it) {
        const int key = it.key() == Qt::EditRole ? static_cast<int>(Qt::DisplayRole) : it.key();
        if (it.value().isValid())
            item->m_values.insert(key, it.value());
        else
            item->m_values.remove(key);
        changed.append(it.key());
    }
    itemDataChanged(item, changed);
    return true;
}

/*!
    \reimp
*/
bool QStandardItemModel::insertRows(int row, int count, const QModelIndex &parent) {
    QStandardItem *item = parent.isValid() ? ensureItem(parent) : m_root.get();
    if (!item || row < 0 || row > item->m_rows || count < 1)
        return false;
    item->insertRows(row, count);
    return true;
}

/*!
    \reimp
*/
bool QStandardItemModel::insertColumns(int column, int count, const QModelIndex &parent) {
    QStandardItem *item = parent.isValid() ? ensureItem(parent) : m_root.get();
    if (!item || column < 0 || column > item->m_columns || count < 1)
        return false;
    item->insertColumns(column, count);
    return true;
}

/*!
    \reimp
*/
bool QStandardItemModel::removeRows(int row, int count, const QModelIndex &parent) {
    QStandardItem *item = parent.isValid() ? itemFromIndex(parent) : m_root.get();
    if (!item || row < 0 || count < 1 || row + count > item->m_rows)
        return false;
    item->removeRows(row, count);
    return true;
}

/*!
    \reimp
*/
bool QStandardItemModel::removeColumns(int column, int count, const QModelIndex &parent) {
    QStandardItem *item = parent.isValid() ? itemFromIndex(parent) : m_root.get();
    if (!item || column < 0 || count < 1 || column + count > item->m_columns)
        return false;
    item->removeColumns(column, count);
    return true;
}

/*!
    \reimp
*/
Qt::ItemFlags QStandardItemModel::flags(const QModelIndex &index) const {
    if (!index.isValid())
        return m_root->flags();
    const QStandardItem *item =
        static_cast<const QStandardItem *>(index.internalPointer())->child(index.row(), index.column());
    if (item)
        return item->flags();
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDragEnabled |
           Qt::ItemIsDropEnabled;
}

/*!
    \reimp
*/
Qt::DropActions QStandardItemModel::supportedDropActions() const { return Qt::CopyAction | Qt::MoveAction; }

/*!
    \reimp

    Sorts all rows of the model, at every level, by \a column in \a order, comparing the data for sortRole().
*/
void QStandardItemModel::sort(int column, Qt::SortOrder order) { m_root->sortChildren(column, order); }

/*!
    Deletes all items and header items, and leaves no rows and no columns.
*/
void QStandardItemModel::clear() {
    beginResetModel();
    m_root->setModel(nullptr);
    m_root = std::make_unique<QStandardItem>();
    m_root->m_model = this;
    m_root->m_flags = Qt::ItemIsDropEnabled;
    qDeleteAll(m_horizontalHeader);
    qDeleteAll(m_verticalHeader);
    m_horizontalHeader.clear();
    m_verticalHeader.clear();
    endResetModel();
}

/*!
    Returns the item of \a index, or \c nullptr for an invalid index. An empty place of the table gets an item,
    made from the itemPrototype().
*/
QStandardItem *QStandardItemModel::itemFromIndex(const QModelIndex &index) const {
    if (!index.isValid() || index.model() != this)
        return nullptr;
    auto *parentItem = static_cast<QStandardItem *>(index.internalPointer());
    if (index.row() >= parentItem->m_rows || index.column() >= parentItem->m_columns)
        return nullptr;
    QStandardItem *item = parentItem->child(index.row(), index.column());
    if (!item) {
        item = newItem();
        parentItem->adopt(item, index.row(), index.column());
    }
    return item;
}

QStandardItem *QStandardItemModel::ensureItem(const QModelIndex &index) const { return itemFromIndex(index); }

/*!
    Returns the index of \a item, or an invalid index if it is not in this model.
*/
QModelIndex QStandardItemModel::indexFromItem(const QStandardItem *item) const {
    if (!item || item->m_model != this || !item->m_parent)
        return QModelIndex();
    const int row = item->row();
    return row < 0 ? QModelIndex() : createIndex(row, item->column(), item->m_parent);
}

/*!
    Returns the top-level item at \a row and \a column, or \c nullptr if there is none.
*/
QStandardItem *QStandardItemModel::item(int row, int column) const { return m_root->child(row, column); }

/*!
    Sets the top-level item at \a row and \a column to \a item, and deletes the item that was there. The model grows
    to hold it, and takes ownership of \a item.
*/
void QStandardItemModel::setItem(int row, int column, QStandardItem *item) { m_root->setChild(row, column, item); }

/*!
    \fn void QStandardItemModel::setItem(int row, QStandardItem *item)
    \overload

    Sets the top-level item at \a row in the first column to \a item.
*/

/*!
    \fn QStandardItem *QStandardItemModel::invisibleRootItem() const

    Returns the item that the top-level items are children of. It has no index, and the model owns it.
*/

/*!
    Returns the header item of \a column, or \c nullptr if it has none.
*/
QStandardItem *QStandardItemModel::horizontalHeaderItem(int column) const {
    return column >= 0 && column < m_horizontalHeader.size() ? m_horizontalHeader.at(column) : nullptr;
}

/*!
    Sets the header item of \a column to \a item, and deletes the one it had. The model grows to have the column,
    and takes ownership of \a item.
*/
void QStandardItemModel::setHorizontalHeaderItem(int column, QStandardItem *item) {
    if (column < 0 || (item && (item->m_parent || item->m_model)))
        return;
    if (column >= columnCount())
        setColumnCount(column + 1);
    if (column >= m_horizontalHeader.size())
        m_horizontalHeader.resize(column + 1, nullptr);
    if (m_horizontalHeader.at(column) == item)
        return;
    delete m_horizontalHeader.at(column);
    m_horizontalHeader[column] = item;
    if (item)
        item->m_model = this;
    emit headerDataChanged(Qt::Horizontal, column, column);
}

/*!
    Returns the header item of \a row, or \c nullptr if it has none.
*/
QStandardItem *QStandardItemModel::verticalHeaderItem(int row) const {
    return row >= 0 && row < m_verticalHeader.size() ? m_verticalHeader.at(row) : nullptr;
}

/*!
    Sets the header item of \a row to \a item, and deletes the one it had. The model grows to have the row, and
    takes ownership of \a item.
*/
void QStandardItemModel::setVerticalHeaderItem(int row, QStandardItem *item) {
    if (row < 0 || (item && (item->m_parent || item->m_model)))
        return;
    if (row >= rowCount())
        setRowCount(row + 1);
    if (row >= m_verticalHeader.size())
        m_verticalHeader.resize(row + 1, nullptr);
    if (m_verticalHeader.at(row) == item)
        return;
    delete m_verticalHeader.at(row);
    m_verticalHeader[row] = item;
    if (item)
        item->m_model = this;
    emit headerDataChanged(Qt::Vertical, row, row);
}

/*!
    Sets the texts of the column headers to \a labels, and adds columns if there are more labels than columns.
*/
void QStandardItemModel::setHorizontalHeaderLabels(const QStringList &labels) {
    if (labels.size() > columnCount())
        setColumnCount(static_cast<int>(labels.size()));
    for (int i = 0; i < labels.size(); ++i) {
        QStandardItem *item = horizontalHeaderItem(i);
        if (!item) {
            item = newItem();
            setHorizontalHeaderItem(i, item);
        }
        item->setText(labels.at(i));
    }
}

/*!
    Sets the texts of the row headers to \a labels, and adds rows if there are more labels than rows.
*/
void QStandardItemModel::setVerticalHeaderLabels(const QStringList &labels) {
    if (labels.size() > rowCount())
        setRowCount(static_cast<int>(labels.size()));
    for (int i = 0; i < labels.size(); ++i) {
        QStandardItem *item = verticalHeaderItem(i);
        if (!item) {
            item = newItem();
            setVerticalHeaderItem(i, item);
        }
        item->setText(labels.at(i));
    }
}

/*!
    \fn void QStandardItemModel::setRowCount(int rows)

    Sets the number of top-level rows to \a rows.
*/

/*!
    \fn void QStandardItemModel::setColumnCount(int columns)

    Sets the number of top-level columns to \a columns.
*/

/*!
    \fn void QStandardItemModel::appendRow(const QList<QStandardItem *> &items)

    Adds a top-level row at the end with \a items as its columns.
*/

/*!
    \fn void QStandardItemModel::appendRow(QStandardItem *item)
    \overload

    Adds a top-level row at the end with \a item in its first column.
*/

/*!
    \fn void QStandardItemModel::appendColumn(const QList<QStandardItem *> &items)

    Adds a top-level column at the end with \a items as its rows.
*/

/*!
    \fn void QStandardItemModel::insertRow(int row, const QList<QStandardItem *> &items)

    Inserts a top-level row at \a row with \a items as its columns.
*/

/*!
    \fn void QStandardItemModel::insertRow(int row, QStandardItem *item)
    \overload

    Inserts a top-level row at \a row with \a item in its first column.
*/

/*!
    \fn void QStandardItemModel::insertColumn(int column, const QList<QStandardItem *> &items)

    Inserts a top-level column at \a column with \a items as its rows.
*/

/*!
    \fn bool QStandardItemModel::insertRow(int row, const QModelIndex &parent)
    \overload

    Inserts an empty row at \a row under \a parent. Returns \c true on success.
*/

/*!
    \fn bool QStandardItemModel::insertColumn(int column, const QModelIndex &parent)
    \overload

    Inserts an empty column at \a column under \a parent. Returns \c true on success.
*/

/*!
    \fn QStandardItem *QStandardItemModel::takeItem(int row, int column)

    Removes the top-level item at \a row and \a column without deleting it, and returns it.
*/

/*!
    \fn QList<QStandardItem *> QStandardItemModel::takeRow(int row)

    Removes the top-level row \a row and returns its items.
*/

/*!
    \fn QList<QStandardItem *> QStandardItemModel::takeColumn(int column)

    Removes the top-level column \a column and returns its items.
*/

/*!
    Removes the header item of \a column without deleting it, and returns it; the caller owns it.
*/
QStandardItem *QStandardItemModel::takeHorizontalHeaderItem(int column) {
    QStandardItem *item = horizontalHeaderItem(column);
    if (!item)
        return nullptr;
    m_horizontalHeader[column] = nullptr;
    item->m_model = nullptr;
    emit headerDataChanged(Qt::Horizontal, column, column);
    return item;
}

/*!
    Removes the header item of \a row without deleting it, and returns it; the caller owns it.
*/
QStandardItem *QStandardItemModel::takeVerticalHeaderItem(int row) {
    QStandardItem *item = verticalHeaderItem(row);
    if (!item)
        return nullptr;
    m_verticalHeader[row] = nullptr;
    item->m_model = nullptr;
    emit headerDataChanged(Qt::Vertical, row, row);
    return item;
}

/*!
    \fn const QStandardItem *QStandardItemModel::itemPrototype() const

    Returns the item that new items are cloned from, or \c nullptr if they are plain QStandardItem objects.
*/

/*!
    Sets the item that the model clones for the items it makes itself, such as for setData() on an empty place, to
    \a item. The model takes ownership of it.
*/
void QStandardItemModel::setItemPrototype(const QStandardItem *item) {
    if (item == m_prototype)
        return;
    delete m_prototype;
    m_prototype = item;
}

QStandardItem *QStandardItemModel::newItem() const { return m_prototype ? m_prototype->clone() : new QStandardItem; }

/*!
    Returns the items in \a column whose text matches \a text as \a flags say; with Qt::MatchRecursive the
    children are searched too.
*/
QList<QStandardItem *> QStandardItemModel::findItems(const QString &text, Qt::MatchFlags flags, int column) const {
    QList<QStandardItem *> found;
    std::function<void(const QStandardItem *)> search = [&](const QStandardItem *parent) {
        for (int r = 0; r < parent->m_rows; ++r) {
            QStandardItem *item = parent->child(r, column);
            if (item && textMatches(item->text(), text, flags))
                found.append(item);
            if (flags & Qt::MatchRecursive)
                for (int c = 0; c < parent->m_columns; ++c)
                    if (const QStandardItem *child = parent->child(r, c))
                        search(child);
        }
    };
    search(m_root.get());
    return found;
}

/*!
    \fn int QStandardItemModel::sortRole() const

    Returns the role whose data sort() compares, Qt::DisplayRole by default.
*/

/*!
    \fn void QStandardItemModel::setSortRole(int role)

    Sets the role whose data sort() compares to \a role.
*/

void QStandardItemModel::itemDataChanged(QStandardItem *item, const QList<int> &roles) {
    if (item == m_root.get())
        return;
    if (!item->m_parent) {
        headerChanged(item);
        return;
    }
    const QModelIndex index = indexFromItem(item);
    emit dataChanged(index, index, roles);
    emit itemChanged(item);
}

void QStandardItemModel::headerChanged(QStandardItem *item) {
    const qsizetype column = m_horizontalHeader.indexOf(item);
    if (column >= 0) {
        emit headerDataChanged(Qt::Horizontal, static_cast<int>(column), static_cast<int>(column));
        return;
    }
    const qsizetype row = m_verticalHeader.indexOf(item);
    if (row >= 0)
        emit headerDataChanged(Qt::Vertical, static_cast<int>(row), static_cast<int>(row));
}

#include "moc_qstandarditemmodel.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
