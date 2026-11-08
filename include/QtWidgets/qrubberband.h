// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qwidget.h>

class QRubberBand : public QWidget {
    Q_OBJECT
public:
    enum Shape { Line, Rectangle };
    Q_ENUM(Shape)

    explicit QRubberBand(Shape s, QWidget *p = nullptr);
    ~QRubberBand() override;

    Shape shape() const { return m_shape; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Shape m_shape;
};
