// MIT License - see LICENSE
#include <QtCore/qsortfilterproxymodel.h>

#include <QtCore/qcollator.h>
#include <QtCore/qdatetime.h>

#include <algorithm>

namespace {
bool isNumber(int type) {
    switch (type) {
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::Char:
    case QMetaType::SChar:
    case QMetaType::UChar:
    case QMetaType::Double:
    case QMetaType::Float:
    case QMetaType::Bool:
        return true;
    default:
        return false;
    }
}
} // namespace

/*!
    Constructs a sorting and filtering proxy with the given \a parent. It
    does not filter, and sorts only after sort() is called.
*/
QSortFilterProxyModel::QSortFilterProxyModel(QObject *parent) : QAbstractProxyModel(parent) {}

/*!
    Destroys the proxy.
*/
QSortFilterProxyModel::~QSortFilterProxyModel() {
    for (const QMetaObject::Connection &c : m_connections)
        QObject::disconnect(c);
}

/*!
    \reimp
    Sets the source model to \a sourceModel and resets the proxy.
*/
void QSortFilterProxyModel::setSourceModel(QAbstractItemModel *sourceModel) {
    beginResetModel();
    for (const QMetaObject::Connection &c : m_connections)
        QObject::disconnect(c);
    m_connections.clear();
    m_mappings.clear();
    m_layoutDepth = 0;
    m_saved.clear();
    QAbstractProxyModel::setSourceModel(sourceModel);
    if (QAbstractItemModel *s = sourceModel) {
        using M = QAbstractItemModel;
        auto before = [this] { beginLayout(); };
        auto after = [this] { endLayout(); };
        m_connections << connect(s, &M::rowsAboutToBeInserted, this, before)
                      << connect(s, &M::rowsInserted, this, after);
        m_connections << connect(s, &M::rowsAboutToBeRemoved, this, before) << connect(s, &M::rowsRemoved, this, after);
        m_connections << connect(s, &M::rowsAboutToBeMoved, this, before) << connect(s, &M::rowsMoved, this, after);
        m_connections << connect(s, &M::columnsAboutToBeInserted, this, before)
                      << connect(s, &M::columnsInserted, this, after);
        m_connections << connect(s, &M::columnsAboutToBeRemoved, this, before)
                      << connect(s, &M::columnsRemoved, this, after);
        m_connections << connect(s, &M::columnsAboutToBeMoved, this, before)
                      << connect(s, &M::columnsMoved, this, after);
        m_connections << connect(s, &M::layoutAboutToBeChanged, this, before)
                      << connect(s, &M::layoutChanged, this, after);
        m_connections << connect(s, &M::modelAboutToBeReset, this, [this] {
            beginResetModel();
            m_mappings.clear();
        });
        m_connections << connect(s, &M::modelReset, this, [this] {
            m_mappings.clear();
            endResetModel();
        });
        m_connections << connect(
            s, &M::dataChanged, this, [this](const QModelIndex &tl, const QModelIndex &br, const QList<int> &roles) {
                if (m_dynamic) {
                    relayout();
                    return;
                }
                for (int row = tl.row(); row <= br.row(); ++row) {
                    const QModelIndex first = mapFromSource(this->sourceModel()->index(row, tl.column(), tl.parent()));
                    const QModelIndex last = mapFromSource(this->sourceModel()->index(row, br.column(), tl.parent()));
                    if (first.isValid() && last.isValid())
                        dataChanged(first, last, roles);
                }
            });
        m_connections << connect(s, &M::headerDataChanged, this, [this](Qt::Orientation o, int, int) {
            const int count = o == Qt::Horizontal ? columnCount() : rowCount();
            if (count > 0)
                headerDataChanged(o, 0, count - 1);
        });
    }
    endResetModel();
}

void QSortFilterProxyModel::beginLayout() {
    if (m_layoutDepth++ > 0)
        return;
    layoutAboutToBeChanged();
    m_saved.clear();
    for (const QModelIndex &proxy : persistentIndexList())
        m_saved.append({proxy, mapToSource(proxy)});
}

void QSortFilterProxyModel::endLayout() {
    if (m_layoutDepth == 0)
        return;
    --m_layoutDepth;
    if (m_layoutDepth > 0)
        return;
    m_mappings.clear();
    QModelIndexList from, to;
    for (const auto &entry : m_saved) {
        from.append(entry.first);
        to.append(mapFromSource(entry.second));
    }
    m_saved.clear();
    changePersistentIndexList(from, to);
    layoutChanged();
}

