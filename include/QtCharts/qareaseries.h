// MIT License - see LICENSE
#pragma once

#include <QtCharts/qabstractseries.h>
#include <QtCore/qpoint.h>
#include <QtGui/qbrush.h>
#include <QtGui/qcolor.h>
#include <QtGui/qfont.h>
#include <QtGui/qpen.h>

class QLineSeries;

class QAreaSeries : public QAbstractSeries {
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY borderColorChanged)
    Q_PROPERTY(
        bool pointLabelsVisible READ pointLabelsVisible WRITE setPointLabelsVisible NOTIFY pointLabelsVisibilityChanged)
    Q_PROPERTY(
        QString pointLabelsFormat READ pointLabelsFormat WRITE setPointLabelsFormat NOTIFY pointLabelsFormatChanged)
public:
    explicit QAreaSeries(QObject *parent = nullptr);
    explicit QAreaSeries(QLineSeries *upperSeries, QLineSeries *lowerSeries = nullptr);
    ~QAreaSeries() override;

    QAbstractSeries::SeriesType type() const override;

    void setUpperSeries(QLineSeries *series);
    QLineSeries *upperSeries() const;
    void setLowerSeries(QLineSeries *series);
    QLineSeries *lowerSeries() const;

    void setPen(const QPen &pen);
    QPen pen() const;
    void setBrush(const QBrush &brush);
    QBrush brush() const;
    void setColor(const QColor &color);
    QColor color() const;
    void setBorderColor(const QColor &color);
    QColor borderColor() const;

    void setPointsVisible(bool visible = true);
    bool pointsVisible() const;
    void setPointLabelsFormat(const QString &format);
    QString pointLabelsFormat() const;
    void setPointLabelsVisible(bool visible = true);
    bool pointLabelsVisible() const;

signals:
    /*!
        This signal is emitted when the user clicks the area at \a point, in the coordinates of the series.
    */
    void clicked(const QPointF &point) { emitSignal(&QAreaSeries::clicked, point); }
    /*!
        This signal is emitted when the mouse enters or leaves the area at \a point; \a state is \c true when
        it enters.
    */
    void hovered(const QPointF &point, bool state) { emitSignal(&QAreaSeries::hovered, point, state); }
    /*!
        This signal is emitted when the user presses a mouse button on the area at \a point.
    */
    void pressed(const QPointF &point) { emitSignal(&QAreaSeries::pressed, point); }
    /*!
        This signal is emitted when the user releases a mouse button on the area at \a point.
    */
    void released(const QPointF &point) { emitSignal(&QAreaSeries::released, point); }
    /*!
        This signal is emitted when the user double-clicks the area at \a point.
    */
    void doubleClicked(const QPointF &point) { emitSignal(&QAreaSeries::doubleClicked, point); }
    /*!
        This signal is emitted when the fill color changes to \a color.
    */
    void colorChanged(QColor color) { emitSignal(&QAreaSeries::colorChanged, color); }
    /*!
        This signal is emitted when the border color changes to \a color.
    */
    void borderColorChanged(QColor color) { emitSignal(&QAreaSeries::borderColorChanged, color); }
    /*!
        This signal is emitted when the format of the point labels changes to \a format.
    */
    void pointLabelsFormatChanged(const QString &format) { emitSignal(&QAreaSeries::pointLabelsFormatChanged, format); }
    /*!
        This signal is emitted when the point labels are shown or hidden; \a visible is the new state.
    */
    void pointLabelsVisibilityChanged(bool visible) { emitSignal(&QAreaSeries::pointLabelsVisibilityChanged, visible); }

private:
    friend class QtChartsPrivate::ChartEngine;
    void watch(QLineSeries *series);

    QLineSeries *m_upper = nullptr;
    QLineSeries *m_lower = nullptr;
    bool m_penSet = false;
    QPen m_pen;
    bool m_brushSet = false;
    QBrush m_brush;
    bool m_pointsVisible = false;
    QString m_pointLabelsFormat;
    bool m_pointLabelsVisible = false;
};
