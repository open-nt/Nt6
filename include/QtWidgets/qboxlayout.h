// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qlayout.h>

#include <vector>

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
    void setGeometry(const QRect &rect) override;

private:
    struct Entry {
        QLayoutItem *item;
        bool owned;
        int stretch;
    };
    void insertEntry(int index, QLayoutItem *item, bool owned, int stretch);
    bool horizontal() const { return m_direction == LeftToRight || m_direction == RightToLeft; }

    std::vector<Entry> m_items;
    Direction m_direction;
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
