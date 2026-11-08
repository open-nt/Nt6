// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qwidget.h>

class QPainter;

class QFrame : public QWidget {
    Q_OBJECT
    Q_PROPERTY(Shape frameShape READ frameShape WRITE setFrameShape)
    Q_PROPERTY(Shadow frameShadow READ frameShadow WRITE setFrameShadow)
    Q_PROPERTY(int lineWidth READ lineWidth WRITE setLineWidth)
    Q_PROPERTY(int midLineWidth READ midLineWidth WRITE setMidLineWidth)
    Q_PROPERTY(int frameWidth READ frameWidth)
public:
    enum Shape { NoFrame = 0, Box = 1, Panel = 2, HLine = 4, VLine = 5, StyledPanel = 6 };
    enum Shadow { Plain = 0x10, Raised = 0x20, Sunken = 0x30 };
    Q_ENUM(Shape)
    Q_ENUM(Shadow)

    explicit QFrame(QWidget *parent = nullptr);
    ~QFrame() override;

    Shape frameShape() const;
    void setFrameShape(Shape shape);
    Shadow frameShadow() const;
    void setFrameShadow(Shadow shadow);
    int frameStyle() const;
    void setFrameStyle(int style);
    int lineWidth() const { return m_lineWidth; }
    void setLineWidth(int width);
    int midLineWidth() const { return m_midLineWidth; }
    void setMidLineWidth(int width);
    int frameWidth() const;
    QRect frameRect() const { return rect(); }
    QRect contentsRect() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void drawFrame(QPainter *painter);

private:
    int m_style = NoFrame | Plain;
    int m_lineWidth = 1;
    int m_midLineWidth = 0;
};
