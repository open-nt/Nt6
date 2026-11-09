// MIT License - see LICENSE
#include <QtCore/qitemselectionmodel.h>

#include <algorithm>
#include <map>
#include <set>
#include <vector>

namespace {

bool selectable(const QModelIndex &index) {
    const Qt::ItemFlags f = index.flags();
    return f.testFlag(Qt::ItemIsSelectable) && f.testFlag(Qt::ItemIsEnabled);
}

// All cells of a selection, ignoring whether they can be selected.
std::set<QModelIndex> cells(const QItemSelection &selection) {
    std::set<QModelIndex> r;
    for (const QItemSelectionRange &range : selection) {
        if (!range.isValid())
            continue;
        const QModelIndex parent = range.parent();
        const QAbstractItemModel *model = range.model();
        for (int row = range.top(); row <= range.bottom(); ++row)
            for (int column = range.left(); column <= range.right(); ++column)
                r.insert(model->index(row, column, parent));
    }
    return r;
}

// Turns cells into ranges: runs of columns in each row, merged with the rows below that have the same run.
QItemSelection toSelection(const std::set<QModelIndex> &set) {
    struct Run {
        QModelIndex parent;
        const QAbstractItemModel *model;
        int row, left, right;
    };
    std::map<std::pair<QModelIndex, int>, std::vector<int>> byRow; // (parent, row) -> columns
    std::map<QModelIndex, const QAbstractItemModel *> models;
    for (const QModelIndex &index : set) {
        if (!index.isValid())
            continue;
        byRow[{index.parent(), index.row()}].push_back(index.column());
        models[index.parent()] = index.model();
    }
    std::vector<Run> runs;
    for (auto &entry : byRow) {
        std::vector<int> &cols = entry.second;
        std::sort(cols.begin(), cols.end());
        std::size_t k = 0;
        while (k < cols.size()) {
            std::size_t e = k;
            while (e + 1 < cols.size() && cols[e + 1] == cols[e] + 1)
                ++e;
            runs.push_back({entry.first.first, models[entry.first.first], entry.first.second, cols[k], cols[e]});
            k = e + 1;
        }
    }
    QItemSelection r;
    std::vector<bool> used(runs.size(), false);
    for (std::size_t i = 0; i < runs.size(); ++i) {
        if (used[i])
            continue;
        used[i] = true;
        int bottom = runs[i].row;
        // Runs are sorted by parent and row, so the continuation is further down the list.
        for (std::size_t j = i + 1; j < runs.size(); ++j) {
            if (used[j] || runs[j].parent != runs[i].parent)
                continue;
            if (runs[j].row == bottom + 1 && runs[j].left == runs[i].left && runs[j].right == runs[i].right) {
                used[j] = true;
                ++bottom;
            } else if (runs[j].row > bottom + 1) {
                break;
            }
        }
        const QAbstractItemModel *m = runs[i].model;
        r.append(QItemSelectionRange(m->index(runs[i].row, runs[i].left, runs[i].parent),
                                     m->index(bottom, runs[i].right, runs[i].parent)));
    }
    return r;
}

std::set<QModelIndex> setUnion(const std::set<QModelIndex> &a, const std::set<QModelIndex> &b) {
    std::set<QModelIndex> r = a;
    r.insert(b.begin(), b.end());
    return r;
}
std::set<QModelIndex> setMinus(const std::set<QModelIndex> &a, const std::set<QModelIndex> &b) {
    std::set<QModelIndex> r;
    std::set_difference(a.begin(), a.end(), b.begin(), b.end(), std::inserter(r, r.end()));
    return r;
}
std::set<QModelIndex> setXor(const std::set<QModelIndex> &a, const std::set<QModelIndex> &b) {
    std::set<QModelIndex> r;
    std::set_symmetric_difference(a.begin(), a.end(), b.begin(), b.end(), std::inserter(r, r.end()));
    return r;
}

// Applies the selection command (Select, Deselect or Toggle) of current to base.
std::set<QModelIndex> combine(const std::set<QModelIndex> &base, const std::set<QModelIndex> &current, int command) {
    if (command & QItemSelectionModel::Toggle)
        return setXor(base, current);
    if (command & QItemSelectionModel::Deselect)
        return setMinus(base, current);
    if (command & QItemSelectionModel::Select)
        return setUnion(base, current);
    return base;
}

} // namespace

