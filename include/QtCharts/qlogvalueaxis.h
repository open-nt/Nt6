// MIT License - see LICENSE
#pragma once

#include <QtCharts/qabstractaxis.h>

class QLogValueAxis : public QAbstractAxis {
    Q_OBJECT
    Q_PROPERTY(qreal min READ min WRITE setMin NOTIFY minChanged)
    Q_PROPERTY(qreal max READ max WRITE setMax NOTIFY maxChanged)
    Q_PROPERTY(QString labelFormat READ labelFormat WRITE setLabelFormat NOTIFY labelFormatChanged)
    Q_PROPERTY(qreal base READ base WRITE setBase NOTIFY baseChanged)
    Q_PROPERTY(int tickCount READ tickCount NOTIFY tickCountChanged)
    Q_PROPERTY(int minorTickCount READ minorTickCount WRITE setMinorTickCount NOTIFY minorTickCountChanged)
public:
    explicit QLogValueAxis(QObject *parent = nullptr);
    ~QLogValueAxis() override;

    AxisType type() const override;

    void setMin(qreal min);
    qreal min() const;
    void setMax(qreal max);
    qreal max() const;
    void setRange(qreal min, qreal max);

    void setLabelFormat(const QString &format);
    QString labelFormat() const;
    void setBase(qreal base);
    qreal base() const;
    int tickCount() const;
    void setMinorTickCount(int minorTickCount);
    int minorTickCount() const;

signals:
    /*!
        This signal is emitted when the minimum of the axis changes to \a min.
    */
    void minChanged(qreal min) { emitSignal(&QLogValueAxis::minChanged, min); }
    /*!
        This signal is emitted when the maximum of the axis changes to \a max.
    */
    void maxChanged(qreal max) { emitSignal(&QLogValueAxis::maxChanged, max); }
    /*!
        This signal is emitted when the range of the axis changes to \a min and \a max.
    */
    void rangeChanged(qreal min, qreal max) { emitSignal(&QLogValueAxis::rangeChanged, min, max); }
    /*!
        This signal is emitted when the label format changes to \a format.
    */
    void labelFormatChanged(const QString &format) { emitSignal(&QLogValueAxis::labelFormatChanged, format); }
    /*!
        This signal is emitted when the base of the logarithm changes to \a base.
    */
    void baseChanged(qreal base) { emitSignal(&QLogValueAxis::baseChanged, base); }
    /*!
        This signal is emitted when the number of ticks changes to \a tickCount.
    */
    void tickCountChanged(int tickCount) { emitSignal(&QLogValueAxis::tickCountChanged, tickCount); }
    /*!
        This signal is emitted when the number of minor ticks changes to \a minorTickCount.
    */
    void minorTickCountChanged(int minorTickCount) {
        emitSignal(&QLogValueAxis::minorTickCountChanged, minorTickCount);
    }

protected:
    void setRangeFromVariant(const QVariant &min, const QVariant &max) override;

private:
    void updateTickCount();

    qreal m_min = 1.0;
    qreal m_max = 1.0;
    qreal m_base = 10.0;
    int m_tickCount = 0;
    int m_minorTickCount = 0;
    QString m_labelFormat;
};
