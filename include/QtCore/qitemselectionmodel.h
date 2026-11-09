// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qflags.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>

#include <utility>

/*!
    \class QItemSelectionRange
    \brief The QItemSelectionRange class manages information about a range of selected items in a model.

    A range is a rectangle of items under one parent, from topLeft() to
    bottomRight(). The corners are persistent indexes, so a range follows its
    items when rows and columns are inserted or removed.

    \differences
    None.
    \enddifferences

    \sa QItemSelection, QItemSelectionModel
*/
class QItemSelectionRange {
public:
    /*!
        Constructs an empty range.
    */
    QItemSelectionRange() = default;
    /*!
        Constructs a range from \a topL to \a bottomR, which must have the
        same parent.
    */
    QItemSelectionRange(const QModelIndex &topL, const QModelIndex &bottomR) : tl(topL), br(bottomR) {}
    /*!
        Constructs a range that holds the single item \a index.
    */
    explicit QItemSelectionRange(const QModelIndex &index) : tl(index), br(index) {}
    /*!
        Swaps this range with \a other.
    */
    void swap(QItemSelectionRange &other) noexcept {
        tl.swap(other.tl);
        br.swap(other.br);
    }

    /*!
        Returns the row of the top of the range.
    */
    int top() const { return tl.row(); }
    /*!
        Returns the column of the left side of the range.
    */
    int left() const { return tl.column(); }
    /*!
        Returns the row of the bottom of the range.
    */
    int bottom() const { return br.row(); }
    /*!
        Returns the column of the right side of the range.
    */
    int right() const { return br.column(); }
    /*!
        Returns the number of columns of the range.
    */
    int width() const { return br.column() - tl.column() + 1; }
    /*!
        Returns the number of rows of the range.
    */
    int height() const { return br.row() - tl.row() + 1; }
    /*!
        Returns the top-left corner of the range.
    */
    const QPersistentModelIndex &topLeft() const { return tl; }
    /*!
        Returns the bottom-right corner of the range.
    */
    const QPersistentModelIndex &bottomRight() const { return br; }
    /*!
        Returns the parent of the items in the range.
    */
    QModelIndex parent() const { return tl.parent(); }
    /*!
        Returns the model of the range.
    */
    const QAbstractItemModel *model() const { return tl.model(); }
    /*!
        Returns \c true if \a index is inside the range; otherwise returns
        \c false.
    */
    bool contains(const QModelIndex &index) const {
        return index.parent() == parent() && index.model() == model() &&
               contains(index.row(), index.column(), index.parent());
    }
    /*!
        \overload
        Returns \c true if the item at \a row and \a column under
        \a parentIndex is inside the range.
    */
    bool contains(int row, int column, const QModelIndex &parentIndex) const {
        return isValid() && parentIndex == parent() && row >= top() && row <= bottom() && column >= left() &&
               column <= right();
    }
    bool intersects(const QItemSelectionRange &other) const;
    QItemSelectionRange intersected(const QItemSelectionRange &other) const;
    /*!
        Returns \c true if both corners are valid and form a rectangle under
        one parent; otherwise returns \c false.
    */
    bool isValid() const {
        return tl.isValid() && br.isValid() && tl.parent() == br.parent() && top() <= bottom() && left() <= right();
    }
    bool isEmpty() const;
    QModelIndexList indexes() const;

    friend bool operator==(const QItemSelectionRange &a, const QItemSelectionRange &b) {
        return a.tl == b.tl && a.br == b.br;
    }
    friend bool operator!=(const QItemSelectionRange &a, const QItemSelectionRange &b) { return !(a == b); }

private:
    QPersistentModelIndex tl, br;
};