void QSortFilterProxyModel::relayout() {
    beginLayout();
    endLayout();
}

bool QSortFilterProxyModel::hasAcceptedDescendant(const QModelIndex &sourceParent) const {
    const QAbstractItemModel *s = sourceModel();
    for (int row = 0; row < s->rowCount(sourceParent); ++row) {
        if (filterAcceptsRow(row, sourceParent))
            return true;
        const QModelIndex child = s->index(row, 0, sourceParent);
        if (s->hasChildren(child) && hasAcceptedDescendant(child))
            return true;
    }
    return false;
}

bool QSortFilterProxyModel::acceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
    if (filterAcceptsRow(sourceRow, sourceParent))
        return true;
    if (m_autoAcceptChildren) {
        // Accepted if an ancestor is accepted on its own.
        for (QModelIndex p = sourceParent; p.isValid(); p = p.parent())
            if (filterAcceptsRow(p.row(), p.parent()))
                return true;
    }
    if (m_recursive) {
        const QModelIndex child = sourceModel()->index(sourceRow, 0, sourceParent);
        return sourceModel()->hasChildren(child) && hasAcceptedDescendant(child);
    }
    return false;
}

QSortFilterProxyModel::Mapping *QSortFilterProxyModel::mappingFor(const QModelIndex &sourceParent) const {
    auto it = m_mappings.find(sourceParent);
    if (it != m_mappings.end())
        return it->second.get();
    auto m = std::make_unique<Mapping>();
    m->sourceParent = sourceParent;
    const QAbstractItemModel *s = sourceModel();
    const int rows = s ? s->rowCount(sourceParent) : 0;
    const int columns = s ? s->columnCount(sourceParent) : 0;
    m->proxyColumns.assign(static_cast<std::size_t>(columns), -1);
    for (int c = 0; c < columns; ++c) {
        if (filterAcceptsColumn(c, sourceParent)) {
            m->proxyColumns[static_cast<std::size_t>(c)] = static_cast<int>(m->sourceColumns.size());
            m->sourceColumns.push_back(c);
        }
    }
    for (int r = 0; r < rows; ++r)
        if (acceptsRow(r, sourceParent))
            m->sourceRows.push_back(r);
    if (m_sortColumn >= 0 && m_sortColumn < static_cast<int>(m->sourceColumns.size())) {
        const int column = m->sourceColumns[static_cast<std::size_t>(m_sortColumn)];
        std::stable_sort(m->sourceRows.begin(), m->sourceRows.end(), [&](int a, int b) {
            const QModelIndex left = s->index(a, column, sourceParent), right = s->index(b, column, sourceParent);
            return m_sortOrder == Qt::AscendingOrder ? lessThan(left, right) : lessThan(right, left);
        });
    }
    m->proxyRows.assign(static_cast<std::size_t>(rows), -1);
    for (std::size_t k = 0; k < m->sourceRows.size(); ++k)
        m->proxyRows[static_cast<std::size_t>(m->sourceRows[k])] = static_cast<int>(k);
    Mapping *raw = m.get();
    m_mappings.emplace(sourceParent, std::move(m));
    return raw;
}

/*!
    \reimp
    Returns the source index of \a proxyIndex.
*/
QModelIndex QSortFilterProxyModel::mapToSource(const QModelIndex &proxyIndex) const {
    if (!sourceModel() || !proxyIndex.isValid() || proxyIndex.model() != this)
        return QModelIndex();
    const auto *m = static_cast<const Mapping *>(proxyIndex.constInternalPointer());
    if (proxyIndex.row() >= static_cast<int>(m->sourceRows.size()) ||
        proxyIndex.column() >= static_cast<int>(m->sourceColumns.size()))
        return QModelIndex();
    return sourceModel()->index(m->sourceRows[static_cast<std::size_t>(proxyIndex.row())],
                                m->sourceColumns[static_cast<std::size_t>(proxyIndex.column())], m->sourceParent);
}

/*!
    \reimp
    Returns the proxy index of \a sourceIndex, or an invalid index if the
    item is filtered out.
*/
QModelIndex QSortFilterProxyModel::mapFromSource(const QModelIndex &sourceIndex) const {
    if (!sourceModel() || !sourceIndex.isValid() || sourceIndex.model() != sourceModel())
        return QModelIndex();
    const QModelIndex sourceParent = sourceIndex.parent();
    if (sourceParent.isValid() && !mapFromSource(sourceParent).isValid())
        return QModelIndex(); // the parent is filtered out
    Mapping *m = mappingFor(sourceParent);
    const auto r = static_cast<std::size_t>(sourceIndex.row()), c = static_cast<std::size_t>(sourceIndex.column());
    if (r >= m->proxyRows.size() || c >= m->proxyColumns.size() || m->proxyRows[r] < 0 || m->proxyColumns[c] < 0)
        return QModelIndex();
    return createIndex(m->proxyRows[r], m->proxyColumns[c], m);
}

