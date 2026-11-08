// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qlayout.h>

#include <map>
#include <vector>

namespace QtPrivate {
struct Cell;
}

class QGridLayout : public QLayout {
    Q_OBJECT
public:
    explicit QGridLayout(QWidget *parent = nullptr);
    ~QGridLayout() override;

    void addWidget(QWidget *widget, int row, int column, Qt::Alignment alignment = Qt::Alignment());
    void addWidget(QWidget *widget, int row, int column, int rowSpan, int columnSpan,
                   Qt::Alignment alignment = Qt::Alignment());
    void addLayout(QLayout *layout, int row, int column, Qt::Alignment alignment = Qt::Alignment());
    void addLayout(QLayout *layout, int row, int column, int rowSpan, int columnSpan,
                   Qt::Alignment alignment = Qt::Alignment());
    void addItem(QLayoutItem *item, int row, int column, int rowSpan = 1, int columnSpan = 1,
                 Qt::Alignment alignment = Qt::Alignment());

    int rowCount() const { return m_rows; }
    int columnCount() const { return m_columns; }
    QLayoutItem *itemAtPosition(int row, int column) const;
    void getItemPosition(int index, int *row, int *column, int *rowSpan, int *columnSpan) const;

    void setRowStretch(int row, int stretch);
    int rowStretch(int row) const;
    void setColumnStretch(int column, int stretch);
    int columnStretch(int column) const;
    void setRowMinimumHeight(int row, int height);
    int rowMinimumHeight(int row) const;
    void setColumnMinimumWidth(int column, int width);
    int columnMinimumWidth(int column) const;

    int horizontalSpacing() const;
    void setHorizontalSpacing(int spacing);
    int verticalSpacing() const;
    void setVerticalSpacing(int spacing);

    void addItem(QLayoutItem *item) override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    int count() const override { return static_cast<int>(m_items.size()); }
    QSize sizeHint() const override;
    QSize minimumSize() const override;
    Qt::Orientations expandingDirections() const override;
    void setGeometry(const QRect &rect) override;

private:
    struct Entry {
        QLayoutItem *item;
        bool owned;
        int row;
        int column;
        int rowSpan;
        int columnSpan;
    };
    void insert(QLayoutItem *item, bool owned, int row, int column, int rowSpan, int columnSpan);
    std::vector<QtPrivate::Cell> axisCells(bool columns) const;
    int gap(bool columns) const;

    std::vector<Entry> m_items;
    std::map<int, int> m_rowStretch, m_columnStretch, m_rowMinimum, m_columnMinimum;
    int m_rows = 0;
    int m_columns = 0;
    int m_horizontalSpacing = -1;
    int m_verticalSpacing = -1;
};
