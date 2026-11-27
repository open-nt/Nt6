// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qlayout.h>

#include <vector>

namespace QtPrivate {
struct Cell;
}

class QBoxLayout : public QLayout {
    Q_OBJECT
public:
    enum Direction { LeftToRight, RightToLeft, TopToBottom, BottomToTop };

    explicit QBoxLayout(Direction direction, QWidget *parent = nullptr);
    ~QBoxLayout() override;

    Direction direction() const { return m_direction; }
    void setDirection(Direction direction);

    void addSpacing(int size);
    void addStretch(int stretch = 0);
    void addWidget(QWidget *widget, int stretch = 0, Qt::Alignment alignment = Qt::Alignment());
    void addLayout(QLayout *layout, int stretch = 0);
    void insertSpacing(int index, int size);
    void insertStretch(int index, int stretch = 0);
    void insertWidget(int index, QWidget *widget, int stretch = 0, Qt::Alignment alignment = Qt::Alignment());
    void insertLayout(int index, QLayout *layout, int stretch = 0);
    void addSpacerItem(QSpacerItem *spacerItem);
    void insertSpacerItem(int index, QSpacerItem *spacerItem);
    void insertItem(int index, QLayoutItem *item);
    void addStrut(int size);
    void setStretch(int index, int stretch);
    int stretch(int index) const;
    bool setStretchFactor(QWidget *widget, int stretch);

    void addItem(QLayoutItem *item) override;
    void removeItem(QLayoutItem *item) override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    int count() const override { return static_cast<int>(m_items.size()); }
    QSize sizeHint() const override;
    QSize minimumSize() const override;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int w) const override;
    int minimumHeightForWidth(int w) const override;
    void setGeometry(const QRect &rect) override;

private:
    struct Entry {
        QLayoutItem *item;
        bool owned;
        int stretch;
    };
    void insertEntry(int index, QLayoutItem *item, bool owned, int stretch);
    // The items as they share the length of the box.
    std::vector<QtPrivate::Cell> cells() const;
    int heightAtWidth(int w, bool minimum) const;
    bool horizontal() const { return m_direction == LeftToRight || m_direction == RightToLeft; }

    std::vector<Entry> m_items;
    Direction m_direction;
    int m_strut = 0; // addStrut(): the least size across
    QLayoutItem *replaceAt(int index, QLayoutItem *item) override;
};

class QHBoxLayout : public QBoxLayout {
    Q_OBJECT
public:
    explicit QHBoxLayout(QWidget *parent = nullptr);
    ~QHBoxLayout() override;
};

class QVBoxLayout : public QBoxLayout {
    Q_OBJECT
public:
    explicit QVBoxLayout(QWidget *parent = nullptr);
    ~QVBoxLayout() override;
};
