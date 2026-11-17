// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>
#include <QtGui/qbrush.h>
#include <QtGui/qfont.h>
#include <QtGui/qicon.h>

#include <memory>

class QStandardItemModel;

class QStandardItem {
public:
    enum ItemType { Type = 0, UserType = 1000 };

    QStandardItem();
    explicit QStandardItem(const QString &text);
    QStandardItem(const QIcon &icon, const QString &text);
    explicit QStandardItem(int rows, int columns = 1);
    virtual ~QStandardItem();

    virtual QVariant data(int role = Qt::UserRole + 1) const;
    virtual void setData(const QVariant &value, int role = Qt::UserRole + 1);
    void clearData();

    QString text() const { return data(Qt::DisplayRole).toString(); }
    void setText(const QString &text) { setData(text, Qt::DisplayRole); }
    QIcon icon() const { return data(Qt::DecorationRole).value<QIcon>(); }
    void setIcon(const QIcon &icon) { setData(QVariant::fromValue(icon), Qt::DecorationRole); }
    QString toolTip() const { return data(Qt::ToolTipRole).toString(); }
    void setToolTip(const QString &toolTip) { setData(toolTip, Qt::ToolTipRole); }
    QString statusTip() const { return data(Qt::StatusTipRole).toString(); }
    void setStatusTip(const QString &statusTip) { setData(statusTip, Qt::StatusTipRole); }
    QString whatsThis() const { return data(Qt::WhatsThisRole).toString(); }
    void setWhatsThis(const QString &whatsThis) { setData(whatsThis, Qt::WhatsThisRole); }
    QSize sizeHint() const { return data(Qt::SizeHintRole).toSize(); }
    void setSizeHint(const QSize &size) { setData(size, Qt::SizeHintRole); }
    QFont font() const { return data(Qt::FontRole).value<QFont>(); }
    void setFont(const QFont &font) { setData(QVariant::fromValue(font), Qt::FontRole); }
    Qt::Alignment textAlignment() const { return Qt::Alignment::fromInt(data(Qt::TextAlignmentRole).toInt()); }
    void setTextAlignment(Qt::Alignment alignment) { setData(static_cast<int>(alignment), Qt::TextAlignmentRole); }
    QBrush background() const { return data(Qt::BackgroundRole).value<QBrush>(); }
    void setBackground(const QBrush &brush) { setData(QVariant::fromValue(brush), Qt::BackgroundRole); }
    QBrush foreground() const { return data(Qt::ForegroundRole).value<QBrush>(); }
    void setForeground(const QBrush &brush) { setData(QVariant::fromValue(brush), Qt::ForegroundRole); }
    Qt::CheckState checkState() const { return static_cast<Qt::CheckState>(data(Qt::CheckStateRole).toInt()); }
    void setCheckState(Qt::CheckState state) { setData(static_cast<int>(state), Qt::CheckStateRole); }
    QString accessibleText() const { return data(Qt::AccessibleTextRole).toString(); }
    void setAccessibleText(const QString &text) { setData(text, Qt::AccessibleTextRole); }
    QString accessibleDescription() const { return data(Qt::AccessibleDescriptionRole).toString(); }
    void setAccessibleDescription(const QString &text) { setData(text, Qt::AccessibleDescriptionRole); }

    Qt::ItemFlags flags() const { return m_flags; }
    void setFlags(Qt::ItemFlags flags);
    bool isEnabled() const { return m_flags & Qt::ItemIsEnabled; }
    void setEnabled(bool enabled);
    bool isEditable() const { return m_flags & Qt::ItemIsEditable; }
    void setEditable(bool editable);
    bool isSelectable() const { return m_flags & Qt::ItemIsSelectable; }
    void setSelectable(bool selectable);
    bool isCheckable() const { return m_flags & Qt::ItemIsUserCheckable; }
    void setCheckable(bool checkable);
    bool isAutoTristate() const { return m_flags & Qt::ItemIsAutoTristate; }
    void setAutoTristate(bool tristate);
    bool isUserTristate() const { return m_flags & Qt::ItemIsUserTristate; }
    void setUserTristate(bool tristate);
    bool isDragEnabled() const { return m_flags & Qt::ItemIsDragEnabled; }
    void setDragEnabled(bool dragEnabled);
    bool isDropEnabled() const { return m_flags & Qt::ItemIsDropEnabled; }
    void setDropEnabled(bool dropEnabled);