/*!
    Returns the regular expression used to filter rows.

    \sa setFilterRegularExpression()
*/
QRegularExpression QSortFilterProxyModel::filterRegularExpression() const { return m_filter; }

/*!
    \since 5.12
    Sets the filter to the regular expression \a regularExpression and
    filters again. Its case-insensitivity option sets filterCaseSensitivity().
*/
void QSortFilterProxyModel::setFilterRegularExpression(const QRegularExpression &regularExpression) {
    const bool csChanged = regularExpression.patternOptions().testFlag(QRegularExpression::CaseInsensitiveOption) !=
                           m_filter.patternOptions().testFlag(QRegularExpression::CaseInsensitiveOption);
    m_filter = regularExpression;
    invalidateFilter();
    if (csChanged)
        filterCaseSensitivityChanged(filterCaseSensitivity());
}

/*!
    \overload
    \since 5.12
    Sets the filter to a regular expression with the pattern \a pattern,
    keeping the case sensitivity.
*/
void QSortFilterProxyModel::setFilterRegularExpression(const QString &pattern) {
    QRegularExpression re(pattern, m_filter.patternOptions());
    m_filter = re;
    invalidateFilter();
}

/*!
    Sets the filter to the shell wildcard \a pattern, which may match
    anywhere in the text.
*/
void QSortFilterProxyModel::setFilterWildcard(const QString &pattern) {
    setFilterRegularExpression(
        QRegularExpression::wildcardToRegularExpression(pattern, QRegularExpression::UnanchoredWildcardConversion));
}

/*!
    Sets the filter to the fixed string \a pattern, which matches anywhere in
    the text.
*/
void QSortFilterProxyModel::setFilterFixedString(const QString &pattern) {
    setFilterRegularExpression(QRegularExpression::escape(pattern));
}

/*!
    Returns the source column the filter tests, or -1 for all columns. The
    default is 0.

    \sa setFilterKeyColumn()
*/
int QSortFilterProxyModel::filterKeyColumn() const { return m_filterKeyColumn; }

/*!
    Sets the source column the filter tests to \a column; -1 tests all
    columns, accepting a row if any column matches.
*/
void QSortFilterProxyModel::setFilterKeyColumn(int column) {
    m_filterKeyColumn = column;
    invalidateFilter();
}

/*!
    Returns the case sensitivity of the filter.
*/
Qt::CaseSensitivity QSortFilterProxyModel::filterCaseSensitivity() const {
    return m_filter.patternOptions().testFlag(QRegularExpression::CaseInsensitiveOption) ? Qt::CaseInsensitive
                                                                                         : Qt::CaseSensitive;
}

/*!
    Sets the case sensitivity of the filter to \a cs.
*/
void QSortFilterProxyModel::setFilterCaseSensitivity(Qt::CaseSensitivity cs) {
    if (cs == filterCaseSensitivity())
        return;
    QRegularExpression::PatternOptions o = m_filter.patternOptions();
    if (cs == Qt::CaseInsensitive)
        o |= QRegularExpression::CaseInsensitiveOption;
    else
        o &= ~QRegularExpression::PatternOptions(QRegularExpression::CaseInsensitiveOption);
    m_filter.setPatternOptions(o);
    invalidateFilter();
    filterCaseSensitivityChanged(cs);
}

/*!
    Returns the case sensitivity of sorting.
*/
Qt::CaseSensitivity QSortFilterProxyModel::sortCaseSensitivity() const { return m_sortCs; }

/*!
    Sets the case sensitivity of sorting to \a cs.
*/
void QSortFilterProxyModel::setSortCaseSensitivity(Qt::CaseSensitivity cs) {
    if (m_sortCs == cs)
        return;
    m_sortCs = cs;
    relayout();
    sortCaseSensitivityChanged(cs);
}

/*!
    Returns \c true if strings are sorted with QCollator; otherwise returns
    \c false.
*/
bool QSortFilterProxyModel::isSortLocaleAware() const { return m_localeAware; }

