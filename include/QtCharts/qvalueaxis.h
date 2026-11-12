// MIT License - see LICENSE
#pragma once

#include <QtCharts/qabstractaxis.h>

class QValueAxis : public QAbstractAxis {
    Q_OBJECT
    Q_PROPERTY(int tickCount READ tickCount WRITE setTickCount NOTIFY tickCountChanged)
    Q_PROPERTY(qreal min READ min WRITE setMin NOTIFY minChanged)
    Q_PROPERTY(qreal max READ max WRITE setMax NOTIFY maxChanged)
    Q_PROPERTY(QString labelFormat READ labelFormat WRITE setLabelFormat NOTIFY labelFormatChanged)
    Q_PROPERTY(int minorTickCount READ minorTickCount WRITE setMinorTickCount NOTIFY minorTickCountChanged)
    Q_PROPERTY(qreal tickAnchor READ tickAnchor WRITE setTickAnchor NOTIFY tickAnchorChanged)
    Q_PROPERTY(qreal tickInterval READ tickInterval WRITE setTickInterval NOTIFY tickIntervalChanged)
    Q_PROPERTY(TickType tickType READ tickType WRITE setTickType NOTIFY tickTypeChanged)
public:
    enum TickType { TicksDynamic = 0, TicksFixed };
    Q_ENUM(TickType)

    explicit QValueAxis(QObject *parent = nullptr);
    ~QValueAxis() override;

    AxisType type() const override;

    void setMin(qreal min);
    qreal min() const;
    void setMax(qreal max);
    qreal max() const;
    void setRange(qreal min, qreal max);

    void setTickCount(int count);
    int tickCount() const;
    void setMinorTickCount(int count);
    int minorTickCount() const;
    void setTickAnchor(qreal anchor);
    qreal tickAnchor() const;
    void setTickInterval(qreal interval);
    qreal tickInterval() const;
    void setTickType(QValueAxis::TickType type);
    QValueAxis::TickType tickType() const;

    void setLabelFormat(const QString &format);
    QString labelFormat() const;

public slots:
    void applyNiceNumbers();

signals:
    /*!
        This signal is emitted when the minimum of the axis changes to \a min.
    */
    void minChanged(qreal min) { emitSignal(&QValueAxis::minChanged, min); }
    /*!
        This signal is emitted when the maximum of the axis changes to \a max.
    */
    void maxChanged(qreal max) { emitSignal(&QValueAxis::maxChanged, max); }
    /*!
        This signal is emitted when the range of the axis changes to \a min and \a max.
    */
    void rangeChanged(qreal min, qreal max) { emitSignal(&QValueAxis::rangeChanged, min, max); }
    /*!
        This signal is emitted when the number of ticks changes to \a tickCount.
    */
    void tickCountChanged(int tickCount) { emitSignal(&QValueAxis::tickCountChanged, tickCount); }
    /*!
        This signal is emitted when the number of minor ticks changes to \a tickCount.
    */
    void minorTickCountChanged(int tickCount) { emitSignal(&QValueAxis::minorTickCountChanged, tickCount); }
    /*!
        This signal is emitted when the label format changes to \a format.
    */
    void labelFormatChanged(const QString &format) { emitSignal(&QValueAxis::labelFormatChanged, format); }
    /*!
        This signal is emitted when the tick interval changes to \a interval.
    */
    void tickIntervalChanged(qreal interval) { emitSignal(&QValueAxis::tickIntervalChanged, interval); }
    /*!
        This signal is emitted when the tick anchor changes to \a anchor.
    */
    void tickAnchorChanged(qreal anchor) { emitSignal(&QValueAxis::tickAnchorChanged, anchor); }
    /*!
        This signal is emitted when the tick type changes to \a type.
    */
    void tickTypeChanged(QValueAxis::TickType type) { emitSignal(&QValueAxis::tickTypeChanged, type); }

protected:
    void setRangeFromVariant(const QVariant &min, const QVariant &max) override;

private:
    qreal m_min = 0.0;
    qreal m_max = 0.0;
    int m_tickCount = 5;
    int m_minorTickCount = 0;
    qreal m_tickAnchor = 0.0;
    qreal m_tickInterval = 0.0;
    TickType m_tickType = TicksFixed;
    QString m_labelFormat;
};