// ------------------------------------------------------------------ QItemSelectionRange

/*!
    Returns \c true if this range and \a other share an item; otherwise
    returns \c false.
*/
bool QItemSelectionRange::intersects(const QItemSelectionRange &other) const {
    return isValid() && other.isValid() && parent() == other.parent() && model() == other.model() &&
           top() <= other.bottom() && bottom() >= other.top() && left() <= other.right() && right() >= other.left();
}

/*!
    \since 4.2
    Returns the range of the items that this range and \a other share, or an
    empty range.
*/
QItemSelectionRange QItemSelectionRange::intersected(const QItemSelectionRange &other) const {
    if (!intersects(other))
        return QItemSelectionRange();
    const QModelIndex p = parent();
    const QAbstractItemModel *m = model();
    return QItemSelectionRange(m->index(std::max(top(), other.top()), std::max(left(), other.left()), p),
                               m->index(std::min(bottom(), other.bottom()), std::min(right(), other.right()), p));
}

/*!
    Returns \c true if the range is invalid or none of its items can be
    selected; otherwise returns \c false.
*/
bool QItemSelectionRange::isEmpty() const { return indexes().isEmpty(); }

/*!
    Returns the items of the range that are selectable and enabled.
*/
QModelIndexList QItemSelectionRange::indexes() const {
    QModelIndexList r;
    if (!isValid())
        return r;
    const QModelIndex p = parent();
    for (int row = top(); row <= bottom(); ++row) {
        for (int column = left(); column <= right(); ++column) {
            const QModelIndex index = model()->index(row, column, p);
            if (selectable(index))
                r.append(index);
        }
    }
    return r;
}

// ------------------------------------------------------------------ QItemSelection

/*!
    Constructs a selection of the range from \a topLeft to \a bottomRight.
*/
QItemSelection::QItemSelection(const QModelIndex &topLeft, const QModelIndex &bottomRight) {
    select(topLeft, bottomRight);
}

/*!
    Adds the range from \a topLeft to \a bottomRight, which must have the same
    parent and model, to the selection.
*/
void QItemSelection::select(const QModelIndex &topLeft, const QModelIndex &bottomRight) {
    if (!topLeft.isValid() || !bottomRight.isValid() || topLeft.parent() != bottomRight.parent() ||
        topLeft.model() != bottomRight.model())
        return;
    const QAbstractItemModel *m = topLeft.model();
    const QModelIndex p = topLeft.parent();
    append(QItemSelectionRange(
        m->index(std::min(topLeft.row(), bottomRight.row()), std::min(topLeft.column(), bottomRight.column()), p),
        m->index(std::max(topLeft.row(), bottomRight.row()), std::max(topLeft.column(), bottomRight.column()), p)));
}

/*!
    Returns \c true if \a index is selectable, enabled and inside one of the
    ranges; otherwise returns \c false.
*/
bool QItemSelection::contains(const QModelIndex &index) const {
    if (!selectable(index))
        return false;
    return std::any_of(begin(), end(), [&](const QItemSelectionRange &r) { return r.contains(index); });
}

/*!
    Returns the selectable items of the selection, each once.
*/
QModelIndexList QItemSelection::indexes() const {
    QModelIndexList r;
    std::set<QModelIndex> seen;
    for (const QItemSelectionRange &range : *this)
        for (const QModelIndex &index : range.indexes())
            if (seen.insert(index).second)
                r.append(index);
    return r;
}

