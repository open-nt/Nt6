// MIT License - see LICENSE
#pragma once

#include <QtCore/qvariant.h>
#include <QtGui/qpixmap.h>
#include <QtWidgets/qabstractitemdelegate.h>

class QStyledItemDelegate;

class QItemDelegate : public QAbstractItemDelegate {
    Q_OBJECT
    Q_PROPERTY(bool clipping READ hasClipping WRITE setClipping)
public:
    explicit QItemDelegate(QObject *parent = nullptr);
    ~QItemDelegate() override;

    bool hasClipping() const { return m_clipping; }
    void setClipping(bool clip);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;

protected:
    virtual void drawDisplay(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect,
                             const QString &text) const;
    virtual void drawDecoration(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect,
                                const QPixmap &pixmap) const;
    virtual void drawFocus(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect) const;
    virtual void drawCheck(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect,
                           Qt::CheckState state) const;
    void drawBackground(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void doLayout(const QStyleOptionViewItem &option, QRect *checkRect, QRect *pixmapRect, QRect *textRect,
                  bool hint) const;
    QRect rect(const QStyleOptionViewItem &option, const QModelIndex &index, int role) const;
    QStyleOptionViewItem setOptions(const QModelIndex &index, const QStyleOptionViewItem &option) const;
    QPixmap decoration(const QStyleOptionViewItem &option, const QVariant &variant) const;
    bool eventFilter(QObject *object, QEvent *event) override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                     const QModelIndex &index) override;

private:
    QString textOf(const QModelIndex &index, const QStyleOptionViewItem &option) const;

    QStyledItemDelegate *m_editors; // makes, fills and reads the editors
    bool m_clipping = true;
};
