// MIT License - see LICENSE
#pragma once

#include <QtCharts/qchartglobal.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtGui/qbrush.h>
#include <QtGui/qcolor.h>
#include <QtGui/qfont.h>
#include <QtGui/qpen.h>

class QAbstractBarSeries;

class QBarSet : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)
    Q_PROPERTY(QPen pen READ pen WRITE setPen NOTIFY penChanged)
    Q_PROPERTY(QBrush brush READ brush WRITE setBrush NOTIFY brushChanged)
    Q_PROPERTY(QBrush labelBrush READ labelBrush WRITE setLabelBrush NOTIFY labelBrushChanged)
    Q_PROPERTY(QFont labelFont READ labelFont WRITE setLabelFont NOTIFY labelFontChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY borderColorChanged)
    Q_PROPERTY(QColor labelColor READ labelColor WRITE setLabelColor NOTIFY labelColorChanged)
public:
    explicit QBarSet(const QString &label, QObject *parent = nullptr);
    ~QBarSet() override;

    void setLabel(const QString &label);
    QString label() const;

    void append(const qreal value);
    void append(const QList<qreal> &values);
    QBarSet &operator<<(const qreal &value);
    void insert(const int index, const qreal value);
    void remove(const int index, const int count = 1);
    void replace(const int index, const qreal value);
    qreal at(const int index) const;
    qreal operator[](const int index) const;
    int count() const;
    qreal sum() const;

    void setPen(const QPen &pen);
    QPen pen() const;
    void setBrush(const QBrush &brush);
    QBrush brush() const;
    void setLabelBrush(const QBrush &brush);
    QBrush labelBrush() const;
    void setLabelFont(const QFont &font);
    QFont labelFont() const;

    QColor color();
    void setColor(QColor color);
    QColor borderColor();
    void setBorderColor(QColor color);
    QColor labelColor();
    void setLabelColor(QColor color);

signals:
    /*!
        This signal is emitted when the user clicks the bar at \a index.
    */
    void clicked(int index) { emitSignal(&QBarSet::clicked, index); }
    /*!
        This signal is emitted when the mouse enters or leaves the bar at \a index; \a status is \c true when it
        enters.
    */
    void hovered(bool status, int index) { emitSignal(&QBarSet::hovered, status, index); }
    /*!
        This signal is emitted when the user presses a mouse button on the bar at \a index.
    */
    void pressed(int index) { emitSignal(&QBarSet::pressed, index); }
    /*!
        This signal is emitted when the user releases a mouse button on the bar at \a index.
    */
    void released(int index) { emitSignal(&QBarSet::released, index); }
    /*!
        This signal is emitted when the user double-clicks the bar at \a index.
    */
    void doubleClicked(int index) { emitSignal(&QBarSet::doubleClicked, index); }
    /*!
        This signal is emitted when the pen of the bars changes.
    */
    void penChanged() { emitSignal(&QBarSet::penChanged); }
    /*!
        This signal is emitted when the brush of the bars changes.
    */
    void brushChanged() { emitSignal(&QBarSet::brushChanged); }
    /*!
        This signal is emitted when the label of the set changes.
    */
    void labelChanged() { emitSignal(&QBarSet::labelChanged); }
    /*!
        This signal is emitted when the brush of the value labels changes.
    */
    void labelBrushChanged() { emitSignal(&QBarSet::labelBrushChanged); }
    /*!
        This signal is emitted when the font of the value labels changes.
    */
    void labelFontChanged() { emitSignal(&QBarSet::labelFontChanged); }
    /*!
        This signal is emitted when the fill color changes to \a color.
    */
    void colorChanged(QColor color) { emitSignal(&QBarSet::colorChanged, color); }
    /*!
        This signal is emitted when the border color changes to \a color.
    */
    void borderColorChanged(QColor color) { emitSignal(&QBarSet::borderColorChanged, color); }
    /*!
        This signal is emitted when the color of the value labels changes to \a color.
    */
    void labelColorChanged(QColor color) { emitSignal(&QBarSet::labelColorChanged, color); }
    /*!
        This signal is emitted when \a count values are added, starting at \a index.
    */
    void valuesAdded(int index, int count) { emitSignal(&QBarSet::valuesAdded, index, count); }
    /*!
        This signal is emitted when \a count values are removed, starting at \a index.
    */
    void valuesRemoved(int index, int count) { emitSignal(&QBarSet::valuesRemoved, index, count); }
    /*!
        This signal is emitted when the value at \a index changes.
    */
    void valueChanged(int index) { emitSignal(&QBarSet::valueChanged, index); }

private:
    friend class QtChartsPrivate::ChartEngine;
    void markChanged();

    QAbstractBarSeries *m_series = nullptr;
    QString m_label;
    QList<qreal> m_values;
    bool m_penSet = false;
    QPen m_pen;
    bool m_brushSet = false;
    QBrush m_brush;
    bool m_labelBrushSet = false;
    QBrush m_labelBrush;
    bool m_labelFontSet = false;
    QFont m_labelFont;
};