/*!
    Merges \a other into this selection according to \a command, a
    combination of QItemSelectionModel::SelectionFlag values: Select adds its
    items, Deselect removes them and Toggle flips them.
*/
void QItemSelection::merge(const QItemSelection &other, int command) {
    if (other.isEmpty() ||
        !(command & (QItemSelectionModel::Select | QItemSelectionModel::Deselect | QItemSelectionModel::Toggle)))
        return;
    *this = toSelection(combine(cells(*this), cells(other), command));
}

/*!
    Splits \a range by \a other and appends to \a result the ranges that hold
    the items of \a range that are not in \a other.
*/
void QItemSelection::split(const QItemSelectionRange &range, const QItemSelectionRange &other, QItemSelection *result) {
    if (!result)
        return;
    QItemSelection a;
    a.append(range);
    QItemSelection b;
    b.append(other);
    result->append(toSelection(setMinus(cells(a), cells(b))));
}

// ------------------------------------------------------------------ QItemSelectionModel

/*!
    Constructs a selection model for \a model.
*/
QItemSelectionModel::QItemSelectionModel(QAbstractItemModel *model) { setModel(model); }

/*!
    Constructs a selection model for \a model with the given \a parent.
*/
QItemSelectionModel::QItemSelectionModel(QAbstractItemModel *model, QObject *parent) : QObject(parent) {
    setModel(model);
}

/*!
    Destroys the selection model.
*/
QItemSelectionModel::~QItemSelectionModel() {
    for (const QMetaObject::Connection &c : m_connections)
        QObject::disconnect(c);
}

/*!
    \since 5.5
    Sets the model to \a model, clearing the selection and the current item
    without emitting signals, and emits modelChanged().

    \sa model()
*/
void QItemSelectionModel::setModel(QAbstractItemModel *model) {
    if (m_model == model)
        return;
    for (const QMetaObject::Connection &c : m_connections)
        QObject::disconnect(c);
    m_connections.clear();
    m_model = model;
    m_ranges.clear();
    m_current.clear();
    m_currentIndex = QPersistentModelIndex();
    if (model) {
        auto saved = std::make_shared<QList<QPersistentModelIndex>>();
        m_connections << connect(model, &QAbstractItemModel::modelReset, this, [this] { reset(); });
        m_connections << connect(model, &QAbstractItemModel::layoutAboutToBeChanged, this, [this, saved] {
            finalize();
            saved->clear();
            for (const QModelIndex &index : cells(m_ranges))
                saved->append(QPersistentModelIndex(index));
        });
        m_connections << connect(model, &QAbstractItemModel::layoutChanged, this, [this, saved] {
            std::set<QModelIndex> set;
            for (const QPersistentModelIndex &p : *saved)
                if (p.isValid())
                    set.insert(p);
            m_ranges = toSelection(set);
            saved->clear();
        });
    }
    modelChanged(model);
}

/*!
    Returns the current item, or an invalid index.

    \sa setCurrentIndex()
*/
QModelIndex QItemSelectionModel::currentIndex() const { return m_currentIndex; }

/*!
    Returns \c true if \a index is selected; otherwise returns \c false.
*/
bool QItemSelectionModel::isSelected(const QModelIndex &index) const {
    if (!m_model || index.model() != m_model)
        return false;
    return selection().contains(index);
}

/*!
    Returns \c true if all selectable items of \a row under \a parent are
    selected; otherwise returns \c false.
*/
bool QItemSelectionModel::isRowSelected(int row, const QModelIndex &parent) const {
    if (!m_model)
        return false;
    const QItemSelection sel = selection();
    bool any = false;
    for (int column = 0; column < m_model->columnCount(parent); ++column) {
        const QModelIndex index = m_model->index(row, column, parent);
        if (!selectable(index))
            continue;
        if (!sel.contains(index))
            return false;
        any = true;
    }
    return any;
}

