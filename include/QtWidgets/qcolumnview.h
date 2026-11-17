// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qpointer.h>
#include <QtWidgets/qabstractitemview.h>

#include <vector>

class QListView;

class QColumnView : public QAbstractItemView {
    Q_OBJECT
    Q_PROPERTY(bool resizeGripsVisible READ resizeGripsVisible WRITE setResizeGripsVisible)
public:
    explicit QColumnView(QWidget *parent = nullptr);
    ~QColumnView() override;

    void setModel(QAbstractItemModel *model) override;
    void setRootIndex(const QModelIndex &index) override;
    QModelIndex indexAt(const QPoint &point) const override;
    QRect visualRect(const QModelIndex &index) const override;
    void scrollTo(const QModelIndex &index, ScrollHint hint = EnsureVisible) override;
    void selectAll() override;
    QSize sizeHint() const override;

    QList<int> columnWidths() const;
    void setColumnWidths(const QList<int> &list);
    QWidget *previewWidget() const { return m_preview; }
    void setPreviewWidget(QWidget *widget);
    bool resizeGripsVisible() const { return m_grips; }
    void setResizeGripsVisible(bool visible) { m_grips = visible; }

signals:
    /*!
        This signal is emitted when the preview widget shows the item \a index, an item without children that was
        chosen.
    */
    void updatePreviewWidget(const QModelIndex &index) { emitSignal(&QColumnView::updatePreviewWidget, index); }

protected:
    virtual QAbstractItemView *createColumn(const QModelIndex &rootIndex);
    void initializeColumn(QAbstractItemView *column) const;
    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    void openFrom(int columnIndex, const QModelIndex &index); // shows the children of `index` after that column
    void truncate(int keep);
    void placeColumns();

    std::vector<QPointer<QAbstractItemView>> m_columns;
    QPointer<QWidget> m_preview;
    QPointer<QWidget> m_previewHolder; // the column that shows the preview widget
    QList<int> m_widths;
    bool m_grips = true;
    bool m_opening = false;
};
