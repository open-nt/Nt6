// MIT License - see LICENSE
#pragma once

#include <QtCore/qlocale.h>
#include <QtCore/qvariant.h>
#include <QtWidgets/qabstractitemdelegate.h>

class QStyledItemDelegate : public QAbstractItemDelegate {
    Q_OBJECT
public:
    explicit QStyledItemDelegate(QObject *parent = nullptr);
    ~QStyledItemDelegate() override;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;

    virtual QString displayText(const QVariant &value, const QLocale &locale) const;

protected:
    virtual void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const;
    bool eventFilter(QObject *object, QEvent *event) override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                     const QModelIndex &index) override;
};
