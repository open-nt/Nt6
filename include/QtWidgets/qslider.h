// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qabstractslider.h>

class QStyleOptionSlider;

class QSlider : public QAbstractSlider {
    Q_OBJECT
    Q_PROPERTY(TickPosition tickPosition READ tickPosition WRITE setTickPosition)
    Q_PROPERTY(int tickInterval READ tickInterval WRITE setTickInterval)
public:
    enum TickPosition {
        NoTicks = 0,
        TicksAbove = 1,
        TicksLeft = TicksAbove,
        TicksBelow = 2,
        TicksRight = TicksBelow,
        TicksBothSides = 3
    };
    Q_ENUM(TickPosition)

    explicit QSlider(QWidget *parent = nullptr);
    explicit QSlider(Qt::Orientation orientation, QWidget *parent = nullptr);
    ~QSlider() override;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    TickPosition tickPosition() const { return m_ticks; }
    void setTickPosition(TickPosition position);
    int tickInterval() const { return m_tickInterval; }
    void setTickInterval(int ti);

protected:
    void initStyleOption(QStyleOptionSlider *option) const;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRect handleRect() const;
    int valueAt(const QPoint &pos) const;

    int m_grabOffset = 0;
    TickPosition m_ticks = NoTicks;
    int m_tickInterval = 0;
    int tickSpace() const; // the room the ticks take at each side that has them
};
