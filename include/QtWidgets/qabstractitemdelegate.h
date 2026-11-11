// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qobject.h>
#include <QtCore/qsize.h>
#include <QtWidgets/qstyleoption.h>

class QAbstractItemView;
class QPainter;
class QWidget;

class QAbstractItemDelegate : public QObject {
    Q_OBJECT
public:
    /*!
        \enum QAbstractItemDelegate::EndEditHint

        What a view should do after an editor was closed.

        \value NoHint Nothing in particular.
        \value EditNextItem Open an editor on the next item.
        \value EditPreviousItem Open an editor on the item before.
        \value SubmitModelCache The data was written to the model.
        \value RevertModelCache The edit was given up; the model keeps what it had.
    */
    enum EndEditHint { NoHint, EditNextItem, EditPreviousItem, SubmitModelCache, RevertModelCache };
    Q_ENUM(EndEditHint)

    explicit QAbstractItemDelegate(QObject *parent = nullptr);
    ~QAbstractItemDelegate() override;

    /*!
        Draws the item \a index with \a painter, in the rectangle and with
        the state, palette and font of \a option.
    */
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const = 0;
    /*!
        Returns the size that the item \a index needs to show its data,
        with the font and the other settings of \a option.
    */
    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const = 0;

    virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    virtual void destroyEditor(QWidget *editor, const QModelIndex &index) const;
    virtual void setEditorData(QWidget *editor, const QModelIndex &index) const;
    virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
    virtual void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const;
    virtual bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                             const QModelIndex &index);

signals:
    /*!
        This signal is emitted when the data of \a editor should be
        written to the model. The view calls setModelData() for it.
    */
    void commitData(QWidget *editor) { emitSignal(&QAbstractItemDelegate::commitData, editor); }
    /*!
        This signal is emitted when the user has finished with \a editor.
        The view removes the editor, and \a hint tells it what to do next.
    */
    void closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint = NoHint) {
        emitSignal(&QAbstractItemDelegate::closeEditor, editor, hint);
    }
    /*!
        This signal is emitted when the size hint of the item \a index
        changed, so that a view lays its items out again.
    */
    void sizeHintChanged(const QModelIndex &index) { emitSignal(&QAbstractItemDelegate::sizeHintChanged, index); }
};
