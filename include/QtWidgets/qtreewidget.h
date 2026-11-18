// MIT License - see LICENSE
#pragma once

#include <QtGui/qicon.h>

#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>
#include <QtGui/qbrush.h>
#include <QtGui/qfont.h>
#include <QtWidgets/qtreeview.h>

#include <vector>

class QMimeData;
class QTreeWidget;
class QDataStream;
namespace QtPrivate {
class TreeModel;
}

class QTreeWidgetItem {
public:
    /*!
        \enum QTreeWidgetItem::ItemType

        The types of items, as type() returns them.

        \value Type The type of a plain item.
        \value UserType The first number that a subclass can use for a type of its own.
    */
    enum ItemType { Type = 0, UserType = 1000 };
    /*!
        \enum QTreeWidgetItem::ChildIndicatorPolicy

        When an item has an arrow for opening it.

        \value ShowIndicator Always, also without children: for an item whose children are loaded when it is opened.
        \value DontShowIndicator Never.
        \value DontShowIndicatorWhenChildless Only while it has children. This is the default.
    */
    enum ChildIndicatorPolicy { ShowIndicator, DontShowIndicator, DontShowIndicatorWhenChildless };

    explicit QTreeWidgetItem(int type = Type);
    explicit QTreeWidgetItem(const QStringList &strings, int type = Type);
    explicit QTreeWidgetItem(QTreeWidget *treeview, int type = Type);
    QTreeWidgetItem(QTreeWidget *treeview, const QStringList &strings, int type = Type);
    explicit QTreeWidgetItem(QTreeWidgetItem *parent, int type = Type);
    QTreeWidgetItem(QTreeWidgetItem *parent, const QStringList &strings, int type = Type);
    QTreeWidgetItem(const QTreeWidgetItem &other);
    virtual ~QTreeWidgetItem();

    virtual QTreeWidgetItem *clone() const;

    QTreeWidget *treeWidget() const { return m_view; }
    QTreeWidgetItem *parent() const;
    QTreeWidgetItem *child(int index) const;
    int childCount() const { return static_cast<int>(m_children.size()); }
    int indexOfChild(QTreeWidgetItem *child) const;
    int columnCount() const { return static_cast<int>(m_values.size()); }

    void addChild(QTreeWidgetItem *child);
    void insertChild(int index, QTreeWidgetItem *child);
    void removeChild(QTreeWidgetItem *child);
    QTreeWidgetItem *takeChild(int index);
    void addChildren(const QList<QTreeWidgetItem *> &children);
    void insertChildren(int index, const QList<QTreeWidgetItem *> &children);
    QList<QTreeWidgetItem *> takeChildren();

    Qt::ItemFlags flags() const { return m_flags; }
    void setFlags(Qt::ItemFlags flags);
    bool isDisabled() const { return !(m_flags & Qt::ItemIsEnabled); }
    void setDisabled(bool disabled);
    bool isSelected() const;
    void setSelected(bool select);
    bool isExpanded() const;
    void setExpanded(bool expand);
    bool isHidden() const;
    void setHidden(bool hide);
    bool isFirstColumnSpanned() const;
    void setFirstColumnSpanned(bool span);
    ChildIndicatorPolicy childIndicatorPolicy() const { return m_policy; }
    void setChildIndicatorPolicy(ChildIndicatorPolicy policy);

    QString text(int column) const { return data(column, Qt::DisplayRole).toString(); }
    void setText(int column, const QString &text) { setData(column, Qt::DisplayRole, text); }
    QString toolTip(int column) const { return data(column, Qt::ToolTipRole).toString(); }
    void setToolTip(int column, const QString &toolTip) { setData(column, Qt::ToolTipRole, toolTip); }
    QFont font(int column) const;
    void setFont(int column, const QFont &font);
    int textAlignment(int column) const { return data(column, Qt::TextAlignmentRole).toInt(); }
    void setTextAlignment(int column, int alignment) { setData(column, Qt::TextAlignmentRole, alignment); }
    QBrush background(int column) const;
    void setBackground(int column, const QBrush &brush);
    QBrush foreground(int column) const;
    void setForeground(int column, const QBrush &brush);
    Qt::CheckState checkState(int column) const {
        return static_cast<Qt::CheckState>(data(column, Qt::CheckStateRole).toInt());
    }
    void setCheckState(int column, Qt::CheckState state) { setData(column, Qt::CheckStateRole, static_cast<int>(state)); }
    /*!
        \since 6.0
        Returns the icon of the item in \a column; a null icon if it has none.
    */
    QIcon icon(int column) const { return data(column, Qt::DecorationRole).value<QIcon>(); }
    /*!
        \since 6.0
        Sets the icon of the item in \a column to \a icon, shown before its text in the icon size of the tree.
    */
    void setIcon(int column, const QIcon &icon) { setData(column, Qt::DecorationRole, QVariant::fromValue(icon)); }
    QString statusTip(int column) const { return data(column, Qt::StatusTipRole).toString(); }
    void setStatusTip(int column, const QString &statusTip) { setData(column, Qt::StatusTipRole, statusTip); }
    QString whatsThis(int column) const { return data(column, Qt::WhatsThisRole).toString(); }
    void setWhatsThis(int column, const QString &whatsThis) { setData(column, Qt::WhatsThisRole, whatsThis); }
    QSize sizeHint(int column) const { return data(column, Qt::SizeHintRole).toSize(); }
    void setSizeHint(int column, const QSize &size) { setData(column, Qt::SizeHintRole, size); }
    virtual void read(QDataStream &in);
    virtual void write(QDataStream &out) const;