/*!
    Sets whether strings are sorted with QCollator for the default locale to
    \a on.
*/
void QSortFilterProxyModel::setSortLocaleAware(bool on) {
    if (m_localeAware == on)
        return;
    m_localeAware = on;
    relayout();
    sortLocaleAwareChanged(on);
}

/*!
    \since 4.5
    Returns the proxy column used for sorting, or -1 if the rows keep the
    source order.
*/
int QSortFilterProxyModel::sortColumn() const { return m_sortColumn; }

/*!
    \since 4.5
    Returns the sort order.
*/
Qt::SortOrder QSortFilterProxyModel::sortOrder() const { return m_sortOrder; }

/*!
    Returns \c true if the proxy sorts and filters again when the source
    data changes; otherwise returns \c false. The default is \c true.
*/
bool QSortFilterProxyModel::dynamicSortFilter() const { return m_dynamic; }

/*!
    Sets whether the proxy sorts and filters again when the source data
    changes to \a enable.
*/
void QSortFilterProxyModel::setDynamicSortFilter(bool enable) {
    if (m_dynamic == enable)
        return;
    m_dynamic = enable;
    if (enable)
        relayout();
    dynamicSortFilterChanged(enable);
}

/*!
    Returns the role whose data is compared when sorting; the default is
    Qt::DisplayRole.
*/
int QSortFilterProxyModel::sortRole() const { return m_sortRole; }

/*!
    Sets the role whose data is compared when sorting to \a role.
*/
void QSortFilterProxyModel::setSortRole(int role) {
    if (m_sortRole == role)
        return;
    m_sortRole = role;
    relayout();
    sortRoleChanged(role);
}

/*!
    Returns the role whose data the filter tests; the default is
    Qt::DisplayRole.
*/
int QSortFilterProxyModel::filterRole() const { return m_filterRole; }

/*!
    Sets the role whose data the filter tests to \a role.
*/
void QSortFilterProxyModel::setFilterRole(int role) {
    if (m_filterRole == role)
        return;
    m_filterRole = role;
    invalidateFilter();
    filterRoleChanged(role);
}

/*!
    \since 5.10
    Returns \c true if a row is also accepted when one of its descendants is;
    otherwise returns \c false.
*/
bool QSortFilterProxyModel::isRecursiveFilteringEnabled() const { return m_recursive; }

/*!
    \since 5.10
    Sets whether a row is also accepted when one of its descendants is to
    \a recursive.
*/
void QSortFilterProxyModel::setRecursiveFilteringEnabled(bool recursive) {
    if (m_recursive == recursive)
        return;
    m_recursive = recursive;
    invalidateFilter();
    recursiveFilteringEnabledChanged(recursive);
}

/*!
    \since 6.0
    Returns \c true if the children of an accepted row are accepted too;
    otherwise returns \c false.
*/
bool QSortFilterProxyModel::autoAcceptChildRows() const { return m_autoAcceptChildren; }

/*!
    \since 6.0
    Sets whether the children of an accepted row are accepted too to
    \a accept.
*/
void QSortFilterProxyModel::setAutoAcceptChildRows(bool accept) {
    if (m_autoAcceptChildren == accept)
        return;
    m_autoAcceptChildren = accept;
    invalidateFilter();
    autoAcceptChildRowsChanged(accept);
}

/*!
    \reimp
    Returns the proxy index for \a row and \a column under \a parent.
*/
QModelIndex QSortFilterProxyModel::index(int row, int column, const QModelIndex &parent) const {
    if (!sourceModel() || row < 0 || column < 0)
        return QModelIndex();
    const QModelIndex sourceParent = mapToSource(parent);
    if (parent.isValid() && !sourceParent.isValid())
        return QModelIndex();
    Mapping *m = mappingFor(sourceParent);
    if (row >= static_cast<int>(m->sourceRows.size()) || column >= static_cast<int>(m->sourceColumns.size()))
        return QModelIndex();
    return createIndex(row, column, m);
}

/*!
    \reimp
    Returns the parent of \a child.
*/
QModelIndex QSortFilterProxyModel::parent(const QModelIndex &child) const {
    if (!child.isValid() || child.model() != this)
        return QModelIndex();
    const auto *m = static_cast<const Mapping *>(child.constInternalPointer());
    return m->sourceParent.isValid() ? mapFromSource(m->sourceParent) : QModelIndex();
}

