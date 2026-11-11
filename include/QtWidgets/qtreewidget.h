// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>
#include <QtGui/qbrush.h>
#include <QtGui/qfont.h>
#include <QtWidgets/qtreeview.h>

#include <vector>

class QTreeWidget;
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

    virtual QVariant data(int column, int role) const;
    virtual void setData(int column, int role, const QVariant &value);

    void sortChildren(int column, Qt::SortOrder order);
    virtual bool operator<(const QTreeWidgetItem &other) const;
    QTreeWidgetItem &operator=(const QTreeWidgetItem &other);

    int type() const { return m_type; }

private:
    friend class QTreeWidget;
    friend class QtPrivate::TreeModel;
    void setView(QTreeWidget *view);
    void sortRecursively(int column, Qt::SortOrder order);

    QTreeWidget *m_view = nullptr;
    QTreeWidgetItem *m_parent = nullptr; // the invisible root for a top-level item
    std::vector<QTreeWidgetItem *> m_children;
    std::vector<QMap<int, QVariant>> m_values; // for each column: the data by role
    Qt::ItemFlags m_flags = Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
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

private:
    friend class QTreeWidgetItem;
    friend class QtPrivate::TreeModel;
    void setModel(QAbstractItemModel *model) override;
    QtPrivate::TreeModel *treeModel() const { return m_model; }

    QtPrivate::TreeModel *m_model;
    QTreeWidgetItem *m_root;       // the parent of the top-level items; it has the column count
    QTreeWidgetItem *m_headerItem; // its texts are the header labels
};
