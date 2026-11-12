// MIT License - see LICENSE
#pragma once

#include <QtCharts/qabstractseries.h>
#include <QtCore/qlist.h>
#include <QtCore/qpoint.h>
#include <QtGui/qbrush.h>
#include <QtGui/qcolor.h>
#include <QtGui/qfont.h>
#include <QtGui/qpen.h>

class QXYSeries : public QAbstractSeries {
    Q_OBJECT
    Q_PROPERTY(bool pointsVisible READ pointsVisible WRITE setPointsVisible)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(
        QString pointLabelsFormat READ pointLabelsFormat WRITE setPointLabelsFormat NOTIFY pointLabelsFormatChanged)
    Q_PROPERTY(
        bool pointLabelsVisible READ pointLabelsVisible WRITE setPointLabelsVisible NOTIFY pointLabelsVisibilityChanged)
    Q_PROPERTY(QFont pointLabelsFont READ pointLabelsFont WRITE setPointLabelsFont NOTIFY pointLabelsFontChanged)
    Q_PROPERTY(QColor pointLabelsColor READ pointLabelsColor WRITE setPointLabelsColor NOTIFY pointLabelsColorChanged)
    Q_PROPERTY(bool pointLabelsClipping READ pointLabelsClipping WRITE setPointLabelsClipping NOTIFY
                   pointLabelsClippingChanged)
    Q_PROPERTY(qreal markerSize READ markerSize WRITE setMarkerSize NOTIFY markerSizeChanged)
public:
    ~QXYSeries() override;

    void append(qreal x, qreal y);
    void append(const QPointF &point);
    void append(const QList<QPointF> &points);
    void replace(qreal oldX, qreal oldY, qreal newX, qreal newY);
    void replace(const QPointF &oldPoint, const QPointF &newPoint);
    void replace(int index, qreal newX, qreal newY);
    void replace(int index, const QPointF &newPoint);
    void replace(const QList<QPointF> &points);
    void remove(qreal x, qreal y);
    void remove(const QPointF &point);
    void remove(int index);
    void removePoints(int index, int count);
    void insert(int index, const QPointF &point);
    void clear();

    int count() const;
    QList<QPointF> points() const;
    const QPointF &at(int index) const;

    QXYSeries &operator<<(const QPointF &point);
    QXYSeries &operator<<(const QList<QPointF> &points);

    virtual void setPen(const QPen &pen);
    QPen pen() const;
    virtual void setBrush(const QBrush &brush);
    QBrush brush() const;
    virtual void setColor(const QColor &color);
    virtual QColor color() const;

    void setPointsVisible(bool visible = true);
    bool pointsVisible() const;

    void setPointLabelsFormat(const QString &format);
    QString pointLabelsFormat() const;
    void setPointLabelsVisible(bool visible = true);
    bool pointLabelsVisible() const;
    void setPointLabelsFont(const QFont &font);
    QFont pointLabelsFont() const;
    void setPointLabelsColor(const QColor &color);
    QColor pointLabelsColor() const;
    void setPointLabelsClipping(bool enabled = true);
    bool pointLabelsClipping() const;

    void setMarkerSize(qreal size);
    qreal markerSize() const;

signals:
    /*!
        This signal is emitted when the user clicks the series at \a point, in the coordinates of the series.
    */
    void clicked(const QPointF &point) { emitSignal(&QXYSeries::clicked, point); }
    /*!
        This signal is emitted when the mouse enters or leaves the series at \a point; \a state is \c true when
        it enters.
    */
    void hovered(const QPointF &point, bool state) { emitSignal(&QXYSeries::hovered, point, state); }
    /*!
        This signal is emitted when the user presses a mouse button on the series at \a point.
    */
    void pressed(const QPointF &point) { emitSignal(&QXYSeries::pressed, point); }
    /*!
        This signal is emitted when the user releases a mouse button on the series at \a point.
    */
    void released(const QPointF &point) { emitSignal(&QXYSeries::released, point); }
    /*!
        This signal is emitted when the user double-clicks the series at \a point.
    */
    void doubleClicked(const QPointF &point) { emitSignal(&QXYSeries::doubleClicked, point); }
    /*!
        This signal is emitted when the point at \a index is replaced.
    */
    void pointReplaced(int index) { emitSignal(&QXYSeries::pointReplaced, index); }
    /*!
        This signal is emitted when the point at \a index is removed.
    */
    void pointRemoved(int index) { emitSignal(&QXYSeries::pointRemoved, index); }
    /*!
        This signal is emitted when a point is added at \a index.
    */
    void pointAdded(int index) { emitSignal(&QXYSeries::pointAdded, index); }
    /*!
        This signal is emitted when the color of the line changes to \a color.
    */
    void colorChanged(QColor color) { emitSignal(&QXYSeries::colorChanged, color); }
    /*!
        This signal is emitted when all points are replaced at once.
    */
    void pointsReplaced() { emitSignal(&QXYSeries::pointsReplaced); }
    /*!
        This signal is emitted when the format of the point labels changes to \a format.
    */
    void pointLabelsFormatChanged(const QString &format) { emitSignal(&QXYSeries::pointLabelsFormatChanged, format); }
    /*!
        This signal is emitted when the point labels are shown or hidden; \a visible is the new state.
    */
    void pointLabelsVisibilityChanged(bool visible) { emitSignal(&QXYSeries::pointLabelsVisibilityChanged, visible); }
    /*!
        This signal is emitted when the font of the point labels changes to \a font.
    */
    void pointLabelsFontChanged(const QFont &font) { emitSignal(&QXYSeries::pointLabelsFontChanged, font); }
    /*!
        This signal is emitted when the color of the point labels changes to \a color.
    */
    void pointLabelsColorChanged(const QColor &color) { emitSignal(&QXYSeries::pointLabelsColorChanged, color); }
    /*!
        This signal is emitted when clipping of the point labels is turned on or off; \a clipping is the new state.
    */
    void pointLabelsClippingChanged(bool clipping) { emitSignal(&QXYSeries::pointLabelsClippingChanged, clipping); }
    /*!
        This signal is emitted when \a count points are removed, starting at \a index.
    */
    void pointsRemoved(int index, int count) { emitSignal(&QXYSeries::pointsRemoved, index, count); }
    /*!
        This signal is emitted when the pen of the series changes to \a pen.
    */
    void penChanged(const QPen &pen) { emitSignal(&QXYSeries::penChanged, pen); }
    /*!
        This signal is emitted when the size of the markers changes to \a size.
    */
    void markerSizeChanged(qreal size) { emitSignal(&QXYSeries::markerSizeChanged, size); }

protected:
    explicit QXYSeries(QObject *parent = nullptr);

private:
    friend class QtChartsPrivate::ChartEngine;

    QList<QPointF> m_points;
    bool m_penSet = false;
    QPen m_pen;
    bool m_brushSet = false;
    QBrush m_brush;
    bool m_pointsVisible = false;
    QString m_pointLabelsFormat;
    bool m_pointLabelsVisible = false;
    bool m_pointLabelsFontSet = false;
    QFont m_pointLabelsFont;
    bool m_pointLabelsColorSet = false;
    QColor m_pointLabelsColor;
    bool m_pointLabelsClipping = true;
    qreal m_markerSize = 15.0;
};