    virtual QVariant data(int column, int role) const;
    virtual void setData(int column, int role, const QVariant &value);

    void sortChildren(int column, Qt::SortOrder order);
    virtual bool operator<(const QTreeWidgetItem &other) const;
    QTreeWidgetItem &operator=(const QTreeWidgetItem &other);

    int type() const { return m_type; }

protected:
    void emitDataChanged();

private:
    friend class QTreeWidget;
    friend class QtPrivate::TreeModel;
    void setView(QTreeWidget *view);
    void sortRecursively(int column, Qt::SortOrder order);

    QTreeWidget *m_view = nullptr;
    QTreeWidgetItem *m_parent = nullptr; // the invisible root for a top-level item
    std::vector<QTreeWidgetItem *> m_children;
    std::vector<QMap<int, QVariant>> m_values; // for each column: the data by role
    Qt::ItemFlags m_flags = Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled |
                            Qt::ItemIsDropEnabled;
    ChildIndicatorPolicy m_policy = DontShowIndicatorWhenChildless;
    int m_type;
};

class QTreeWidget : public QTreeView {
    Q_OBJECT
    Q_PROPERTY(int columnCount READ columnCount WRITE setColumnCount)
    Q_PROPERTY(int topLevelItemCount READ topLevelItemCount)
public:
    explicit QTreeWidget(QWidget *parent = nullptr);
    ~QTreeWidget() override;

    int columnCount() const;
    void setColumnCount(int columns);

    QTreeWidgetItem *invisibleRootItem() const { return m_root; }
    QTreeWidgetItem *topLevelItem(int index) const;
    int topLevelItemCount() const;
    void insertTopLevelItem(int index, QTreeWidgetItem *item);
    void addTopLevelItem(QTreeWidgetItem *item);
    QTreeWidgetItem *takeTopLevelItem(int index);
    int indexOfTopLevelItem(QTreeWidgetItem *item) const;
    void insertTopLevelItems(int index, const QList<QTreeWidgetItem *> &items);
    void addTopLevelItems(const QList<QTreeWidgetItem *> &items);

    QTreeWidgetItem *headerItem() const { return m_headerItem; }
    void setHeaderLabels(const QStringList &labels);
    void setHeaderLabel(const QString &label) { setHeaderLabels(QStringList{label}); }

    QTreeWidgetItem *currentItem() const;
    int currentColumn() const;
    void setCurrentItem(QTreeWidgetItem *item);
    void setCurrentItem(QTreeWidgetItem *item, int column);

    QTreeWidgetItem *itemAt(const QPoint &p) const;
    QTreeWidgetItem *itemAt(int x, int y) const { return itemAt(QPoint(x, y)); }
    QRect visualItemRect(const QTreeWidgetItem *item) const;

    int sortColumn() const;
    void sortItems(int column, Qt::SortOrder order);

    void editItem(QTreeWidgetItem *item, int column = 0);

    QList<QTreeWidgetItem *> selectedItems() const;
    QList<QTreeWidgetItem *> findItems(const QString &text, Qt::MatchFlags flags, int column = 0) const;

    QTreeWidgetItem *itemAbove(const QTreeWidgetItem *item) const;
    QTreeWidgetItem *itemBelow(const QTreeWidgetItem *item) const;

    QModelIndex indexFromItem(const QTreeWidgetItem *item, int column = 0) const;
    QWidget *itemWidget(QTreeWidgetItem *item, int column) const;
    void setItemWidget(QTreeWidgetItem *item, int column, QWidget *widget);
    void removeItemWidget(QTreeWidgetItem *item, int column) { setItemWidget(item, column, nullptr); }
    QTreeWidgetItem *itemFromIndex(const QModelIndex &index) const;