/*!
    \class QItemSelection
    \brief The QItemSelection class manages information about selected items in a model.

    A QItemSelection is a list of QItemSelectionRange objects. select()
    adds a range, merge() combines selections and indexes() lists the
    selected items.

    \differences
    merge() takes the command as an \c int, because QItemSelectionModel is
    declared after this class; QItemSelectionModel::SelectionFlags values
    convert to it. merge() and split() produce an equivalent but not
    necessarily identical set of ranges to Qt's.
    \enddifferences

    \sa QItemSelectionRange, QItemSelectionModel
*/
class QItemSelection : public QList<QItemSelectionRange> {
public:
    /*!
        Constructs an empty selection.
    */
    QItemSelection() = default;
    QItemSelection(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void select(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    bool contains(const QModelIndex &index) const;
    QModelIndexList indexes() const;
    void merge(const QItemSelection &other, int command);
    static void split(const QItemSelectionRange &range, const QItemSelectionRange &other, QItemSelection *result);
};

/*!
    \class QItemSelectionModel
    \brief The QItemSelectionModel class keeps track of a view's selected items.

    A selection model holds the selection and the current item of a model, as
    used by views. select() changes the selection with a combination of
    SelectionFlag values, and setCurrentIndex() moves the current item.

    \code
    QItemSelectionModel selection(&model);
    selection.select(model.index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
    selection.isRowSelected(0); // true
    \endcode

    \differences
    Selections are kept as cells and turned back into ranges, which is simple
    but slower than Qt for very large selections. The selection follows the
    model through persistent indexes; layout changes keep the selected items
    selected.
    \enddifferences

    \sa QItemSelection, QAbstractItemModel
*/
class QItemSelectionModel : public QObject {
    Q_OBJECT
public:
    /*!
        \enum QItemSelectionModel::SelectionFlag

        \value NoUpdate The selection does not change.
        \value Clear The whole selection is cleared first.
        \value Select The items are selected.
        \value Deselect The items are deselected.
        \value Toggle The items are selected or deselected depending on their state.
        \value Current The current selection is updated (as with a rubber band)
               instead of being added to the committed selection.
        \value Rows Whole rows are affected.
        \value Columns Whole columns are affected.
        \value SelectCurrent Select | Current.
        \value ToggleCurrent Toggle | Current.
        \value ClearAndSelect Clear | Select.
    */
    enum SelectionFlag {
        NoUpdate = 0x0000,
        Clear = 0x0001,
        Select = 0x0002,
        Deselect = 0x0004,
        Toggle = 0x0008,
        Current = 0x0010,
        Rows = 0x0020,
        Columns = 0x0040,
        SelectCurrent = Select | Current,
        ToggleCurrent = Toggle | Current,
        ClearAndSelect = Clear | Select
    };
    Q_DECLARE_FLAGS(SelectionFlags, SelectionFlag)

    explicit QItemSelectionModel(QAbstractItemModel *model = nullptr);
    explicit QItemSelectionModel(QAbstractItemModel *model, QObject *parent);
    ~QItemSelectionModel() override;

    QModelIndex currentIndex() const;
    bool isSelected(const QModelIndex &index) const;
    bool isRowSelected(int row, const QModelIndex &parent = QModelIndex()) const;
    bool isColumnSelected(int column, const QModelIndex &parent = QModelIndex()) const;
    bool rowIntersectsSelection(int row, const QModelIndex &parent = QModelIndex()) const;
    bool columnIntersectsSelection(int column, const QModelIndex &parent = QModelIndex()) const;
    bool hasSelection() const;
    QModelIndexList selectedIndexes() const;
    QModelIndexList selectedRows(int column = 0) const;
    QModelIndexList selectedColumns(int row = 0) const;
    const QItemSelection selection() const;
    QAbstractItemModel *model();
    const QAbstractItemModel *model() const;
    void setModel(QAbstractItemModel *model);

public slots:
    virtual void setCurrentIndex(const QModelIndex &index, QItemSelectionModel::SelectionFlags command);
    virtual void select(const QModelIndex &index, QItemSelectionModel::SelectionFlags command);
    virtual void select(const QItemSelection &selection, QItemSelectionModel::SelectionFlags command);
    virtual void clear();
    virtual void reset();
    void clearSelection();
    virtual void clearCurrentIndex();

signals:
    /*!
        This signal is emitted when the selection changes: \a selected holds
        the items that became selected and \a deselected those that stopped
        being selected.
    */
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) {
        emitSignal(&QItemSelectionModel::selectionChanged, selected, deselected);
    }
    /*!
        This signal is emitted when the current item changes from \a previous
        to \a current.
    */
    void currentChanged(const QModelIndex &current, const QModelIndex &previous) {
        emitSignal(&QItemSelectionModel::currentChanged, current, previous);
    }
    /*!
        This signal is emitted when the current item moves to another row,
        with the new \a current and the \a previous item.
    */
    void currentRowChanged(const QModelIndex &current, const QModelIndex &previous) {
        emitSignal(&QItemSelectionModel::currentRowChanged, current, previous);
    }
    /*!
        This signal is emitted when the current item moves to another column,
        with the new \a current and the \a previous item.
    */
    void currentColumnChanged(const QModelIndex &current, const QModelIndex &previous) {
        emitSignal(&QItemSelectionModel::currentColumnChanged, current, previous);
    }
    /*!
        \since 5.5
        This signal is emitted when the model changes to \a model.
    */
    void modelChanged(QAbstractItemModel *model) { emitSignal(&QItemSelectionModel::modelChanged, model); }

protected:
    void emitSelectionChanged(const QItemSelection &newSelection, const QItemSelection &oldSelection);

private:
    QItemSelection expanded(const QItemSelection &selection, SelectionFlags command) const;
    void finalize();
    QPointer<QAbstractItemModel> m_model;
    QItemSelection m_ranges;
    QItemSelection m_current;
    SelectionFlags m_currentCommand;
    QPersistentModelIndex m_currentIndex;
    QList<QMetaObject::Connection> m_connections;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QItemSelectionModel::SelectionFlags)