/*!
    Returns \c true if all selectable items of \a column under \a parent are
    selected; otherwise returns \c false.
*/
bool QItemSelectionModel::isColumnSelected(int column, const QModelIndex &parent) const {
    if (!m_model)
        return false;
    const QItemSelection sel = selection();
    bool any = false;
    for (int row = 0; row < m_model->rowCount(parent); ++row) {
        const QModelIndex index = m_model->index(row, column, parent);
        if (!selectable(index))
            continue;
        if (!sel.contains(index))
            return false;
        any = true;
    }
    return any;
}

/*!
    Returns \c true if any item of \a row under \a parent is selected;
    otherwise returns \c false.
*/
bool QItemSelectionModel::rowIntersectsSelection(int row, const QModelIndex &parent) const {
    if (!m_model)
        return false;
    const QItemSelection sel = selection();
    for (int column = 0; column < m_model->columnCount(parent); ++column)
        if (sel.contains(m_model->index(row, column, parent)))
            return true;
    return false;
}

/*!
    Returns \c true if any item of \a column under \a parent is selected;
    otherwise returns \c false.
*/
bool QItemSelectionModel::columnIntersectsSelection(int column, const QModelIndex &parent) const {
    if (!m_model)
        return false;
    const QItemSelection sel = selection();
    for (int row = 0; row < m_model->rowCount(parent); ++row)
        if (sel.contains(m_model->index(row, column, parent)))
            return true;
    return false;
}

/*!
    Returns \c true if any item is selected; otherwise returns \c false.
*/
bool QItemSelectionModel::hasSelection() const { return !selection().indexes().isEmpty(); }

/*!
    Returns the selected items.
*/
QModelIndexList QItemSelectionModel::selectedIndexes() const { return selection().indexes(); }

/*!
    Returns the items in \a column of the rows whose selectable items are all
    selected.
*/
QModelIndexList QItemSelectionModel::selectedRows(int column) const {
    QModelIndexList r;
    std::set<std::pair<QModelIndex, int>> seen;
    for (const QModelIndex &index : selectedIndexes()) {
        const std::pair<QModelIndex, int> key{index.parent(), index.row()};
        if (seen.count(key))
            continue;
        seen.insert(key);
        if (isRowSelected(index.row(), index.parent()))
            r.append(m_model->index(index.row(), column, index.parent()));
    }
    return r;
}

/*!
    Returns the items in \a row of the columns whose selectable items are all
    selected.
*/
QModelIndexList QItemSelectionModel::selectedColumns(int row) const {
    QModelIndexList r;
    std::set<std::pair<QModelIndex, int>> seen;
    for (const QModelIndex &index : selectedIndexes()) {
        const std::pair<QModelIndex, int> key{index.parent(), index.column()};
        if (seen.count(key))
            continue;
        seen.insert(key);
        if (isColumnSelected(index.column(), index.parent()))
            r.append(m_model->index(row, index.column(), index.parent()));
    }
    return r;
}

/*!
    Returns the selection: the committed ranges combined with the current
    selection.
*/
const QItemSelection QItemSelectionModel::selection() const {
    QItemSelection valid;
    for (const QItemSelectionRange &r : m_ranges)
        if (r.isValid())
            valid.append(r);
    if (m_current.isEmpty())
        return valid;
    return toSelection(combine(cells(valid), cells(m_current), m_currentCommand.toInt()));
}

/*!
    Returns the model.
*/
QAbstractItemModel *QItemSelectionModel::model() { return m_model; }

/*!
    \overload
    Returns the model.
*/
const QAbstractItemModel *QItemSelectionModel::model() const { return m_model; }

/*!
    Makes \a index the current item and changes the selection with
    \a command, then emits currentChanged() and, as appropriate,
    currentRowChanged() and currentColumnChanged().
*/
void QItemSelectionModel::setCurrentIndex(const QModelIndex &index, SelectionFlags command) {
    if (!m_model || (index.isValid() && index.model() != m_model))
        return;
    if (index == m_currentIndex) {
        if (command != NoUpdate)
            select(index, command);
        return;
    }
    const QModelIndex previous = m_currentIndex;
    m_currentIndex = index;
    if (command != NoUpdate)
        select(index, command);
    currentChanged(index, previous);
    if (index.row() != previous.row() || index.parent() != previous.parent())
        currentRowChanged(index, previous);
    if (index.column() != previous.column() || index.parent() != previous.parent())
        currentColumnChanged(index, previous);
}

