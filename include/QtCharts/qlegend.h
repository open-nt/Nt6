// MIT License - see LICENSE
#pragma once

#include <QtCharts/qchartglobal.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>
#include <QtGui/qbrush.h>
#include <QtGui/qcolor.h>
#include <QtGui/qfont.h>
#include <QtGui/qpen.h>

class QChart;

class QLegend : public QObject {
    Q_OBJECT
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible)
    Q_PROPERTY(
        bool backgroundVisible READ isBackgroundVisible WRITE setBackgroundVisible NOTIFY backgroundVisibleChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY borderColorChanged)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
    Q_PROPERTY(QColor labelColor READ labelColor WRITE setLabelColor NOTIFY labelColorChanged)
    Q_PROPERTY(bool reverseMarkers READ reverseMarkers WRITE setReverseMarkers NOTIFY reverseMarkersChanged)
    Q_PROPERTY(bool showToolTips READ showToolTips WRITE setShowToolTips NOTIFY showToolTipsChanged)
    Q_PROPERTY(MarkerShape markerShape READ markerShape WRITE setMarkerShape NOTIFY markerShapeChanged)
public:
    enum MarkerShape {
        MarkerShapeDefault,
        MarkerShapeRectangle,
        MarkerShapeCircle,
        MarkerShapeFromSeries,
        MarkerShapeRotatedRectangle,
        MarkerShapeTriangle,
        MarkerShapeStar,
        MarkerShapePentagon
    };
    Q_ENUM(MarkerShape)

    ~QLegend() override;

    void setBrush(const QBrush &brush);
    QBrush brush() const;
    void setColor(QColor color);
    QColor color();
    void setPen(const QPen &pen);
    QPen pen() const;
    void setBorderColor(QColor color);
    QColor borderColor();
    void setFont(const QFont &font);
    QFont font() const;
    void setLabelBrush(const QBrush &brush);
    QBrush labelBrush() const;
    void setLabelColor(QColor color);
    QColor labelColor() const;

    void setAlignment(Qt::Alignment alignment);
    Qt::Alignment alignment() const;

    void setVisible(bool visible);
    bool isVisible() const;
    void show();
    void hide();

    void setBackgroundVisible(bool visible = true);
    bool isBackgroundVisible() const;
    void setReverseMarkers(bool reverseMarkers = true);
    bool reverseMarkers();
    void setShowToolTips(bool show);
    bool showToolTips() const;
    MarkerShape markerShape() const;
    void setMarkerShape(MarkerShape shape);

signals:
    /*!
        This signal is emitted when the background is shown or hidden; \a visible is the new state.
    */
    void backgroundVisibleChanged(bool visible) { emitSignal(&QLegend::backgroundVisibleChanged, visible); }
    /*!
        This signal is emitted when the background color changes to \a color.
    */
    void colorChanged(QColor color) { emitSignal(&QLegend::colorChanged, color); }
    /*!
        This signal is emitted when the border color changes to \a color.
    */
    void borderColorChanged(QColor color) { emitSignal(&QLegend::borderColorChanged, color); }
    /*!
        This signal is emitted when the font changes to \a font.
    */
    void fontChanged(QFont font) { emitSignal(&QLegend::fontChanged, font); }
    /*!
        This signal is emitted when the color of the labels changes to \a color.
    */
    void labelColorChanged(QColor color) { emitSignal(&QLegend::labelColorChanged, color); }
    /*!
        This signal is emitted when the order of the markers is reversed or restored; \a reverseMarkers is the
        new state.
    */
    void reverseMarkersChanged(bool reverseMarkers) { emitSignal(&QLegend::reverseMarkersChanged, reverseMarkers); }
    /*!
        This signal is emitted when tool tips are turned on or off; \a showToolTips is the new state.
    */
    void showToolTipsChanged(bool showToolTips) { emitSignal(&QLegend::showToolTipsChanged, showToolTips); }
    /*!
        This signal is emitted when the shape of the markers changes to \a shape.
    */
    void markerShapeChanged(QLegend::MarkerShape shape) { emitSignal(&QLegend::markerShapeChanged, shape); }

private:
    friend class QtChartsPrivate::ChartEngine;
    explicit QLegend(QChart *chart);
    void markChanged();

    QChart *m_chart;
    Qt::Alignment m_alignment = Qt::AlignTop;
    bool m_visible = true;
    bool m_backgroundVisible = false;
    bool m_reverseMarkers = false;
    bool m_showToolTips = false;
    MarkerShape m_markerShape = MarkerShapeRectangle;
    bool m_brushSet = false;
    QBrush m_brush;
    bool m_penSet = false;
    QPen m_pen;
    bool m_fontSet = false;
    QFont m_font;
    bool m_labelBrushSet = false;
    QBrush m_labelBrush;
};
