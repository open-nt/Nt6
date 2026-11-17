// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtWidgets/qgraphicslayout.h>

class QGraphicsLinearLayout : public QGraphicsLayout {
public:
    explicit QGraphicsLinearLayout(QGraphicsLayoutItem *parent = nullptr);
    explicit QGraphicsLinearLayout(Qt::Orientation orientation, QGraphicsLayoutItem *parent = nullptr);
    ~QGraphicsLinearLayout() override;

    void setOrientation(Qt::Orientation orientation);
    Qt::Orientation orientation() const { return m_orientation; }

    void addItem(QGraphicsLayoutItem *item) { insertItem(-1, item); }
    void addStretch(int stretch = 1) { insertStretch(-1, stretch); }
    void insertItem(int index, QGraphicsLayoutItem *item);
    void insertStretch(int index, int stretch = 1);
    void removeItem(QGraphicsLayoutItem *item);
    void removeAt(int index) override;

    void setSpacing(qreal spacing);
    qreal spacing() const;
    void setItemSpacing(int index, qreal spacing);
    qreal itemSpacing(int index) const;
    void setStretchFactor(QGraphicsLayoutItem *item, int stretch);
    int stretchFactor(QGraphicsLayoutItem *item) const;
    void setAlignment(QGraphicsLayoutItem *item, Qt::Alignment alignment);
    Qt::Alignment alignment(QGraphicsLayoutItem *item) const;

    void setGeometry(const QRectF &rect) override;
    int count() const override;
    QGraphicsLayoutItem *itemAt(int index) const override;
    void invalidate() override;

protected:
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;

private:
    struct Entry {
        QGraphicsLayoutItem *item; // null for a stretch
        int stretch;
        Qt::Alignment alignment;
        qreal spacing; // after the entry; -1: spacing()
    };
    int entryOf(const QGraphicsLayoutItem *item) const;
    int entryOfItem(int index) const; // the entry of the item at `index`, stretches skipped
    qreal spacingAfter(int entry) const;

    Qt::Orientation m_orientation;
    QList<Entry> m_entries;
    qreal m_spacing = -1; // -1: from the style
};
