// MIT License - see LICENSE
#pragma once

#include <QtCharts/qxyseries.h>

class QScatterSeries : public QXYSeries {
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY borderColorChanged)
    Q_PROPERTY(MarkerShape markerShape READ markerShape WRITE setMarkerShape NOTIFY markerShapeChanged)
    Q_PROPERTY(qreal markerSize READ markerSize WRITE setMarkerSize NOTIFY markerSizeChanged)
public:
    enum MarkerShape {
        MarkerShapeCircle,
        MarkerShapeRectangle,
        MarkerShapeRotatedRectangle,
        MarkerShapeTriangle,
        MarkerShapeStar,
        MarkerShapePentagon
    };
    Q_ENUM(MarkerShape)

    explicit QScatterSeries(QObject *parent = nullptr);
    ~QScatterSeries() override;

    QAbstractSeries::SeriesType type() const override;

    void setPen(const QPen &pen) override;
    void setBrush(const QBrush &brush) override;
    void setColor(const QColor &color) override;
    QColor color() const override;
    void setBorderColor(const QColor &color);
    QColor borderColor() const;
    MarkerShape markerShape() const;
    void setMarkerShape(MarkerShape shape);

signals:
    /*!
        This signal is emitted when the border color of the markers changes to \a color.
    */
    void borderColorChanged(QColor color) { emitSignal(&QScatterSeries::borderColorChanged, color); }
    /*!
        This signal is emitted when the shape of the markers changes to \a shape.
    */
    void markerShapeChanged(MarkerShape shape) { emitSignal(&QScatterSeries::markerShapeChanged, shape); }

private:
    MarkerShape m_shape = MarkerShapeCircle;
};
