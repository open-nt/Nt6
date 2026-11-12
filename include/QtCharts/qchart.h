// MIT License - see LICENSE
#pragma once

#include <QtCharts/qabstractaxis.h>
#include <QtCharts/qabstractseries.h>
#include <QtCharts/qchartglobal.h>
#include <QtCharts/qlegend.h>
#include <QtCore/qhash.h>
#include <QtCore/qpair.h>
#include <QtCore/qlist.h>
#include <QtCore/qmargins.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtCore/qpointer.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtGui/qbrush.h>
#include <QtGui/qfont.h>
#include <QtGui/qpen.h>

class QPainter;
class QWidget;

class QChart : public QObject {
    Q_OBJECT
    Q_PROPERTY(QChart::ChartTheme theme READ theme WRITE setTheme)
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(bool backgroundVisible READ isBackgroundVisible WRITE setBackgroundVisible)
    Q_PROPERTY(bool dropShadowEnabled READ isDropShadowEnabled WRITE setDropShadowEnabled)
    Q_PROPERTY(qreal backgroundRoundness READ backgroundRoundness WRITE setBackgroundRoundness)
    Q_PROPERTY(QChart::AnimationOptions animationOptions READ animationOptions WRITE setAnimationOptions)
    Q_PROPERTY(int animationDuration READ animationDuration WRITE setAnimationDuration)
    Q_PROPERTY(QMargins margins READ margins WRITE setMargins)
    Q_PROPERTY(QChart::ChartType chartType READ chartType)
    Q_PROPERTY(bool plotAreaBackgroundVisible READ isPlotAreaBackgroundVisible WRITE setPlotAreaBackgroundVisible)
    Q_PROPERTY(QRectF plotArea READ plotArea WRITE setPlotArea NOTIFY plotAreaChanged)
public:
    enum ChartTheme {
        ChartThemeLight = 0,
        ChartThemeBlueCerulean,
        ChartThemeDark,
        ChartThemeBrownSand,
        ChartThemeBlueNcs,
        ChartThemeHighContrast,
        ChartThemeBlueIcy,
        ChartThemeQt
    };
    Q_ENUM(ChartTheme)

    enum AnimationOption { NoAnimation = 0x0, GridAxisAnimations = 0x1, SeriesAnimations = 0x2, AllAnimations = 0x3 };
    Q_ENUM(AnimationOption)
    Q_DECLARE_FLAGS(AnimationOptions, AnimationOption)

    enum ChartType { ChartTypeUndefined = 0, ChartTypeCartesian, ChartTypePolar };
    Q_ENUM(ChartType)

    explicit QChart(QObject *parent = nullptr);
    ~QChart() override;

    void addSeries(QAbstractSeries *series);
    void removeSeries(QAbstractSeries *series);
    void removeAllSeries();
    QList<QAbstractSeries *> series() const;

    void addAxis(QAbstractAxis *axis, Qt::Alignment alignment);
    void removeAxis(QAbstractAxis *axis);
    QList<QAbstractAxis *> axes(Qt::Orientations orientation = Qt::Horizontal | Qt::Vertical,
                                QAbstractSeries *series = nullptr) const;
    void createDefaultAxes();

    void setTheme(QChart::ChartTheme theme);
    QChart::ChartTheme theme() const;

    void setTitle(const QString &title);
    QString title() const;
    void setTitleFont(const QFont &font);
    QFont titleFont() const;
    void setTitleBrush(const QBrush &brush);
    QBrush titleBrush() const;

    void setBackgroundBrush(const QBrush &brush);
    QBrush backgroundBrush() const;
    void setBackgroundPen(const QPen &pen);
    QPen backgroundPen() const;
    void setBackgroundVisible(bool visible = true);
    bool isBackgroundVisible() const;
    void setDropShadowEnabled(bool enabled = true);
    bool isDropShadowEnabled() const;
    void setBackgroundRoundness(qreal diameter);
    qreal backgroundRoundness() const;

    void setPlotAreaBackgroundBrush(const QBrush &brush);
    QBrush plotAreaBackgroundBrush() const;
    void setPlotAreaBackgroundPen(const QPen &pen);
    QPen plotAreaBackgroundPen() const;
    void setPlotAreaBackgroundVisible(bool visible = true);
    bool isPlotAreaBackgroundVisible() const;

    void setAnimationOptions(AnimationOptions options);
    AnimationOptions animationOptions() const;
    void setAnimationDuration(int msecs);
    int animationDuration() const;

    QLegend *legend() const;

    void setMargins(const QMargins &margins);
    QMargins margins() const;
    QRectF plotArea() const;
    void setPlotArea(const QRectF &rect);

    void zoomIn();
    void zoomIn(const QRectF &rect);
    void zoomOut();
    void zoom(qreal factor);
    void zoomReset();
    bool isZoomed();
    void scroll(qreal dx, qreal dy);

    QPointF mapToValue(const QPointF &position, QAbstractSeries *series = nullptr);
    QPointF mapToPosition(const QPointF &value, QAbstractSeries *series = nullptr);

    ChartType chartType() const;

    void setGeometry(const QRectF &rect);
    QRectF geometry() const;
    void resize(const QSizeF &size);
    QSizeF size() const;
    void paint(QPainter *painter);
    void update();

signals:
    /*!
        This signal is emitted when the plot area of the chart changes to \a plotArea, for example because
        the chart was resized or a title was added.
    */
    void plotAreaChanged(const QRectF &plotArea) { emitSignal(&QChart::plotAreaChanged, plotArea); }

private:
    friend class QtChartsPrivate::ChartEngine;

    QList<QAbstractSeries *> m_series;
    QList<QAbstractAxis *> m_axes;
    QLegend *m_legend;
    ChartTheme m_theme = ChartThemeLight;
    QString m_title;
    bool m_titleFontSet = false;
    QFont m_titleFont;
    bool m_titleBrushSet = false;
    QBrush m_titleBrush;
    bool m_backgroundBrushSet = false;
    QBrush m_backgroundBrush;
    bool m_backgroundPenSet = false;
    QPen m_backgroundPen;
    bool m_backgroundVisible = true;
    bool m_dropShadow = false;
    qreal m_roundness = 0.0;
    bool m_plotBrushSet = false;
    QBrush m_plotBrush;
    bool m_plotPenSet = false;
    QPen m_plotPen;
    bool m_plotBackgroundVisible = false;
    AnimationOptions m_animationOptions = NoAnimation;
    int m_animationDuration = 1000;
    QMargins m_margins = QMargins(20, 20, 20, 20);
    QRectF m_geometry = QRectF(0, 0, 400, 300);
    QRectF m_plotArea;
    bool m_fixedPlotArea = false;
    QWidget *m_view = nullptr;
    // The ranges of the axes before the first zoom or scroll, to go back to.
    QHash<QAbstractAxis *, QPair<qreal, qreal>> m_zoomOrigin;
    // What the mouse is over, for the hovered() signals.
    QPointer<QAbstractSeries> m_hoverSeries;
    QPointer<QObject> m_hoverItem;
    int m_hoverIndex = -1;
    QPointF m_hoverPoint;
    // What a mouse button went down on, for the clicked() signals.
    QPointer<QAbstractSeries> m_pressSeries;
    QPointer<QObject> m_pressItem;
    int m_pressIndex = -1;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QChart::AnimationOptions)
