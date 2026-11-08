// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qwidget.h>

class QSizeGrip : public QWidget {
    Q_OBJECT
public:
    explicit QSizeGrip(QWidget *parent);
    ~QSizeGrip() override;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QPoint m_pressGlobal;
    QSize m_startSize;
    bool m_dragging = false;
};
