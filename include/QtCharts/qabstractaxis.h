// MIT License - see LICENSE
#pragma once

#include <QtCharts/qchartglobal.h>
#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>
#include <QtGui/qbrush.h>
#include <QtGui/qcolor.h>
#include <QtGui/qfont.h>
#include <QtGui/qpen.h>

class QChart;
class QAbstractSeries;

class QAbstractAxis : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool lineVisible READ isLineVisible WRITE setLineVisible NOTIFY lineVisibleChanged)
    Q_PROPERTY(QColor color READ linePenColor WRITE setLinePenColor NOTIFY colorChanged)
    Q_PROPERTY(bool labelsVisible READ labelsVisible WRITE setLabelsVisible NOTIFY labelsVisibleChanged)
    Q_PROPERTY(int labelsAngle READ labelsAngle WRITE setLabelsAngle NOTIFY labelsAngleChanged)
    Q_PROPERTY(QFont labelsFont READ labelsFont WRITE setLabelsFont NOTIFY labelsFontChanged)
    Q_PROPERTY(QColor labelsColor READ labelsColor WRITE setLabelsColor NOTIFY labelsColorChanged)
    Q_PROPERTY(bool gridVisible READ isGridLineVisible WRITE setGridLineVisible NOTIFY gridVisibleChanged)
    Q_PROPERTY(QColor gridLineColor READ gridLineColor WRITE setGridLineColor NOTIFY gridLineColorChanged)
    Q_PROPERTY(
        bool minorGridVisible READ isMinorGridLineVisible WRITE setMinorGridLineVisible NOTIFY minorGridVisibleChanged)
    Q_PROPERTY(bool shadesVisible READ shadesVisible WRITE setShadesVisible NOTIFY shadesVisibleChanged)
    Q_PROPERTY(QColor shadesColor READ shadesColor WRITE setShadesColor NOTIFY shadesColorChanged)
    Q_PROPERTY(bool titleVisible READ isTitleVisible WRITE setTitleVisible NOTIFY titleVisibleChanged)
    Q_PROPERTY(QString titleText READ titleText WRITE setTitleText NOTIFY titleTextChanged)
    Q_PROPERTY(QFont titleFont READ titleFont WRITE setTitleFont NOTIFY titleFontChanged)
    Q_PROPERTY(Qt::Orientation orientation READ orientation)
    Q_PROPERTY(Qt::Alignment alignment READ alignment)
    Q_PROPERTY(bool reverse READ isReverse WRITE setReverse NOTIFY reverseChanged)
public:
    enum AxisType {
        AxisTypeNoAxis = 0x0,
        AxisTypeValue = 0x1,
        AxisTypeBarCategory = 0x2,
        AxisTypeCategory = 0x4,
        AxisTypeDateTime = 0x8,
        AxisTypeLogValue = 0x10,
        AxisTypeColor = 0x20
    };
    Q_ENUM(AxisType)
    Q_DECLARE_FLAGS(AxisTypes, AxisType)

    ~QAbstractAxis() override;

    virtual AxisType type() const = 0;

    bool isVisible() const;
    void setVisible(bool visible = true);
    void show();
    void hide();

    bool isLineVisible() const;
    void setLineVisible(bool visible = true);
    QPen linePen() const;
    void setLinePen(const QPen &pen);
    QColor linePenColor() const;
    void setLinePenColor(QColor color);

    bool isGridLineVisible() const;
    void setGridLineVisible(bool visible = true);
    QPen gridLinePen() const;
    void setGridLinePen(const QPen &pen);
    QColor gridLineColor() const;
    void setGridLineColor(const QColor &color);
    bool isMinorGridLineVisible() const;
    void setMinorGridLineVisible(bool visible = true);
    QPen minorGridLinePen() const;
    void setMinorGridLinePen(const QPen &pen);
    QColor minorGridLineColor() const;
    void setMinorGridLineColor(const QColor &color);

    bool labelsVisible() const;
    void setLabelsVisible(bool visible = true);
    QBrush labelsBrush() const;
    void setLabelsBrush(const QBrush &brush);
    QFont labelsFont() const;
    void setLabelsFont(const QFont &font);
    int labelsAngle() const;
    void setLabelsAngle(int angle);
    QColor labelsColor() const;
    void setLabelsColor(QColor color);

    bool isTitleVisible() const;
    void setTitleVisible(bool visible = true);
    QBrush titleBrush() const;
    void setTitleBrush(const QBrush &brush);
    QFont titleFont() const;
    void setTitleFont(const QFont &font);
    QString titleText() const;
    void setTitleText(const QString &title);

    bool shadesVisible() const;
    void setShadesVisible(bool visible = true);
    QPen shadesPen() const;
    void setShadesPen(const QPen &pen);
    QBrush shadesBrush() const;
    void setShadesBrush(const QBrush &brush);
    QColor shadesColor() const;
    void setShadesColor(QColor color);
    QColor shadesBorderColor() const;
    void setShadesBorderColor(QColor color);

    Qt::Orientation orientation() const;
    Qt::Alignment alignment() const;

    void setMin(const QVariant &min);
    void setMax(const QVariant &max);
    void setRange(const QVariant &min, const QVariant &max);

    bool isReverse() const;
    void setReverse(bool reverse = true);