    void setSelectionModel(QItemSelectionModel *selectionModel) override;

public slots:
    void scrollToItem(const QTreeWidgetItem *item, QAbstractItemView::ScrollHint hint = EnsureVisible);
    void expandItem(const QTreeWidgetItem *item);
    void collapseItem(const QTreeWidgetItem *item);
    void clear();

signals:
    /*!
        This signal is emitted when a mouse button is pressed on \a item, in the column \a column.
    */
    void itemPressed(QTreeWidgetItem *item, int column) { emitSignal(&QTreeWidget::itemPressed, item, column); }
    /*!
        This signal is emitted when \a item is clicked, in the column \a column.
    */
    void itemClicked(QTreeWidgetItem *item, int column) { emitSignal(&QTreeWidget::itemClicked, item, column); }
    /*!
        This signal is emitted when \a item is double-clicked, in the column \a column.
    */
    void itemDoubleClicked(QTreeWidgetItem *item, int column) {
        emitSignal(&QTreeWidget::itemDoubleClicked, item, column);
    }
    /*!
        This signal is emitted when the user activates \a item, in the column \a column: with a double click,
        or with Return or Enter.
    */
    void itemActivated(QTreeWidgetItem *item, int column) { emitSignal(&QTreeWidget::itemActivated, item, column); }
    /*!
        This signal is emitted when the data of \a item changed in the column \a column: its text, its check
        state or anything else, by the user or by the program.
    */
    void itemChanged(QTreeWidgetItem *item, int column) { emitSignal(&QTreeWidget::itemChanged, item, column); }
    /*!
        This signal is emitted when \a item was expanded.
    */
    void itemExpanded(QTreeWidgetItem *item) { emitSignal(&QTreeWidget::itemExpanded, item); }
    /*!
        This signal is emitted when \a item was collapsed.
    */
    void itemCollapsed(QTreeWidgetItem *item) { emitSignal(&QTreeWidget::itemCollapsed, item); }
    /*!
        This signal is emitted when the current item changed from \a previous to \a current; either can be
        \c nullptr.
    */
    void currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous) {
        emitSignal(&QTreeWidget::currentItemChanged, current, previous);
    }
    /*!
        This signal is emitted when the selection changed; selectedItems() tells what is selected now.
    */
    void itemSelectionChanged() { emitSignal(&QTreeWidget::itemSelectionChanged); }

protected:
    virtual QStringList mimeTypes() const;
    virtual QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const;
    virtual bool dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action);
    virtual Qt::DropActions supportedDropActions() const;

private:
    friend class QTreeWidgetItem;
    friend class QtPrivate::TreeModel;
    void setModel(QAbstractItemModel *model) override;
    QtPrivate::TreeModel *treeModel() const { return m_model; }

    QtPrivate::TreeModel *m_model;
    QTreeWidgetItem *m_root;       // the parent of the top-level items; it has the column count
    QTreeWidgetItem *m_headerItem; // its texts are the header labels
};

class QTreeWidgetItemIterator {
public:
    enum IteratorFlag {
        All = 0x00000000,
        Hidden = 0x00000001,
        NotHidden = 0x00000002,
        Selected = 0x00000004,
        Unselected = 0x00000008,
        Selectable = 0x00000010,
        NotSelectable = 0x00000020,
        DragEnabled = 0x00000040,
        DragDisabled = 0x00000080,
        DropEnabled = 0x00000100,
        DropDisabled = 0x00000200,
        HasChildren = 0x00000400,
        NoChildren = 0x00000800,
        Checked = 0x00001000,
        NotChecked = 0x00002000,
        Enabled = 0x00004000,
        Disabled = 0x00008000,
        Editable = 0x00010000,
        NotEditable = 0x00020000,
        UserFlag = 0x01000000
    };
    Q_DECLARE_FLAGS(IteratorFlags, IteratorFlag)

    explicit QTreeWidgetItemIterator(QTreeWidget *widget, IteratorFlags flags = All);
    explicit QTreeWidgetItemIterator(QTreeWidgetItem *item, IteratorFlags flags = All);
    QTreeWidgetItemIterator(const QTreeWidgetItemIterator &it) = default;
    QTreeWidgetItemIterator &operator=(const QTreeWidgetItemIterator &it) = default;
    ~QTreeWidgetItemIterator() = default;

    QTreeWidgetItem *operator*() const { return m_current; }
    QTreeWidgetItemIterator &operator++();
    QTreeWidgetItemIterator operator++(int) {
        QTreeWidgetItemIterator old = *this;
        ++*this;
        return old;
    }
    QTreeWidgetItemIterator &operator--();
    QTreeWidgetItemIterator operator--(int) {
        QTreeWidgetItemIterator old = *this;
        --*this;
        return old;
    }
    QTreeWidgetItemIterator &operator+=(int n);
    QTreeWidgetItemIterator &operator-=(int n);

private:
    bool matches(const QTreeWidgetItem *item) const;
    QTreeWidgetItem *next(QTreeWidgetItem *item) const;     // in depth-first order, any item
    QTreeWidgetItem *previous(QTreeWidgetItem *item) const; // likewise, backwards

    QTreeWidget *m_widget = nullptr;
    QTreeWidgetItem *m_current = nullptr;
    IteratorFlags m_flags;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QTreeWidgetItemIterator::IteratorFlags)
