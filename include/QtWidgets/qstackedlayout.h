// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qlayout.h>

#include <vector>

class QStackedLayout : public QLayout {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
public:
    explicit QStackedLayout(QWidget *parent = nullptr);
    ~QStackedLayout() override;

    int addWidget(QWidget *widget);
    int insertWidget(int index, QWidget *widget);
    using QLayoutItem::widget;
    QWidget *widget(int index) const;
    QWidget *currentWidget() const;
    int currentIndex() const { return m_current; }

    void addItem(QLayoutItem *item) override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    int count() const override { return static_cast<int>(m_items.size()); }
    QSize sizeHint() const override;
    QSize minimumSize() const override;
    Qt::Orientations expandingDirections() const override;
    void setGeometry(const QRect &rect) override;

public slots:
    void setCurrentIndex(int index);
    void setCurrentWidget(QWidget *widget);

signals:
    /*!
        This signal is emitted when the current widget changes, with its \a index, or -1 if there is none.
    */
    void currentChanged(int index) { emitSignal(&QStackedLayout::currentChanged, index); }
    /*!
        This signal is emitted when the widget that was at \a index is removed from the layout.
    */
    void widgetRemoved(int index) { emitSignal(&QStackedLayout::widgetRemoved, index); }

private:
    std::vector<QLayoutItem *> m_items;
    int m_current = -1;
};