signals:
    /*!
        This signal is emitted when the axis is shown or hidden; \a visible is the new state.
    */
    void visibleChanged(bool visible) { emitSignal(&QAbstractAxis::visibleChanged, visible); }
    /*!
        This signal is emitted when the pen of the axis line changes to \a pen.
    */
    void linePenChanged(const QPen &pen) { emitSignal(&QAbstractAxis::linePenChanged, pen); }
    /*!
        This signal is emitted when the axis line is shown or hidden; \a visible is the new state.
    */
    void lineVisibleChanged(bool visible) { emitSignal(&QAbstractAxis::lineVisibleChanged, visible); }
    /*!
        This signal is emitted when the labels are shown or hidden; \a visible is the new state.
    */
    void labelsVisibleChanged(bool visible) { emitSignal(&QAbstractAxis::labelsVisibleChanged, visible); }
    /*!
        This signal is emitted when the brush of the labels changes to \a brush.
    */
    void labelsBrushChanged(const QBrush &brush) { emitSignal(&QAbstractAxis::labelsBrushChanged, brush); }
    /*!
        This signal is emitted when the font of the labels changes to \a font.
    */
    void labelsFontChanged(const QFont &font) { emitSignal(&QAbstractAxis::labelsFontChanged, font); }
    /*!
        This signal is emitted when the angle of the labels changes to \a angle.
    */
    void labelsAngleChanged(int angle) { emitSignal(&QAbstractAxis::labelsAngleChanged, angle); }
    /*!
        This signal is emitted when the pen of the grid lines changes to \a pen.
    */
    void gridLinePenChanged(const QPen &pen) { emitSignal(&QAbstractAxis::gridLinePenChanged, pen); }
    /*!
        This signal is emitted when the grid lines are shown or hidden; \a visible is the new state.
    */
    void gridVisibleChanged(bool visible) { emitSignal(&QAbstractAxis::gridVisibleChanged, visible); }
    /*!
        This signal is emitted when the minor grid lines are shown or hidden; \a visible is the new state.
    */
    void minorGridVisibleChanged(bool visible) { emitSignal(&QAbstractAxis::minorGridVisibleChanged, visible); }
    /*!
        This signal is emitted when the pen of the minor grid lines changes to \a pen.
    */
    void minorGridLinePenChanged(const QPen &pen) { emitSignal(&QAbstractAxis::minorGridLinePenChanged, pen); }
    /*!
        This signal is emitted when the color of the grid lines changes to \a color.
    */
    void gridLineColorChanged(const QColor &color) { emitSignal(&QAbstractAxis::gridLineColorChanged, color); }
    /*!
        This signal is emitted when the color of the minor grid lines changes to \a color.
    */
    void minorGridLineColorChanged(const QColor &color) {
        emitSignal(&QAbstractAxis::minorGridLineColorChanged, color);
    }
    /*!
        This signal is emitted when the color of the axis line changes to \a color.
    */
    void colorChanged(QColor color) { emitSignal(&QAbstractAxis::colorChanged, color); }
    /*!
        This signal is emitted when the color of the labels changes to \a color.
    */
    void labelsColorChanged(QColor color) { emitSignal(&QAbstractAxis::labelsColorChanged, color); }
    /*!
        This signal is emitted when the title text changes to \a title.
    */
    void titleTextChanged(const QString &title) { emitSignal(&QAbstractAxis::titleTextChanged, title); }
    /*!
        This signal is emitted when the brush of the title changes to \a brush.
    */
    void titleBrushChanged(const QBrush &brush) { emitSignal(&QAbstractAxis::titleBrushChanged, brush); }
    /*!
        This signal is emitted when the title is shown or hidden; \a visible is the new state.
    */
    void titleVisibleChanged(bool visible) { emitSignal(&QAbstractAxis::titleVisibleChanged, visible); }
    /*!
        This signal is emitted when the font of the title changes to \a font.
    */
    void titleFontChanged(const QFont &font) { emitSignal(&QAbstractAxis::titleFontChanged, font); }
    /*!
        This signal is emitted when the shades are shown or hidden; \a visible is the new state.
    */
    void shadesVisibleChanged(bool visible) { emitSignal(&QAbstractAxis::shadesVisibleChanged, visible); }
    /*!
        This signal is emitted when the fill color of the shades changes to \a color.
    */
    void shadesColorChanged(QColor color) { emitSignal(&QAbstractAxis::shadesColorChanged, color); }
    /*!
        This signal is emitted when the border color of the shades changes to \a color.
    */
    void shadesBorderColorChanged(QColor color) { emitSignal(&QAbstractAxis::shadesBorderColorChanged, color); }
    /*!
        This signal is emitted when the pen of the shades changes to \a pen.
    */
    void shadesPenChanged(const QPen &pen) { emitSignal(&QAbstractAxis::shadesPenChanged, pen); }
    /*!
        This signal is emitted when the brush of the shades changes to \a brush.
    */
    void shadesBrushChanged(const QBrush &brush) { emitSignal(&QAbstractAxis::shadesBrushChanged, brush); }
    /*!
        This signal is emitted when the direction of the axis changes; \a reverse is the new state.
    */
    void reverseChanged(bool reverse) { emitSignal(&QAbstractAxis::reverseChanged, reverse); }

