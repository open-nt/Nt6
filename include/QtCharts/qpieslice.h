// MIT License - see LICENSE
#pragma once

#include <QtCharts/qchartglobal.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtGui/qbrush.h>
#include <QtGui/qcolor.h>
#include <QtGui/qfont.h>
#include <QtGui/qpen.h>

class QPieSeries;

class QPieSlice : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)
    Q_PROPERTY(qreal value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(bool labelVisible READ isLabelVisible WRITE setLabelVisible NOTIFY labelVisibleChanged)
    Q_PROPERTY(LabelPosition labelPosition READ labelPosition WRITE setLabelPosition)
    Q_PROPERTY(bool exploded READ isExploded WRITE setExploded)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY borderColorChanged)
    Q_PROPERTY(int borderWidth READ borderWidth WRITE setBorderWidth NOTIFY borderWidthChanged)
    Q_PROPERTY(QColor labelColor READ labelColor WRITE setLabelColor NOTIFY labelColorChanged)
    Q_PROPERTY(QFont labelFont READ labelFont WRITE setLabelFont NOTIFY labelFontChanged)
    Q_PROPERTY(qreal labelArmLengthFactor READ labelArmLengthFactor WRITE setLabelArmLengthFactor)
    Q_PROPERTY(qreal explodeDistanceFactor READ explodeDistanceFactor WRITE setExplodeDistanceFactor)
    Q_PROPERTY(qreal percentage READ percentage NOTIFY percentageChanged)
    Q_PROPERTY(qreal startAngle READ startAngle NOTIFY startAngleChanged)
    Q_PROPERTY(qreal angleSpan READ angleSpan NOTIFY angleSpanChanged)
public:
    enum LabelPosition { LabelOutside, LabelInsideHorizontal, LabelInsideTangential, LabelInsideNormal };
    Q_ENUM(LabelPosition)

    explicit QPieSlice(QObject *parent = nullptr);
    QPieSlice(const QString &label, qreal value, QObject *parent = nullptr);
    ~QPieSlice() override;

    void setLabel(const QString &label);
    QString label() const;
    void setValue(qreal value);
    qreal value() const;

    void setLabelVisible(bool visible = true);
    bool isLabelVisible() const;
    LabelPosition labelPosition();
    void setLabelPosition(LabelPosition position);

    void setExploded(bool exploded = true);
    bool isExploded() const;

    void setPen(const QPen &pen);
    QPen pen() const;
    QColor borderColor();
    void setBorderColor(QColor color);
    int borderWidth();
    void setBorderWidth(int width);
    void setBrush(const QBrush &brush);
    QBrush brush() const;
    QColor color();
    void setColor(QColor color);

    void setLabelBrush(const QBrush &brush);
    QBrush labelBrush() const;
    QColor labelColor();
    void setLabelColor(QColor color);
    void setLabelFont(const QFont &font);
    QFont labelFont() const;

    void setLabelArmLengthFactor(qreal factor);
    qreal labelArmLengthFactor() const;
    void setExplodeDistanceFactor(qreal factor);
    qreal explodeDistanceFactor() const;

    qreal percentage() const;
    qreal startAngle() const;
    qreal angleSpan() const;

    QPieSeries *series() const;

signals:
    /*!
        This signal is emitted when the user clicks the slice.
    */
    void clicked() { emitSignal(&QPieSlice::clicked); }
    /*!
        This signal is emitted when the mouse enters or leaves the slice; \a state is \c true when it enters.
    */
    void hovered(bool state) { emitSignal(&QPieSlice::hovered, state); }
    /*!
        This signal is emitted when the user presses a mouse button on the slice.
    */
    void pressed() { emitSignal(&QPieSlice::pressed); }
    /*!
        This signal is emitted when the user releases a mouse button on the slice.
    */
    void released() { emitSignal(&QPieSlice::released); }
    /*!
        This signal is emitted when the user double-clicks the slice.
    */
    void doubleClicked() { emitSignal(&QPieSlice::doubleClicked); }
    /*!
        This signal is emitted when the label changes.
    */
    void labelChanged() { emitSignal(&QPieSlice::labelChanged); }
    /*!
        This signal is emitted when the value changes.
    */
    void valueChanged() { emitSignal(&QPieSlice::valueChanged); }
    /*!
        This signal is emitted when the label is shown or hidden.
    */
    void labelVisibleChanged() { emitSignal(&QPieSlice::labelVisibleChanged); }
    /*!
        This signal is emitted when the pen changes.
    */
    void penChanged() { emitSignal(&QPieSlice::penChanged); }
    /*!
        This signal is emitted when the brush changes.
    */
    void brushChanged() { emitSignal(&QPieSlice::brushChanged); }
    /*!
        This signal is emitted when the brush of the label changes.
    */
    void labelBrushChanged() { emitSignal(&QPieSlice::labelBrushChanged); }
    /*!
        This signal is emitted when the font of the label changes.
    */
    void labelFontChanged() { emitSignal(&QPieSlice::labelFontChanged); }
    /*!
        This signal is emitted when the share of the slice in the series changes.
    */
    void percentageChanged() { emitSignal(&QPieSlice::percentageChanged); }
    /*!
        This signal is emitted when the angle at which the slice starts changes.
    */
    void startAngleChanged() { emitSignal(&QPieSlice::startAngleChanged); }
    /*!
        This signal is emitted when the angle that the slice spans changes.
    */
    void angleSpanChanged() { emitSignal(&QPieSlice::angleSpanChanged); }
    /*!
        This signal is emitted when the fill color changes.
    */
    void colorChanged() { emitSignal(&QPieSlice::colorChanged); }
    /*!
        This signal is emitted when the border color changes.
    */
    void borderColorChanged() { emitSignal(&QPieSlice::borderColorChanged); }
    /*!
        This signal is emitted when the border width changes.
    */
    void borderWidthChanged() { emitSignal(&QPieSlice::borderWidthChanged); }
    /*!
        This signal is emitted when the color of the label changes.
    */
    void labelColorChanged() { emitSignal(&QPieSlice::labelColorChanged); }

private:
    friend class QtChartsPrivate::ChartEngine;
    friend class QPieSeries;
    void markChanged();

    QPieSeries *m_series = nullptr;
    QString m_label;
    qreal m_value = 0.0;
    bool m_labelVisible = false;
    LabelPosition m_labelPosition = LabelOutside;
    bool m_exploded = false;
    qreal m_armLength = 0.15;
    qreal m_explodeDistance = 0.15;
    qreal m_percentage = 0.0;
    qreal m_startAngle = 0.0;
    qreal m_angleSpan = 0.0;
    bool m_penSet = false;
    QPen m_pen;
    bool m_brushSet = false;
    QBrush m_brush;
    bool m_labelBrushSet = false;
    QBrush m_labelBrush;
    bool m_labelFontSet = false;
    QFont m_labelFont;
};
