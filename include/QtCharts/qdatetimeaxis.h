// MIT License - see LICENSE
#pragma once

#include <QtCharts/qabstractaxis.h>
#include <QtCore/qdatetime.h>

class QDateTimeAxis : public QAbstractAxis {
    Q_OBJECT
    Q_PROPERTY(int tickCount READ tickCount WRITE setTickCount NOTIFY tickCountChanged)
    Q_PROPERTY(QDateTime min READ min WRITE setMin NOTIFY minChanged)
    Q_PROPERTY(QDateTime max READ max WRITE setMax NOTIFY maxChanged)
    Q_PROPERTY(QString format READ format WRITE setFormat NOTIFY formatChanged)
public:
    explicit QDateTimeAxis(QObject *parent = nullptr);
    ~QDateTimeAxis() override;

    AxisType type() const override;

    void setMin(QDateTime min);
    QDateTime min() const;
    void setMax(QDateTime max);
    QDateTime max() const;
    void setRange(QDateTime min, QDateTime max);

    void setFormat(const QString &format);
    QString format() const;
    void setTickCount(int count);
    int tickCount() const;

signals:
    /*!
        This signal is emitted when the minimum of the axis changes to \a min.
    */
    void minChanged(QDateTime min) { emitSignal(&QDateTimeAxis::minChanged, min); }
    /*!
        This signal is emitted when the maximum of the axis changes to \a max.
    */
    void maxChanged(QDateTime max) { emitSignal(&QDateTimeAxis::maxChanged, max); }
    /*!
        This signal is emitted when the range of the axis changes to \a min and \a max.
    */
    void rangeChanged(QDateTime min, QDateTime max) { emitSignal(&QDateTimeAxis::rangeChanged, min, max); }
    /*!
        This signal is emitted when the format of the labels changes to \a format.
    */
    void formatChanged(const QString &format) { emitSignal(&QDateTimeAxis::formatChanged, format); }
    /*!
        This signal is emitted when the number of ticks changes to \a tick.
    */
    void tickCountChanged(int tick) { emitSignal(&QDateTimeAxis::tickCountChanged, tick); }

protected:
    void setRangeFromVariant(const QVariant &min, const QVariant &max) override;

private:
    friend class QtChartsPrivate::ChartEngine;
    void setRangeMSecs(qreal min, qreal max);

    qreal m_min = 0.0; // milliseconds since the epoch
    qreal m_max = 0.0;
    int m_tickCount = 5;
    QString m_format;
};