    QStandardItem *parent() const;
    int row() const;
    int column() const;
    QModelIndex index() const;
    QStandardItemModel *model() const { return m_model; }

    int rowCount() const { return m_rows; }
    void setRowCount(int rows);
    int columnCount() const { return m_columns; }
    void setColumnCount(int columns);
    bool hasChildren() const { return m_rows > 0 && m_columns > 0; }
    QStandardItem *child(int row, int column = 0) const;
    void setChild(int row, int column, QStandardItem *item);
    void setChild(int row, QStandardItem *item) { setChild(row, 0, item); }

    void insertRow(int row, const QList<QStandardItem *> &items);
    void insertRow(int row, QStandardItem *item) { insertRow(row, QList<QStandardItem *>{item}); }
    void insertColumn(int column, const QList<QStandardItem *> &items);
    void insertRows(int row, const QList<QStandardItem *> &items);
    void insertRows(int row, int count);
    void insertColumns(int column, int count);
    void removeRow(int row) { removeRows(row, 1); }
    void removeColumn(int column) { removeColumns(column, 1); }
    void removeRows(int row, int count);
    void removeColumns(int column, int count);
    void appendRow(const QList<QStandardItem *> &items) { insertRow(m_rows, items); }
    void appendRow(QStandardItem *item) { insertRow(m_rows, item); }
    void appendRows(const QList<QStandardItem *> &items) { insertRows(m_rows, items); }
    void appendColumn(const QList<QStandardItem *> &items) { insertColumn(m_columns, items); }

    QStandardItem *takeChild(int row, int column = 0);
    QList<QStandardItem *> takeRow(int row);
    QList<QStandardItem *> takeColumn(int column);

    void sortChildren(int column, Qt::SortOrder order = Qt::AscendingOrder);

    virtual QStandardItem *clone() const;
    virtual int type() const;
    virtual bool operator<(const QStandardItem &other) const;

protected:
    QStandardItem(const QStandardItem &other);
    QStandardItem &operator=(const QStandardItem &other);
    void emitDataChanged();

private:
    friend class QStandardItemModel;
    qsizetype slot(int row, int column) const { return static_cast<qsizetype>(row) * m_columns + column; }
    void adopt(QStandardItem *item, int row, int column);
    void setModel(QStandardItemModel *model);
    QModelIndex childParentIndex() const; // the index that the children of this item have as their parent
    void sortRecursive(int column, Qt::SortOrder order, int role);

    QMap<int, QVariant> m_values;
    Qt::ItemFlags m_flags =
        Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    QStandardItem *m_parent = nullptr;
    QStandardItemModel *m_model = nullptr;
    QList<QStandardItem *> m_children; // m_rows * m_columns, row by row; null where there is no item
    int m_rows = 0;
    int m_columns = 0;
    mutable int m_lastIndex = -1; // a guess at the position of this item in its parent
};

class QStandardItemModel : public QAbstractItemModel {
    Q_OBJECT
    Q_PROPERTY(int sortRole READ sortRole WRITE setSortRole)
public:
    explicit QStandardItemModel(QObject *parent = nullptr);
    QStandardItemModel(int rows, int columns, QObject *parent = nullptr);
    ~QStandardItemModel() override;

