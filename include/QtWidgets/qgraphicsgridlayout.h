// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtWidgets/qgraphicslayout.h>

#include <vector>

class QGraphicsGridLayout : public QGraphicsLayout {
public:
    explicit QGraphicsGridLayout(QGraphicsLayoutItem *parent = nullptr);
    ~QGraphicsGridLayout() override;

    void addItem(QGraphicsLayoutItem *item, int row, int column, int rowSpan, int columnSpan,
                 Qt::Alignment alignment = Qt::Alignment());
    void addItem(QGraphicsLayoutItem *item, int row, int column, Qt::Alignment alignment = Qt::Alignment()) {
        addItem(item, row, column, 1, 1, alignment);
    }

    void setHorizontalSpacing(qreal spacing);
    qreal horizontalSpacing() const;
    void setVerticalSpacing(qreal spacing);
    qreal verticalSpacing() const;
    void setSpacing(qreal spacing);
    void setRowSpacing(int row, qreal spacing);
    qreal rowSpacing(int row) const;
    void setColumnSpacing(int column, qreal spacing);
    qreal columnSpacing(int column) const;

    void setRowStretchFactor(int row, int stretch);
    int rowStretchFactor(int row) const;
    void setColumnStretchFactor(int column, int stretch);
    int columnStretchFactor(int column) const;
    void setRowMinimumHeight(int row, qreal height);
    qreal rowMinimumHeight(int row) const;
    void setRowPreferredHeight(int row, qreal height);
    qreal rowPreferredHeight(int row) const;
    void setRowMaximumHeight(int row, qreal height);
    qreal rowMaximumHeight(int row) const;
    void setRowFixedHeight(int row, qreal height);
    void setColumnMinimumWidth(int column, qreal width);
    qreal columnMinimumWidth(int column) const;
    void setColumnPreferredWidth(int column, qreal width);
    qreal columnPreferredWidth(int column) const;
    void setColumnMaximumWidth(int column, qreal width);
    qreal columnMaximumWidth(int column) const;
    void setColumnFixedWidth(int column, qreal width);
    void setRowAlignment(int row, Qt::Alignment alignment);
    Qt::Alignment rowAlignment(int row) const;
    void setColumnAlignment(int column, Qt::Alignment alignment);
    Qt::Alignment columnAlignment(int column) const;
    void setAlignment(QGraphicsLayoutItem *item, Qt::Alignment alignment);
    Qt::Alignment alignment(QGraphicsLayoutItem *item) const;

    int rowCount() const;
    int columnCount() const;
    QGraphicsLayoutItem *itemAt(int row, int column) const;
    int count() const override;
    QGraphicsLayoutItem *itemAt(int index) const override;
    void removeAt(int index) override;
    void removeItem(QGraphicsLayoutItem *item);

    void invalidate() override;
    void setGeometry(const QRectF &rect) override;

protected:
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;

private:
    struct Entry {
        QGraphicsLayoutItem *item;
        int row, column, rowSpan, columnSpan;
        Qt::Alignment alignment;
    };
    // The settings of a row or a column; a negative size is not set.
    struct Line {
        int stretch = 0;
        qreal spacing = -1;
        qreal minimum = -1;
        qreal preferred = -1;
        qreal maximum = -1;
        Qt::Alignment alignment;
    };
    Line &line(Qt::Orientation orientation, int index);
    Line lineAt(Qt::Orientation orientation, int index) const;
    // The minimum, preferred and maximum sizes of the rows or the columns.
    void lineSizes(Qt::Orientation orientation, std::vector<qreal> *minimum, std::vector<qreal> *preferred,
                   std::vector<qreal> *maximum) const;
    qreal gap(Qt::Orientation orientation, int index) const; // the spacing after a row or a column

    std::vector<Entry> m_entries;
    std::vector<Line> m_rows;
    std::vector<Line> m_columns;
    qreal m_hSpacing = -1;
    qreal m_vSpacing = -1;
};