protected:
    explicit QAbstractAxis(QObject *parent = nullptr);

    void markChanged();
    virtual void setRangeFromVariant(const QVariant &min, const QVariant &max) = 0;

private:
    friend class QtChartsPrivate::ChartEngine;

    QChart *m_chart = nullptr;
    QList<QAbstractSeries *> m_series;
    Qt::Orientation m_orientation = Qt::Horizontal;
    Qt::Alignment m_alignment;
    bool m_userRange = false;

    bool m_visible = true;
    bool m_lineVisible = true;
    bool m_gridVisible = true;
    bool m_minorGridVisible = false;
    bool m_labelsVisible = true;
    bool m_titleVisible = true;
    bool m_shadesVisible = false;
    bool m_reverse = false;
    int m_labelsAngle = 0;
    QString m_titleText;

    // A theme supplies what has not been set; QChart::setTheme() forgets what has.
    bool m_linePenSet = false;
    QPen m_linePen;
    bool m_gridPenSet = false;
    QPen m_gridPen;
    bool m_minorGridPenSet = false;
    QPen m_minorGridPen;
    bool m_labelsBrushSet = false;
    QBrush m_labelsBrush;
    bool m_labelsFontSet = false;
    QFont m_labelsFont;
    bool m_titleBrushSet = false;
    QBrush m_titleBrush;
    bool m_titleFontSet = false;
    QFont m_titleFont;
    bool m_shadesPenSet = false;
    QPen m_shadesPen;
    bool m_shadesBrushSet = false;
    QBrush m_shadesBrush;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QAbstractAxis::AxisTypes)
