// MIT License - see LICENSE
#pragma once

#include <QtCharts/qabstractseries.h>
#include <QtCharts/qpieslice.h>
#include <QtCore/qlist.h>

class QPieSeries : public QAbstractSeries {
    Q_OBJECT
    Q_PROPERTY(qreal horizontalPosition READ horizontalPosition WRITE setHorizontalPosition)
    Q_PROPERTY(qreal verticalPosition READ verticalPosition WRITE setVerticalPosition)
    Q_PROPERTY(qreal size READ pieSize WRITE setPieSize)
    Q_PROPERTY(qreal startAngle READ pieStartAngle WRITE setPieStartAngle)
    Q_PROPERTY(qreal endAngle READ pieEndAngle WRITE setPieEndAngle)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(qreal sum READ sum NOTIFY sumChanged)
    Q_PROPERTY(qreal holeSize READ holeSize WRITE setHoleSize)
public:
    explicit QPieSeries(QObject *parent = nullptr);
    ~QPieSeries() override;

    QAbstractSeries::SeriesType type() const override;

    bool append(QPieSlice *slice);
    bool append(const QList<QPieSlice *> &slices);
    QPieSeries &operator<<(QPieSlice *slice);
    QPieSlice *append(const QString &label, qreal value);
    bool insert(int index, QPieSlice *slice);
    bool remove(QPieSlice *slice);
    bool take(QPieSlice *slice);
    void clear();

    QList<QPieSlice *> slices() const;
    int count() const;
    bool isEmpty() const;
    qreal sum() const;

    void setHoleSize(qreal holeSize);
    qreal holeSize() const;
    void setHorizontalPosition(qreal relativePosition);
    qreal horizontalPosition() const;
    void setVerticalPosition(qreal relativePosition);
    qreal verticalPosition() const;
    void setPieSize(qreal relativeSize);
    qreal pieSize() const;
    void setPieStartAngle(qreal startAngle);
    qreal pieStartAngle() const;
    void setPieEndAngle(qreal endAngle);
    qreal pieEndAngle() const;

    void setLabelsVisible(bool visible = true);
    void setLabelsPosition(QPieSlice::LabelPosition position);

signals:
    /*!
        This signal is emitted when \a slices are added to the series.
    */
    void added(const QList<QPieSlice *> &slices) { emitSignal(&QPieSeries::added, slices); }
    /*!
        This signal is emitted when \a slices are removed from the series.
    */
    void removed(const QList<QPieSlice *> &slices) { emitSignal(&QPieSeries::removed, slices); }
    /*!
        This signal is emitted when the user clicks \a slice.
    */
    void clicked(QPieSlice *slice) { emitSignal(&QPieSeries::clicked, slice); }
    /*!
        This signal is emitted when the mouse enters or leaves \a slice; \a state is \c true when it enters.
    */
    void hovered(QPieSlice *slice, bool state) { emitSignal(&QPieSeries::hovered, slice, state); }
    /*!
        This signal is emitted when the user presses a mouse button on \a slice.
    */
    void pressed(QPieSlice *slice) { emitSignal(&QPieSeries::pressed, slice); }
    /*!
        This signal is emitted when the user releases a mouse button on \a slice.
    */
    void released(QPieSlice *slice) { emitSignal(&QPieSeries::released, slice); }
    /*!
        This signal is emitted when the user double-clicks \a slice.
    */
    void doubleClicked(QPieSlice *slice) { emitSignal(&QPieSeries::doubleClicked, slice); }
    /*!
        This signal is emitted when the number of slices changes.
    */
    void countChanged() { emitSignal(&QPieSeries::countChanged); }
    /*!
        This signal is emitted when the sum of the values changes.
    */
    void sumChanged() { emitSignal(&QPieSeries::sumChanged); }

private:
    friend class QtChartsPrivate::ChartEngine;
    friend class QPieSlice;
    bool adopt(QPieSlice *slice);
    void updateAngles();

    QList<QPieSlice *> m_slices;
    qreal m_sum = 0.0;
    qreal m_holeSize = 0.0;
    qreal m_horizontal = 0.5;
    qreal m_vertical = 0.5;
    qreal m_size = 0.7;
    qreal m_startAngle = 0.0;
    qreal m_endAngle = 360.0;
};