/*!
    Changes the selection of the item \a index with \a command.
*/
void QItemSelectionModel::select(const QModelIndex &index, SelectionFlags command) {
    QItemSelection sel;
    if (index.isValid())
        sel.append(QItemSelectionRange(index));
    select(sel, command);
}

QItemSelection QItemSelectionModel::expanded(const QItemSelection &selection, SelectionFlags command) const {
    if (!m_model || !(command & (Rows | Columns)))
        return selection;
    QItemSelection r;
    for (const QItemSelectionRange &range : selection) {
        if (!range.isValid())
            continue;
        const QModelIndex p = range.parent();
        const int top = command & Columns ? 0 : range.top();
        const int bottom = command & Columns ? m_model->rowCount(p) - 1 : range.bottom();
        const int left = command & Rows ? 0 : range.left();
        const int right = command & Rows ? m_model->columnCount(p) - 1 : range.right();
        r.append(QItemSelectionRange(m_model->index(top, left, p), m_model->index(bottom, right, p)));
    }
    return r;
}

void QItemSelectionModel::finalize() {
    if (m_current.isEmpty())
        return;
    m_ranges = selection();
    m_current.clear();
}

/*!
    Changes the selection with \a selection and \a command: Clear empties it
    first, Select, Deselect and Toggle change the items of \a selection, and
    Rows and Columns extend them to whole rows or columns. With Current, the
    change replaces the previous current change instead of being committed.
    Emits selectionChanged() if the selection changed.
*/
void QItemSelectionModel::select(const QItemSelection &selection, SelectionFlags command) {
    if (!m_model || command == NoUpdate)
        return;
    const QItemSelection sel = expanded(selection, command);
    const QItemSelection old = this->selection();
    if (command & Clear) {
        m_ranges.clear();
        m_current.clear();
    }
    if (!(command & Current))
        finalize();
    if (command & (Toggle | Select | Deselect)) {
        m_currentCommand = command;
        m_current = sel;
    }
    emitSelectionChanged(this->selection(), old);
}

/*!
    Clears the selection and the current item, emitting the signals.
*/
void QItemSelectionModel::clear() {
    clearSelection();
    clearCurrentIndex();
}

/*!
    Clears the selection and the current item without emitting signals.
*/
void QItemSelectionModel::reset() {
    const bool blocked = blockSignals(true);
    clear();
    blockSignals(blocked);
}

/*!
    \since 5.0
    Clears the selection and emits selectionChanged().
*/
void QItemSelectionModel::clearSelection() {
    const QItemSelection old = selection();
    m_ranges.clear();
    m_current.clear();
    emitSelectionChanged(QItemSelection(), old);
}

/*!
    \since 4.4
    Clears the current item and emits currentChanged().
*/
void QItemSelectionModel::clearCurrentIndex() {
    const QModelIndex previous = m_currentIndex;
    m_currentIndex = QPersistentModelIndex();
    if (previous.isValid()) {
        currentChanged(QModelIndex(), previous);
        currentRowChanged(QModelIndex(), previous);
        currentColumnChanged(QModelIndex(), previous);
    }
}

/*!
    Emits selectionChanged() with the difference between \a newSelection and
    \a oldSelection, unless they hold the same items.
*/
void QItemSelectionModel::emitSelectionChanged(const QItemSelection &newSelection, const QItemSelection &oldSelection) {
    const std::set<QModelIndex> now = cells(newSelection), before = cells(oldSelection);
    const std::set<QModelIndex> added = setMinus(now, before), removed = setMinus(before, now);
    if (added.empty() && removed.empty())
        return;
    selectionChanged(toSelection(added), toSelection(removed));
}

#include "moc_qitemselectionmodel.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
