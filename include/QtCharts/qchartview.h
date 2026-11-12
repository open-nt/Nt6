// MIT License - see LICENSE
#pragma once

#include <QtCharts/qchart.h>
#include <QtCore/qflags.h>
#include <QtCore/qpoint.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qwidget.h>

class QRubberBand;

class QChartView : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QChartView::RubberBands rubberBand READ rubberBand WRITE setRubberBand)
public:
    enum RubberBand {
        NoRubberBand = 0x0,
        VerticalRubberBand = 0x1,
        HorizontalRubberBand = 0x2,
        RectangleRubberBand = 0x3,
        ClickThroughRubberBand = 0x80
    };
    Q_ENUM(RubberBand)
    Q_DECLARE_FLAGS(RubberBands, RubberBand)

    explicit QChartView(QWidget *parent = nullptr);
    explicit QChartView(QChart *chart, QWidget *parent = nullptr);
    ~QChartView() override;

    void setRubberBand(const RubberBands &rubberBands);
    RubberBands rubberBand() const;

    void setChart(QChart *chart);
    QChart *chart() const;

    void setRenderHint(QPainter::RenderHint hint, bool enabled = true);
    void setRenderHints(QPainter::RenderHints hints);
    QPainter::RenderHints renderHints() const;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void attach(QChart *chart);
    QRectF bandRect() const;

    QChart *m_chart = nullptr;
    RubberBands m_rubberBand = NoRubberBand;
    QPainter::RenderHints m_hints;
    QRubberBand *m_band = nullptr;
    QPointF m_bandOrigin;
    bool m_banding = false;
    bool m_createdChart = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QChartView::RubberBands)
