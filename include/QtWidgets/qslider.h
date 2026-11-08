// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qabstractslider.h>

class QSlider : public QAbstractSlider {
    Q_OBJECT
public:
    explicit QSlider(QWidget *parent = nullptr);
    explicit QSlider(Qt::Orientation orientation, QWidget *parent = nullptr);
    ~QSlider() override;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRect handleRect() const;
    int valueAt(const QPoint &pos) const;

    int m_grabOffset = 0;
};