/*!
    \reimp
    Returns the index at \a row and \a column next to \a idx.
*/
QModelIndex QSortFilterProxyModel::sibling(int row, int column, const QModelIndex &idx) const {
    if (!idx.isValid() || idx.model() != this)
        return QModelIndex();
    auto *m = static_cast<Mapping *>(idx.internalPointer());
    if (row < 0 || column < 0 || row >= static_cast<int>(m->sourceRows.size()) ||
        column >= static_cast<int>(m->sourceColumns.size()))
        return QModelIndex();
    return createIndex(row, column, m);
}

/*!
    \reimp
    Returns the number of accepted rows under \a parent.
*/
int QSortFilterProxyModel::rowCount(const QModelIndex &parent) const {
    if (!sourceModel())
        return 0;
    const QModelIndex sourceParent = mapToSource(parent);
    if (parent.isValid() && !sourceParent.isValid())
        return 0;
    return static_cast<int>(mappingFor(sourceParent)->sourceRows.size());
}

/*!
    \reimp
    Returns the number of accepted columns under \a parent.
*/
int QSortFilterProxyModel::columnCount(const QModelIndex &parent) const {
    if (!sourceModel())
        return 0;
    const QModelIndex sourceParent = mapToSource(parent);
    if (parent.isValid() && !sourceParent.isValid())
        return 0;
    return static_cast<int>(mappingFor(sourceParent)->sourceColumns.size());
}

/*!
    \reimp
    Returns \c true if \a parent has accepted child rows and columns.
*/
bool QSortFilterProxyModel::hasChildren(const QModelIndex &parent) const {
    if (!sourceModel())
        return false;
    const QModelIndex sourceParent = mapToSource(parent);
    if (parent.isValid() && !sourceParent.isValid())
        return false;
    if (!sourceModel()->hasChildren(sourceParent))
        return false;
    if (sourceModel()->canFetchMore(sourceParent))
        return true;
    const Mapping *m = mappingFor(sourceParent);
    return !m->sourceRows.empty() && !m->sourceColumns.empty();
}

/*!
    \reimp
    Inserts \a count rows into the source before the source row shown at
    \a row under \a parent (at the end of the source when \a row is
    rowCount()).
*/
bool QSortFilterProxyModel::insertRows(int row, int count, const QModelIndex &parent) {
    if (!sourceModel() || row < 0 || row > rowCount(parent) || count <= 0)
        return false;
    const QModelIndex sourceParent = mapToSource(parent);
    const Mapping *m = mappingFor(sourceParent);
    const int sourceRow = row >= static_cast<int>(m->sourceRows.size()) ? sourceModel()->rowCount(sourceParent)
                                                                        : m->sourceRows[static_cast<std::size_t>(row)];
    return sourceModel()->insertRows(sourceRow, count, sourceParent);
}

/*!
    \reimp
    Inserts \a count columns into the source before the source column shown
    at \a column under \a parent.
*/
bool QSortFilterProxyModel::insertColumns(int column, int count, const QModelIndex &parent) {
    if (!sourceModel() || column < 0 || column > columnCount(parent) || count <= 0)
        return false;
    const QModelIndex sourceParent = mapToSource(parent);
    const Mapping *m = mappingFor(sourceParent);
    const int sourceColumn = column >= static_cast<int>(m->sourceColumns.size())
                                 ? sourceModel()->columnCount(sourceParent)
                                 : m->sourceColumns[static_cast<std::size_t>(column)];
    return sourceModel()->insertColumns(sourceColumn, count, sourceParent);
}

/*!
    \reimp
    Removes the source rows shown at \a row to \a row + \a count - 1 under
    \a parent.
*/
bool QSortFilterProxyModel::removeRows(int row, int count, const QModelIndex &parent) {
    if (!sourceModel() || row < 0 || count <= 0 || row + count > rowCount(parent))
        return false;
    const QModelIndex sourceParent = mapToSource(parent);
    const Mapping *m = mappingFor(sourceParent);
    std::vector<int> rows(m->sourceRows.begin() + row, m->sourceRows.begin() + row + count);
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    bool ok = true;
    // Remove runs of consecutive source rows from the bottom up.
    std::size_t k = 0;
    while (k < rows.size()) {
        std::size_t e = k;
        while (e + 1 < rows.size() && rows[e + 1] == rows[e] - 1)
            ++e;
        ok = sourceModel()->removeRows(rows[e], static_cast<int>(e - k + 1), sourceParent) && ok;
        k = e + 1;
    }
    return ok;
}

