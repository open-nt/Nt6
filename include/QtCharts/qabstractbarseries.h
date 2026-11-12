// MIT License - see LICENSE
#pragma once

#include <QtCharts/qabstractseries.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

class QBarSet;

class QAbstractBarSeries : public QAbstractSeries {
    Q_OBJECT
    Q_PROPERTY(qreal barWidth READ barWidth WRITE setBarWidth)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool labelsVisible READ isLabelsVisible WRITE setLabelsVisible NOTIFY labelsVisibleChanged)
    Q_PROPERTY(QString labelsFormat READ labelsFormat WRITE setLabelsFormat NOTIFY labelsFormatChanged)
    Q_PROPERTY(LabelsPosition labelsPosition READ labelsPosition WRITE setLabelsPosition NOTIFY labelsPositionChanged)
    Q_PROPERTY(qreal labelsAngle READ labelsAngle WRITE setLabelsAngle NOTIFY labelsAngleChanged)
    Q_PROPERTY(int labelsPrecision READ labelsPrecision WRITE setLabelsPrecision NOTIFY labelsPrecisionChanged)
public:
    enum LabelsPosition { LabelsCenter = 0, LabelsInsideEnd, LabelsInsideBase, LabelsOutsideEnd };
    Q_ENUM(LabelsPosition)

    ~QAbstractBarSeries() override;

    void setBarWidth(qreal width);
    qreal barWidth() const;

    bool append(QBarSet *set);
    bool remove(QBarSet *set);
    bool take(QBarSet *set);
    bool append(const QList<QBarSet *> &sets);
    bool insert(int index, QBarSet *set);
    int count() const;
    QList<QBarSet *> barSets() const;
    void clear();

    void setLabelsVisible(bool visible = true);
    bool isLabelsVisible() const;
    void setLabelsFormat(const QString &format);
    QString labelsFormat() const;
    void setLabelsAngle(qreal angle);
    qreal labelsAngle() const;
    void setLabelsPosition(QAbstractBarSeries::LabelsPosition position);
    QAbstractBarSeries::LabelsPosition labelsPosition() const;
    void setLabelsPrecision(int precision);
    int labelsPrecision() const;

signals:
    /*!
        This signal is emitted when the user clicks the bar at \a index of \a barset.
    */
    void clicked(int index, QBarSet *barset) { emitSignal(&QAbstractBarSeries::clicked, index, barset); }
    /*!
        This signal is emitted when the mouse enters or leaves the bar at \a index of \a barset; \a status is
        \c true when it enters.
    */
    void hovered(bool status, int index, QBarSet *barset) {
        emitSignal(&QAbstractBarSeries::hovered, status, index, barset);
    }
    /*!
        This signal is emitted when the user presses a mouse button on the bar at \a index of \a barset.
    */
    void pressed(int index, QBarSet *barset) { emitSignal(&QAbstractBarSeries::pressed, index, barset); }
    /*!
        This signal is emitted when the user releases a mouse button on the bar at \a index of \a barset.
    */
    void released(int index, QBarSet *barset) { emitSignal(&QAbstractBarSeries::released, index, barset); }
    /*!
        This signal is emitted when the user double-clicks the bar at \a index of \a barset.
    */
    void doubleClicked(int index, QBarSet *barset) { emitSignal(&QAbstractBarSeries::doubleClicked, index, barset); }
    /*!
        This signal is emitted when the number of bar sets changes.
    */
    void countChanged() { emitSignal(&QAbstractBarSeries::countChanged); }
    /*!
        This signal is emitted when the value labels are shown or hidden.
    */
    void labelsVisibleChanged() { emitSignal(&QAbstractBarSeries::labelsVisibleChanged); }
    /*!
        This signal is emitted when the format of the value labels changes to \a format.
    */
    void labelsFormatChanged(const QString &format) { emitSignal(&QAbstractBarSeries::labelsFormatChanged, format); }
    /*!
        This signal is emitted when the position of the value labels changes to \a position.
    */
    void labelsPositionChanged(QAbstractBarSeries::LabelsPosition position) {
        emitSignal(&QAbstractBarSeries::labelsPositionChanged, position);
    }
    /*!
        This signal is emitted when the angle of the value labels changes to \a angle.
    */
    void labelsAngleChanged(qreal angle) { emitSignal(&QAbstractBarSeries::labelsAngleChanged, angle); }
    /*!
        This signal is emitted when the precision of the value labels changes to \a precision.
    */
    void labelsPrecisionChanged(int precision) { emitSignal(&QAbstractBarSeries::labelsPrecisionChanged, precision); }
    /*!
        This signal is emitted when \a sets are added to the series.
    */
    void barsetsAdded(const QList<QBarSet *> &sets) { emitSignal(&QAbstractBarSeries::barsetsAdded, sets); }
    /*!
        This signal is emitted when \a sets are removed from the series.
    */
    void barsetsRemoved(const QList<QBarSet *> &sets) { emitSignal(&QAbstractBarSeries::barsetsRemoved, sets); }

protected:
    explicit QAbstractBarSeries(QObject *parent = nullptr);

private:
    friend class QtChartsPrivate::ChartEngine;
    bool adopt(QBarSet *set);

    QList<QBarSet *> m_sets;
    qreal m_barWidth = 0.5;
    bool m_labelsVisible = false;
    QString m_labelsFormat;
    qreal m_labelsAngle = 0.0;
    LabelsPosition m_labelsPosition = LabelsCenter;
    int m_labelsPrecision = 6;
};
