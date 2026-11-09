// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractproxymodel.h>
#include <QtCore/qregularexpression.h>

#include <map>
#include <memory>
#include <vector>

/*!
    \class QSortFilterProxyModel
    \brief The QSortFilterProxyModel class provides support for sorting and filtering data passed between another model and a view.

    A QSortFilterProxyModel presents the rows of its source model sorted
    and filtered, without changing the source. Rows are filtered with a
    regular expression on the filterKeyColumn() and sorted by a column with
    sort(); subclasses customize both by reimplementing filterAcceptsRow()
    and lessThan().

    \code
    QSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.setFilterFixedString(u"an"_s);
    proxy.setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy.sort(0);
    \endcode

    With dynamicSortFilter() (the default), the proxy sorts and filters again
    when the source changes.

    \differences
    Changes to the source and to the filter are announced as layout changes
    (layoutAboutToBeChanged() and layoutChanged()), with persistent indexes
    updated, instead of the finer row insertion and removal signals of Qt.
    The \c beginFilterChange() and \c endFilterChange() functions and the
    bindable properties are not provided.
    \enddifferences

    \sa QAbstractProxyModel, QIdentityProxyModel
*/
class QSortFilterProxyModel : public QAbstractProxyModel {
    Q_OBJECT
public:
    explicit QSortFilterProxyModel(QObject *parent = nullptr);
    ~QSortFilterProxyModel() override;

    void setSourceModel(QAbstractItemModel *sourceModel) override;
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;

    QRegularExpression filterRegularExpression() const;
    int filterKeyColumn() const;
    void setFilterKeyColumn(int column);
    Qt::CaseSensitivity filterCaseSensitivity() const;
    void setFilterCaseSensitivity(Qt::CaseSensitivity cs);
    Qt::CaseSensitivity sortCaseSensitivity() const;
    void setSortCaseSensitivity(Qt::CaseSensitivity cs);
    bool isSortLocaleAware() const;
    void setSortLocaleAware(bool on);
    int sortColumn() const;
    Qt::SortOrder sortOrder() const;
    bool dynamicSortFilter() const;
    void setDynamicSortFilter(bool enable);
    int sortRole() const;
    void setSortRole(int role);
    int filterRole() const;
    void setFilterRole(int role);
    bool isRecursiveFilteringEnabled() const;
    void setRecursiveFilteringEnabled(bool recursive);
    bool autoAcceptChildRows() const;
    void setAutoAcceptChildRows(bool accept);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    QModelIndex sibling(int row, int column, const QModelIndex &idx) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool insertColumns(int column, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeColumns(int column, int count, const QModelIndex &parent = QModelIndex()) override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

public slots:
    void setFilterRegularExpression(const QString &pattern);
    void setFilterRegularExpression(const QRegularExpression &regularExpression);
    void setFilterWildcard(const QString &pattern);
    void setFilterFixedString(const QString &pattern);
    void invalidate();

signals:
    /*!
        \since 6.0
        This signal is emitted when dynamicSortFilter() changes to
        \a dynamicSortFilter.
    */
    void dynamicSortFilterChanged(bool dynamicSortFilter) {
        emitSignal(&QSortFilterProxyModel::dynamicSortFilterChanged, dynamicSortFilter);
    }
    /*!
        \since 6.0
        This signal is emitted when the filter case sensitivity changes to
        \a filterCaseSensitivity.
    */
    void filterCaseSensitivityChanged(Qt::CaseSensitivity filterCaseSensitivity) {
        emitSignal(&QSortFilterProxyModel::filterCaseSensitivityChanged, filterCaseSensitivity);
    }
    /*!
        \since 6.0
        This signal is emitted when the sort case sensitivity changes to
        \a sortCaseSensitivity.
    */
    void sortCaseSensitivityChanged(Qt::CaseSensitivity sortCaseSensitivity) {
        emitSignal(&QSortFilterProxyModel::sortCaseSensitivityChanged, sortCaseSensitivity);
    }
    /*!
        \since 6.0
        This signal is emitted when locale-aware sorting is switched to
        \a sortLocaleAware.
    */
    void sortLocaleAwareChanged(bool sortLocaleAware) {
        emitSignal(&QSortFilterProxyModel::sortLocaleAwareChanged, sortLocaleAware);
    }
    /*!
        \since 6.0
        This signal is emitted when the sort role changes to \a sortRole.
    */
    void sortRoleChanged(int sortRole) { emitSignal(&QSortFilterProxyModel::sortRoleChanged, sortRole); }
    /*!
        \since 6.0
        This signal is emitted when the filter role changes to \a filterRole.
    */
    void filterRoleChanged(int filterRole) { emitSignal(&QSortFilterProxyModel::filterRoleChanged, filterRole); }
    /*!
        \since 6.0
        This signal is emitted when recursive filtering is switched to
        \a recursiveFilteringEnabled.
    */
    void recursiveFilteringEnabledChanged(bool recursiveFilteringEnabled) {
        emitSignal(&QSortFilterProxyModel::recursiveFilteringEnabledChanged, recursiveFilteringEnabled);
    }
    /*!
        \since 6.0
        This signal is emitted when the automatic acceptance of child rows is
        switched to \a autoAcceptChildRows.
    */
    void autoAcceptChildRowsChanged(bool autoAcceptChildRows) {
        emitSignal(&QSortFilterProxyModel::autoAcceptChildRowsChanged, autoAcceptChildRows);
    }

protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;
    virtual bool filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const;
    virtual bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const;
    void invalidateFilter();
    void invalidateRowsFilter();
    void invalidateColumnsFilter();

private:
    struct Mapping {
        QModelIndex sourceParent;
        std::vector<int> sourceRows, proxyRows;       // proxy row -> source row, source row -> proxy row or -1
        std::vector<int> sourceColumns, proxyColumns; // the same for columns
    };
    Mapping *mappingFor(const QModelIndex &sourceParent) const;
    bool acceptsRow(int sourceRow, const QModelIndex &sourceParent) const;
    bool hasAcceptedDescendant(const QModelIndex &sourceParent) const;
    void beginLayout();
    void endLayout();
    void relayout();

    QRegularExpression m_filter;
    int m_filterKeyColumn = 0;
    int m_filterRole = Qt::DisplayRole;
    int m_sortRole = Qt::DisplayRole;
    int m_sortColumn = -1;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
    Qt::CaseSensitivity m_sortCs = Qt::CaseSensitive;
    bool m_localeAware = false;
    bool m_dynamic = true;
    bool m_recursive = false;
    bool m_autoAcceptChildren = false;
    int m_layoutDepth = 0;
    mutable std::map<QModelIndex, std::unique_ptr<Mapping>> m_mappings;
    QList<std::pair<QPersistentModelIndex, QPersistentModelIndex>> m_saved;
    QList<QMetaObject::Connection> m_connections;
};