    void setItemRoleNames(const QHash<int, QByteArray> &roleNames);
    QHash<int, QByteArray> roleNames() const override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    using QObject::parent;
    QModelIndex sibling(int row, int column, const QModelIndex &idx) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    bool clearItemData(const QModelIndex &index) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value,
                       int role = Qt::EditRole) override;
    QMap<int, QVariant> itemData(const QModelIndex &index) const override;
    bool setItemData(const QModelIndex &index, const QMap<int, QVariant> &roles) override;
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool insertColumns(int column, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeColumns(int column, int count, const QModelIndex &parent = QModelIndex()) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    Qt::DropActions supportedDropActions() const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    void clear();

    QStandardItem *itemFromIndex(const QModelIndex &index) const;
    QModelIndex indexFromItem(const QStandardItem *item) const;

    QStandardItem *item(int row, int column = 0) const;
    void setItem(int row, int column, QStandardItem *item);
    void setItem(int row, QStandardItem *item) { setItem(row, 0, item); }
    QStandardItem *invisibleRootItem() const { return m_root.get(); }

    QStandardItem *horizontalHeaderItem(int column) const;
    void setHorizontalHeaderItem(int column, QStandardItem *item);
    QStandardItem *verticalHeaderItem(int row) const;
    void setVerticalHeaderItem(int row, QStandardItem *item);
    void setHorizontalHeaderLabels(const QStringList &labels);
    void setVerticalHeaderLabels(const QStringList &labels);

    void setRowCount(int rows) { m_root->setRowCount(rows); }
    void setColumnCount(int columns) { m_root->setColumnCount(columns); }

    void appendRow(const QList<QStandardItem *> &items) { m_root->appendRow(items); }
    void appendRow(QStandardItem *item) { m_root->appendRow(item); }
    void appendColumn(const QList<QStandardItem *> &items) { m_root->appendColumn(items); }
    void insertRow(int row, const QList<QStandardItem *> &items) { m_root->insertRow(row, items); }
    void insertRow(int row, QStandardItem *item) { m_root->insertRow(row, item); }
    void insertColumn(int column, const QList<QStandardItem *> &items) { m_root->insertColumn(column, items); }
    bool insertRow(int row, const QModelIndex &parent = QModelIndex()) { return insertRows(row, 1, parent); }
    bool insertColumn(int column, const QModelIndex &parent = QModelIndex()) {
        return insertColumns(column, 1, parent);
    }

    QStandardItem *takeItem(int row, int column = 0) { return m_root->takeChild(row, column); }
    QList<QStandardItem *> takeRow(int row) { return m_root->takeRow(row); }
    QList<QStandardItem *> takeColumn(int column) { return m_root->takeColumn(column); }
    QStandardItem *takeHorizontalHeaderItem(int column);
    QStandardItem *takeVerticalHeaderItem(int row);

    const QStandardItem *itemPrototype() const { return m_prototype; }
    void setItemPrototype(const QStandardItem *item);

    QList<QStandardItem *> findItems(const QString &text, Qt::MatchFlags flags = Qt::MatchExactly,
                                     int column = 0) const;

    int sortRole() const { return m_sortRole; }
    void setSortRole(int role) { m_sortRole = role; }

signals:
    /*!
        This signal is emitted when the data of \a item changes.
    */
    void itemChanged(QStandardItem *item) { emitSignal(&QStandardItemModel::itemChanged, item); }

private:
    Q_DISABLE_COPY(QStandardItemModel)
    friend class QStandardItem;
    QStandardItem *newItem() const;
    QStandardItem *ensureItem(const QModelIndex &index) const;
    void itemDataChanged(QStandardItem *item, const QList<int> &roles);
    void headerChanged(QStandardItem *item);

    // Access for QStandardItem to the notifications of the model.
    void notifyInsertRows(const QModelIndex &parent, int first, int last, bool begin) {
        begin ? beginInsertRows(parent, first, last) : endInsertRows();
    }
    void notifyRemoveRows(const QModelIndex &parent, int first, int last, bool begin) {
        begin ? beginRemoveRows(parent, first, last) : endRemoveRows();
    }
    void notifyInsertColumns(const QModelIndex &parent, int first, int last, bool begin) {
        begin ? beginInsertColumns(parent, first, last) : endInsertColumns();
    }
    void notifyRemoveColumns(const QModelIndex &parent, int first, int last, bool begin) {
        begin ? beginRemoveColumns(parent, first, last) : endRemoveColumns();
    }

    std::unique_ptr<QStandardItem> m_root;
    QList<QStandardItem *> m_horizontalHeader; // owned; null where a section has none
    QList<QStandardItem *> m_verticalHeader;
    const QStandardItem *m_prototype = nullptr;
    QHash<int, QByteArray> m_roleNames;
    int m_sortRole = Qt::DisplayRole;
};
