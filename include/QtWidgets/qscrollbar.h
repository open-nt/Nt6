// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qabstractslider.h>

class QMenu;
class QStyleOptionSlider;

class QScrollBar : public QAbstractSlider {
    Q_OBJECT
public:
    explicit QScrollBar(QWidget *parent = nullptr);
    explicit QScrollBar(Qt::Orientation orientation, QWidget *parent = nullptr);
    ~QScrollBar() override;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    QMenu *createStandardContextMenu(QPoint position);

protected:
    void initStyleOption(QStyleOptionSlider *option) const;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QRect handleRect() const;
    int valueAt(const QPoint &pos) const;

    int m_grabOffset = 0;
};