/*!
    \reimp
    Removes the source columns shown at \a column to \a column + \a count - 1
    under \a parent.
*/
bool QSortFilterProxyModel::removeColumns(int column, int count, const QModelIndex &parent) {
    if (!sourceModel() || column < 0 || count <= 0 || column + count > columnCount(parent))
        return false;
    const QModelIndex sourceParent = mapToSource(parent);
    const Mapping *m = mappingFor(sourceParent);
    std::vector<int> columns(m->sourceColumns.begin() + column, m->sourceColumns.begin() + column + count);
    std::sort(columns.begin(), columns.end(), std::greater<int>());
    bool ok = true;
    for (int c : columns)
        ok = sourceModel()->removeColumns(c, 1, sourceParent) && ok;
    return ok;
}

/*!
    \reimp
    Sorts the rows by the proxy column \a column in \a order. A \a column of
    -1 restores the order of the source.
*/
void QSortFilterProxyModel::sort(int column, Qt::SortOrder order) {
    if (m_sortColumn == column && m_sortOrder == order && column >= 0)
        return;
    m_sortColumn = column;
    m_sortOrder = order;
    relayout();
}

/*!
    Sorts and filters the rows again.

    \sa invalidateFilter()
*/
void QSortFilterProxyModel::invalidate() { relayout(); }

/*!
    Filters the rows and columns again, for subclasses whose filter changed.
*/
void QSortFilterProxyModel::invalidateFilter() { relayout(); }

/*!
    \since 6.0
    Filters the rows again.
*/
void QSortFilterProxyModel::invalidateRowsFilter() { relayout(); }

/*!
    \since 6.0
    Filters the columns again.
*/
void QSortFilterProxyModel::invalidateColumnsFilter() { relayout(); }

/*!
    Returns \c true if the source row \a source_row under \a source_parent
    is shown. The default accepts the row if the filter is empty or matches
    the data of filterKeyColumn() (any column if it is -1) for filterRole().
*/
bool QSortFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    if (m_filter.pattern().isEmpty())
        return true;
    const QAbstractItemModel *s = sourceModel();
    if (m_filterKeyColumn == -1) {
        for (int c = 0; c < s->columnCount(source_parent); ++c)
            if (m_filter.match(s->index(source_row, c, source_parent).data(m_filterRole).toString()).hasMatch())
                return true;
        return false;
    }
    if (m_filterKeyColumn >= s->columnCount(source_parent))
        return true;
    return m_filter.match(s->index(source_row, m_filterKeyColumn, source_parent).data(m_filterRole).toString())
        .hasMatch();
}

/*!
    Returns \c true if the source column \a source_column under
    \a source_parent is shown. The default accepts all columns.
*/
bool QSortFilterProxyModel::filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const {
    Q_UNUSED(source_column)
    Q_UNUSED(source_parent)
    return true;
}

/*!
    Returns \c true if the source item \a source_left sorts before
    \a source_right. The default compares the data of sortRole(): numbers by
    value, dates and times chronologically, and everything else as strings,
    with sortCaseSensitivity() or, if isSortLocaleAware(), with QCollator.
*/
bool QSortFilterProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const {
    const QVariant l = source_left.data(m_sortRole), r = source_right.data(m_sortRole);
    if (!l.isValid() || !r.isValid())
        return !l.isValid() && r.isValid(); // invalid data sorts first
    const int lt = l.typeId(), rt = r.typeId();
    if (isNumber(lt) && isNumber(rt)) {
        if (lt == QMetaType::Double || lt == QMetaType::Float || rt == QMetaType::Double || rt == QMetaType::Float)
            return l.toDouble() < r.toDouble();
        if (lt == QMetaType::ULongLong || rt == QMetaType::ULongLong)
            return l.toULongLong() < r.toULongLong();
        return l.toLongLong() < r.toLongLong();
    }
    if (lt == QMetaType::QDateTime && rt == QMetaType::QDateTime)
        return l.toDateTime() < r.toDateTime();
    if (lt == QMetaType::QDate && rt == QMetaType::QDate)
        return l.toDate() < r.toDate();
    if (lt == QMetaType::QTime && rt == QMetaType::QTime)
        return l.toTime() < r.toTime();
    const QString ls = l.toString(), rs = r.toString();
    if (m_localeAware) {
        QCollator collator;
        collator.setCaseSensitivity(m_sortCs);
        return collator.compare(ls, rs) < 0;
    }
    return ls.compare(rs, m_sortCs) < 0;
}

#include "moc_qsortfilterproxymodel.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
